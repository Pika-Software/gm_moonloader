#include "core.hpp"
#include "config.hpp"
#include "global.hpp"
#include "utils.hpp"
#include "lua_api.hpp"

#include <moonengine/engine.hpp>

using namespace MoonLoader;

#if IS_SERVERSIDE
#include "filesystem.hpp"
#include "compiler.hpp"
#include "watchdog.hpp"
#include "errors.hpp"
#include <GarrysMod/InterfacePointers.hpp>
#include <detouring/classproxy.hpp>
#include <detouring/hook.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaShared.h>
#include <tier1/convar.h>

extern "C" {
    #include <lua.h>
}

ConVar Core::cvar_detour_getinfo("moonloader_detour_getinfo", "1", FCVAR_ARCHIVE, "Detour debug.getinfo for better source lines");

std::vector<ConVar*> moonloader_convars = {
    &Core::cvar_detour_getinfo
};

#define FILESYSTEM_INTERFACE_VERSION "VFileSystem022"
inline IFileSystem* LoadFilesystem() {
    auto iface = Utils::LoadInterface<IFileSystem>("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION);
    return iface != nullptr ? iface : InterfacePointers::FileSystem();
}

#define GMOD_LUASHARED_INTERFACE "LUASHARED003"
inline GarrysMod::Lua::ILuaShared* LoadLuaShared() {
    return Utils::LoadInterface<GarrysMod::Lua::ILuaShared>("lua_shared", GMOD_LUASHARED_INTERFACE);
}

class MoonLoader::ILuaSharedProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaShared, MoonLoader::ILuaSharedProxy> {
public:
    ILuaSharedProxy(GarrysMod::Lua::ILuaShared* lua_shared) {
        Initialize(lua_shared);
        if (!Hook(&GarrysMod::Lua::ILuaShared::LoadFile, &ILuaSharedProxy::LoadFile))
            throw std::runtime_error("failed to hook ILuaShared::LoadFile");
    }

    ~ILuaSharedProxy() {
        UnHook(&GarrysMod::Lua::ILuaShared::LoadFile);
    }

    virtual GarrysMod::Lua::File* LoadFile(const std::string& _path, const std::string& _pathID, bool fromDatatable, bool fromFile) {
        auto lua_shared = This();
        auto LUA = lua_shared->GetLuaInterface(GarrysMod::Lua::State::SERVER);
        auto core = Core::Get(LUA);
        auto file = Call(&GarrysMod::Lua::ILuaShared::LoadFile, _path, _pathID, fromDatatable, fromFile);
        if (core && fromFile) {
            std::string path = _path.c_str(); // std::string from gmod have different ABI, be careful
            std::string pathID = _pathID.c_str();

            if (pathID == "lsv" && core->FindMoonScript(path)) {
                if (!core->compiler->CompileFile(path))
                    return nullptr;

                file = Call(&GarrysMod::Lua::ILuaShared::LoadFile, _path, _pathID, 0, 1);
                if (file != nullptr)
                    core->watchdog->CacheFile(path, file);
            }
        }
        return file;
    }
};

class MoonLoader::ILuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, MoonLoader::ILuaInterfaceProxy> {
public:
    std::unordered_set<std::string> regular_scripts;
    std::unique_ptr<Errors> clientside_error_handler;

    ILuaInterfaceProxy(GarrysMod::Lua::ILuaInterface* LUA) {
        Initialize(LUA);
        //if (!Hook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, &ILuaInterfaceProxy::FindAndRunScript))
        //    throw std::runtime_error("failed to hook ILuaInterface::FindAndRunScript");
        if (!Hook(&GarrysMod::Lua::ILuaInterface::Cycle, &ILuaInterfaceProxy::Cycle))
            throw std::runtime_error("failed to hook ILuaInterface::Cycle");
        if (!Hook(&GarrysMod::Lua::ILuaInterface::SetPathID, &ILuaInterfaceProxy::SetPathID))
            throw std::runtime_error("failed to hook ILuaInterface::SetPathID");
    }

    ~ILuaInterfaceProxy() {
        //UnHook(&GarrysMod::Lua::ILuaInterface::FindAndRunScript);
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
                break;
            }
        }
    }

    virtual void Cycle() {
        Call(&GarrysMod::Lua::ILuaInterface::Cycle);
        if (auto core = Core::Get(This()); core && core->watchdog) core->watchdog->Think();
    }

    //virtual bool FindAndRunScript(const char* fileName, bool run, bool showErrors, const char* runReason, bool noReturns) {
    //    auto core = Core::Get(This());
    //    if (!core || fileName == NULL)
    //        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);

    //    //This()->MsgColour(This()->IsServer() ? Color{128,128,255,255} : Color{255, 255, 128, 255}, "%s [%s] %s %s %s\n", fileName, runReason, run ? "run" : "find", showErrors ? "withErrors" : "silent", noReturns ? "noReturns" : "withReturns");
    //    auto& regular_scripts = core->lua_interface_detour->regular_scripts;

    //    // If file was included before any modifications, then just skip any processing
    //    if (regular_scripts.find(fileName) != regular_scripts.end())
    //        return Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, fileName, run, showErrors, runReason, noReturns);

    //    std::string path = fileName;
    //    bool is_moonscript = core->FindMoonScript(path);
    //    if (is_moonscript) {
    //        // Alrighty, everything safe and we can with no worries compile! Yay!
    //        if (!core->compiler->CompileFile(path))
    //            return false;

    //        path = fileName; // Preserve original file path for the god's sake
    //        Utils::SetFileExtension(path, "lua"); // Do not forget to pass lua file to gmod!!
    //    }

    //    bool success = Call(&GarrysMod::Lua::ILuaInterface::FindAndRunScript, path.c_str(), run, showErrors, runReason, noReturns);
    //    if (success && !is_moonscript) {
    //        // If file wasn't detected as moonscript, and successfully was included by Garry's Mod
    //        // Just ignore any processing later to not break anything
    //        regular_scripts.insert(path);
    //    }

    //    return success;
    //}
};

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

