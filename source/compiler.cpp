#include "compiler.hpp"
#include "global.hpp"
#include "watchdog.hpp"
#include "filesystem.hpp"

#include <tier1/utlbuffer.h>
#include <filesystem.h>
#include <moonengine/engine.hpp>

namespace MoonLoader {
    bool Compiler::CompileMoonScript(std::string path) {
        auto readData = g_pFilesystem->ReadBinaryFile(path, GMOD_LUA_PATH_ID);
        if (readData.empty())
            return false;

        auto data = g_pMoonEngine->CompileString(readData.data(), readData.size());
        if (data.empty())
            return false;

        // Create directories for .lua file
        std::string dir = path;
        g_pFilesystem->StripFileName(dir);
        g_pFilesystem->CreateDirs(dir.c_str(), "MOONLOADER");

        // Watch for changes of .moon file
        g_pWatchdog->WatchFile(path, GMOD_LUA_PATH_ID);

        // Write compiled code to .lua file
        g_pFilesystem->SetFileExtension(path, "lua");
        return g_pFilesystem->WriteToFile(path, "MOONLOADER", data.c_str(), data.size());
    }
}