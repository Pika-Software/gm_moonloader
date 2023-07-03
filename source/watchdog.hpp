#ifndef MOONLOADER_WATCHDOG_HPP
#define MOONLOADER_WATCHDOG_HPP

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <tier0/platform.h>
#include <efsw/efsw.hpp>
#include <GarrysMod/Lua/LuaInterface.h>

namespace MoonLoader {
    class WatchdogListener : public efsw::FileWatchListener {
    public:
        void handleFileAction(efsw::WatchID watchid, const std::string& dir,
            const std::string& filename, efsw::Action action,
            std::string oldFilename) override;
    };

    class Watchdog {
        std::unique_ptr<efsw::FileWatcher> m_Watcher;
        std::unique_ptr<WatchdogListener> m_WatchdogListener;
        std::unordered_map<std::string, efsw::WatchID> m_WatchIDs;
        std::unordered_set<std::string> m_WatchedFiles;

        std::mutex m_Lock;
        std::queue<std::string> m_ModifiedFiles;
        std::unordered_map<std::string, uint64> m_ModifiedFileDelays;

    public:
        Watchdog();

        inline bool IsFileWatched(const std::string& path) { return m_WatchedFiles.find(path) != m_WatchedFiles.end(); }
        inline bool IsDirectoryWatched(const std::string& path) { return m_WatchIDs.find(path) != m_WatchIDs.end(); }

        void OnFileModified(const std::string& path);

        // Directory path must be absolute
        void WatchDirectory(const std::string& path);
        void WatchFile(const std::string& path, const char* pathID);
        void Think(GarrysMod::Lua::ILuaInterface* LUA);
    };
}

#endif // MOONLOADER_WATCHDOG_HPP