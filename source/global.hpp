#ifndef MOONLOADER_GLOBAL_HPP
#define MOONLOADER_GLOBAL_HPP

#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <Platform.hpp>

#if IS_SERVERSIDE
#if ARCHITECTURE_IS_X86
#include <Color.h>
#else
#include <color.h>
#endif

#include <unordered_set>
#else
struct Color {
    uint8_t r, g, b, a;
};
#endif

namespace MoonLoader {
    static Color MESSAGE_COLOR = {255, 236, 153, 255};
#define CACHE_PATH "cache/moonloader/"
#define CACHE_PATH_LUA "cache/moonloader/lua/"
}

#endif //MOONLOADER_GLOBAL_HPP