#include "compiler.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"
#include "utils.hpp"
#include "core.hpp"

#include <tier1/utlbuffer.h>
#include <filesystem.h>
#include <moonengine/engine.hpp>
#include <yuescript/yue_compiler.h>
#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

inline size_t LookupLineFromOffset(std::string_view str, size_t offset) {
    int line = 1;
    for (size_t pos = 0; pos < str.size(); pos++) {
        if (str[pos] == '\n')
            line++;

        if (pos == offset)
            return line;
    }
    return -1;
}

bool Compiler::WasModified(GarrysMod::Lua::ILuaInterface* LUA, const std::string& path) {
    size_t lastModification = fs->GetFileTime(path, LUA->GetPathID());
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

    auto readData = fs->ReadBinaryFile(path, LUA->GetPathID());
    if (readData.empty())
        return false;

    // Watch for changes of .moon file
    watchdog->WatchFile(path, LUA->GetPathID());

    MoonEngine::Engine::CompiledLines lines;
    std::string data;
    if (!moonengine->CompileStringEx(readData.data(), readData.size(), data, &lines)) {
        Warning("[Moonloader] Compilation of '%s' failed:\n%s\n", path.c_str(), data.c_str());
        return false;
    }

    // Create directories for .lua file
    std::string dir = path;
    fs->StripFileName(dir);
    fs->CreateDirs(dir.c_str(), "MOONLOADER");

    // Write compiled code to .lua file
    std::string compiledPath = path;
    fs->SetFileExtension(compiledPath, "lua");
    if (!fs->WriteToFile(compiledPath, "MOONLOADER", data.c_str(), data.size()))
        return false;

    // Add debug information
    std::string fullSourcePath = fs->RelativeToFullPath(path, LUA->GetPathID());
    fullSourcePath = fs->FullToRelativePath(fullSourcePath, "garrysmod"); // Platform is the root directory of the game, after garrysmod/
    Filesystem::Normalize(fullSourcePath);

    std::string fullCompiledPath = fs->RelativeToFullPath(path, "MOONLOADER");
    fullCompiledPath = fs->FullToRelativePath(fullCompiledPath, "garrysmod");
    Filesystem::Normalize(fullCompiledPath);

    MoonDebug debug {};
    debug.sourcePath = std::move(path);
    debug.fullSourcePath = std::move(fullSourcePath);
    debug.compiledPath = std::move(compiledPath);
    debug.fullCompiledPath = std::move(fullCompiledPath);
    debug.lastFileModification = fs->GetFileTime(debug.sourcePath, LUA->GetPathID());

    for (auto it = lines.begin(); it != lines.end(); ++it) {
        debug.lines.insert_or_assign(it->first, LookupLineFromOffset({ readData.data(), readData.size() }, it->second));
    }

    m_CompiledFiles.insert_or_assign(debug.sourcePath, debug);

    // Notice lua about compiled file
    LUA->PushString(debug.sourcePath.c_str());
    Utils::RunHook(LUA, "MoonFileCompiled", 1, 0);

    return true;
}

bool Compiler::CompileFile(const std::string& path) {
    // TODO: check if file wasnt modified
    auto code = fs->ReadTextFile(path, core->LUA->GetPathID());
    if (code.empty()) return false;

    // TODO: watch for file changes

    CompiledFile compiled_file;
    std::string lua_code;
    compiled_file.path = path;
    if (Utils::FileExtension(path) == "yue") {
        auto info = yuecompiler->compile(code);
        if (info.error) {
            Warning("[Moonloader] Yuescript compilation of '%s' failed:\n%s\n", path.c_str(), info.error->displayMessage.c_str());
            return false;
        }
        lua_code = std::move(info.codes);
    } else {
        auto info = moonengine->CompileString2(code);
        if (info.error) {
            Warning("[Moonloader] Moonscript compilation of '%s' failed:\n%s\n", path.c_str(), info.error->display_msg.c_str());
            return false;
        }
        lua_code = std::move(info.lua_code);
    }

    std::string dir = path;
    fs->StripFileName(dir);
    fs->CreateDirs(dir, "MOONLOADER");

    compiled_file.output_path = path;
    fs->SetFileExtension(compiled_file.output_path, "lua");
    if (!fs->WriteToFile(compiled_file.output_path, "MOONLOADER", lua_code.c_str(), lua_code.size()))
        return false;

    compiled_files.insert_or_assign(path, compiled_file);

    return true;
}
