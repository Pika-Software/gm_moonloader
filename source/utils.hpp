#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <GarrysMod/Lua/LuaInterface.h>
#include <chrono>
#include <optional>
#include <memory>
#include <unordered_map>
#include <map>
#include <sstream>

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
    inline void RightTrim(std::string &s) {
        s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(), s.end());
    }
    inline void LowerCase(std::string& path) {
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    }
    inline void UpperCase(std::string& path) {
        std::transform(path.begin(), path.end(), path.begin(), ::toupper);
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
    inline std::optional<double> OptNumber(GarrysMod::Lua::ILuaBase* LUA, int index) {
        return LUA->IsType(index, GarrysMod::Lua::Type::Number) ? 
            std::optional(LUA->GetNumber(index)) : 
            std::nullopt;
    }
    inline bool DeveloperEnabled(GarrysMod::Lua::ILuaBase* LUA) {
        LUA->PushString("developer");
        return LuaBoolFromValue(LUA, "cvars.Bool", 1).value_or(false);
    }

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

    inline std::optional<int> FindClosestLine(const std::map<int, int>& lines, int line) {
        int closest = -1;
        for (auto& [key, value] : lines) {
            if (key > line) break;
            closest = value;
        }
        return closest >= 0 ? std::optional(closest) : std::nullopt;
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

// --------------------------- Path manipulation ---------------------------
namespace MoonLoader::Utils::Path {
    // Resolves path, removes ".." and "." segments, and removes any duplicate slashes
    inline void Resolve(std::string& path) {
        std::vector<std::string_view> segments;
        auto startPos = path.begin();
        auto pointer = startPos;
        bool hasWindowsDrive = false;
        for (auto pointer = startPos;; pointer++) {
            if (pointer == path.end() || *pointer == '/' || *pointer == '\\') {
                std::string_view part(&*startPos, pointer - startPos);
                if (path == "..") {
                    // Pop only if there are segments, and it is not a windows drive
                    if (segments.size() > 0 && (segments.size() != 1 || !hasWindowsDrive))
                        segments.pop_back();
                } else if (part == ".") {
                    // If single dot is at the end, then add empty segment
                    if (pointer == path.end()) 
                        segments.push_back("");
                } else {
                    // Detect if first segment is a windows drive
                    if (segments.size() == 0 && part.length() == 2 && std::isalpha(part[0]) && part[1] == ':')
                        hasWindowsDrive = true;
       
                    // Do not add empty segments
                    // only if it is the first segment or the last one
                    if (part.length() != 0 || pointer == path.begin() || pointer == path.end())
                        segments.push_back(part);
                }

                startPos = pointer + 1;
                if (pointer == path.end())
                    break;
            }
        }

        std::stringstream buffer;
        for (auto it = segments.begin(); it != segments.end(); it++) {
            buffer << *it;
            if (it != segments.end() - 1) buffer << "/";
        }
        path = buffer.str();
    }
    inline void FixSlashes(std::string& path, char delimiter = '/') {
        std::replace(path.begin(), path.end(), '\\', delimiter);
        std::replace(path.begin(), path.end(), '/', delimiter);
    }
    inline void Normalize(std::string& path, char delimiter = '/') {
        FixSlashes(path, delimiter);
        LowerCase(path);
        Resolve(path);
    }
    // dir/file.ext -> dir/
    inline std::string_view Directory(std::string_view path) {
        size_t pos = path.find_last_of("/\\");
        return pos != std::string::npos ? path.substr(0, pos + 1) : std::string_view();
    }
    // dir/file.ext -> file.ext
    inline std::string_view FileName(std::string_view path) {
        auto namePos = path.find_last_of("/\\");
        return namePos != std::string_view::npos ? path.substr(namePos + 1) : path;
    }
    // dir/file.ext -> ext
    inline std::string_view Extension(std::string_view path) {
        auto fileName = FileName(path);
        auto extPos = !fileName.empty() ? fileName.find_last_of('.') : std::string_view::npos;
        return extPos != std::string_view::npos ? fileName.substr(extPos + 1) : std::string_view();
    }
    // dir/file.ext -> dir/
    inline void StripFileName(std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) path.erase(pos + 1);
        else path.clear();
    }
    // dir/file.ext -> dir/file
    inline void StripExtension(std::string& path) {
        auto namePos = path.find_last_of("/\\");
        auto extPos = path.find_last_of('.');
        if (extPos != std::string::npos && (namePos == std::string::npos || extPos > namePos))
            path.erase(extPos);
    }
    // dir/file.ext + .txt -> dir/file.txt
    inline void SetExtension(std::string& path, std::string_view ext) {
        StripExtension(path);
        if (!ext.empty() && ext.front() != '.')
            path += '.';
        path += ext;
    }
    inline std::string Join(std::initializer_list<std::string_view> paths) {
        std::string result = {};
        for (auto& path : paths) {
            if (result.empty() || result.back() == '/' || result.back() == '\\') {
                result.append(path);
            } else if (path.empty()) {
                continue;
            } else {
                result.push_back('/');
                result.append(path);
            }
        }
        return result;
    }
    template <typename... Paths> inline std::string Join(Paths... paths) {
        return Join({std::forward<Paths>(paths)...});
    }
}

#endif // MOONLOADER_UTILS_HPP
