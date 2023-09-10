#ifndef MOONLOADER_AUTOREFRESH_HPP
#define MOONLOADER_AUTOREFRESH_HPP

#pragma once

#include <string_view>
#include <memory>

class INetworkStringTable;

namespace MoonLoader {
    class Core;

    class AutoRefresh {
        std::shared_ptr<Core> core;
        INetworkStringTable* client_files = nullptr;

    public:
        AutoRefresh(std::shared_ptr<Core> core);

        bool Sync(std::string_view path);
    };
}

#endif // MOONLOADER_AUTOREFRESH_HPP
