#include "core.hpp"
#include "config.hpp"
#include "global.hpp"
#include "utils.hpp"
#include "lua_api.hpp"

#include <moonengine/engine.hpp>

#if IS_SERVERSIDE
#include "filesystem.hpp"
#include "compiler.hpp"
#include "watchdog.hpp"
#include "autorefresh.hpp"
#include "errors.hpp"
#include <GarrysMod/InterfacePointers.hpp>
#include <detouring/classproxy.hpp>
#include <detouring/hook.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <tier1/convar.h>
#include <GarrysMod/Lua/LuaShared.h>

extern "C" {
    #include <lua.h>
}

ConVar MoonLoader::Core::cvar_detour_getinfo("moonloader_detour_getinfo", "1", FCVAR_ARCHIVE, "Detour debug.getinfo for better source lines");

std::vector<ConVar*> moonloader_convars = {
    &MoonLoader::Core::cvar_detour_getinfo
};

#define FILESYSTEM_INTERFACE_VERSION "VFileSystem022"
inline IFileSystem* LoadFilesystem() {
    auto iface = MoonLoader::Utils::LoadInterface<IFileSystem>("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION);
    return iface != nullptr ? iface : InterfacePointers::FileSystem();
}

class MoonLoader::ILuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, MoonLoader::ILuaInterfaceProxy> {
public:
    std::unordered_set<std::string> autorefresh_scripts;
    std::unordered_set<std::string> regular_scripts;
    std::unique_ptr<Errors> clientside_error_handler;

    ILuaInterfaceProxy(GarrysMod::Lua::ILuaInterface* LUA) {
        Initialize(LUA);
        if (!Hook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, &ILuaInterfaceProxy::FindAndRunScript))
            throw std::runtime_error("failed to hook ILuaInterface::FindAndRunScript");
        if (!Hook(&GarrysMod::Lua::ILuaInterface::Cycle, &ILuaInterfaceProxy::Cycle))
            throw std::runtime_error("failed to hook ILuaInterface::Cycle");
        if (!Hook(&GarrysMod::Lua::ILuaInterface::SetPathID, &ILuaInterfaceProxy::SetPathID))
            throw std::runtime_error("failed to hook ILuaInterface::SetPathID");
    }

    ~ILuaInterfaceProxy() {
        UnHook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript);
        UnHook(&GarrysMod::Lua::ILuaInterface::Cycle);
        UnHook(&GarrysMod::Lua::ILuaInterface::SetPathID);
    }

    virtual void SetPathID(const char *pathID) {
        Call(&GarrysMod::Lua::ILuaInterface::SetPathID, pathID);
        if (strcmp(pathID, "lcl") != 0) return;

        for (auto& core : Core::GetAll()) {
            if (core->LUA->IsServer()) {
                core->lua_interface_detour->clientside_error_handler
                    = std::make_unique<Errors>(core, This());
                
                if (core->autorefresh)
                    core->autorefresh->SetClientLua(This());
                break;
            }
        }
    }

    virtual void Cycle() {
        Call(&GarrysMod::Lua::ILuaInterface::Cycle);
        if (auto core = Core::Get(This()); core && core->watchdog) core->watchdog->Think();
    }

    virtual bool FindAndRunScript(const char* fileName, bool run, bool showErrors, const char* runReason, bool noReturns) {
        auto core = Core::Get(This());
        if (!core || fileName == NULL)
            return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);

        //This()->MsgColour(This()->IsServer() ? Color{128,128,255,255} : Color{255, 255, 128, 255}, "%s [%s] %s %s %s\n", fileName, runReason, run ? "run" : "find", showErrors ? "withErrors" : "silent", noReturns ? "noReturns" : "withReturns");
        auto& autorefresh_scripts = core->lua_interface_detour->autorefresh_scripts;
        auto& regular_scripts = core->lua_interface_detour->regular_scripts;

        // If file was included before any modifications, then just skip any processing
        if (regular_scripts.find(fileName) != regular_scripts.end())
            return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);

        std::string path = fileName;
        bool is_moonscript = core->FindMoonScript(path);
        if (is_moonscript) {
            //// All my homies hate auto-reloads by gmod
            //if (strcmp(runReason, "!RELOAD") == 0) return false;

            //// If `run` boolean is true, then we should autorefresh this file in the future
            //if (run) autorefresh_scripts.insert(path);

            // Alrighty, everything safe and we can with no worries compile! Yay!
            if (!core->compiler->CompileFile(path))
                return false;

            if (strcmp(runReason, "!MOONRELOAD") == 0) {
                //// Auto-reloads by moonloader is da best defacto
                //runReason = "!RELOAD";
                //if (autorefresh_scripts.find(path) == autorefresh_scripts.end()) {
                //    // File isn't in autorefresh list? *heavy voice* Not good.
                //    return false;
                //}

                return false;
            }

            // If file was reloaded, then we need to reload it on clients (for OSX only ofc)
            #if SYSTEM_IS_MACOSX
            if (strcmp(runReason, "!RELOAD") == 0 && core->autorefresh) {
                if (!core->autorefresh->Sync(path))
                    Warning("[Moonloader] Failed to autorefresh %s\n", path.c_str());
            }
            #endif

            path = fileName; // Preserve original file path for the god's sake
            Utils::SetFileExtension(path, "lua"); // Do not forget to pass lua file to gmod!!
        }

        bool success = Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, path.c_str(), run, showErrors, runReason, noReturns);
        if (success && !is_moonscript)
            // If file wasn't detected as moonscript, and successfully was included by Garry's Mod
            // Just ignore any processing later to not break anything
            regular_scripts.insert(path);

        return success;
    }
};
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

