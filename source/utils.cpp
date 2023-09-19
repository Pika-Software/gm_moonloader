#include "utils.hpp"
#include "global.hpp"
#include "filesystem.hpp"
#include "core.hpp"

#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

bool Utils::FindMoonScript(GarrysMod::Lua::ILuaInterface* LUA, std::string& path) {
    if (auto core = Core::Get(LUA)) {
        const char* currentDir = LUA->GetPath();
        if (currentDir) {
            std::string absolutePath = Utils::JoinPaths(currentDir, path);
            Utils::SetFileExtension(absolutePath, "yue");
            if (core->fs->Exists(absolutePath, LUA->GetPathID())) {
                path = std::move(absolutePath);
                return true;
            }
            Utils::SetFileExtension(absolutePath, "moon");
            if (core->fs->Exists(absolutePath, LUA->GetPathID())) {
                path = std::move(absolutePath);
                return true;
            }
        }

        std::string moonPath = path;
        Utils::SetFileExtension(moonPath, "yue");
        if (core->fs->Exists(moonPath, LUA->GetPathID())) {
            path = std::move(moonPath);
            return true;
        }
        Utils::SetFileExtension(moonPath, "moon");
        if (core->fs->Exists(moonPath, LUA->GetPathID())) {
            path = std::move(moonPath);
            return true;
        }
    }
    return false;
}
