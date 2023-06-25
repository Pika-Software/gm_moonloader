#include "global.hpp"
#include "utils.hpp"
#include "config.hpp"

#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

constexpr unsigned int MAX_VERSION_LENGTH = 32;

bool IsVersionGreater(int major, int minor, int patch) {
    if (MOONLOADER_VERSION_MAJOR != major) return MOONLOADER_VERSION_MAJOR < major;
    if (MOONLOADER_VERSION_MINOR != minor) return MOONLOADER_VERSION_MINOR < minor;
    if (MOONLOADER_VERSION_PATCH != patch) return MOONLOADER_VERSION_PATCH < patch;
    return false;
}

LUA_FUNCTION(ValidateVersion) {
    unsigned int bodySize = 0;
    const char* body = LUA->GetString(1, &bodySize);
    int code = LUA->GetNumber(4);

    if (code == 200 || bodySize <= MAX_VERSION_LENGTH) {
        int major, minor, patch;
        if (sscanf(body, "%d.%d.%d", &major, &minor, &patch) == 3 && IsVersionGreater(major, minor, patch)) {
            Msg("[Mooloader] New version available: %d.%d.%d -> %d.%d.%d (%s)\n", 
                MOONLOADER_VERSION_MAJOR, MOONLOADER_VERSION_MINOR, MOONLOADER_VERSION_PATCH,
                major, minor, patch,
                MOONLOADER_URL
            );
        }
    }
    return 0;
}

LUA_FUNCTION(CheckVersion) {
    Utils::FindValue(LUA, "http.Fetch");
    LUA->PushString("https://raw.githubusercontent.com/Pika-Software/gm_moonloader/main/VERSION");
    LUA->PushCFunction(ValidateVersion);
    if (LUA->PCall(2, 0, 0) != 0) {
        LUA->Pop();
    }

    return 0;
}

void MoonLoader::StartVersionCheck(GarrysMod::Lua::ILuaInterface* LUA) {
    Utils::FindValue(LUA, "timer.Simple");
    LUA->PushNumber(5); // Only check for an update after 5 seconds
    LUA->PushCFunction(CheckVersion);
    if (LUA->PCall(2, 0, 0) != 0) {
        LUA->Pop();
    }
}
