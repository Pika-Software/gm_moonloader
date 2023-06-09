#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <algorithm>
#include <GarrysMod/Lua/LuaInterface.h>
#include <chrono>
#include <optional>

#if IS_SERVERSIDE
#include <GarrysMod/FactoryLoader.hpp>
#include <GarrysMod/ModuleLoader.hpp>
#endif

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
    // dir/file.ext -> file.ext
    inline std::string_view FileName(std::string_view path) {
        auto namePos = path.find_first_of("/\\");
        return namePos != std::string_view::npos ? path.substr(namePos + 1) : path;
    }
    // dir/file.ext -> ext
    inline std::string_view FileExtension(std::string_view path) {
        auto fileName = FileName(path);
        auto extPos = !fileName.empty() ? fileName.find_first_of('.') : std::string_view::npos;
        return extPos != std::string_view::npos ? fileName.substr(extPos + 1) : std::string_view{};
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
    // dir/file.ext -> dir/file
    inline void StripFileExtension(std::string& path) {
        auto namePos = path.find_last_of('.');
        size_t slashPos = path.find_last_of("/\\");
        if (namePos != std::string::npos && (slashPos == std::string::npos || namePos > slashPos)) 
            path.erase(namePos);
    }
    // dir/file.ext + .bak -> dir/file.bak
    inline void SetFileExtension(std::string& path, std::string_view ext) {
        if (FileExtension(path) == ext) return;
        StripFileExtension(path);
        if (!ext.empty() && ext.front() != '.') path += '.';
        path += ext;
    }

    inline void RemovePrefix(std::string& str, std::string_view prefix) {
        if (StartsWith(str, prefix))
            str.erase(0, prefix.size());
    }

    // ---------------------------
    // - Lua utils               -
    // ---------------------------
    inline void FindValue(GarrysMod::Lua::ILuaBase* LUA, std::string_view path) {
        size_t firstPos = 0;
        size_t endPos = 0;
        do {
            firstPos = endPos;
            endPos = path.find(".", endPos) + 1;
            std::string name{ path.substr(firstPos, endPos != 0 ? endPos - firstPos - 1 : path.size()) };

            LUA->GetField(firstPos == 0 ? GarrysMod::Lua::INDEX_GLOBAL : -1, name.c_str());
            if (firstPos != 0) LUA->Remove(-2);
            if (!LUA->IsType(-1, GarrysMod::Lua::Type::Table)) break;
        } while (endPos != 0);
    }
    inline bool RunHook(GarrysMod::Lua::ILuaInterface* LUA, const std::string& hookName, int nArgs, int nReturns) {
        FindValue(LUA, "hook.Run");
        if (!LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
            LUA->Pop();
            return false;
        }

        LUA->Insert(-nArgs - 1);
        LUA->PushString(hookName.c_str());
        LUA->Insert(-nArgs - 1);
        if (LUA->PCall(nArgs + 1, nReturns, 0) != 0) {
            LUA->ErrorNoHalt("[MoonLoader] Failed to run hook '%s': %s\n", hookName.c_str(), LUA->GetString(-1));
            LUA->Pop();
            return false;
        }

        return true;
    }
    inline bool PopBool(GarrysMod::Lua::ILuaBase* LUA) {
        bool value = LUA->GetBool(-1);
        LUA->Pop();
        return value;
    }
    inline std::optional<bool> LuaBoolFromValue(GarrysMod::Lua::ILuaBase* LUA, std::string_view path, int args) {
        FindValue(LUA, path);
        if (LUA->IsType(-1, GarrysMod::Lua::Type::Bool)) {
            return PopBool(LUA);
        }
        else if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
            LUA->Insert(-1 - args);
            if (LUA->PCall(args, 1, 0) != 0) {
                LUA->Pop();
                return std::nullopt;
            }
            return PopBool(LUA);
        }

        LUA->Pop();
        return std::nullopt;
    }
    inline bool DeveloperEnabled(GarrysMod::Lua::ILuaBase* LUA) {
        LUA->PushString("developer");
        return LuaBoolFromValue(LUA, "cvars.Bool", 1).value_or(false);
    }
    bool FindMoonScript(GarrysMod::Lua::ILuaInterface* LUA, std::string& path);

    // ---------------------------
    // - Other                   -
    // ---------------------------
    inline uint64_t Timestamp() {
        // Oh, yesss! I love one-liners!
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock().now().time_since_epoch()).count();
    }

#if IS_SERVERSIDE
    template<class T>
    inline T* LoadInterface(const char* moduleName, const char* version) {
        SourceSDK::FactoryLoader module(moduleName);
        return module.GetInterface<T>(version);
    }

    template<class T = void>
    inline T* LoadSymbol(const char* moduleName, const std::string& symbol) {
        SourceSDK::ModuleLoader module(moduleName);
        return reinterpret_cast<T*>(module.GetSymbol(symbol));
    }
#endif
}

#endif // MOONLOADER_UTILS_HPP