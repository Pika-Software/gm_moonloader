#include "entry.hpp"
#include "../lua.hpp"

#include <tier0/dbg.h>
#include <string>
#include <string_view>
#include <cmrc/cmrc.hpp>
#include <algorithm>

CMRC_DECLARE(MoonEngine);

void preload_file(lua_State* L, std::string filePath) {
    auto fs = cmrc::MoonEngine::get_filesystem();

    try {
        auto file = fs.open(filePath);
        lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
        if (luaL_loadstring(L, file.begin()) != LUA_OK) {
            lua_pop(L, 2);
            throw;
        }

        // Modify our filePath, so it can be as package path
        filePath = filePath.substr(0, filePath.size() - 4); // Remove .lua extension
        std::replace(filePath.begin(), filePath.end(), '/', '.'); // Replace '/' to '.'

        lua_setfield(L, -2, filePath.c_str()); // Set our compiled string to preload table
        lua_pop(L, 1); // Pop preload table
    } catch (...) {
        Warning("[MoonEngine] Failed to preload file: %s\n", filePath.c_str());
    }
}

void preload_folder(lua_State* L, std::string dir) {
    auto fs = cmrc::MoonEngine::get_filesystem();
    for (const auto& f : fs.iterate_directory(dir)) {
        std::string filePath = dir + "/" + f.filename();
        if (f.is_directory()) {
            preload_folder(L, filePath);
            continue;
        }

        preload_file(L, filePath);
    }
}

int luaopen_moonscript(lua_State* L) {
    preload_folder(L, "moonscript");
    return 0;
}