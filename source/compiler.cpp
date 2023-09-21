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

    
    compiled_file.full_output_path = fs->TransverseRelativePath(compiled_file.output_path, "MOONLOADER", "garrysmod");
    compiled_file.update_date = fs->GetFileTime(path, core->LUA->GetPathID());
    compiled_files.insert_or_assign(path, compiled_file);

    return true;
}