std::vector<std::shared_ptr<Core>> Core::GetAll() {
    std::vector<std::shared_ptr<Core>> cores;
    for (auto& [_, core] : g_Cores)
        cores.push_back(core);
    return cores;
}

bool Core::FindMoonScript(std::string& path) {
    const char* currentDir = LUA->GetPath();
    const char* pathID = LUA->GetPathID();
    if (currentDir) {
        std::string absolutePath = Utils::JoinPaths(currentDir, path);
        Utils::NormalizePath(absolutePath);
        Utils::SetFileExtension(absolutePath, "yue");
        if (fs->Exists(absolutePath, pathID)) {
            path = std::move(absolutePath);
            return true;
        }
        Utils::SetFileExtension(absolutePath, "moon");
        if (fs->Exists(absolutePath, pathID)) {
            path = std::move(absolutePath);
            return true;
        }
    }

    std::string moonPath = path;
    Utils::NormalizePath(moonPath);
    Utils::SetFileExtension(moonPath, "yue");
    if (fs->Exists(moonPath, pathID)) {
        path = std::move(moonPath);
        return true;
    }
    Utils::SetFileExtension(moonPath, "moon");
    if (fs->Exists(moonPath, pathID)) {
        path = std::move(moonPath);
        return true;
    }
    return false;
}

//void Core::PrepareFiles() {
//    // Since Garry's Mod uses different method of finding init.lua in gamemodes
//    // we need to precompile files, so Garry's Mod can find them
//    
//    // Preparing files for gamemodes
//    for (auto gamemode : fs->Find("gamemodes/*", "GAME")) {
//        if (fs->IsDirectory(Utils::JoinPaths("gamemodes", gamemode), "GAME")) {
//            std::string init_file = Utils::JoinPaths(gamemode, "gamemode/cl_init.lua");
//            if (FindMoonScript(init_file))
//                compiler->CompileFile(init_file);
//        }
//    }
//}

void Core::Initialize(GarrysMod::Lua::ILuaInterface* LUA) {
    this->LUA = LUA;

    if (weak_from_this().expired())
        throw std::runtime_error("core is not valid shared_ptr (weak_ptr is expired)");

    try {
        moonengine = std::make_shared<MoonEngine::Engine>();
    } catch (const std::exception& e) {
        throw std::runtime_error(Utils::Format("failed to initialize moonengine: %s", e.what()));
    }

#if IS_SERVERSIDE
    fs = std::make_shared<Filesystem>(LoadFilesystem());
    engine_server = InterfacePointers::VEngineServer();
    watchdog = std::make_shared<Watchdog>(shared_from_this(), fs);
    watchdog->Start();
    compiler = std::make_shared<Compiler>(shared_from_this(), fs, moonengine, watchdog);
    errors = std::make_shared<Errors>(shared_from_this());

    lua_interface_detour = std::make_shared<ILuaInterfaceProxy>(LUA);

#if SYSTEM_IS_MACOSX
    autorefresh = std::make_shared<AutoRefresh>(shared_from_this());
#endif

    DevMsg("[Moonloader] Removed %d files from cache\n", fs->Remove(CACHE_PATH, "GAME"));
    fs->CreateDirs(CACHE_PATH_LUA);
    //fs->CreateDirs(CACHE_PATH_GAMEMODES);
    fs->AddSearchPath("garrysmod/" CACHE_PATH, "GAME", true);
    fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "MOONLOADER");
    //fs->AddSearchPath("garrysmod/" CACHE_PATH_GAMEMODES, "MOONLOADER");

    fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, LUA->GetPathID(), true);
    //fs->AddSearchPath("garrysmod/" CACHE_PATH_GAMEMODES, LUA->GetPathID(), true);
    if (LUA->IsServer()) {
        fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl", true);
        fs->AddSearchPath("garrysmod/" CACHE_PATH_GAMEMODES, "lcl", true);
    }

    cvar = InterfacePointers::Cvar();
    if (cvar == nullptr) throw std::runtime_error("failed to get ICvar interface");
    for (ConVar* convar : moonloader_convars)
        cvar->RegisterConCommand(convar);

    //PrepareFiles();
#endif

    lua_api = std::make_shared<LuaAPI>(shared_from_this());
    lua_api->Initialize(LUA);

    g_Cores[LUA] = shared_from_this();
}

void Core::Deinitialize() {
#if IS_SERVERSIDE
    fs->RemoveSearchPath("garrysmod/" CACHE_PATH_LUA, LUA->GetPathID());
    if (LUA->IsServer())
        fs->RemoveSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl");
    if (cvar)
        for (ConVar* convar : moonloader_convars)
            cvar->UnregisterConCommand(convar);
#endif

    if (lua_api) lua_api->Deinitialize();
    lua_api.reset();
    autorefresh.reset();
    lua_interface_detour.reset();
    errors.reset();
    compiler.reset();
    watchdog.reset();
    fs.reset();
    moonengine.reset();
    LUA = nullptr;
}

