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
#include <unordered_set>
#include <steam/steam_api.h>

extern "C" {
    #include <lua.h>
}

const char* MoonLoader::GMOD_LUA_PATH_ID = nullptr;
IFileSystem* g_pFullFileSystem = nullptr;
Detouring::Hook lua_getinfo_hook;
std::unordered_set<std::string> g_IncludedFiles;

using namespace MoonLoader;

std::unique_ptr<GarrysMod::Lua::ILuaInterface> MoonLoader::g_pLua;
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

namespace LuaFuncs {
    LUA_FUNCTION(ToLua) {
        const char* code = LUA->CheckString(1);
        MoonEngine::Engine::CompiledLines lines;
        std::string compiledCode = g_pMoonEngine->CompileString(code, &lines);
        
        // lua_code
        LUA->PushString(compiledCode.c_str());

        // line_table
        LUA->CreateTable();
        for (auto& line : lines) {
            LUA->PushNumber(line.first);
            LUA->PushNumber(line.second);
            LUA->SetTable(-3);
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
}

class ILuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, ILuaInterfaceProxy> {
public:
    bool Init() {
        Initialize(g_pLua.get());
        return Hook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, &ILuaInterfaceProxy::FindAndRunScript) &&
            Hook(&GarrysMod::Lua::ILuaInterface::Cycle, &ILuaInterfaceProxy::Cycle);
    }

    void Deinit() {
        UnHook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript);
        UnHook(&GarrysMod::Lua::ILuaInterface::Cycle);
    }

    virtual void Cycle() {
        Call(&GarrysMod::Lua::ILuaInterface::Cycle);
        if (This() == g_pLua.get()) {
            g_pWatchdog->Think(); // Watch for file changes
            SteamAPI_RunCallbacks();
        }
    }

    virtual bool FindAndRunScript(const char* _fileName, bool run, bool showErrors, const char* runReason, bool noReturns) {
        // Just for safety
        if (_fileName == NULL)
            return false;

        std::string fileName = _fileName;

        // Only do compilation in our realm
        // Hmm, maybe it would be cool if you load moonloader in menu state, and then you can use it in server or client state for example
        if (This() == g_pLua.get()) {
            bool isMoonScript = Filesystem::FileExtension(fileName) == "moon";
            bool isReload = strcmp(runReason, "!RELOAD") == 0;
            if (isMoonScript) {
                // Change to .lua, so we will load compiled version
                Filesystem::SetFileExtension(fileName, "lua");
            }
            
            // Path to .moon script
            std::string moonPath = fileName;
            Filesystem::SetFileExtension(moonPath, "moon");

            if (g_pFilesystem->IsFile(runReason, "GAME")) {
                // First we need to check if we are loading .moon script
                // relative to the current script
                std::string baseDir = g_pFilesystem->TransverseRelativePath(runReason, "GAME", GMOD_LUA_PATH_ID);
                Filesystem::StripFileName(baseDir);
                std::string fullMoonPath = Utils::JoinPaths(baseDir, moonPath);
                if (g_pFilesystem->IsFile(fullMoonPath, GMOD_LUA_PATH_ID)) {
                    moonPath = fullMoonPath;
                }
            }

            // First, check if file exists
            if (g_pFilesystem->IsFile(moonPath, GMOD_LUA_PATH_ID)) {
                // Ignore !RELOAD requests, otherwise we'll get stuck in a loop
                // Writing to .lua files causes a reload, which causes a compile, which causes a reload, etc.
                // Also ignore .lua files that we didn't included
                bool wasIncluded = g_IncludedFiles.find(moonPath) != g_IncludedFiles.end();
                if (isReload) {
                    if (!isMoonScript)
                        return false;

                    if (!wasIncluded) {
                        DevWarning("[Moonloader] %s was not included before, ignoring auto-reload request\n", moonPath.c_str());
                        return false;
                    }
                }

                if (!g_pCompiler->CompileMoonScript(moonPath)) {
                    Warning("[Moonloader] Failed to compile %s\n", moonPath.c_str());
                    return false;
                }

                if (!wasIncluded) {
                    g_IncludedFiles.insert(moonPath);
                }
            }
        }

        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName.c_str(), run, showErrors, runReason, noReturns);
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
            std::string path = g_pFilesystem->RelativeToFullPath(ar->short_src, "GAME");
            path = g_pFilesystem->FullToRelativePath(path, GMOD_LUA_PATH_ID);
            Filesystem::SetFileExtension(path, "moon");
            Filesystem::Normalize(path);

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

    g_pLua = std::unique_ptr<GarrysMod::Lua::ILuaInterface>(reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA));
    MoonLoader::GMOD_LUA_PATH_ID = g_pLua->IsServer() ? "lsv" : g_pLua->IsClient() ? "lcl" : "LuaMenu";

    g_pFullFileSystem = InterfacePointers::FileSystem();
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

    // Detour lua_getinfo and lua_pcall, so we can manipulate error stack traces
    SourceSDK::ModuleLoader lua_shared("lua_shared");
    if (!lua_getinfo_hook.Create(
        lua_shared.GetSymbol("lua_getinfo"),
        reinterpret_cast<void*>(&lua_getinfo_detour)
    )) LUA->ThrowError("failed to detour debug.getinfo");
    if (!lua_getinfo_hook.Enable()) LUA->ThrowError("failed to enable debug.getinfo detour");

    StartVersionCheck(g_pLua.get());

    return 0;
}

GMOD_MODULE_CLOSE() {
    // Deinitialize lua detouring
    ILuaInterfaceProxy::Singleton->Deinit();
    delete ILuaInterfaceProxy::Singleton;
    ILuaInterfaceProxy::Singleton = nullptr;

    lua_getinfo_hook.Destroy();

    // Release all our interfaces
    g_pWatchdog.release();
    g_pCompiler.release();
    g_pMoonEngine.release();
    g_pFilesystem.release();
    g_pLua.release();

    return 0;
}