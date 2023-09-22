#include "errors.hpp"
#include "core.hpp"
#include "compiler.hpp"
#include "utils.hpp"
#include "global.hpp"

#include <GarrysMod/Lua/LuaInterface.h>
#include <regex>

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

void Errors::LuaError(const GarrysMod::Lua::ILuaGameCallback::CLuaError *error) {
    GarrysMod::Lua::ILuaGameCallback::CLuaError custom_error = *error;

    auto error_source = TransformErrorMessage(custom_error.message);

    for (auto& entry : custom_error.stack)
        TransformStackEntry(entry);

    callback->LuaError(&custom_error);
}

