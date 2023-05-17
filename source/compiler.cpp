#include "compiler.hpp"
#include "global.hpp"
#include "watchdog.hpp"

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
    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(path.data(), GMOD_LUA_PATH_ID, buf)) {
        return false;
    };

    //auto fileHash = GetFileHash(buf);
    //auto cachedFile = GetCachedFile(path);
    //if (cachedFile && MD5_Compare(fileHash, cachedFile->hash)) {
    //    // We don't need to recompile this file
    //    return true;
    //}

    // Let's just pray, that data in buffer is proper text file
    auto data = g_pMoonEngine->CompileString((const char*)buf.Base(), buf.TellPut());
    if (data.empty()) {
        // Compiled data is empty, ignoring it
        return false;
    }

    std::string dir = path.substr(0, path.find_last_of("/"));
    g_pFullFileSystem->CreateDirHierarchy(dir.c_str(), "MOONLOADER");

    // Watch for changes of .moon file
    g_pWatchdog->WatchFile(path, GMOD_LUA_PATH_ID);

    path = path.substr(0, path.find_last_of(".")) + ".lua";
    auto fh = g_pFullFileSystem->Open(path.c_str(), "wb", "MOONLOADER");
    if (!fh) {
        return false;
    }

    int written = g_pFullFileSystem->Write(data.c_str(), data.size(), fh);
    g_pFullFileSystem->Close(fh);
    return written == data.size();
}
