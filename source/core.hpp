#ifndef MOONLOADER_CORE_HPP
#define MOONLOADER_CORE_HPP

#pragma once

#include <memory>
#include <GarrysMod/Lua/LuaInterface.h>
#include <unordered_map>

class IVEngineServer;

namespace Detouring {
    class Hook;
}

namespace MoonEngine {
    class Engine;
}

namespace yue {
    class YueCompiler;
}

namespace MoonLoader {
    class LuaAPI;
    class Filesystem;
    class Compiler;
    class Watchdog;
    class AutoRefresh;
    class ILuaInterfaceProxy;
    class Errors;

    class Core : public std::enable_shared_from_this<Core> {
    public:
        GarrysMod::Lua::ILuaInterface* LUA = nullptr;
        std::shared_ptr<MoonEngine::Engine> moonengine;
        std::shared_ptr<yue::YueCompiler> yuecompiler;
        std::shared_ptr<LuaAPI> lua_api;
        std::shared_ptr<Filesystem> fs;
        IVEngineServer* engine_server = nullptr;
        std::shared_ptr<Watchdog> watchdog;
        std::shared_ptr<Compiler> compiler;
        std::shared_ptr<AutoRefresh> autorefresh;
        std::shared_ptr<ILuaInterfaceProxy> lua_interface_detour;
        std::shared_ptr<Errors> errors;
        std::shared_ptr<Detouring::Hook> lua_getinfo_detour;

        static inline std::shared_ptr<Core> Create() { return std::make_shared<Core>(); }
        static std::shared_ptr<Core> Get(GarrysMod::Lua::ILuaBase* LUA);
        static void Remove(GarrysMod::Lua::ILuaBase* LUA);

        void Initialize(GarrysMod::Lua::ILuaInterface* LUA);
        void Deinitialize();
    };
}

#endif
