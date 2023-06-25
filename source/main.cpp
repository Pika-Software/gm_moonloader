#include "global.hpp"
#include "compiler.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"
#include "utils.hpp"
#include "config.hpp"

#include <moonengine/engine.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <detouring/classproxy.hpp>
#include <filesystem.h>
#include <GarrysMod/InterfacePointers.hpp>
#include <detouring/hook.hpp>
#include <GarrysMod/ModuleLoader.hpp>
#include <GarrysMod/FactoryLoader.hpp>
#include <unordered_set>

extern "C" {
    #include <lua.h>
}

const char* MoonLoader::GMOD_LUA_PATH_ID = nullptr;
IFileSystem* g_pFullFileSystem = nullptr;
Detouring::Hook lua_getinfo_hook;
std::unordered_set<std::string> g_IncludedFiles;
int g_AddCSLuaFileRef = -1;

using namespace MoonLoader;

GarrysMod::Lua::ILuaInterface* MoonLoader::g_pLua = nullptr;
std::unique_ptr<MoonEngine::Engine> MoonLoader::g_pMoonEngine;
std::unique_ptr<Compiler> MoonLoader::g_pCompiler;
std::unique_ptr<Watchdog> MoonLoader::g_pWatchdog;
std::unique_ptr<Filesystem> MoonLoader::g_pFilesystem;

bool CPreCacheFile(const std::string& path) {
    DevMsg("[Moonloader] Precaching %s\n", path.c_str());
    return g_pCompiler->CompileMoonScript(path);
}

void CPreCacheDir(const std::string& startPath) {
    for (auto file : g_pFilesystem->Find(Filesystem::Join(startPath, "*"), GMOD_LUA_PATH_ID)) {
        std::string path = Filesystem::Join(startPath, file);
        if (g_pFilesystem->IsDirectory(path, GMOD_LUA_PATH_ID)) {
            CPreCacheDir(path);
        }
        else if (Filesystem::FileExtension(path) == "moon") {
            CPreCacheFile(path);
        }
    }
}

template<class T>
T* LoadInterface(const char* moduleName, const char* version) {
    SourceSDK::FactoryLoader module(moduleName);
    return module.GetInterface<T>(version);
}

