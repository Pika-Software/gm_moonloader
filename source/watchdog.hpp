#ifndef MOONLOADER_WATCHDOG_HPP
#define MOONLOADER_WATCHDOG_HPP

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <ctime>
#include <tier0/platform.h>

namespace efsw {
    class FileWatcher;
    typedef long WatchID;
}

namespace MoonLoader {
    class WatchdogListener;

    class Watchdog {
        efsw::FileWatcher* m_Watcher;
        WatchdogListener* m_WatchdogListener;

        std::unordered_map<std::string, efsw::WatchID> m_WatchIDs;
        std::unordered_set<std::string> m_WatchedFiles;

        std::mutex m_ModifiedFilesMutex;
        std::queue<std::string> m_ModifiedFiles;
        std::unordered_map<std::string, uint64> m_ModifiedFileDelays;

    public:
        Watchdog();
        ~Watchdog();

        void OnFileModified(std::string_view path);

        // Directory path must be absolute
        void WatchDirectory(const std::string& path);
        void WatchFile(const std::string& path, const char* pathID);
        void Think();
    };
}

#endif // MOONLOADER_WATCHDOG_HPP