#include "global.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "lua_api.hpp"
#include <cstdint>
#include <moonengine/engine.hpp>
#include <GarrysMod/Lua/Interface.h>

#if IS_SERVERSIDE
#include "compiler.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"
#include "autorefresh.hpp"

#include <GarrysMod/InterfacePointers.hpp>

#include <detouring/classproxy.hpp>
#include <detouring/hook.hpp>
#include <filesystem.h>
#include <iserver.h>
#include <unordered_map>

extern "C" {
    #include <lua.h>
}
#endif

using namespace MoonLoader;

std::atomic<int> MoonLoader::g_InitializeCount = 0;
std::unique_ptr<MoonEngine::Engine> MoonLoader::g_MoonEngine;

#if IS_SERVERSIDE
Detouring::Hook lua_getinfo_hook;
std::unordered_map<GarrysMod::Lua::ILuaInterface*, std::unordered_set<std::string>> g_IncludedFiles;

std::unordered_set<GarrysMod::Lua::ILuaInterface*> MoonLoader::g_LuaStates;
IServer* MoonLoader::g_Server = nullptr;
IVEngineServer* MoonLoader::g_EngineServer = nullptr;
std::unique_ptr<Compiler> MoonLoader::g_Compiler;
std::unique_ptr<Watchdog> MoonLoader::g_Watchdog;
std::unique_ptr<Filesystem> MoonLoader::g_Filesystem;

IFileSystem* LoadFilesystem() {
    auto iface = Utils::LoadInterface<IFileSystem>("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION);
    return iface != nullptr ? iface : InterfacePointers::FileSystem();
}

class ILuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, ILuaInterfaceProxy> {
public:
    bool Init(GarrysMod::Lua::ILuaInterface* LUA) {
        Initialize(LUA);
        return Hook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, &ILuaInterfaceProxy::FindAndRunScript) &&
            Hook(&GarrysMod::Lua::ILuaInterface::Cycle, &ILuaInterfaceProxy::Cycle);
    }

    void Deinit() {
        UnHook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript);
        UnHook(&GarrysMod::Lua::ILuaInterface::Cycle);
    }

    virtual void Cycle() {
        Call(&GarrysMod::Lua::ILuaInterface::Cycle);
        g_Watchdog->Think(This()); // Watch for file changes
    }

    virtual bool FindAndRunScript(const char* fileName, bool run, bool showErrors, const char* runReason, bool noReturns) {
        auto LUA = This();
        if (fileName == NULL || LUA->IsClient()) {
            return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);
        }

        std::string path = fileName;
        if (Utils::FindMoonScript(LUA, path)) {
            auto& includedFiles = (*g_IncludedFiles.try_emplace(LUA).first).second;

            if (runReason[0] != '!') {
                // Usually when runReason doesn't start with "!",
                // it means that it was included by "include"
                // Allow auto-reloads for this guy in a future
                includedFiles.insert(path);
            }

            if (strcmp(runReason, "!RELOAD") == 0) {
                // All my homies hate auto-reloads by gmod
                return false;
            }

            if (strcmp(runReason, "!MOONRELOAD") == 0) {
                // Auto-reloads by moonloader is da best defacto
                runReason = "!RELOAD";
                if (includedFiles.find(path) == includedFiles.end()) {
                    // File wasn't included before? *heavy voice* Not good.
                    return false;
                }
            }

            // Alrighty, everything safe and we can with no worries compile moonscript! Yay!
            if (g_Compiler->CompileMoonScript(LUA, path)) {
                // If file was reloaded, then we need to reload it on clients (for OSX only ofc)
                if (strcmp(runReason, "!RELOAD") == 0) {
                    AutoRefresh::Sync(path);
                }
            }

            path = fileName; // Preserve original file path for the god's sake
            Utils::SetFileExtension(path, "lua"); // Do not forget to pass lua file to gmod!!
        }

        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, path.c_str(), run, showErrors, runReason, noReturns);
    }

    static std::unique_ptr<ILuaInterfaceProxy> Singleton;
};
std::unique_ptr<ILuaInterfaceProxy> ILuaInterfaceProxy::Singleton;

typedef int (*lua_getinfo_t)(lua_State* L, const char* what, lua_Debug* ar);
int lua_getinfo_detour(lua_State* L, const char* what, lua_Debug* ar) {
    int ret = lua_getinfo_hook.GetTrampoline<lua_getinfo_t>()(L, what, ar);
    if (ret != 0) {
        // File stored in cache/moonloader/lua, so it must be our compiled script?
        if (Utils::StartsWith(ar->short_src, CACHE_PATH_LUA)) {
            std::string path = ar->short_src;
            Utils::RemovePrefix(path, CACHE_PATH_LUA);
            Utils::SetFileExtension(path, "moon");

            auto debugInfo = g_Compiler->GetDebugInfo(path);
            if (debugInfo) {
                strncpy(ar->short_src, debugInfo->fullSourcePath.c_str(), sizeof(ar->short_src));
                ar->currentline = debugInfo->lines[ar->currentline];
            }
        }
    }
    return ret;
}
#endif

