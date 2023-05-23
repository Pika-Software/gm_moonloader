#include "watchdog.hpp"
#include "global.hpp"
#include "compiler.hpp"
#include "filesystem.hpp"
#include "utils.hpp"

#include <efsw/efsw.hpp>
#include <tier0/dbg.h>
#include <chrono>
#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

class MoonLoader::WatchdogListener : public efsw::FileWatchListener {
public:
    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
        const std::string& filename, efsw::Action action,
        std::string oldFilename) override 
    {
        // We only care about modified files
        if (action == efsw::Actions::Modified) {
            std::string path = dir + filename;
            Filesystem::FixSlashes(path);
            g_pWatchdog->OnFileModified(path);
        }
    }
};

Watchdog::Watchdog() {
    m_Watcher = new efsw::FileWatcher();
    m_WatchdogListener = new WatchdogListener();

    // Launch watchdog in a separate thread
    m_Watcher->watch();
}

Watchdog::~Watchdog() {
    delete m_Watcher;
    delete m_WatchdogListener;
}

void Watchdog::OnFileModified(const std::string& path) {
    // We only care about file we are watching
    std::string relativePath = g_pFilesystem->FullToRelativePath(path, GMOD_LUA_PATH_ID);
    Utils::NormalizePath(relativePath);

    std::lock_guard<std::mutex> guard(m_Lock);
    if (!IsFileWatched(relativePath))
        return;
    
    // Add our modified file to the queue
    m_ModifiedFiles.push(std::move(relativePath));
}

void Watchdog::WatchDirectory(const std::string& path) {
    if (IsDirectoryWatched(path))
        // Our watchdog already registered here
        return;

    DevMsg("[Moonloader] Watching for directory %s\n", path.c_str());

    auto id = m_Watcher->addWatch(path.c_str(), m_WatchdogListener, false);
    m_WatchIDs.insert_or_assign(path, id);
}

void MoonLoader::Watchdog::WatchFile(const std::string& path, const char* pathID) {
    if (IsFileWatched(path))
        // Our watchdog already registered here
        return;

    std::string fullPath = g_pFilesystem->RelativeToFullPath(path, pathID);
    Filesystem::Normalize(fullPath);
    Filesystem::StripFileName(fullPath);
    if (fullPath.empty()) {
        DevWarning("[Moonloader] Unable to find full path for %s\n", path.c_str());
        return;
    }

    DevMsg("[Moonloader] Watching for file %s\n", path.c_str());
    WatchDirectory(fullPath);
    m_WatchedFiles.insert(path.c_str());
}

void Watchdog::Think() {
    if (m_ModifiedFiles.empty())
        return;

    auto currentTimestamp = Utils::Timestamp();

    std::lock_guard<std::mutex> guard(m_Lock);
    while (!m_ModifiedFiles.empty()) {
        auto& path = m_ModifiedFiles.front();
        auto timestamp = m_ModifiedFileDelays.find(path);
        // Check if modified file is delayed
        if (timestamp == m_ModifiedFileDelays.end() || (timestamp->second) < currentTimestamp) {
            DevMsg("[Moonloader] %s was updated. Triggering auto-reload...\n", path.c_str());

            // Moonloader should automatically recompile script, if needed
            // And then run it
            g_pLua->FindAndRunScript(path.c_str(), true, true, "!RELOAD", true);

            m_ModifiedFileDelays[path] = currentTimestamp + 200; // Add 200ms delay, before we can reload file again
        }

        m_ModifiedFiles.pop();
    }
}
