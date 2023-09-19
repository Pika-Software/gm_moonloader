#include "core.hpp"
#include "config.hpp"
#include "global.hpp"
#include "utils.hpp"
#include "lua_api.hpp"

#include <moonengine/engine.hpp>
#include <yuescript/yue_compiler.h>

#if IS_SERVERSIDE
#include "filesystem.hpp"
#include "compiler.hpp"
#include "watchdog.hpp"
#include "autorefresh.hpp"
#include <GarrysMod/InterfacePointers.hpp>
#include <detouring/classproxy.hpp>
#include <detouring/hook.hpp>
#include <GarrysMod/Lua/Interface.h>

extern "C" {
    #include <lua.h>
}

#define FILESYSTEM_INTERFACE_VERSION "VFileSystem022"
inline IFileSystem* LoadFilesystem() {
    auto iface = MoonLoader::Utils::LoadInterface<IFileSystem>("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION);
    return iface != nullptr ? iface : InterfacePointers::FileSystem();
}

class MoonLoader::ILuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, MoonLoader::ILuaInterfaceProxy> {
public:
    std::unordered_set<std::string> included_files;

    ILuaInterfaceProxy() {}

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
        if (auto core = Core::Get(This()); core && core->watchdog) core->watchdog->Think();
    }

    virtual bool FindAndRunScript(const char* fileName, bool run, bool showErrors, const char* runReason, bool noReturns) {
        auto core = Core::Get(This());
        if (!core || fileName == NULL)
            return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);

        auto& included_files = core->lua_interface_detour->included_files;
        std::string path = fileName;
        if (Utils::FindMoonScript(core->LUA, path)) {
            if (runReason[0] != '!') {
                // Usually when runReason doesn't start with "!",
                // it means that it was included by "include"
                // Allow auto-reloads for this guy in a future
                included_files.insert(path);
            }

            if (strcmp(runReason, "!RELOAD") == 0) {
                // All my homies hate auto-reloads by gmod
                return false;
            }

            if (strcmp(runReason, "!MOONRELOAD") == 0) {
                // Auto-reloads by moonloader is da best defacto
                runReason = "!RELOAD";
                if (included_files.find(path) == included_files.end()) {
                    // File wasn't included before? *heavy voice* Not good.
                    return false;
                }
            }

            // Alrighty, everything safe and we can with no worries compile! Yay!
            if (core->compiler->CompileFile(path)) {
                // If file was reloaded, then we need to reload it on clients (for OSX only ofc)
                #if SYSTEM_IS_MACOSX
                if (strcmp(runReason, "!RELOAD") == 0 && core->autorefresh) {
                    core->autorefresh->Sync(path);
                }
                #endif
            }

            path = fileName; // Preserve original file path for the god's sake
            Utils::SetFileExtension(path, "lua"); // Do not forget to pass lua file to gmod!!
        }

        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, path.c_str(), run, showErrors, runReason, noReturns);
    }
};

// typedef int (*lua_getinfo_t)(lua_State* L, const char* what, lua_Debug* ar);
// int lua_getinfo_detour_func(lua_State* L, const char* what, lua_Debug* ar) {
//     if (auto core = MoonLoader::Core::Get(L->luabase)) {
//         int ret = core->lua_getinfo_detour->GetTrampoline<lua_getinfo_t>()(L, what, ar);
//         if (ret != 0) {
//             // File stored in cache/moonloader/lua, so it must be our compiled script?
//             if (MoonLoader::Utils::StartsWith(ar->short_src, CACHE_PATH_LUA)) {
//                 std::string path = ar->short_src;
//                 MoonLoader::Utils::RemovePrefix(path, CACHE_PATH_LUA);
//                 MoonLoader::Utils::SetFileExtension(path, "moon");

