#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#include <string>
#include <vector>

namespace MoonLoader::Utils {
    //std::string ReadTextFile(const std::string& path, const char* pathID = 0);
    std::vector<char> ReadBinaryFile(const std::string& path, const char* pathID = 0);

    bool WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len);
}

#endif // MOONLOADER_UTILS_HPP