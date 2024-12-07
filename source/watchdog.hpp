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
#include <detouring/hook.hpp>

namespace GarrysMod::Lua {
    class File;
}

namespace MoonLoader {
    class Watchdog;
    class Filesystem;
    class Core;

    class WatchdogListener : public efsw::FileWatchListener {
    public:
        std::weak_ptr<Watchdog> watchdog;
        void handleFileAction(efsw::WatchID watchid, const std::string& dir,
            const std::string& filename, efsw::Action action,
            std::string oldFilename) override;
    };

    class Watchdog : public std::enable_shared_from_this<Watchdog> {
        std::shared_ptr<Core> core;
        std::shared_ptr<Filesystem> fs;
        std::unique_ptr<efsw::FileWatcher> m_Watcher = std::make_unique<efsw::FileWatcher>();
        std::unique_ptr<WatchdogListener> m_WatchdogListener = std::make_unique<WatchdogListener>();;
        std::unordered_map<std::string, efsw::WatchID> m_WatchIDs;
        std::unordered_set<std::string> m_WatchedFiles;
        std::unordered_map<std::string, GarrysMod::Lua::File*> m_LuaFileCache; // Used for custom autorefresh
        Detouring::Hook m_HandleFileChangeHook;

        std::mutex m_Lock;
        std::queue<std::string> m_ModifiedFiles;
        std::unordered_map<std::string, uint64> m_ModifiedFileDelays;

    public:
        Watchdog(std::shared_ptr<Core>, std::shared_ptr<Filesystem> fs);
        void Start();

        ~Watchdog();

        inline bool IsFileWatched(const std::string& path) { return m_WatchedFiles.find(path) != m_WatchedFiles.end(); }
        inline bool IsDirectoryWatched(const std::string& path) { return m_WatchIDs.find(path) != m_WatchIDs.end(); }

        inline void CacheFile(const std::string& path, GarrysMod::Lua::File* file) { m_LuaFileCache.insert_or_assign(path, file); }
        inline GarrysMod::Lua::File* GetCachedFile(const std::string& path) {
            auto it = m_LuaFileCache.find(path);
            return it != m_LuaFileCache.end() ? it->second : nullptr;
        }

        void OnFileModified(const std::string& path);

        // Directory path must be absolute
        void WatchDirectory(const std::string& path);
        void WatchFile(const std::string& path, const char* pathID);
        void Think();

        // Custom Autorefresh
        void HandleFileChange(const std::string& path);
        void RefreshFile(const std::string& path);
    };
}

#endif // MOONLOADER_WATCHDOG_HPP
