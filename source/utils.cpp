#include "utils.hpp"

using namespace MoonLoader;

void Utils::FindValue(GarrysMod::Lua::ILuaBase* LUA, std::string_view path) {
    size_t firstPos = 0;
    size_t endPos = 0;
    do {
        firstPos = endPos;
        endPos = path.find(".", endPos) + 1;
        std::string name{ path.substr(firstPos, endPos != 0 ? endPos - firstPos - 1 : path.size()) };

        LUA->GetField(firstPos == 0 ? GarrysMod::Lua::INDEX_GLOBAL : -1, name.c_str());
        if (firstPos != 0) LUA->Remove(-2);
        if (!LUA->IsType(-1, GarrysMod::Lua::Type::Table)) break;
    } while (endPos != 0);
}

bool Utils::RunHook(GarrysMod::Lua::ILuaBase* LUA, const std::string& hookName, int nArgs, int nReturns) {
    FindValue(LUA, "hook.Run");
    if (!LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
        LUA->Pop();
        return false;
    }

    LUA->Insert(-nArgs - 1);
    LUA->PushString(hookName.c_str());
    LUA->Insert(-nArgs - 1);
    if (LUA->PCall(nArgs + 1, nReturns, 0) != 0) {
        Warning("[Moonloader] Error while running hook %s: %s\n", hookName.c_str(), LUA->GetString(-1));
        LUA->Pop();
        return false;
    }

    return true;
}