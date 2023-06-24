#ifndef MOONLOADER_COMPILER_HPP
#define MOONLOADER_COMPILER_HPP

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <optional>

namespace MoonLoader {
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
        std::unordered_map<std::string, MoonDebug> m_CompiledFiles;

    public:
        // Gets debug info for compiled .moon file (in LUA directory)
        inline std::optional<MoonDebug> GetDebugInfo(const std::string& path) {
            auto it = m_CompiledFiles.find(path);
            return it != m_CompiledFiles.end() ? std::optional(it->second) : std::nullopt;
        }
        // Checks if .moon file is compiled (in LUA directory)
        inline bool IsCompiled(const std::string& path) {
            return m_CompiledFiles.find(path) != m_CompiledFiles.end();
        }

        bool WasModified(const std::string& path);

        bool CompileMoonScript(std::string path, bool force = false);
    };
}

#endif