#include "global.hpp"
#include "compiler.hpp"
#include "watchdog.hpp"

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

bool CPreCacheFile(const std::string& path) {
    DevMsg("[Moonloader] Precaching %s\n", path.c_str());
    return g_pCompiler->CompileMoonScript(path);
}

void CPreCacheDir(const std::string& startPath) {
    std::string searchPath = startPath + "/*";

    FileFindHandle_t findHandle; // note: FileFINDHandle
    const char* pFilename = g_pFullFileSystem->FindFirstEx(searchPath.c_str(), GMOD_LUA_PATH_ID, &findHandle);
    while (pFilename) {
        if (pFilename[0] != '.') {
            std::string path = startPath + "/" + pFilename;
            const char* fileExt = V_GetFileExtension(path.c_str());
            if (g_pFullFileSystem->IsDirectory(path.c_str(), GMOD_LUA_PATH_ID)) {
                CPreCacheDir(path);
            } else if (fileExt != nullptr && strcmp(fileExt, "moon") == 0) {
                CPreCacheFile(path);
            }
        };

        pFilename = g_pFullFileSystem->FindNext(findHandle);
    }
    g_pFullFileSystem->FindClose(findHandle);
}

namespace LuaFuncs {
    LUA_FUNCTION(ToLua) {
        const char* code = LUA->CheckString(1);
        std::string compiledCode = g_pMoonEngine->CompileString(code);
        LUA->PushString(compiledCode.c_str());
        return 1;
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
        std::string fileName = _fileName;

        // Msg("[%s] %s\n", runReason, fileName.c_str());

        // Only do compilation in our realm
        // Hmm, maybe it would be cool if you load moonloader in server state, and then you can use it in menu state for example
        if (This() == g_pLua.get()) {
            std::string fileExt = V_GetFileExtension(fileName.c_str());
            if (fileExt == "moon") {
                // Replace .moon with .lua
                fileName = fileName.substr(0, fileName.find_last_of(".")) + ".lua";
            }

            // Path to .moon script
            std::string moonPath = fileName.substr(0, fileName.find_last_of(".")) + ".moon";
            if (g_pFullFileSystem->FileExists(moonPath.c_str(), MoonLoader::GMOD_LUA_PATH_ID)) {
                // Ignore !RELOAD requests, otherwise we'll get stuck in a loop
                // Writing to .lua files causes a reload, which causes a compile, which causes a reload, etc.
                if (strcmp(runReason, "!RELOAD") == 0 && fileExt != "moon") {
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

    g_pMoonEngine = std::make_unique<MoonEngine::Engine>();
    g_pCompiler = std::make_unique<Compiler>();
    g_pWatchdog = std::make_unique<Watchdog>();
    g_pFullFileSystem = InterfacePointers::FileSystem();
    if (!g_pFullFileSystem)
        LUA->ThrowError("failed to get IFileSystem");

    if (!g_pMoonEngine->IsInitialized())
        LUA->ThrowError("failed to initialize moonengine");

    ILuaInterfaceProxy::Singleton = new ILuaInterfaceProxy;
    if (!ILuaInterfaceProxy::Singleton->Init())
        LUA->ThrowError("failed to initialize lua detouring");

    g_pFullFileSystem->CreateDirHierarchy("cache/moonloader/lua");
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
    g_pLua.release();

    return 0;
}