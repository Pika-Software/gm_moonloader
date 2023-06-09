#include "compiler.hpp"
#include "global.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"
#include "utils.hpp"

#include <tier1/utlbuffer.h>
#include <filesystem.h>
#include <moonengine/engine.hpp>
#include <GarrysMod/Lua/LuaInterface.h>

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
    bool Compiler::WasModified(GarrysMod::Lua::ILuaInterface* LUA, const std::string& path) {
        size_t lastModification = g_Filesystem->GetFileTime(path, LUA->GetPathID());
        auto debug = GetDebugInfo(path);
        if (debug) {
            return debug->lastFileModification != lastModification;
        }
        return true;
    }

    bool Compiler::CompileMoonScript(GarrysMod::Lua::ILuaInterface* LUA, std::string path, bool force) {
        if (!force && !WasModified(LUA, path)) {
            return true;
        }

        auto readData = g_Filesystem->ReadBinaryFile(path, LUA->GetPathID());
        if (readData.empty())
            return false;

        // Watch for changes of .moon file
        g_Watchdog->WatchFile(path, LUA->GetPathID());

        MoonEngine::Engine::CompiledLines lines;
        auto data = g_MoonEngine->CompileString(readData.data(), readData.size(), &lines);
        if (data.empty())
            return false;

        // Create directories for .lua file
        std::string dir = path;
        g_Filesystem->StripFileName(dir);
        g_Filesystem->CreateDirs(dir.c_str(), "MOONLOADER");

        // Write compiled code to .lua file
        std::string compiledPath = path;
        g_Filesystem->SetFileExtension(compiledPath, "lua");
        if (!g_Filesystem->WriteToFile(compiledPath, "MOONLOADER", data.c_str(), data.size()))
            return false;

        // Add debug information
        std::string fullSourcePath = g_Filesystem->RelativeToFullPath(path, LUA->GetPathID());
        fullSourcePath = g_Filesystem->FullToRelativePath(fullSourcePath, "garrysmod"); // Platform is the root directory of the game, after garrysmod/
        Filesystem::Normalize(fullSourcePath);

        std::string fullCompiledPath = g_Filesystem->RelativeToFullPath(path, "MOONLOADER");
        fullCompiledPath = g_Filesystem->FullToRelativePath(fullCompiledPath, "garrysmod");
        Filesystem::Normalize(fullCompiledPath);

        MoonDebug debug {};
        debug.sourcePath = std::move(path);
        debug.fullSourcePath = std::move(fullSourcePath);
        debug.compiledPath = std::move(compiledPath);
        debug.fullCompiledPath = std::move(fullCompiledPath);
        debug.lastFileModification = g_Filesystem->GetFileTime(debug.sourcePath, LUA->GetPathID());

        for (auto it = lines.begin(); it != lines.end(); ++it) {
            debug.lines.insert_or_assign(it->first, LookupLineFromOffset({ readData.data(), readData.size() }, it->second));
        }

        m_CompiledFiles.insert_or_assign(debug.sourcePath, debug);

        // Notice lua about compiled file
        LUA->PushString(debug.sourcePath.c_str());
        Utils::RunHook(LUA, "MoonFileCompiled", 1, 0);

        return true;
    }
}