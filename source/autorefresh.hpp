#ifndef MOONLOADER_AUTOREFRESH_HPP
#define MOONLOADER_AUTOREFRESH_HPP

#pragma once

#include <string_view>

namespace MoonLoader::AutoRefresh {
    bool Sync(std::string_view path);
    void Initialize();
    void Deinitialize();
}

#endif // MOONLOADER_AUTOREFRESH_HPP
