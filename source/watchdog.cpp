#include "watchdog.hpp"
#include "global.hpp"
#include "compiler.hpp"
#include "filesystem.hpp"
#include "utils.hpp"
#include "core.hpp"
#include "config.hpp"

#include <tier0/dbg.h>
#include <chrono>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/LuaShared.h>
#include <GarrysMod/FunctionPointers.hpp>
#include <GarrysMod/Symbol.hpp>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>

namespace Symbols {
    typedef void (GMCOMMON_CALLING_CONVENTION_STD *HandleFileChange_t)(const std::string& path);

    std::vector<Symbol> HandleFileChange = {
#if ARCHITECTURE_IS_X86
    #if SYSTEM_IS_WINDOWS
        Symbol::FromSignature("\x55\x8b\xec\x83\xec\x60\x56\x8b\x75\x08\x8d\x45\xd0\x56\x50\xe8\x2A\x2A\x2A\x2A\x83\xc4\x08\x83\x7d\xe0\x00\x0f\x84"),
    #elif SYSTEM_IS_LINUX
        Symbol::FromSignature("\x55\x89\xe5\x57\x56\x53\x8d\x5d\x98\x83\xec\x7c\x8b\x75\x08\x89\x1c\x24\x89\x74\x24\x04\xe8\x2A\x2A\x2A\x2A\x8b\x45\x98\x83\xec\x04\x8b\x50\xf4\x85\xd2\x0f\x84"),
    #endif
#elif ARCHITECTURE_IS_X86_64
    #if SYSTEM_IS_WINDOWS
        Symbol::FromSignature("\x48\x89\x5c\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7c\x24\x20\x55\x48\x8d\x6c\x24\xa9\x48\x81\xec\xb0\x00\x00\x00\x48\x8b\x05\x2A\x2A\x2A\x2A\x48\x33\xc4\x48\x89\x45\x47\x48\x8b\xf9\x48\x8b\xd1\x48\x8d\x4d\xc7\xe8\x2A\x2A\x2A\x2A\x48\x83\x7d\xd7\x00\x0f\x84"),
    #elif SYSTEM_IS_LINUX
        Symbol::FromSignature("\x55\x48\x89\xfe\x48\x89\xe5\x41\x57\x41\x56\x41\x55\x41\x54\x4c\x8d\x65\x80\x53\x48\x89\xfb\x4c\x89\xe7\x48\x83\xec\x68\xe8\x2A\x2A\x2A\x2A\x48\x8b\x45\x80\x48\x83\x78\xe8\x00\x75")
    #elif SYSTEM_IS_MACOSX
        Symbol::FromSignature("\x55\x48\x89\xe5\x53\x48\x81\xec\x88\x00\x00\x00\x48\x89\xfb\x48\x8d\x7d\xc8\x48\x89\xde\xe8\x2A\x2A\x2A\x2A\x8a\x4d\xc8\x89\xc8\x24\x01\x74"),
    #endif
#endif
    };

    static SymbolFinder finder;
    template<typename T>
    static inline std::vector<T> ResolveSymbol(SourceSDK::FactoryLoader& loader, const Symbol& symbol) {
        static_assert(std::is_pointer<T>::value, "T must be a pointer");

        // vector used to catch if signature eventually find more than one function
        // I'm still new into signatures, so they may be wrong
        std::vector<T> pointers;
        void* ptr = nullptr;
        while (ptr = finder.Resolve(loader.GetModule(), symbol.name.c_str(), symbol.length, ptr)) {
            pointers.push_back( reinterpret_cast<T>(ptr) );
            ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) + 1);
        }
        return pointers;
    }

    template<typename T>
    static inline std::vector<T> ResolveSymbols(SourceSDK::FactoryLoader& loader, const std::vector<Symbol>& symbols) {
        for (const auto& symbol : symbols) {
            auto pointers = ResolveSymbol<T>(loader, symbol);
            if (!pointers.empty())
                return pointers;
        }
        return {};
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
        std::string path = Utils::Path::Join(dir + filename);
        if (auto watchdog = this->watchdog.lock())
            watchdog->OnFileModified(path);
    }
}

Watchdog::Watchdog(std::shared_ptr<Core> core, std::shared_ptr<Filesystem> fs) 
    : core(core), fs(fs) 
{
    m_HandleFileChangeHook = std::make_unique<Detouring::Hook>();
    SourceSDK::FactoryLoader server_loader("server");

    auto HandleFileChange_pointers = Symbols::ResolveSymbols<Symbols::HandleFileChange_t>(server_loader, Symbols::HandleFileChange);
    if (HandleFileChange_pointers.size() == 1) {
        auto HandleFileChange_original = HandleFileChange_pointers[0];
        if (m_HandleFileChangeHook->Create((void*)HandleFileChange_original, (void*)HandleFileChange_detour) && m_HandleFileChangeHook->Enable()) {
            DevMsg("[Moonloader] HandleFileChange: %p\n", HandleFileChange_original);
        } else {
            core->LUA->ErrorNoHalt("[Moonloader] Failed to hook HandleFileChange function! Autorefresh won't work properly.\n\tPlease, report to " MOONLOADER_URL "/issues\n");
        }
    } else if (HandleFileChange_pointers.empty()) {
        core->LUA->ErrorNoHalt("[Moonloader] HandleFileChange not found! Autorefresh won't work properly.\n\tPlease, report to " MOONLOADER_URL "/issues\n" );
    } else {
        core->LUA->ErrorNoHalt("[Moonloader] Too many functions were found for HandleFileChange signature! Autorefresh won't work properly.\n\tPlease, report to " MOONLOADER_URL "/issues\n");
    }
}

void Watchdog::Start() {
    m_Watcher->watch();
    m_WatchdogListener->watchdog = weak_from_this();
}

Watchdog::~Watchdog() {
    m_HandleFileChangeHook->Disable();
}

void Watchdog::OnFileModified(const std::string& path) {
    // We only care about file we are watching
    std::string relativePath = fs->FullToRelativePath(path, core->LUA->GetPathID());
    Utils::Path::Normalize(relativePath);

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
    Utils::Path::Normalize(fullPath);
    Utils::Path::StripFileName(fullPath);
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
                if (auto file = GetCachedFile(path)) {
                    RefreshFile(file->name);
                } else {
                    Warning("[Moonloader] Unable to find file %s in cache. Can't autorefresh it :(\n", path.c_str());
                }
            }

            m_ModifiedFileDelays[path] = currentTimestamp + 200; // Add 200ms delay, before we can reload file again
        }

        m_ModifiedFiles.pop();
    }
}

void Watchdog::HandleFileChange(const std::string& path) {
    std::string strPath = path.c_str();
    Utils::Path::Normalize(strPath);
    if (core->compiler->FindFileByFullOutputPath(strPath)) {
        // We are handling our own file, supress engine's file change
        return;
    }

    RefreshFile(path);
}

void Watchdog::RefreshFile(const std::string& path) {
    if (m_HandleFileChangeHook->IsValid()) {
        m_HandleFileChangeHook->GetTrampoline<Symbols::HandleFileChange_t>()(path);
    } else {
        // TODO: use lua_refresh_file?
    }
}
