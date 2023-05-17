#include "utils.hpp"

#include <filesystem.h>

using namespace MoonLoader;

std::string Utils::ReadTextFile(const std::string& path, const char* pathID) {
    std::string result;
    FileHandle_t file = g_pFullFileSystem->Open(path.c_str(), "r", pathID);
    if (file) {
        int fileSize = g_pFullFileSystem->Size(file);
        result.resize(fileSize + 1);
        result[fileSize + 1] = '\0';

        g_pFullFileSystem->Read(result.data(), fileSize, file);
        g_pFullFileSystem->Close(file);
    }
    return result;
}