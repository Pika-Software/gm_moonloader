#include "moonengine/engine.hpp"

#include <stdexcept>
#include <string>
#include <algorithm>
#include <chrono>

#include "lua.hpp"
#include "moonscript/entry.hpp"

using namespace MoonEngine;

inline double timestamp() {
    using clock = std::chrono::steady_clock;
    using dur = std::chrono::duration<double, std::milli>;
    using tsp = std::chrono::time_point<clock, dur>;
    tsp now = clock::now();
    return now.time_since_epoch().count();
}

inline void push_moon_options(lua_State* L, const CompileOptions& options = {}) {
    lua_createtable(L, 0, 1);
    lua_pushboolean(L, options.implicitly_return_root); lua_setfield(L, -2, "implicitly_return_root");
}

inline size_t lua_gc_count(lua_State* L) {
    return lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
}

inline int print(lua_State* L) {
    int top = lua_gettop(L);
    for (int i = 0; i < top; i++) {
        printf("%s\t", lua_tolstring(L, i + 1, 0));
    }
    printf("\n");
    return 0;
}

inline int xpcall_errhandler(lua_State* L) {
    // luaL_traceback(L, L, lua_tostring(L, 1), 0);
    printf("an error occured: %s\n", lua_tostring(L, 1));
    // lua_pop(L, 1);
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
            default:
                printf("%d: %s\n", i, lua_typename(L, t));
                break;
        }
        i--;
    }
    printf("--------------- Stack Dump Finished ---------------\n");
}

void LuaStateDeleter::operator()(lua_State* L) { lua_close(L); }

Engine::Engine() {
    m_State = std::unique_ptr<lua_State, LuaStateDeleter>(luaL_newstate(),
                                                          LuaStateDeleter());
    auto L = m_State.get();

    luaL_openlibs(L);  // Initialize Lua standard libraries

    lua_pushcfunction(L, print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, luaopen_moonscript);
    if (lua_pcall(L, 0, 0, 0) != 0)
        throw std::runtime_error(lua_tostring(L, -1));

    lua_getglobal(L, "require");
    lua_pushstring(L, "moonscript.base");
    if (lua_pcall(L, 1, 1, 0) != 0)
        throw std::runtime_error(lua_tostring(L, -1));

    lua_getfield(L, -1, "to_lua");
    m_ToLuaRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);  // Pop moonscript.base table

    lua_getglobal(L, "require");
    lua_pushstring(L, "moonscript.parse");
    if (lua_pcall(L, 1, 1, 0) != 0)
        throw std::runtime_error(lua_tostring(L, -1));

    lua_getfield(L, -1, "string");
    m_ParseStringRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);  // Pop moonscript.parse table

    lua_getglobal(L, "require");
    lua_pushstring(L, "moonscript.compile");
    if (lua_pcall(L, 1, 1, 0) != 0)
        throw std::runtime_error(lua_tostring(L, -1));

    lua_getfield(L, -1, "tree");
    m_CompileTreeRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "format_error");
    m_CompileFormatErrorRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);  // Pop moonscript.tree table

    if (m_ToLuaRef <= 0) throw std::runtime_error("to_lua not found");
    if (m_ParseStringRef <= 0) throw std::runtime_error("parse.string not found");
    if (m_CompileTreeRef <= 0) throw std::runtime_error("compile.tree not found");
    if (m_CompileFormatErrorRef <= 0) throw std::runtime_error("compile.format_error not found");
}

std::pair<int, int> Engine::OffsetToLine(std::string_view str, int pos) {
    int line_pos = 0;
    int line = 1;
    int len = std::clamp(pos, 0, (int)str.length());
    for (int i = 0; i < len; i++) {
        if (str[i] == '\n') {
            line++;
            line_pos = i + 1;
        }
    }
    return {line, len - line_pos + 1};
}

void Engine::RunLua(const char* luaCode) {
    auto L = m_State.get();
    lua_pushcfunction(L, xpcall_errhandler);
    if (luaL_loadstring(L, luaCode) != 0)
        throw std::runtime_error(lua_tostring(L, -1));

    if (lua_pcall(L, 0, 0, -2) != 0) lua_pop(L, 1);

    lua_pop(L, 1);
}

bool MoonEngine::Engine::CompileStringEx(const char* moonCode, size_t len,
                                         std::string& output,
                                         CompiledLines* lineTable) {
    auto L = m_State.get();
    const char* result = nullptr;
    size_t resultLen = 0;

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_ToLuaRef);
    lua_pushlstring(L, moonCode, len);
    if (lua_pcall(L, 1, 2, 0) != 0) {
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

std::string Engine::CompileString(const char* moonCode, size_t len,
                                              CompiledLines* lineTable) {
    std::string result;
    bool success = CompileStringEx(moonCode, len, result, lineTable);
    if (!success) throw std::runtime_error(result);

    return result;
}

CompileInfo Engine::CompileString2(std::string_view moonCode, const CompileOptions& options) {
    auto L = m_State.get();
    CompileInfo info;

    // Preparing
    lua_gc(L, LUA_GCSTOP, 0); // We don't need GC to slowdown transpilation
    std::shared_ptr<void> _(nullptr, [L](...) { lua_gc(L, LUA_GCRESTART, 0); });
    info.memory_usage = lua_gc_count(L);

    // Parsing
    info.parse_time = timestamp();
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_ParseStringRef);
    lua_pushlstring(L, moonCode.data(), moonCode.size());
    if (lua_pcall(L, 1, 2, 0) != 0) {
        std::string err = lua_tostring(L, -1); lua_pop(L, 1);
        return CompileInfo::FromError(err, "failed to run parser.string: " + err);
    }
    if (lua_isnil(L, -2)) {
        std::string err = lua_tostring(L, -1); lua_pop(L, 2);
        return CompileInfo::FromError(err);
    }
    lua_pop(L, 1); // Pop error
    info.parse_time = timestamp() - info.parse_time;

    // Compiling
    info.compile_time = timestamp();
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_CompileTreeRef);
    lua_insert(L, -2);
    push_moon_options(L, options);
    if (lua_pcall(L, 2, 3, 0) != 0) {
        std::string err = lua_tostring(L, -1); lua_pop(L, 1);
        return CompileInfo::FromError(err, "failed to run compile.tree: " + err);
    }
    if (lua_isnil(L, -3)) {
        std::string msg, display_msg;
        CompileInfo::Pos pos;
        msg = lua_tostring(L, -2);
        if (lua_isnumber(L, -1)) pos = OffsetToPos(moonCode, lua_tointeger(L, -1));
        // Formatting error
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_CompileFormatErrorRef);
        lua_insert(L, -3);
        lua_pushlstring(L, moonCode.data(), moonCode.size());
        if (lua_pcall(L, 3, 1, 0) == 0) { display_msg = lua_tostring(L, -1); }
        lua_pop(L, 2);
        return CompileInfo::FromError(msg, display_msg, pos);
    }

    info.lua_code = lua_tostring(L, -3);
    if (lua_istable(L, -2)) {
        lua_pushnil(L);
        while (lua_next(L, -3) != 0) {
            size_t line = lua_tonumber(L, -2);
            size_t pos = lua_tonumber(L, -1);
            info.posmap.insert_or_assign(line, OffsetToPos(moonCode, pos));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 3);

    info.compile_time = timestamp() - info.compile_time;
    info.memory_usage = lua_gc_count(L) - info.memory_usage;
    return info;
}
