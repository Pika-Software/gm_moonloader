#ifndef MOONLOADER_COMPILER_HPP
#define MOONLOADER_COMPILER_HPP

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <GarrysMod/Lua/LuaInterface.h>

namespace MoonEngine {
    class Engine;
}

namespace MoonLoader {
    class Filesystem;
    class Watchdog;

    class Compiler {
    public:
        struct MoonDebug {
            // Compiled line -> original line
            std::unordered_map<size_t, size_t> lines;

            std::string sourcePath;
            std::string fullSourcePath;
            std::string compiledPath;
            std::string fullCompiledPath;

            size_t lastFileModification;
        };

    private:
        std::shared_ptr<Filesystem> fs;
        std::shared_ptr<MoonEngine::Engine> moonengine;
        std::shared_ptr<Watchdog> watchdog;
        std::unordered_map<std::string, MoonDebug> m_CompiledFiles;

    public:
        Compiler(std::shared_ptr<Filesystem> fs, std::shared_ptr<MoonEngine::Engine> moonengine, std::shared_ptr<Watchdog> watchdog) 
            : fs(fs), moonengine(moonengine), watchdog(watchdog) {}

        // Gets debug info for compiled .moon file (in LUA directory)
        inline std::optional<MoonDebug> GetDebugInfo(const std::string& path) {
            auto it = m_CompiledFiles.find(path);
            return it != m_CompiledFiles.end() ? std::optional(it->second) : std::nullopt;
        }
        // Checks if .moon file is compiled (in LUA directory)
        inline bool IsCompiled(const std::string& path) {
            return m_CompiledFiles.find(path) != m_CompiledFiles.end();
        }

        bool WasModified(GarrysMod::Lua::ILuaInterface* LUA, const std::string& path);

        bool CompileMoonScript(GarrysMod::Lua::ILuaInterface* LUA, std::string path, bool force = false);
    };
}

#endif