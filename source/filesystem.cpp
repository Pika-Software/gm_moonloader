#include "filesystem.hpp"
#include "utils.hpp"

#include <tier0/dbg.h>
#include <filesystem.h>
#include <filesystem> // std filesystem
#include <algorithm>
#include <sstream>

namespace MoonLoader {
    // ---------------------------- Filesystem ----------------------------
    Filesystem::Filesystem(IFileSystem* fs) {
        if (fs == nullptr) throw std::runtime_error("IFileSystem is null");
        m_InternalFS = fs;
    }

    char* Filesystem::PathBuffer() {
        // I hate allocating big buffers on stack, but I don't want to use heap
        // So here is a thread_local static buffer
        // I pray that it is not allocated on stack
        thread_local static std::vector<char> pathBuffer(PathBufferSize());
        return pathBuffer.data();
    }
    size_t Filesystem::PathBufferSize() {
        return 8192;
    }

    // Path manipulation
    std::string Filesystem::RelativeToFullPath(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        bool success = m_InternalFS->RelativePathToFullPath(path.c_str(), pathID, PathBuffer(), PathBufferSize()) != NULL;
        return success ? std::string(PathBuffer()) : std::string{};
    }
    std::string Filesystem::FullToRelativePath(const std::string& fullPath, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        bool success = m_InternalFS->FullPathToRelativePathEx(fullPath.c_str(), pathID, PathBuffer(), PathBufferSize());
        return success ? std::string(PathBuffer()) : std::string{};
    }
    std::string Filesystem::TransverseRelativePath(const std::string& relativePath, const char* fromPathID, const char* toPathID) {
        std::string result = RelativeToFullPath(relativePath, fromPathID);
        if (!result.empty()) result = FullToRelativePath(result, toPathID);
        if (!result.empty()) Utils::Path::Normalize(result);
        return result;
    }
    // -------------------------

    bool Filesystem::Exists(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        return m_InternalFS->FileExists(path.c_str(), pathID);
    }
    bool Filesystem::IsDirectory(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        return m_InternalFS->IsDirectory(path.c_str(), pathID);
    }

    Filesystem::FileFinder Filesystem::Find(const std::string& wildcard, const char* pathID) {
        return FileFinder(this, wildcard, pathID);
    }

    int Filesystem::RemoveFile(const std::string& filename, const char* pathID) {
        int isFile = IsFile(filename, pathID);
        std::lock_guard<std::mutex> lock(m_IOLock);
        m_InternalFS->RemoveFile(filename.c_str(), pathID);
        return isFile ? 1 : 0;
    }
    int Filesystem::RemoveDir(const std::string& dir, const char* pathID) {
        int removed = 0;
        // We need to remove all files in the directory first
        for (const auto [fileName, isDir] : Find(Utils::Path::Join(dir, "*"), pathID)) {
            removed += Remove(Utils::Path::Join(dir, fileName), pathID);
        }
        // After removing all files, we can safely remove the directory
        return RemoveFile(dir, pathID) + removed;
    }
    int Filesystem::Remove(const std::string& path, const char* pathID) {
        if (IsDirectory(path, pathID)) {
            return RemoveDir(path, pathID);
        } else {
            return RemoveFile(path, pathID);
        }
    }

    void Filesystem::CreateDirs(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        m_InternalFS->CreateDirHierarchy(path.c_str(), pathID);
    }

    std::vector<char> Filesystem::ReadBinaryFile(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        std::vector<char> result = {};
        FileHandle_t fh = m_InternalFS->Open(path.c_str(), "rb", pathID);
        if (fh) {
            int fileSize = m_InternalFS->Size(fh);
            result.resize(fileSize);

            m_InternalFS->Read(result.data(), result.size(), fh);
            m_InternalFS->Close(fh);
        }
        return result;
    }
    std::string Filesystem::ReadTextFile(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        FileHandle_t fh = m_InternalFS->Open(path.c_str(), "rb", pathID);
        if (fh) {
            std::string result(m_InternalFS->Size(fh), '\0');

            m_InternalFS->Read(&result[0], result.size(), fh);
            m_InternalFS->Close(fh);
            return result;
        }
        return {};
    }
    bool Filesystem::WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        FileHandle_t fh = m_InternalFS->Open(path.c_str(), "wb", pathID);
        if (!fh)
            return false;

        int written = m_InternalFS->Write(data, len, fh);
        m_InternalFS->Close(fh);
        return written == len;
    }

    size_t Filesystem::GetFileTime(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        return m_InternalFS->GetFileTime(path.c_str(), pathID);
    }

    void Filesystem::AddSearchPath(const std::string& path, const char* pathID, bool addToFront) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        m_InternalFS->AddSearchPath(path.c_str(), pathID, addToFront ? PATH_ADD_TO_HEAD : PATH_ADD_TO_TAIL);
    }
    void Filesystem::RemoveSearchPath(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        m_InternalFS->RemoveSearchPath(path.c_str(), pathID);
    }

    // ---------------------------- FileFinder ----------------------------
    bool Filesystem::FileFinder::Iterator::SetValue(const char* pFileName) {
        if (pFileName == 0) {
            if (m_Handle != 0) {
                m_Filesystem->m_IOLock.lock();
                m_Filesystem->m_InternalFS->FindClose(m_Handle);
                m_Filesystem->m_IOLock.unlock();
            }
    
            m_Filesystem = nullptr;
            m_Handle = 0;
            m_Value = {};
            return false;
        }

        m_Filesystem->m_IOLock.lock();
        m_Value = {pFileName, m_Filesystem->m_InternalFS->FindIsDirectory(m_Handle)};
        m_Filesystem->m_IOLock.unlock();
        return true;
    }

    Filesystem::FileFinder::Iterator& Filesystem::FileFinder::Iterator::operator++() {
        if (m_Handle == 0)
            return *this;
        
        m_Filesystem->m_IOLock.lock();
        const char* pFileName = m_Filesystem->m_InternalFS->FindNext(m_Handle);
        m_Filesystem->m_IOLock.unlock();

        SetValue(pFileName);

        return *this;
    }

    Filesystem::FileFinder::Iterator Filesystem::FileFinder::begin() {
        if (!m_Filesystem || m_SearchWildcard.empty())
            return end();
        
        FileFindHandle_t handle;
        m_Filesystem->m_IOLock.lock();
        const char* pFileName = m_Filesystem->m_InternalFS->FindFirstEx(m_SearchWildcard.c_str(), m_PathID.empty() ? nullptr : m_PathID.c_str(), &handle);
        m_Filesystem->m_IOLock.unlock();

        auto iterator = Iterator(m_Filesystem, handle, pFileName);

        // Skip `.` and `..`
        while (true) {
            const auto [fileName, isDir] = *iterator;
            if (fileName != "." && fileName != "..") break;
            ++iterator;
        }

        return iterator;
    }
}
