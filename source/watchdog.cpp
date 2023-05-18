#include "watchdog.hpp"
#include "global.hpp"
#include "compiler.hpp"
#include "utils.hpp"

#include <efsw/efsw.hpp>
#include <tier0/dbg.h>
#include <filesystem.h>
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
            Utils::Path::FixSlashes(path);
            g_pWatchdog->OnFileModified(path);
        }
    }
};

Watchdog::Watchdog() {
    m_Watcher = new efsw::FileWatcher();
    m_WatchdogListener = new WatchdogListener();

    m_Watcher->watch();
}

Watchdog::~Watchdog() {
    delete m_Watcher;
    delete m_WatchdogListener;
}

void Watchdog::OnFileModified(std::string_view path) {
    // We only care about .moon files
    if (Utils::Path::FileExtension(path) != "moon")
        return;

    m_ModifiedFilesMutex.lock();
    m_ModifiedFiles.push(std::string(path.begin(), path.end()));
    m_ModifiedFilesMutex.unlock();
}

void Watchdog::WatchDirectory(const std::string& path) {
    if (m_WatchIDs.find(path.c_str()) != m_WatchIDs.end()) {
        // Our watchdog already registered here
        return;
    }

    DevMsg("[Moonloader] Watching directory %s\n", path.c_str());

    auto id = m_Watcher->addWatch(path.c_str(), m_WatchdogListener, false);
    m_WatchIDs.insert({ path.c_str(), id });
}

void MoonLoader::Watchdog::WatchFile(const std::string& path, const char* pathID) {
    if (m_WatchedFiles.find(path.c_str()) != m_WatchedFiles.end()) {
        // Our watchdog already registered here
        return;
    }

    std::string fullPath = Utils::Path::RelativeToFullPath(path, pathID);
    Utils::Path::FixSlashes(fullPath);
    Utils::Path::StripFileName(fullPath);
    if (fullPath.empty()) {
        DevWarning("[Moonloader] Unable to find full path for %s\n", path.c_str());
        return;
    }

    DevMsg("[Moonloader] Watching file %s\n", path.c_str());
    WatchDirectory(fullPath);
    m_WatchedFiles.insert(path.c_str());
}

void Watchdog::Think() {
    if (m_ModifiedFiles.empty()) {
        return;
    }

    std::time_t currentTimestamp = Utils::Timestamp();

    std::lock_guard<std::mutex> guard(m_ModifiedFilesMutex);
    while (!m_ModifiedFiles.empty()) {
        auto& path = m_ModifiedFiles.front();
        auto timestamp = m_ModifiedFileDelays.find(path);
        // Check if modified file is delayed
        if (timestamp == m_ModifiedFileDelays.end() || (timestamp->second) < currentTimestamp) {
            std::string relativePath = Utils::Path::FullToRelativePath(path, GMOD_LUA_PATH_ID);
            Utils::Path::FixSlashes(relativePath);
            if (!relativePath.empty()) {
                DevMsg("[Moonloader] Reloading script %s\n", relativePath.c_str());

                // Moonloader should automatically recompile script, if needed
                // And then run it
                g_pLua->FindAndRunScript(relativePath.c_str(), true, true, "!RELOAD", true);
            } else {
                DevWarning("[Moonloader] Auto-reload failed: unable to find relative path for %s\n", path.c_str());
            }

            m_ModifiedFileDelays[path] = currentTimestamp + 200; // Add 1 second delay
        }

        m_ModifiedFiles.pop();
    }
}
