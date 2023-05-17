#include "watchdog.hpp"
#include "global.hpp"
#include "compiler.hpp"

#include <efsw/efsw.hpp>
#include <tier0/dbg.h>
#include <filesystem.h>
#include <chrono>
#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

uint64 GetCurrentMillisecondsTime() {
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock().now().time_since_epoch());
    return dur.count();
}

class MoonLoader::WatchdogListener : public efsw::FileWatchListener {
public:
    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
        const std::string& filename, efsw::Action action,
        std::string oldFilename) override 
    {
        // We only care about modified files
        if (action == efsw::Actions::Modified) {
            std::string path = dir + filename;
            V_FixSlashes(path.data(), '/');
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

    DevMsg("[Moonloader] Watching file %s\n", path.c_str());

    auto pathBuf = new char[1024];
    g_pFullFileSystem->RelativePathToFullPath(path.c_str(), pathID, pathBuf, 1024);
    V_ExtractFilePath(pathBuf, pathBuf, 1024);
    V_FixSlashes(pathBuf, '/');
    WatchDirectory(pathBuf);
    delete[] pathBuf;

    m_WatchedFiles.insert(path.c_str());
}

void Watchdog::Think() {
    if (m_ModifiedFiles.empty()) {
        return;
    }

    std::time_t currentTimestamp = GetCurrentMillisecondsTime();

    m_ModifiedFilesMutex.lock();
    while (!m_ModifiedFiles.empty()) {
        auto& path = m_ModifiedFiles.front();
        auto timestamp = m_ModifiedFileDelays.find(path);
        // Check if modified file is delayed
        if (timestamp == m_ModifiedFileDelays.end() || (timestamp->second) < currentTimestamp) {
            std::string_view fileExt = V_GetFileExtension(path.c_str());
            if (fileExt == "moon") {
                auto pathBuf = new char[512];
                g_pFullFileSystem->FullPathToRelativePathEx(path.c_str(), GMOD_LUA_PATH_ID, pathBuf, 512);
                V_FixSlashes(pathBuf, '/');

                DevMsg("[Moonloader] Reloading script %s\n", pathBuf);
                
                // Moonloader should automatically recompile script, if needed
                // And then run it
                g_pLua->FindAndRunScript(pathBuf, true, true, "!RELOAD", true);
                delete[] pathBuf;
            }

            m_ModifiedFileDelays[path] = currentTimestamp + 200; // Add 1 second delay
        }

        m_ModifiedFiles.pop();
    }
    m_ModifiedFilesMutex.unlock();
}
