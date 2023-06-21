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

// Copied from https://lua-users.org/lists/lua-l/2006-03/msg00335.html
void stackDump(lua_State* L) {
    int i = lua_gettop(L);
    Msg(" ----------------  Stack Dump ----------------\n");
    while (i) {
        int t = lua_type(L, i);
        switch (t) {
        case LUA_TSTRING:
            Msg("%d:`%s'\n", i, lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            Msg("%d: %s\n", i, lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            Msg("%d: %g\n", i, lua_tonumber(L, i));
            break;
        default: Msg("%d: %s\n", i, lua_typename(L, t)); break;
        }
        i--;
    }
    Msg("--------------- Stack Dump Finished ---------------\n");
}


Engine::Engine() {
    m_State = luaL_newstate(); // Create new Lua state
    luaL_openlibs(m_State); // Initialize Lua standard libraries

    lua_getfield(m_State, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(m_State, luaopen_lpeg);
    lua_setfield(m_State, -2, "lpeg");
    lua_pop(m_State, 1); // Pop preload table

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

bool MoonEngine::Engine::CompileStringEx(const char* moonCode, size_t len, std::string& output, CompiledLines* lineTable) {
    if (m_ToLuaRef <= 0) {
        output = "Moonengine isn't initialized";
        return false; 
    }

    const char* result = nullptr;
    size_t resultLen = 0;

    lua_rawgeti(m_State, LUA_REGISTRYINDEX, m_ToLuaRef);
    lua_pushlstring(m_State, moonCode, len);
    if (lua_pcall(m_State, 1, 2, 0) != LUA_OK) {
        // Error while calling function
        // Ideally shouldn't happen
        result = lua_tolstring(m_State, -1, &resultLen);
        output = std::string(result, resultLen);
        lua_pop(m_State, 1);
        return false;
    }

    if (lua_type(m_State, -2) == LUA_TSTRING) {
        // First argument is string, so it's success
        result = lua_tolstring(m_State, -2, &resultLen);
        output = std::string(result, resultLen);

        if (lineTable != nullptr && lua_type(m_State, -1) == LUA_TTABLE) {
            lua_pushnil(m_State);
            while (lua_next(m_State, -2) != 0) {
                size_t line = lua_tonumber(m_State, -2);
                size_t pos = lua_tonumber(m_State, -1);
                lineTable->insert_or_assign(line, pos);
                lua_pop(m_State, 1);
            }
        }
        return true;
    } else {
        // First argument is nil, so it's error
        result = lua_tolstring(m_State, -1, &resultLen);
        output = std::string(result, resultLen);
        return false;
    }
}

std::string MoonEngine::Engine::CompileString(const char* moonCode, size_t len, CompiledLines* lineTable) {
    std::string result;
    bool success = CompileStringEx(moonCode, len, result, lineTable);
    if (!success) {
        Warning("[MoonEngine] %s\n", result.c_str());
    }

    return result;
}
