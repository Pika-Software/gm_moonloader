#include "utils.hpp"

#include <filesystem.h>

using namespace MoonLoader;

//std::string Utils::ReadTextFile(const std::string& path, const char* pathID) {
//    std::string result;
//    FileHandle_t file = g_pFullFileSystem->Open(path.c_str(), "r", pathID);
//    if (file) {
//        int fileSize = g_pFullFileSystem->Size(file);
//        result.resize(fileSize + 1);
//        result[fileSize + 1] = '\0';
//
//        g_pFullFileSystem->Read(result.data(), fileSize, file);
//        g_pFullFileSystem->Close(file);
//    }
//    return result;
//}

std::vector<char> Utils::ReadBinaryFile(const std::string& path, const char* pathID) {
    std::vector<char> result = {};
    FileHandle_t fh = g_pFullFileSystem->Open(path.c_str(), "rb", pathID);
    if (fh) {
        int fileSize = g_pFullFileSystem->Size(fh);
        result.resize(fileSize);

        g_pFullFileSystem->Read(result.data(), result.size(), fh);
        g_pFullFileSystem->Close(fh);
    }
    return result;
}

bool Utils::WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len) {
    FileHandle_t fh = g_pFullFileSystem->Open(path.c_str(), "wb", pathID);
    if (!fh)
        return false;

    int written = g_pFullFileSystem->Write(data, len, fh);
    g_pFullFileSystem->Close(fh);
    return written == len;
}
