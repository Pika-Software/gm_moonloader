#ifndef MOONLOADER_LUA_API_HPP
#define MOONLOADER_LUA_API_HPP

#include <GarrysMod/Lua/LuaInterface.h>

namespace MoonLoader::LuaAPI {
    void Initialize(GarrysMod::Lua::ILuaInterface* LUA);
    void Deinitialize();

    void BeginVersionCheck(GarrysMod::Lua::ILuaInterface* LUA);
}

#endif //MOONLOADER_LUA_API_HPP
