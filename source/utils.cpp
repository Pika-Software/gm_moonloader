#include "utils.hpp"
#include "global.hpp"
#include "filesystem.hpp"

#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

bool Utils::FindMoonScript(GarrysMod::Lua::ILuaInterface* LUA, std::string& path) {
    std::string moonPath = path;
    Utils::SetFileExtension(moonPath, "moon");

    const char* currentDir = LUA->GetPath();
    if (currentDir) {
        std::string absolutePath = Utils::JoinPaths(currentDir, moonPath);
        if (g_Filesystem->Exists(absolutePath, LUA->GetPathID())) {
            path = std::move(absolutePath);
            return true;
        }
    }
    if (g_Filesystem->Exists(moonPath, LUA->GetPathID())) {
        path = std::move(moonPath);
        return true;
    }
    return false;
}
