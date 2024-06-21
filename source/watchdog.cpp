#include "watchdog.hpp"
#include "global.hpp"
#include "compiler.hpp"
#include "filesystem.hpp"
#include "utils.hpp"
#include "core.hpp"

#include <tier0/dbg.h>
#include <chrono>
#include <GarrysMod/Lua/LuaInterface.h>

using namespace MoonLoader;

void WatchdogListener::handleFileAction(efsw::WatchID watchid, const std::string& dir,
    const std::string& filename, efsw::Action action,
    std::string oldFilename
) {
    // We only care about modified files
    if (action == efsw::Actions::Modified) {
        std::string path = dir + filename;
        Filesystem::FixSlashes(path);
        if (auto watchdog = this->watchdog.lock())
            watchdog->OnFileModified(path);
    }
}

Watchdog::Watchdog(std::shared_ptr<Core> core, std::shared_ptr<Filesystem> fs) 
    : core(core), fs(fs) {}

void Watchdog::Start() {
    m_Watcher->watch();
    m_WatchdogListener->watchdog = weak_from_this();
}

void Watchdog::OnFileModified(const std::string& path) {
    // We only care about file we are watching
    std::string relativePath = fs->FullToRelativePath(path, core->LUA->GetPathID());
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

    auto id = m_Watcher->addWatch(path.c_str(), m_WatchdogListener.get(), false);
    m_WatchIDs.insert_or_assign(path, id);
}

void MoonLoader::Watchdog::WatchFile(const std::string& path, const char* pathID) {
    if (IsFileWatched(path))
        // Our watchdog already registered here
        return;

    std::string fullPath = fs->RelativeToFullPath(path, pathID);
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
            Msg("Watchdog: %s was updated.\n", path.c_str());

            // Moonloader should automatically recompile script, if needed
            // And then run it
            core->LUA->FindAndRunScript(path.c_str(), true, true, "!MOONRELOAD", true);

            m_ModifiedFileDelays[path] = currentTimestamp + 200; // Add 200ms delay, before we can reload file again
        }

        m_ModifiedFiles.pop();
    }
}
