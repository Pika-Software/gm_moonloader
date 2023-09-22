#include "errors.hpp"
#include "core.hpp"
#include "compiler.hpp"
#include "utils.hpp"
#include "global.hpp"
#include "filesystem.hpp"

#include <GarrysMod/Lua/LuaInterface.h>
#include <regex>
#include <sstream>
#include <algorithm>
#include <map>

using namespace MoonLoader;

Errors::Errors(std::shared_ptr<Core> core) : core(core) {
    auto LUA = reinterpret_cast<GarrysMod::Lua::CLuaInterface*>(core->LUA);
    callback = LUA->GetLuaGameCallback();
    LUA->SetLuaGameCallback(this);
}

Errors::~Errors() {
    auto LUA = reinterpret_cast<GarrysMod::Lua::CLuaInterface*>(core->LUA);
    LUA->SetLuaGameCallback(callback);
}

void Errors::TransformStackEntry(GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry& entry) {
    if (!Utils::StartsWith(entry.source, CACHE_PATH_LUA)) return; // TODO: Make it better
    if (auto info = core->compiler->FindFileByFullOutputPath(entry.source)) {
        entry.source = info->full_source_path;
        if (const auto it = info->line_map.find(entry.line); it != info->line_map.end())
            entry.line = it->second;
    }
}

std::optional<GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry> Errors::TransformErrorMessage(std::string& err) {
    static std::regex ERROR_MESSAGE_REGEX("^(.*?):(\\d+): (.+)$", 
        std::regex_constants::optimize | std::regex_constants::multiline);
    std::smatch match;
    if (std::regex_search(err, match, ERROR_MESSAGE_REGEX)) {
        GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry entry;
        entry.source = match[1].str();
        entry.line = std::atoi(match[2].first.base());
        std::string message = match[3].str();

        TransformStackEntry(entry);
        TransformErrorMessage(message);
        err = entry.source + ":" + std::to_string(entry.line) + ": " + message;
        return entry;
    }
    return {};
}

inline std::map<int, std::string> ReadLines(std::string_view code, int bottom_line, int top_line) {
    std::map<int, std::string> lines;
    Utils::Split(code, [&](auto line_str, auto current_line) {
        if (current_line < bottom_line || current_line > top_line) return;
        lines[current_line] = line_str;
    });
    return lines;
}

inline void TrimLines(std::map<int, std::string>& lines) {
    size_t min_spaces = -1;
    for (auto& [num, line] : lines) {
        size_t spaces = std::distance(line.cbegin(), std::find_if_not(line.cbegin(), line.cend(), ::isspace));
        if (spaces == line.size()) continue;
        min_spaces = std::min(min_spaces, spaces);
    }

    for (auto& [num, line] : lines) {
        if (line.size() > min_spaces)
            line.erase(0, min_spaces);
        Utils::RightTrim(line);
    }
}

void Errors::PrintSourceFile(std::string_view code, int error_line) {
    auto lines = ReadLines(code, std::max(error_line - 5, 0), error_line + 2);

    TrimLines(lines);

    for (const auto& [num, line] : lines) {
        std::stringstream ss;
        ss << " " << (error_line == num ? "-->" : "") << "\t";
        ss << num << "\t";
        ss << "| " << line;
        core->LUA->Msg("%s\n", ss.str().c_str());
    }
}

void Errors::LuaError(const GarrysMod::Lua::ILuaGameCallback::CLuaError *error) {
    GarrysMod::Lua::ILuaGameCallback::CLuaError custom_error = *error;

    auto error_source = TransformErrorMessage(custom_error.message);

    for (auto& entry : custom_error.stack)
        TransformStackEntry(entry);

    callback->LuaError(&custom_error);

    if (error_source) {
        auto code = core->fs->ReadTextFile(error_source->source, "garrysmod");
        if (!code.empty())
            PrintSourceFile(code, error_source->line);
    }
}

