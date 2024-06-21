#include "global.hpp"
#include "core.hpp"
#include "utils.hpp"
#include "config.hpp"
#include <GarrysMod/Lua/Interface.h>

using namespace MoonLoader;

GMOD_MODULE_OPEN() {
    GarrysMod::Lua::ILuaInterface* ILUA = reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA);

    if (Utils::DeveloperEnabled(LUA)) {
        ILUA->MsgColour(MESSAGE_COLOR, "Moonloader %s made by Pika-Software (%s)\n", MOONLOADER_FULL_VERSION, MOONLOADER_URL);
    }

    auto core = Core::Create();
    try {
        core->Initialize(ILUA);
    } catch (const std::exception& e) {
        core->Deinitialize();
        core.reset();

        ILUA->ThrowError(Utils::Format("error during moonloader core initialization: %s", e.what()).c_str());
    }

    return 0;
}

GMOD_MODULE_CLOSE() {
    GarrysMod::Lua::ILuaInterface* ILUA = reinterpret_cast<GarrysMod::Lua::ILuaInterface*>(LUA);

    if (auto core = Core::Get(ILUA)) {
        core->Deinitialize();
        Core::Remove(ILUA);

        if (core.use_count() > 1) {
            core->LUA->MsgColour({255, 0, 0, 255}, "[Moonloader] Core still has %d referenced! Memory leak!!!\n", core.use_count() - 1);
        }
    }

    return 0;
}