IFileSystem* LoadFilesystem() {
#ifdef CLIENT_DLL
    return InterfacePointers::Internal::Client::FileSystem();
#else
    IFileSystem* ptr;
    if ((ptr = LoadInterface<IFileSystem>("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION)) != nullptr) 
        return ptr;
    
    if ((ptr = LoadInterface<IFileSystem>("dedicated", FILESYSTEM_INTERFACE_VERSION)) != nullptr) 
        return ptr;
    
    if ((ptr = LoadInterface<IFileSystem>("server", FILESYSTEM_INTERFACE_VERSION)) != nullptr) 
        return ptr;
#endif
}

namespace LuaFuncs {
    LUA_FUNCTION(ToLua) {
        LUA->CheckType(1, GarrysMod::Lua::Type::String);
        unsigned int codeLen = 0;
        const char* code = LUA->GetString(1, &codeLen);

        std::string result;
        MoonEngine::Engine::CompiledLines lines;
        bool success = g_pMoonEngine->CompileStringEx(code, codeLen, result, &lines);

        if (success) {
            // lua_code
            LUA->PushString(result.c_str(), result.size());

            // line_table
            LUA->CreateTable();
            for (auto& line : lines) {
                LUA->PushNumber(line.first);
                LUA->PushNumber(line.second);
                LUA->SetTable(-3);
            }
        } else {
            LUA->PushNil();
            LUA->PushString(result.c_str(), result.size());
        }

        return 2;
    }

    LUA_FUNCTION(PreCacheDir) {
        CPreCacheDir(LUA->CheckString(1));
        return 0;
    }

    LUA_FUNCTION(PreCacheFile) {
        LUA->PushBool(CPreCacheFile(LUA->CheckString(1)));
        return 1;
    }

    LUA_FUNCTION(AddCSLuaFile) {
        if (g_AddCSLuaFileRef != -1) {
            if (!LUA->IsType(1, GarrysMod::Lua::Type::String)) {
                LUA->ReferencePush(g_AddCSLuaFileRef);
                LUA->Call(0, 0);
                return 0;
            }

            std::string targetFile = LUA->GetString(1);
            if (Utils::FindMoonScript(targetFile)) {
                g_pCompiler->CompileMoonScript(targetFile);
                Filesystem::SetFileExtension(targetFile, "lua");
            }

            LUA->ReferencePush(g_AddCSLuaFileRef);
            LUA->PushString(targetFile.c_str());
            LUA->Call(1, 0);
        }
        return 0;
    }
}

class ILuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, ILuaInterfaceProxy> {
public:
    bool Init() {
        Initialize(g_pLua);
        return Hook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, &ILuaInterfaceProxy::FindAndRunScript) &&
            Hook(&GarrysMod::Lua::ILuaInterface::Cycle, &ILuaInterfaceProxy::Cycle);
    }

    void Deinit() {
        UnHook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript);
        UnHook(&GarrysMod::Lua::ILuaInterface::Cycle);
    }

    virtual void Cycle() {
        Call(&GarrysMod::Lua::ILuaInterface::Cycle);
        if (This() == g_pLua) {
            g_pWatchdog->Think(); // Watch for file changes
        }
    }

    virtual bool FindAndRunScript(const char* fileName, bool run, bool showErrors, const char* runReason, bool noReturns) {
        if (This() != g_pLua || fileName == NULL) {
            return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);
        }

        std::string path = fileName;
        if (Utils::FindMoonScript(path)) {
            g_pCompiler->CompileMoonScript(path);

            if (runReason[0] != '!') {
                // Usually when runReason doesn't start with "!",
                // it means that it was included by "include"
                // Allow auto-reloads for this guy in a future
                g_IncludedFiles.insert(path);
            }

            if (strcmp(runReason, "!RELOAD") == 0) {
                // All my homies hate auto-reloads by gmod
                return false;
            }

            if (strcmp(runReason, "!MOONRELOAD") == 0) {
                // Auto-reloads by moonloader is da best defacto
                runReason = "!RELOAD";
                if (g_IncludedFiles.find(path) == g_IncludedFiles.end()) {
                    // File wasn't included before? *heavy voice* Not good.
                    return false;
                }
            }

            // Alrighty, everything safe and we can with no worries compile moonscript! Yay!
            g_pCompiler->CompileMoonScript(path);

            path = fileName; // Preserve original file path for the god's sake
            Utils::SetFileExtension(path, "lua"); // Do not forget to pass lua file to gmod!!
        }

        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, path.c_str(), run, showErrors, runReason, noReturns);
    }

    static ILuaInterfaceProxy* Singleton;
};
ILuaInterfaceProxy* ILuaInterfaceProxy::Singleton;

typedef int (*lua_getinfo_t)(lua_State* L, const char* what, lua_Debug* ar);
int lua_getinfo_detour(lua_State* L, const char* what, lua_Debug* ar) {
    int ret = lua_getinfo_hook.GetTrampoline<lua_getinfo_t>()(L, what, ar);
    if (ret != 0) {
        // File stored in cache/moonloader, so it must be our compiled script?
        if (Utils::StartsWith(ar->short_src, "cache/moonloader/lua")) {
            // Yeah, that's weird path transversies, but it works
            std::string path = g_pFilesystem->TransverseRelativePath(ar->short_src, "GAME", GMOD_LUA_PATH_ID);
            Filesystem::SetFileExtension(path, "moon");

            auto debugInfo = g_pCompiler->GetDebugInfo(path);
            if (debugInfo) {
                strncpy(ar->short_src, debugInfo->fullSourcePath.c_str(), sizeof(ar->short_src));
                ar->currentline = debugInfo->lines[ar->currentline];
            }
        }
    }
    return ret;
}