GMOD_MODULE_OPEN() {
    GarrysMod::Lua::ILuaInterface* ILUA = reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA);
    ILUA->MsgColour(MESSAGE_COLOR, "Moonloader %s made by Pika-Software (%s)\n", MOONLOADER_FULL_VERSION, MOONLOADER_URL);

    if (!g_MoonEngine) g_MoonEngine = std::make_unique<MoonEngine::Engine>();
    if (!g_MoonEngine->IsInitialized())
        LUA->ThrowError("failed to initialize moonengine");

    // Why do you need to initialize moonloader more than once?
    // Because serverside module can be initialized from menu and server!
    g_InitializeCount++;
    if (g_InitializeCount > MAX_INITIALIZE_COUNT) LUA->ThrowError("you have tried to initialize moonloader too many times");

#if IS_SERVERSIDE
    if (g_LuaStates.find(ILUA) != g_LuaStates.end()) LUA->ThrowError("moonloader is already initialized for this lua state >:(");
    g_LuaStates.insert(ILUA);

    IFileSystem* filesystem = LoadFilesystem();
    if (!filesystem)
        LUA->ThrowError("failed to get filesystem interface");

    g_Server = InterfacePointers::Server();
    g_EngineServer = InterfacePointers::VEngineServer();

    if (g_InitializeCount == 1) {
        DevMsg("[Moonloader] Initializing...\n");
        g_Filesystem = std::make_unique<Filesystem>(filesystem);
        g_Compiler = std::make_unique<Compiler>();
        g_Watchdog = std::make_unique<Watchdog>();

        DevMsg("[Moonloader] Interfaces:\n");
        DevMsg("\t- IFilesystem: %p\n", filesystem);
        DevMsg("\t- IServer: %p\n", g_Server);
        DevMsg("\t- IVEngineServer: %p\n", g_EngineServer);

        ILuaInterfaceProxy::Singleton = std::make_unique<ILuaInterfaceProxy>();
        if (!ILuaInterfaceProxy::Singleton->Init(ILUA))
            Warning("[Moonloader] Failed to initialize lua detouring. include and CompileFile won't work :(\n");

        //Detour lua_getinfo and lua_pcall, so we can manipulate error stack traces
        lua_getinfo_hook.Create( Utils::LoadSymbol("lua_shared", "lua_getinfo"), reinterpret_cast<void*>(&lua_getinfo_detour) );
        if (!lua_getinfo_hook.Enable()) Warning("[Moonloader] Failed to detour lua_getinfo. Advanced error stack traces won't be enabled\n");
        
        DevMsg("[Moonloader] Removed %d files from cache\n", g_Filesystem->Remove(CACHE_PATH_LUA, "GAME"));

        g_Filesystem->CreateDirs(CACHE_PATH_LUA);
        g_Filesystem->AddSearchPath("garrysmod/" CACHE_PATH, "GAME", true);
        g_Filesystem->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "MOONLOADER");
    }

    g_Filesystem->AddSearchPath("garrysmod/" CACHE_PATH_LUA, ILUA->GetPathID(), true);
    if (g_Server && !g_Server->IsMultiplayer())
        g_Filesystem->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl", true);
#endif

    LuaAPI::Initialize(ILUA);

#if IS_SERVERSIDE
    LuaAPI::BeginVersionCheck(ILUA);

    if (ILUA->IsServer())
        AutoRefresh::Initialize();
#endif

    return 0;
}

GMOD_MODULE_CLOSE() {
    GarrysMod::Lua::ILuaInterface* ILUA = reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA);
    g_InitializeCount--;

    LuaAPI::Deinitialize();

#if IS_SERVERSIDE
    DevMsg("[Moonloader] [%s] Moonloader is shutting down... Bye bye\n", ILUA->IsServer() ? "Server" : ILUA->IsClient() ? "Client" : "Menu");
    
    g_LuaStates.erase(ILUA);
    g_IncludedFiles.erase(ILUA);

    g_Filesystem->RemoveSearchPath("garrysmod/" CACHE_PATH_LUA, ILUA->GetPathID());
    if (g_Server && !g_Server->IsMultiplayer())
        g_Filesystem->RemoveSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl");

    if (ILUA->IsServer()) {
        AutoRefresh::Deinitialize();
        g_Server = nullptr;
        g_EngineServer = nullptr;
    }
#endif

    if (g_InitializeCount == 0) {
        g_MoonEngine.reset();
    
#if IS_SERVERSIDE
        DevMsg("[Moonloader] Full shutdown!\n");

        ILuaInterfaceProxy::Singleton->Deinit();
        ILuaInterfaceProxy::Singleton.reset();
        lua_getinfo_hook.Disable();
        lua_getinfo_hook.Destroy();

        g_Watchdog.reset();
        g_Compiler.reset();
        g_Filesystem.reset();
#endif
    }

    return 0;
}