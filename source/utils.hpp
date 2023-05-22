#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#include <string>
#include <string_view>
#include <algorithm>

namespace MoonLoader::Utils {
    inline bool StartsWith(std::string_view str, std::string_view prefix) {
        return str.rfind(prefix, 0) == 0;
    }

    inline bool EndsWith(std::string_view str, std::string_view suffix) {
        return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }
}

#endif // MOONLOADER_UTILS_HPP