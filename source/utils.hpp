#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#pragma once

#include <string>
#include <string_view>
#include <algorithm>
#include <GarrysMod/Lua/LuaBase.h>
#include <platform.h>
#include <chrono>

namespace MoonLoader::Utils {
    // ---------------------------
    // - String manipulation     -
    // ---------------------------
    inline bool StartsWith(std::string_view str, std::string_view prefix) {
        return str.rfind(prefix, 0) == 0;
    }
    inline bool EndsWith(std::string_view str, std::string_view suffix) {
        return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }
    inline void LowerCase(std::string& path) {
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    }
    inline void UpperCase(std::string& path) {
        std::transform(path.begin(), path.end(), path.begin(), ::toupper);
    }
    inline void FixSlashes(std::string& path, char delimiter = '/') {
        std::replace(path.begin(), path.end(), '\\', delimiter);
        std::replace(path.begin(), path.end(), '/', delimiter);
    }
    inline void NormalizePath(std::string& path, char delimiter = '/') {
        FixSlashes(path, delimiter);
        LowerCase(path);
    }
    // dir/file.ext -> dir
    inline std::string_view GetDirectory(std::string_view path) {
        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos)
            return {};

        return path.substr(0, pos);
    }
    // dir + subdir/file.ext -> dir/subdir/file.ext
    inline std::string JoinPaths(std::string_view path, std::string_view subPath) {
        if (path.empty())
            return std::string(subPath);
        if (subPath.empty())
            return std::string(path);
        if (path.back() == '/' || path.back() == '\\')
            return std::string(path) + std::string(subPath);
        return std::string(path) + '/' + std::string(subPath);
    }

    // ---------------------------
    // - Lua utils               -
    // ---------------------------
    void FindValue(GarrysMod::Lua::ILuaBase* LUA, std::string_view path);
    bool RunHook(GarrysMod::Lua::ILuaBase* LUA, const std::string& hookName, int nArgs, int nReturns);

    // ---------------------------
    // - Other                   -
    // ---------------------------
    inline uint64 Timestamp() {
        // Oh, yesss! I love one-liners!
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock().now().time_since_epoch()).count();
    }
}

#endif // MOONLOADER_UTILS_HPP