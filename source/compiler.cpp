#include "compiler.hpp"
#include "global.hpp"
#include "watchdog.hpp"
#include "utils.hpp"

#include <tier1/utlbuffer.h>
#include <filesystem.h>
#include <moonengine/engine.hpp>

using namespace MoonLoader;

// Yeah, there is a lot of memory copying,
// but I don't care about it, because i just want to make it work
// feel free to optimize it

std::optional<MD5Value_t> Compiler::GetFileHash(std::string path) {
    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(path.c_str(), "GAME", buf)) {
        return {};
    }
    
    return GetFileHash(buf);
}

MD5Value_t Compiler::GetFileHash(CUtlBuffer& buffer) {
    return GetFileHash(buffer.Base(), buffer.TellPut());
}

MD5Value_t Compiler::GetFileHash(void* data, size_t len) {
    MD5Value_t md5;
    MD5_ProcessSingleBuffer(data, len, md5);
    return md5;
}

bool Compiler::CompileMoonScript(std::string path) {
    auto readData = Utils::ReadBinaryFile(path, GMOD_LUA_PATH_ID);
    if (readData.empty())
        return false;

    auto data = g_pMoonEngine->CompileString(readData.data(), readData.size());
    if (data.empty())
        // Compiled data is empty, ignoring it
        return false;

    std::string dir = path.substr(0, path.find_last_of("/"));
    g_pFullFileSystem->CreateDirHierarchy(dir.c_str(), "MOONLOADER");

    // Watch for changes of .moon file
    g_pWatchdog->WatchFile(path, GMOD_LUA_PATH_ID);

    path = path.substr(0, path.find_last_of(".")) + ".lua";
    return Utils::WriteToFile(path, "MOONLOADER", data.c_str(), data.size());
}