GMOD_MODULE_OPEN() {
    DevMsg("Moonloader %s-%s made by Pika-Software (%s)\n", MOONLOADER_VERSION, MOONLOADER_GIT_HASH, MOONLOADER_URL);

    g_pLua = reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA);
    MoonLoader::GMOD_LUA_PATH_ID = g_pLua->GetPathID();

    g_pFullFileSystem = LoadFilesystem();
    if (!g_pFullFileSystem)
        LUA->ThrowError("failed to get IFileSystem");

    g_pFilesystem = std::make_unique<Filesystem>(g_pFullFileSystem);
    g_pMoonEngine = std::make_unique<MoonEngine::Engine>();
    g_pCompiler = std::make_unique<Compiler>();
    g_pWatchdog = std::make_unique<Watchdog>();

    if (!g_pMoonEngine->IsInitialized())
        LUA->ThrowError("failed to initialize moonengine");

    ILuaInterfaceProxy::Singleton = new ILuaInterfaceProxy;
    if (!ILuaInterfaceProxy::Singleton->Init())
        LUA->ThrowError("failed to initialize lua detouring");

    // Cleanup old cache
    int removed = g_pFilesystem->Remove("cache/moonloader/lua", "GAME");
    DevMsg("[Moonloader] Removed %d files from cache\n", removed);

    // Create cache directories, and add them to search path
    g_pFilesystem->CreateDirs("cache/moonloader/lua");
    g_pFullFileSystem->AddSearchPath("garrysmod/cache/moonloader", "GAME", PATH_ADD_TO_HEAD);
    g_pFullFileSystem->AddSearchPath("garrysmod/cache/moonloader/lua", MoonLoader::GMOD_LUA_PATH_ID, PATH_ADD_TO_HEAD);
    g_pFullFileSystem->AddSearchPath("garrysmod/cache/moonloader/lua", "MOONLOADER", PATH_ADD_TO_HEAD);

    LUA->CreateTable();
        LUA->PushString("gm_moonloader"); LUA->SetField(-2, "_NAME");
        LUA->PushString("Pika-Software"); LUA->SetField(-2, "_AUTHORS");
        LUA->PushString(MOONLOADER_VERSION "-" MOONLOADER_GIT_HASH); LUA->SetField(-2, "_VERSION");
        LUA->PushNumber(MOONLOADER_VERSION_MAJOR); LUA->SetField(-2, "_VERSION_MAJOR");
        LUA->PushNumber(MOONLOADER_VERSION_MINOR); LUA->SetField(-2, "_VERSION_MINOR");
        LUA->PushNumber(MOONLOADER_VERSION_PATCH); LUA->SetField(-2, "_VERSION_PATCH");
        LUA->PushString(MOONLOADER_URL); LUA->SetField(-2, "_URL");
        
        LUA->PushCFunction(LuaFuncs::ToLua); LUA->SetField(-2, "ToLua");
        LUA->PushCFunction(LuaFuncs::PreCacheDir); LUA->SetField(-2, "PreCacheDir");
        LUA->PushCFunction(LuaFuncs::PreCacheFile); LUA->SetField(-2, "PreCacheFile");
    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "moonloader");

    // Detour AddCSLuaFile
    if (g_pLua->IsServer()) {
        LUA->GetField(GarrysMod::Lua::INDEX_GLOBAL, "AddCSLuaFile");
        if (LUA->IsType(-1, GarrysMod::Lua::Type::Function))
            g_AddCSLuaFileRef = LUA->ReferenceCreate();
        else
            LUA->Pop();

        LUA->PushCFunction(LuaFuncs::AddCSLuaFile);
        LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "AddCSLuaFile");
    }

    // Detour lua_getinfo and lua_pcall, so we can manipulate error stack traces
    SourceSDK::ModuleLoader lua_shared("lua_shared");
    if (!lua_getinfo_hook.Create(
        lua_shared.GetSymbol("lua_getinfo"),
        reinterpret_cast<void*>(&lua_getinfo_detour)
    )) LUA->ThrowError("failed to detour debug.getinfo");
    if (!lua_getinfo_hook.Enable()) LUA->ThrowError("failed to enable debug.getinfo detour");

    StartVersionCheck(g_pLua);

    return 0;
}

GMOD_MODULE_CLOSE() {
    g_AddCSLuaFileRef = -1;

    // Deinitialize lua detouring
    ILuaInterfaceProxy::Singleton->Deinit();
    delete ILuaInterfaceProxy::Singleton;
    ILuaInterfaceProxy::Singleton = nullptr;

    if (!lua_getinfo_hook.Disable())
        Warning("[Moonloader] Failed to disable lua_getinfo detour\n");
    if (!lua_getinfo_hook.Destroy())
        Warning("[Moonloader] Failed to destroy lua_getinfo detour\n");

    // Release all our interfaces
    g_pWatchdog.release();
    g_pCompiler.release();
    g_pMoonEngine.release();
    g_pFilesystem.release();

    return 0;
}