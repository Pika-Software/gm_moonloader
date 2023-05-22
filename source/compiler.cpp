#include "compiler.hpp"
#include "global.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"

#include <tier1/utlbuffer.h>
#include <filesystem.h>
#include <moonengine/engine.hpp>

size_t LookupLineFromOffset(std::string_view str, size_t offset) {
    int line = 1;
    for (size_t pos = 0; pos < str.size(); pos++) {
        if (str[pos] == '\n')
            line++;

        if (pos == offset)
            return line;
    }
    return -1;
}

namespace MoonLoader {
    bool Compiler::CompileMoonScript(std::string path) {
        auto readData = g_pFilesystem->ReadBinaryFile(path, GMOD_LUA_PATH_ID);
        if (readData.empty())
            return false;

        MoonEngine::Engine::CompiledLines lines;
        auto data = g_pMoonEngine->CompileString(readData.data(), readData.size(), &lines);
        if (data.empty())
            return false;

        // Create directories for .lua file
        std::string dir = path;
        g_pFilesystem->StripFileName(dir);
        g_pFilesystem->CreateDirs(dir.c_str(), "MOONLOADER");

        // Watch for changes of .moon file
        g_pWatchdog->WatchFile(path, GMOD_LUA_PATH_ID);

        // Write compiled code to .lua file
        std::string compiledPath = path;
        g_pFilesystem->SetFileExtension(compiledPath, "lua");
        if (!g_pFilesystem->WriteToFile(compiledPath, "MOONLOADER", data.c_str(), data.size()))
            return false;

        // Add debug information
        std::string fullSourcePath = g_pFilesystem->RelativeToFullPath(path, GMOD_LUA_PATH_ID);
        fullSourcePath = g_pFilesystem->FullToRelativePath(fullSourcePath, "garrysmod"); // Platform is the root directory of the game, after garrysmod/
        Filesystem::Normalize(fullSourcePath);

        std::string fullCompiledPath = g_pFilesystem->RelativeToFullPath(path, "MOONLOADER");
        fullCompiledPath = g_pFilesystem->FullToRelativePath(fullCompiledPath, "garrysmod");
        Filesystem::Normalize(fullCompiledPath);

        MoonDebug debug {};
        debug.sourcePath = std::move(path);
        debug.fullSourcePath = std::move(fullSourcePath);
        debug.compiledPath = std::move(compiledPath);
        debug.fullCompiledPath = std::move(fullCompiledPath);

        for (auto it = lines.begin(); it != lines.end(); ++it) {
            debug.lines.insert_or_assign(it->first, LookupLineFromOffset({ readData.data(), readData.size() }, it->second));
        }

        m_CompiledFiles.insert_or_assign(debug.sourcePath, debug);

        return true;
    }
}