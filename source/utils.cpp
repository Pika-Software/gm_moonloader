#include "utils.hpp"
#include "global.hpp"
#include "filesystem.hpp"
#include "core.hpp"

#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

bool Utils::FindMoonScript(GarrysMod::Lua::ILuaInterface* LUA, std::string& path) {
    if (auto core = Core::Get(LUA)) {
        std::string moonPath = path;
        Utils::SetFileExtension(moonPath, "moon");

        const char* currentDir = LUA->GetPath();
        if (currentDir) {
            std::string absolutePath = Utils::JoinPaths(currentDir, moonPath);
            if (core->fs->Exists(absolutePath, LUA->GetPathID())) {
                path = std::move(absolutePath);
                return true;
            }
        }
        if (core->fs->Exists(moonPath, LUA->GetPathID())) {
            path = std::move(moonPath);
            return true;
        }
    }
    return false;
}