void Core::PrepareDirectory(std::string_view path) {
    for (auto file : fs->Find(Utils::JoinPaths(path, "*"), LUA->GetPathID())) {
        auto filePath = Utils::JoinPaths(path, file);
        auto fileExt = fs->FileExtension(filePath);
        if (fs->IsFile(filePath, LUA->GetPathID())) {
            if (fileExt == "yue" || fileExt == "moon") {
                auto fileDir = filePath;
                fs->StripFileName(fileDir);
                fs->SetFileExtension(filePath, "lua");
                fs->CreateDirs(fileDir, "MOONLOADER");
                fs->WriteToFile(filePath, "MOONLOADER", nullptr, 0); // Just create a dummy file
            }
        } else {
            PrepareDirectory(filePath);
        }
    }
}

void Core::PrepareFiles() {
    DevMsg("[Moonloader] Creating dummy .lua files");
    PrepareDirectory({}); // Precompile all .yue/.moon files to .lua
}
#endif

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

inline bool IsSingleplayer(GarrysMod::Lua::ILuaInterface* LUA) {
    LUA->GetField(GarrysMod::Lua::INDEX_GLOBAL, "game");
    LUA->GetField(-1, "SinglePlayer");
    bool singleplayer = false;
    if (LUA->PCall(0, 1, 0) != 0) {
        LUA->Pop(); // Remove error message
        LUA->PushBool(false);
    }
    singleplayer = LUA->GetBool(-1);
    LUA->Pop(2); // Remove bool and game table
    return singleplayer;
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

#if IS_SERVERSIDE
    if (LUA->IsMenu())
        throw std::runtime_error("Currently gm_moonloader is broken in menu state");

    lua_shared = LoadLuaShared();
    if (lua_shared == nullptr) throw std::runtime_error("failed to get ILuaShared interface");

    fs = std::make_shared<Filesystem>(LoadFilesystem());
    watchdog = std::make_shared<Watchdog>(shared_from_this(), fs);
    watchdog->Start();
    compiler = std::make_shared<Compiler>(shared_from_this(), fs, moonengine, watchdog);
    errors = std::make_shared<Errors>(shared_from_this());

    lua_interface_detour = std::make_shared<ILuaInterfaceProxy>(LUA);
    lua_shared_detour = std::make_shared<ILuaSharedProxy>(lua_shared);

    DevMsg("[Moonloader] Removed %d files from cache\n", fs->Remove(CACHE_PATH, "GAME_WRITE"));
    fs->CreateDirs(CACHE_PATH_LUA);
    fs->AddSearchPath("garrysmod/" CACHE_PATH, "GAME", true);
    fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "MOONLOADER");
    fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, LUA->GetPathID(), true);

    if (LUA->IsServer() && Utils::LuaBoolFromValue(LUA, "game.SinglePlayer", 0).value_or(false)) {
        // Only add these if server is single player
        fs->AddSearchPath("garrysmod/" CACHE_PATH_LUA, "lcl", true);
    }

    cvar = InterfacePointers::Cvar();
    if (cvar == nullptr) throw std::runtime_error("failed to get ICvar interface");
    for (ConVar* convar : moonloader_convars)
        cvar->RegisterConCommand(convar);

    PrepareFiles();
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
    lua_interface_detour.reset();
    lua_shared_detour.reset();
    errors.reset();
    compiler.reset();
    watchdog.reset();
    fs.reset();
    moonengine.reset();
    LUA = nullptr;
}