//                 auto debugInfo = core->compiler->GetDebugInfo(path);
//                 if (debugInfo) {
//                     strncpy(ar->short_src, debugInfo->fullSourcePath.c_str(), sizeof(ar->short_src));
//                     ar->currentline = debugInfo->lines[ar->currentline];
//                 }
//             }
//         }
//         return ret;
//     }
//     return 0;
// }
#endif

using namespace MoonLoader;

std::unordered_map<GarrysMod::Lua::ILuaInterface*, std::shared_ptr<Core>> g_Cores;

std::shared_ptr<Core> Core::Get(GarrysMod::Lua::ILuaBase* LUA) {
    auto it = g_Cores.find(reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA));
    return it != g_Cores.end() ? it->second : nullptr;
}

void Core::Remove(GarrysMod::Lua::ILuaBase* LUA) {
    g_Cores.erase(reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA));
}

void Core::Initialize(GarrysMod::Lua::ILuaInterface* LUA) {
    this->LUA = LUA;

    if (weak_from_this().expired())
        throw std::runtime_error("core is not valid shared_ptr (weak_ptr is expired)");

    try {
        moonengine = std::make_shared<MoonEngine::Engine>();
    } catch (const std::exception& e) {
        throw std::runtime_error(Utils::Format("failed to initialize moonengine: %s", e.what()));
    }

    try {
        yuecompiler = std::make_shared<yue::YueCompiler>();
    } catch (const std::exception& e) {
        throw std::runtime_error(Utils::Format("failed to initialize yuecompiler: %s", e.what()));
    }

#if IS_SERVERSIDE
    fs = std::make_shared<Filesystem>(LoadFilesystem());
    engine_server = InterfacePointers::VEngineServer();
    watchdog = std::make_shared<Watchdog>(shared_from_this(), fs);
    compiler = std::make_shared<Compiler>(shared_from_this(), fs, moonengine, yuecompiler, watchdog);

    lua_interface_detour = std::make_shared<ILuaInterfaceProxy>();
    if (!lua_interface_detour->Init(LUA))
        throw std::runtime_error("failed to initialize ILuaInterface proxy");

    lua_getinfo_detour = std::make_shared<Detouring::Hook>();
    // lua_getinfo_detour->Create(Utils::LoadSymbol("lua_shared", "lua_getinfo"), reinterpret_cast<void*>(&lua_getinfo_detour_func));

#if SYSTEM_IS_MACOSX
    autorefresh = std::make_shared<AutoRefresh>(shared_from_this());
#endif

    DevMsg("[Moonloader] Removed %d files from cache\n", fs->Remove(CACHE_PATH_LUA, "GAME"));
    fs->CreateDirs(CACHE_PATH_LUA);
    fs->AddSearchPath("garrysmod/" CACHE_PATH, "GAME", true);
    fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "MOONLOADER");

    fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, LUA->GetPathID(), true);
    if (LUA->IsServer())
        fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl", true);
#endif

    lua_api = std::make_shared<LuaAPI>(shared_from_this());
    lua_api->Initialize(LUA);

    g_Cores[LUA] = shared_from_this();

#if IS_SERVERSIDE
    // TODO: Fix lua_getinfo detour
    //if (!lua_getinfo_detour->Enable()) throw std::runtime_error("failed to enable lua_getinfo detour");
#endif
}

void Core::Deinitialize() {
#if IS_SERVERSIDE
    if (lua_interface_detour) lua_interface_detour->Deinit();
    if (lua_getinfo_detour) {
        lua_getinfo_detour->Disable();
        lua_getinfo_detour->Destroy();
    }

    fs->RemoveSearchPath("garrysmod/" CACHE_PATH_LUA, LUA->GetPathID());
    if (LUA->IsServer())
        fs->RemoveSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl");
#endif

    if (lua_api) lua_api->Deinitialize();
    lua_api.reset();
    autorefresh.reset();
    lua_interface_detour.reset();
    lua_getinfo_detour.reset();
    compiler.reset();
    watchdog.reset();
    fs.reset();
    yuecompiler.reset();
    moonengine.reset();
    LUA = nullptr;
}

