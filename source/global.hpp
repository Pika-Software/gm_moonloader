#ifndef MOONLOADER_GLOBAL_HPP
#define MOONLOADER_GLOBAL_HPP

#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <Platform.hpp>

#if IS_SERVERSIDE
#include <color.h>
#include <unordered_set>
#else
struct Color {
    uint8_t r, g, b, a;
};
#endif

namespace MoonLoader {
    static Color MESSAGE_COLOR = {255, 236, 153, 255};
    #define CACHE_PATH "cache/moonloader/"
    #define CACHE_PATH_LUA CACHE_PATH "lua/"
    #define CACHE_PATH_GAMEMODES CACHE_PATH "gamemodes/"
    #define ADDONS_CACHE_PATH "addons/moonloader_cache"
}

#endif //MOONLOADER_GLOBAL_HPP
