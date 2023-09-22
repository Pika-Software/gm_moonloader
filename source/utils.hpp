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
#include <memory>

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

    template <char DELIMITER = '\n', class Func>
    inline void Split(std::string_view str, Func f) {
        std::string_view::size_type last = 0, next = 0, line = 1;
        while ((next = str.find(DELIMITER, last)) != std::string_view::npos) {
            f(str.substr(last, next - last), line);
            last = next + 1;
            line++;
        }
        f(str.substr(last), line);
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
    inline std::string_view GetString(GarrysMod::Lua::ILuaBase* LUA, int index) {
        unsigned int len = 0;
        const char* str = LUA->GetString(index, &len);
        return std::string_view(str, len);
    }
    inline std::string_view CheckString(GarrysMod::Lua::ILuaBase* LUA, int index) {
        LUA->CheckType(index, GarrysMod::Lua::Type::String);
        return GetString(LUA, index);
    }
    inline void PushString(GarrysMod::Lua::ILuaBase* LUA, std::string_view str) {
        LUA->PushString(str.data(), str.size());
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
    // https://stackoverflow.com/a/2342176/11635796
    template<typename ... Args>
    std::string Format(const std::string& format, Args ... args) {
        int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
        if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
        auto size = static_cast<size_t>(size_s);
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
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
