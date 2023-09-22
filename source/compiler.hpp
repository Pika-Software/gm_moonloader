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
    class Core;

    class Compiler {
    public:
        struct CompiledFile {
            enum Type {
                Moonscript,
                Yuescript
            };

            std::string source_path;
            std::string full_source_path;
            std::string output_path;
            std::string full_output_path;
            size_t update_date = 0;
            Type type;

            std::unordered_map<int, int> line_map;
        };

    private:
        std::shared_ptr<Core> core;
        std::shared_ptr<Filesystem> fs;
        std::shared_ptr<MoonEngine::Engine> moonengine;
        std::shared_ptr<Watchdog> watchdog;
        std::unordered_map<std::string, CompiledFile> compiled_files;

    public:
        Compiler(std::shared_ptr<Core> core,
                 std::shared_ptr<Filesystem> fs,
                 std::shared_ptr<MoonEngine::Engine> moonengine, 
                 std::shared_ptr<Watchdog> watchdog)
            : core(core), fs(fs), moonengine(moonengine), watchdog(watchdog) {}

        bool NeedsCompile(const std::string& path);
        const CompiledFile* FindFileByFullSourcePath(const std::string& full_source_path) const {
            for (const auto& [path, info] : compiled_files)
                if (info.full_source_path == full_source_path)
                    return &info;
            return nullptr;
        }
        const CompiledFile* FindFileByFullOutputPath(std::string_view full_output_path) const {
            for (const auto& [path, info] : compiled_files)
                if (info.full_output_path == full_output_path)
                    return &info;
            return nullptr;
        }

        bool CompileFile(const std::string& path, bool force = false);
    };
}

#endif
