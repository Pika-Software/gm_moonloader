#include "compiler.hpp"
#include "global.hpp"
#include "watchdog.hpp"
#include "utils.hpp"

#include <tier1/utlbuffer.h>
#include <filesystem.h>
#include <moonengine/engine.hpp>

namespace MoonLoader {
    bool Compiler::CompileMoonScript(std::string path) {
        auto readData = Utils::ReadBinaryFile(path, GMOD_LUA_PATH_ID);
        if (readData.empty())
            return false;

        auto data = g_pMoonEngine->CompileString(readData.data(), readData.size());
        if (data.empty())
            return false;

        // Create directories for .lua file
        std::string dir = path;
        Utils::Path::StripFileName(dir);
        Utils::Path::CreateDirs(dir.c_str(), "MOONLOADER");

        // Watch for changes of .moon file
        g_pWatchdog->WatchFile(path, GMOD_LUA_PATH_ID);

        // Write compiled code to .lua file
        Utils::Path::SetFileExtension(path, "lua");
        return Utils::WriteToFile(path, "MOONLOADER", data.c_str(), data.size());
    }
}