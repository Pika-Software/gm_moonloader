#include "global.hpp"
#include "compiler.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"

#include <moonengine/engine.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <detouring/classproxy.hpp>
#include <filesystem.h>
#include <GarrysMod/InterfacePointers.hpp>

const char* MoonLoader::GMOD_LUA_PATH_ID = nullptr;
IFileSystem* g_pFullFileSystem = nullptr;

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
            if (isMoonScript) {
                // Change to .lua, so we will load compiled version
                Filesystem::SetFileExtension(fileName, "lua");
            }
            
            // Path to .moon script
            std::string moonPath = fileName;
            Filesystem::SetFileExtension(moonPath, "moon");
            if (g_pFilesystem->IsFile(moonPath, GMOD_LUA_PATH_ID)) {
                // Ignore !RELOAD requests, otherwise we'll get stuck in a loop
                // Writing to .lua files causes a reload, which causes a compile, which causes a reload, etc.
                if (strcmp(runReason, "!RELOAD") == 0 && !isMoonScript) {
                    return false;
                }

                if (!g_pCompiler->CompileMoonScript(moonPath)) {
                    Warning("[Moonloader] Failed to compile %s\n", moonPath.c_str());
                    return false;
                }
            }
        }

        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName.c_str(), run, showErrors, runReason, noReturns);
    }

    static ILuaInterfaceProxy* Singleton;
};

ILuaInterfaceProxy* ILuaInterfaceProxy::Singleton;

GMOD_MODULE_OPEN() {
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
        LUA->PushCFunction(LuaFuncs::ToLua); LUA->SetField(-2, "ToLua");
        LUA->PushCFunction(LuaFuncs::PreCacheDir); LUA->SetField(-2, "PreCacheDir");
        LUA->PushCFunction(LuaFuncs::PreCacheFile); LUA->SetField(-2, "PreCacheFile");
    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "moonloader");

    return 0;
}

GMOD_MODULE_CLOSE() {
    // Deinitialize lua detouring
    ILuaInterfaceProxy::Singleton->Deinit();
    delete ILuaInterfaceProxy::Singleton;
    ILuaInterfaceProxy::Singleton = nullptr;

    // Release all our interfaces
    g_pWatchdog.release();
    g_pCompiler.release();
    g_pMoonEngine.release();
    g_pFilesystem.release();
    g_pLua.release();

    return 0;
}