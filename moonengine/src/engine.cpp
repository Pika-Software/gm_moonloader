#include "moonengine/engine.hpp"
#include "lua.hpp"
#include "moonscript/entry.hpp"

#include <string>
#include <stdexcept>

using namespace MoonEngine;

inline int print(lua_State* L) {
    int top = lua_gettop(L);
    for (int i = 0; i < top; i++) {
        printf("%s\t", luaL_tolstring(L, i + 1, 0));
    }
    printf("\n");
    return 0;
}

inline int xpcall_errhandler(lua_State* L) {
    luaL_traceback(L, L, lua_tostring(L, 1), 0);
    printf("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}

// Copied from https://lua-users.org/lists/lua-l/2006-03/msg00335.html
inline void stackDump(lua_State* L) {
    int i = lua_gettop(L);
    printf(" ----------------  Stack Dump ----------------\n");
    while (i) {
        int t = lua_type(L, i);
        switch (t) {
        case LUA_TSTRING:
            printf("%d:`%s'\n", i, lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            printf("%d: %s\n", i, lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            printf("%d: %g\n", i, lua_tonumber(L, i));
            break;
        default: printf("%d: %s\n", i, lua_typename(L, t)); break;
        }
        i--;
    }
    printf("--------------- Stack Dump Finished ---------------\n");
}

void LuaStateDeleter::operator()(lua_State* L) {
    lua_close(L);
}

Engine::Engine() {
    m_State = std::unique_ptr<lua_State, LuaStateDeleter>(luaL_newstate(), LuaStateDeleter());
    auto L = m_State.get();

    luaL_openlibs(L); // Initialize Lua standard libraries

    lua_pushcfunction(L, print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, luaopen_moonscript);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        throw std::runtime_error(lua_tostring(L, -1));

    lua_getglobal(L, "require");
    lua_pushstring(L, "moonscript.base");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK)
        throw std::runtime_error(lua_tostring(L, -1));

    lua_getfield(L, -1, "to_lua");
    m_ToLuaRef = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_pop(L, 1); // Pop moonscript table
}

void Engine::RunLua(const char* luaCode) {
    auto L = m_State.get();
    lua_pushcfunction(L, xpcall_errhandler);
    if (luaL_loadstring(L, luaCode) != LUA_OK)
        throw std::runtime_error(lua_tostring(L, -1));

    if (lua_pcall(L, 0, 0, -2) != LUA_OK)
        lua_pop(L, 1);

    lua_pop(L, 1);
}

bool MoonEngine::Engine::CompileStringEx(const char* moonCode, size_t len, std::string& output, CompiledLines* lineTable) {
    auto L = m_State.get();
    const char* result = nullptr;
    size_t resultLen = 0;

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_ToLuaRef);
    lua_pushlstring(L, moonCode, len);
    if (lua_pcall(L, 1, 2, 0) != LUA_OK) {
        // Error while calling function
        // Ideally shouldn't happen
        result = lua_tolstring(L, -1, &resultLen);
        output = std::string(result, resultLen);
        lua_pop(L, 1);
        return false;
    }

    if (lua_type(L, -2) == LUA_TSTRING) {
        // First argument is string, so it's success
        result = lua_tolstring(L, -2, &resultLen);
        output = std::string(result, resultLen);

        if (lineTable != nullptr && lua_type(L, -1) == LUA_TTABLE) {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                size_t line = lua_tonumber(L, -2);
                size_t pos = lua_tonumber(L, -1);
                lineTable->insert_or_assign(line, pos);
                lua_pop(L, 1);
            }
        }
        return true;
    } else {
        // First argument is nil, so it's error
        result = lua_tolstring(L, -1, &resultLen);
        output = std::string(result, resultLen);
        return false;
    }
}

std::string MoonEngine::Engine::CompileString(const char* moonCode, size_t len, CompiledLines* lineTable) {
    std::string result;
    bool success = CompileStringEx(moonCode, len, result, lineTable);
    if (!success)
        throw std::runtime_error(result);

    return result;
}
