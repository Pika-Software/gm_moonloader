#include "autorefresh.hpp"
#include <Platform.hpp>

using namespace MoonLoader;

// This code purpose is to only fix autorefresh issues on OSX
// If platform isn't OSX, no need in implementing anything

#if !SYSTEM_IS_MACOSX
// should i add something here?
#else
#include "global.hpp"
#include "utils.hpp"
#include "filesystem.hpp"
#include "core.hpp"
#include <tier0/dbg.h>
#include <eiface.h>
#include <iserver.h>
#include <array>
#include <networkstringtabledefs.h>
#include <LzmaLib.h>
#include <Sha256.h>

inline size_t MaximumCompressedSize(size_t inputSize) {
    return (inputSize+1) + (inputSize+1) / 3 + 128;
}

inline std::array<char, 32> GetSHA256(const char* data, size_t len) {
    CSha256 hash;
    std::array<char, 32> result{};
    Sha256_Init(&hash);
    Sha256_Update(&hash, (const uint8_t*)data, len);
    Sha256_Final(&hash, (uint8_t*)result.data());
    return result;
}

std::vector<char> Compress(const std::string& input) {
    // The data is written:
    //	5 bytes:	props
    //	8 bytes:	uncompressed size
    //	the rest:	the actual data
    const size_t iInputLength = input.size() + 1;
    size_t props_size = LZMA_PROPS_SIZE;
    size_t iDestSize = iInputLength + iInputLength / 3 + 128;

    std::vector<char> output(iDestSize + LZMA_PROPS_SIZE + 8, 0);

    const uint8_t* pInputData = reinterpret_cast<const uint8_t*>(input.c_str());
    uint8_t* pPropStart = reinterpret_cast<uint8_t*>(output.data());
    uint8_t* pSizeStart = pPropStart + LZMA_PROPS_SIZE;
    uint8_t* pBodyStart = pSizeStart + 8;

    const int res = LzmaCompress(
        pBodyStart, &iDestSize, // Dest out
        pInputData, iInputLength, // Input in
        pPropStart, &props_size, // Props out
        5, // level [0-9]
        65536, // dict size ( ie 1 << 4 )
        3,
        0,
        2,
        32,
        2
    );

    if (props_size != LZMA_PROPS_SIZE)
        return { };

    if (res != SZ_OK)
        return { };

    // Write our 8 byte LE size
    pSizeStart[0] = iInputLength & 0xFF;
    pSizeStart[1] = (iInputLength >> 8) & 0xFF;
    pSizeStart[2] = (iInputLength >> 16) & 0xFF;
    pSizeStart[3] = (iInputLength >> 24) & 0xFF;
    pSizeStart[4] = 0;
    pSizeStart[5] = 0;
    pSizeStart[6] = 0;
    pSizeStart[7] = 0;

    output.resize(iDestSize + LZMA_PROPS_SIZE + 8);
    return output;
}

inline void SendFileToClient(std::shared_ptr<Core> core, const std::string& path) {
    std::string fileData = core->fs->ReadTextFile(path, "GAME");
    if (fileData.empty()) return;

    const auto compressedData = Compress(fileData);
    if (compressedData.empty()) return;

    // Type 1b + SHA256 32b + compressed substitute Xb + alignment 4b
    int bufferSize = 1 + 32 + compressedData.size() + 4;
    // (File path + NUL) Xb + (SHA256 + compressed substitute) size 4b
    bufferSize += path.size() + 1 + 4;

    std::vector<char> buffer(bufferSize, 0);
    bf_write writer( "moonloader SendLuaFile buffer", buffer.data(), buffer.size() );

    writer.WriteByte(1); // Type
    writer.WriteString(path.c_str());

    writer.WriteUBitLong(32 + compressedData.size(), 32);

    auto hash = GetSHA256(fileData.c_str(), fileData.size() + 1);
    writer.WriteBytes(hash.data(), hash.size());

    writer.WriteBytes(compressedData.data(), compressedData.size());

    DevMsg("[Moonloader] Autorefreshing file %s (payload size %d bytes)\n", path.c_str(), writer.GetNumBytesWritten());

    // TODO: Send to all clients
    core->engine_server->GMOD_SendToClient(0, writer.GetData(), writer.GetNumBitsWritten());
}

bool AutoRefresh::Sync(std::string_view path) {
    if (!client_files) return false;
    std::string fullPath = Utils::JoinPaths(CACHE_PATH_LUA, path);
    Utils::SetFileExtension(fullPath, "lua");

    int fileID = client_files->FindStringIndex(fullPath.c_str());
    if (fileID == (uint16)-1) return false;

    SendFileToClient(core, fullPath);
    return true;
}

AutoRefresh::AutoRefresh(std::shared_ptr<Core> core) : core(core) {
    DevMsg("[Moonloader] Fixing file autorefresh for OSX...\n");
    if (core->engine_server) {
        INetworkStringTableContainer* container = Utils::LoadInterface<INetworkStringTableContainer>("engine", INTERFACENAME_NETWORKSTRINGTABLESERVER);
        if (container) client_files = container->FindTable("client_lua_files");
    }
    if (!client_files) throw std::runtime_error("Failed to find client_lua_files string table");
}

#endif
