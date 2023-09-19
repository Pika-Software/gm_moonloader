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

namespace yue {
    class YueCompiler;
}

namespace MoonLoader {
    class Filesystem;
    class Watchdog;
    class Core;

    class Compiler {
    public:
        struct CompiledFile {
            std::string path;
            std::string output_path;
            size_t update_date = 0;
        };

    private:
        std::shared_ptr<Core> core;
        std::shared_ptr<Filesystem> fs;
        std::shared_ptr<MoonEngine::Engine> moonengine;
        std::shared_ptr<yue::YueCompiler> yuecompiler;
        std::shared_ptr<Watchdog> watchdog;
        std::unordered_map<std::string, CompiledFile> compiled_files;

    public:
        Compiler(std::shared_ptr<Core> core,
                 std::shared_ptr<Filesystem> fs,
                 std::shared_ptr<MoonEngine::Engine> moonengine, 
                 std::shared_ptr<yue::YueCompiler> yuecompiler,
                 std::shared_ptr<Watchdog> watchdog)
            : core(core), fs(fs), moonengine(moonengine), yuecompiler(yuecompiler), watchdog(watchdog) {}

        bool NeedsCompile(const std::string& path);

        bool CompileFile(const std::string& path, bool force = false);
    };
}

#endif
