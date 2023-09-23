#ifndef MOONLOADER_AUTOREFRESH_HPP
#define MOONLOADER_AUTOREFRESH_HPP

#pragma once

#include <string_view>
#include <memory>
#include <GarrysMod/Lua/LuaInterface.h>

class INetworkStringTable;

namespace MoonLoader {
    class Core;

    class AutoRefresh {
        std::shared_ptr<Core> core;
        bool is_singleplayer = false;
        GarrysMod::Lua::ILuaInterface* CLIENT_LUA = nullptr;
        INetworkStringTable* client_files = nullptr;

    public:
        AutoRefresh(std::shared_ptr<Core> core);

        void SetClientLua(GarrysMod::Lua::ILuaInterface* LUA) { CLIENT_LUA = LUA; }
        bool Sync(std::string_view path);
    };
}

#endif // MOONLOADER_AUTOREFRESH_HPP
