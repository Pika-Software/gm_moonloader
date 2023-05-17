#ifndef MOONLOADER_GLOBAL_HPP
#define MOONLOADER_GLOBAL_HPP

#include <memory>

namespace GarrysMod::Lua {
    class ILuaInterface;
}

namespace MoonEngine {
    class Engine;
}

namespace MoonLoader {
    class Compiler;
    class Watchdog;

    extern std::unique_ptr<GarrysMod::Lua::ILuaInterface> g_pLua;
    extern std::unique_ptr<MoonEngine::Engine> g_pMoonEngine;
    extern std::unique_ptr<Compiler> g_pCompiler;
    extern std::unique_ptr<Watchdog> g_pWatchdog;
    extern const char* GMOD_LUA_PATH_ID;
}

#endif //MOONLOADER_GLOBAL_HPP