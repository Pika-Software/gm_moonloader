#include "watchdog.hpp"
#include "global.hpp"
#include "compiler.hpp"
#include "filesystem.hpp"
#include "utils.hpp"
#include "core.hpp"

#include <tier0/dbg.h>
#include <chrono>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/LuaShared.h>
#include <GarrysMod/FunctionPointers.hpp>
#include <GarrysMod/Symbol.hpp>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>

namespace Symbols {
    typedef void (*HandleFileChange_t)(const std::string& path);

    std::vector<Symbol> HandleFileChange = {
#if ARCHITECTURE_IS_X86
#elif ARCHITECTURE_IS_X86_64
    #if SYSTEM_IS_LINUX
        Symbol::FromSignature("\x55\x48\x89\xfe\x48\x89\xe5\x41\x57\x41\x56\x41\x55\x41\x54\x4c\x8d\x65\x80\x53\x48\x89\xfb\x4c\x89\xe7\x48\x83\xec\x68\xe8\x2d\x23\x1c\x00\x48\x8b\x45\x80\x48\x83\x78\xe8\x00\x75\x2a\x48\x8b"),
    #elif SYSTEM_IS_MACOSX
        Symbol::FromSignature("\x55\x48\x89\xe5\x53\x48\x81\xec\x88\x00\x00\x00\x48\x89\xfb\x48\x8d\x7d\xc8\x48\x89\xde\xe8\xd5\x21\x11\x00\x8a\x4d\xc8\x89\xc8\x24\x01\x74\x0e\x48\x8b\x4d\xd0\x48\x85\xc9\x75\x13\xe9\xf5\x00"),
    #endif
#endif
    };

    static SymbolFinder finder;
    template<typename T>
    static inline T ResolveSymbol(SourceSDK::FactoryLoader& loader, const Symbol& symbol) {
        return reinterpret_cast<T>( finder.Resolve(loader.GetModule(), symbol.name.c_str(), symbol.length) );
    }

    template<typename T>
    static inline T ResolveSymbols(SourceSDK::FactoryLoader& loader, const std::vector<Symbol>& symbols) {
        T ptr = nullptr;
        for (const auto& symbol : symbols) {
            ptr = ResolveSymbol<T>(loader, symbol);
            if (ptr != nullptr)
                break;
        }
        return ptr;
    }
}

// This is probably terrible design for menu + server environments
// Probably will be fixed in successor project
void HandleFileChange_detour(const std::string& path) {
    MoonLoader::Core::GetAll().front()->watchdog->HandleFileChange(path);
}

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
    : core(core), fs(fs) 
{
    SourceSDK::FactoryLoader server_loader("server");
    auto HandleFileChange_original = Symbols::ResolveSymbols<Symbols::HandleFileChange_t>(server_loader, Symbols::HandleFileChange);
    if (HandleFileChange_original == nullptr)
        throw std::runtime_error("Failed to resolve HandleFileChange");

    m_HandleFileChangeHook = std::make_unique<Detouring::Hook>((void*)HandleFileChange_original, (void*)HandleFileChange_detour);
    if (!m_HandleFileChangeHook->Enable())
        throw std::runtime_error("Failed to hook HandleFileChange");
}

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

            if (core->compiler->CompileFile(path, true)) {
                // Refresh file
                auto file = core->compiler->FindFileBySourcePath(path);
                RefreshFile(file->full_output_path);
            }

            m_ModifiedFileDelays[path] = currentTimestamp + 200; // Add 200ms delay, before we can reload file again
        }

        m_ModifiedFiles.pop();
    }
}

void Watchdog::HandleFileChange(const std::string& path) {
    const char* strPath = path.c_str();
    if (core->compiler->FindFileByFullOutputPath(strPath)) {
        // We are handling our own file, supress engine's file change
        return;
    }

    RefreshFile(path);
}

void Watchdog::RefreshFile(const std::string& path) {
    m_HandleFileChangeHook->GetTrampoline<Symbols::HandleFileChange_t>()(path);
}
