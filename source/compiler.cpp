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
#include <regex>

using namespace MoonLoader;

std::unordered_map<int, int> ParseYueLines(std::string_view code) {
    static std::regex YUE_LINE_REGEX("--\\s*(\\d*)\\s*$", std::regex_constants::optimize);

    std::unordered_map<int, int> line_map;
    Utils::Split(code, [&](auto line, auto num) {
        std::match_results<std::string_view::const_iterator> match;
        if (std::regex_search(line.cbegin(), line.cend(), match, YUE_LINE_REGEX)) {
            int source_line = std::atoi(match[1].first);
            line_map[num] = source_line;
        }
    });
    return line_map;
}

bool Compiler::NeedsCompile(const std::string& path) {
    auto it = compiled_files.find(path);
    if (it == compiled_files.end()) return true;

    auto update_date = fs->GetFileTime(path, core->LUA->GetPathID());
    return update_date > it->second.update_date;
}

bool Compiler::CompileFile(const std::string& path, bool force) {
    if (!force && !NeedsCompile(path)) return true;

    auto code = fs->ReadTextFile(path, core->LUA->GetPathID());
    if (code.empty()) return false;

    watchdog->WatchFile(path, core->LUA->GetPathID());

    CompiledFile compiled_file;
    std::string lua_code;
    compiled_file.source_path = path;
    if (Utils::FileExtension(path) == "yue") {
        auto info = yuecompiler->compile(code);
        if (info.error) {
            Warning("[Moonloader] Yuescript compilation of '%s' failed:\n%s\n", path.c_str(), info.error->displayMessage.c_str());
            return false;
        }
        compiled_file.line_map = ParseYueLines(info.codes);
        compiled_file.type = CompiledFile::Yuescript;
        lua_code = std::move(info.codes);
    } else {
        auto info = moonengine->CompileString2(code);
        if (info.error) {
            Warning("[Moonloader] Moonscript compilation of '%s' failed:\n%s\n", path.c_str(), info.error->display_msg.c_str());
            return false;
        }
        compiled_file.type = CompiledFile::Yuescript;
        lua_code = std::move(info.lua_code);
        for (const auto& [source_line, pos] : info.posmap)
            compiled_file.line_map[source_line] = pos.line;
    }

    std::string dir = path;
    fs->StripFileName(dir);
    fs->CreateDirs(dir, "MOONLOADER");

    compiled_file.output_path = path;
    fs->SetFileExtension(compiled_file.output_path, "lua");
    if (!fs->WriteToFile(compiled_file.output_path, "MOONLOADER", lua_code.c_str(), lua_code.size()))
        return false;

    compiled_file.full_source_path = fs->TransverseRelativePath(compiled_file.source_path, core->LUA->GetPathID(), "garrysmod");
    compiled_file.full_output_path = fs->TransverseRelativePath(compiled_file.output_path, "MOONLOADER", "garrysmod");
    compiled_file.update_date = fs->GetFileTime(path, core->LUA->GetPathID());
    compiled_files.insert_or_assign(path, compiled_file);

    return true;
}
