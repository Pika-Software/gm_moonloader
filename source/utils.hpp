#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#include <string>

namespace MoonLoader::Utils {
    std::string ReadTextFile(const std::string& path, const char* pathID = 0);
}

#endif // MOONLOADER_UTILS_HPP