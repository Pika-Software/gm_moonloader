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
    LUA = reinterpret_cast<GarrysMod::Lua::CLuaInterface*>(core->LUA);
    callback = LUA->GetLuaGameCallback();
    LUA->SetLuaGameCallback(this);
}

Errors::Errors(std::shared_ptr<Core> core, GarrysMod::Lua::ILuaInterface* _LUA) : core(core) {
    LUA = reinterpret_cast<GarrysMod::Lua::CLuaInterface*>(_LUA);
    callback = LUA->GetLuaGameCallback();
    LUA->SetLuaGameCallback(this);
}

Errors::~Errors() {
    LUA->SetLuaGameCallback(callback);
}

void Errors::TransformStackEntry(std::string& source, int &line) {
    if (!Utils::StartsWith(source, CACHE_PATH_LUA)) return; // TODO: Make it better
    if (auto info = core->compiler->FindFileByFullOutputPath(source)) {
        source = info->full_source_path;
        if (const auto it = info->line_map.find(line); it != info->line_map.end())
            line = it->second;
    }
}

std::optional<ErrorLine> Errors::TransformErrorMessage(std::string& error_message) {
    static std::regex ERROR_MESSAGE_REGEX("^(.*?):(\\d+): (.+)$", 
        std::regex_constants::optimize | std::regex_constants::ECMAScript);

    std::smatch match;
    if (std::regex_search(error_message, match, ERROR_MESSAGE_REGEX)) {
        ErrorLine error;
        error.source = match[1].str();
        error.line = std::stoi(match[2].str());
        error.message = match[3].str();

        TransformStackEntry(error);
        TransformErrorMessage(error.message);
        error_message = error.to_string();
        return error;
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

void Errors::PrintSourceFile(std::string_view code, const ErrorLine& error) {
    auto lines = ReadLines(code, std::max(error.line - 5, 0), error.line + 2);

    TrimLines(lines);

    for (const auto& [num, line] : lines) {
        LUA->MsgColour(Color(125, 125, 125, 255), " %-4d | ", num);
        LUA->MsgColour(Color(175, 192, 198, 255), "%s\n", line.c_str());

        // Check if current line is where error happened
        if (num == error.line) {
            size_t spaces = std::distance(line.cbegin(), std::find_if_not(line.cbegin(), line.cend(), ::isspace));
            LUA->MsgColour(Color(125, 125, 125, 255), "      | ", num);
            LUA->MsgColour(Color(240, 62, 62, 255), "%s^ %s\n", std::string(spaces, ' ').c_str(), error.message.c_str());
        }
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
            PrintSourceFile(code, error_source.value());
    }
}

