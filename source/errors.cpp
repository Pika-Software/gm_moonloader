#include "errors.hpp"
#include "core.hpp"

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

void Errors::LuaError(const GarrysMod::Lua::ILuaGameCallback::CLuaError *error) {
    return callback->LuaError(error);
}

