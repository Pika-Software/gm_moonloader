#ifndef MOONLOADER_GLOBAL_HPP
#define MOONLOADER_GLOBAL_HPP

#pragma once

#include <memory>
#include <atomic>
#include <GarrysMod/Lua/LuaInterface.h>
#include <Platform.hpp>

class IServer;
class IVEngineServer;

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

namespace GarrysMod::Lua {
    class ILuaInterface;
}

namespace MoonEngine {
    class Engine;
}

namespace MoonLoader {
    class Compiler;
    class Watchdog;
    class Filesystem;

    static Color MESSAGE_COLOR = {255, 236, 153, 255};
    #define CACHE_PATH "cache/moonloader/"
    #define CACHE_PATH_LUA "cache/moonloader/lua/"

#if IS_SERVERSIDE
    constexpr int MAX_INITIALIZE_COUNT = 2;
#else
    constexpr int MAX_INITIALIZE_COUNT = 1;
#endif

    extern std::atomic<int> g_InitializeCount;
    extern std::unique_ptr<MoonEngine::Engine> g_MoonEngine;

#if IS_SERVERSIDE
    extern std::unordered_set<GarrysMod::Lua::ILuaInterface*> g_LuaStates;
    extern IVEngineServer* g_EngineServer;
    extern std::unique_ptr<Compiler> g_Compiler;
    extern std::unique_ptr<Watchdog> g_Watchdog;
    extern std::unique_ptr<Filesystem> g_Filesystem;
#endif
}

#endif //MOONLOADER_GLOBAL_HPP