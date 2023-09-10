#ifndef MOONLOADER_LUA_API_HPP
#define MOONLOADER_LUA_API_HPP

#include <GarrysMod/Lua/LuaInterface.h>
#include <memory>

#if IS_SERVERSIDE
#include <GarrysMod/Lua/AutoReference.h>
#endif

namespace MoonLoader {
    class Core;

    class LuaAPI {
        std::shared_ptr<Core> core;
    #if IS_SERVERSIDE
        GarrysMod::Lua::AutoReference AddCSLuaFile_ref;
    #endif

    public:
        LuaAPI(std::shared_ptr<Core> core) : core(core) {}

        void Initialize(GarrysMod::Lua::ILuaInterface* LUA);
        void Deinitialize();

    #if IS_SERVERSIDE
        void BeginVersionCheck(GarrysMod::Lua::ILuaInterface* LUA);
        void AddCSLuaFile(GarrysMod::Lua::ILuaInterface* LUA);
        bool PreCacheFile(GarrysMod::Lua::ILuaInterface* LUA, const std::string& path);
        void PreCacheDir(GarrysMod::Lua::ILuaInterface* LUA, const std::string& startPath);
    #endif
    };
}

#endif //MOONLOADER_LUA_API_HPP
