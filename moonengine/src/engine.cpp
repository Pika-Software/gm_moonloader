#include "moonengine/engine.hpp"
#include "lua.hpp"
#include "moonscript/entry.hpp"

#include <string>
#include <tier0/dbg.h>
#include <lpeg.hpp>

using namespace MoonEngine;

int print(lua_State* L) {
    int top = lua_gettop(L);
    for (int i = 0; i < top; i++) {
        Msg("%s\t", luaL_tolstring(L, i + 1, 0));
    }
    Msg("\n");
    return 0;
}

int xpcall_errhandler(lua_State* L) {
    luaL_traceback(L, L, lua_tostring(L, 1), 0);
    Warning("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}

Engine::Engine() {
    m_State = luaL_newstate(); // Create new Lua state
    luaL_openlibs(m_State); // Initialize Lua standard libraries

    lua_getfield(m_State, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(m_State, luaopen_lpeg);
    lua_setfield(m_State, -2, "lpeg");

    lua_pushcfunction(m_State, print);
    lua_setglobal(m_State, "print");

    lua_pushcfunction(m_State, luaopen_moonscript);
    if (lua_pcall(m_State, 0, 0, 0) != LUA_OK) {
        Warning("failed to preload moonscript: %s\n", lua_tostring(m_State, -1));
        lua_pop(m_State, 1);
        return;
    }

    lua_getglobal(m_State, "require");
    lua_pushstring(m_State, "moonscript.base");
    if (lua_pcall(m_State, 1, 1, 0) != LUA_OK) {
        Warning("failed to load moonscript: %s\n", lua_tostring(m_State, -1));
        lua_pop(m_State, 1);
        return;
    }
    
    lua_getfield(m_State, -1, "to_lua");
    m_ToLuaRef = luaL_ref(m_State, LUA_REGISTRYINDEX);

    lua_pop(m_State, 1); // Pop moonscript table

    m_Initialized = true;
}

Engine::~Engine() {
    lua_close(m_State);
    m_State = nullptr;
}

void Engine::RunLua(const char* luaCode) {
    lua_pushcfunction(m_State, xpcall_errhandler);
    if (luaL_loadstring(m_State, luaCode) != LUA_OK) {
        Warning("failed to parse input: %s\n", lua_tostring(m_State, -1));
        lua_pop(m_State, 2);
        return;
    };
    if (lua_pcall(m_State, 0, 0, -2) != LUA_OK) {
        lua_pop(m_State, 1);
    };
    lua_pop(m_State, 1);
}

std::string MoonEngine::Engine::CompileString(const char* moonCode, size_t len) {
    if (m_ToLuaRef == 0) { return {}; }

    lua_pushcfunction(m_State, xpcall_errhandler);
    lua_rawgeti(m_State, LUA_REGISTRYINDEX, m_ToLuaRef);
    lua_pushlstring(m_State, moonCode, len);
    if (lua_pcall(m_State, 1, 1, -3) != LUA_OK) {
        lua_pop(m_State, 2);
        return {};
    }

    std::string luaCode = lua_tostring(m_State, -1);
    lua_pop(m_State, 2);
    return luaCode;
}

std::string Engine::CompileString(std::string_view moonCode) {
    return CompileString(moonCode.data(), moonCode.size());
}
