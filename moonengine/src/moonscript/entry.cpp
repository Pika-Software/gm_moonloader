#include "entry.hpp"
#include "../lua.hpp"

#include <string>
#include <string_view>
#include <cmrc/cmrc.hpp>
#include <lpeg.hpp>
#include <algorithm>

CMRC_DECLARE(MoonEngine);

class PreloadFileException : public std::exception {
    std::string _fileName;
    std::string _error;

public:
    PreloadFileException(std::string_view fileName, std::string_view error) : _fileName(fileName), _error(error) {}

    virtual const char* what() const noexcept {
        return _error.c_str();
    }

    virtual const char* filename() const noexcept {
        return _fileName.c_str();
    }
};

void preload_file(lua_State* L, std::string filePath) {
    auto fs = cmrc::MoonEngine::get_filesystem();

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    auto file = fs.open(filePath);
    if (luaL_loadstring(L, file.begin()) != 0) {
        throw PreloadFileException(filePath, lua_tostring(L, -1));
    }

    // Modify our filePath, so it can be as package path
    filePath = filePath.substr(0, filePath.size() - 4); // Remove .lua extension
    std::replace(filePath.begin(), filePath.end(), '/', '.'); // Replace '/' to '.'

    lua_setfield(L, -2, filePath.c_str()); // Set our compiled string to preload table
    lua_pop(L, 2); // Pop preload table
}

void preload_folder(lua_State* L, std::string dir) {
    auto fs = cmrc::MoonEngine::get_filesystem();
    for (const auto& f : fs.iterate_directory(dir)) {
        std::string filePath = dir + "/" + f.filename();
        if (f.is_directory())
            preload_folder(L, filePath);
        else
            preload_file(L, filePath);
    }
}

int luaopen_moonscript(lua_State* L) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, luaopen_lpeg);
    lua_setfield(L, -2, "lpeg");
    lua_pop(L, 2); // Pop preload table

    try {
        preload_folder(L, "moonscript");
    } catch (const PreloadFileException& e) {
        luaL_error(L, "failed to preload file \"%s\": %s", e.filename(), e.what());
    } catch (const std::exception& e) {
        luaL_error(L, e.what());
    }

    return 0;
}
