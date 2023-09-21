#include "errors.hpp"
#include "core.hpp"
#include "compiler.hpp"
#include "utils.hpp"
#include "global.hpp"

#include <GarrysMod/Lua/LuaInterface.h>

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
        
    }
}

void Errors::LuaError(const GarrysMod::Lua::ILuaGameCallback::CLuaError *error) {
    GarrysMod::Lua::ILuaGameCallback::CLuaError custom_error = *error;

    for (auto& entry : custom_error.stack)
        TransformStackEntry(entry);

    return callback->LuaError(&custom_error);
}

