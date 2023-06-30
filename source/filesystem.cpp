#include "filesystem.hpp"
#include "utils.hpp"

#include <tier0/dbg.h>
#include <filesystem.h>
#include <algorithm>

constexpr size_t PATH_BUFFER_SIZE = MAX_UNICODE_PATH;

namespace MoonLoader {
    // ---------------------------- Filesystem ----------------------------
    Filesystem::Filesystem(IFileSystem* fs) {
        Assert(fs != nullptr);
        m_InternalFS = fs;
    }

    char* Filesystem::PathBuffer() {
        // I hate allocating big buffers on stack, but I don't want to use heap
        // So here is a thread_local static buffer
        // I pray that it is not allocated on stack
        thread_local static char pathBuffer[PATH_BUFFER_SIZE];
        return pathBuffer;
    }
    size_t Filesystem::PathBufferSize() {
        return PATH_BUFFER_SIZE;
    }

    // --- Path manipulation ---
    std::string& Filesystem::FixSlashes(std::string& path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }
    std::string& Filesystem::LowerCase(std::string& path) {
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
        return path;
    }
    std::string& Filesystem::Normalize(std::string& path) {
        FixSlashes(path);
        LowerCase(path);
        return path;
    }
    std::string_view Filesystem::FileName(std::string_view path) {
        auto namePos = path.find_first_of('/');
        return namePos != std::string_view::npos ? path.substr(namePos + 1) : path;
    }
    std::string_view Filesystem::FileExtension(std::string_view path) {
        auto fileName = FileName(path);
        auto extPos = !fileName.empty() ? fileName.find_first_of('.') : std::string_view::npos;
        return extPos != std::string_view::npos ? fileName.substr(extPos + 1) : std::string_view{};
    }
    std::string& Filesystem::StripFileName(std::string& path) {
        auto namePos = path.find_last_of('/');
        if (namePos != std::string::npos)
            path.erase(namePos);
        return path;
    }
    std::string& Filesystem::StripFileExtension(std::string& path) {
        auto namePos = path.find_last_of('.');
        if (namePos != std::string::npos)
            path.erase(namePos);
        return path;
    }
    std::string& Filesystem::SetFileExtension(std::string& path, std::string_view ext) {
        StripFileExtension(path);
        if (!ext.empty() && ext.front() != '.')
            path += '.';
        path += ext;
        return path;
    }
    std::string Filesystem::Join(std::string_view path, std::string_view subPath) {
        std::string finalPath = std::string(path);
        if (!finalPath.empty() && finalPath.back() != '/')
            finalPath += '/';

        finalPath += subPath;
        return finalPath;
    }

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
        if (!result.empty()) Utils::NormalizePath(result);
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
        for (auto file : Find(Join(dir, "*"), pathID)) {
            removed += Remove(Join(dir, file), pathID);
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
        // Yeah, this is the same as ReadBinaryFile
        // but also copies data to string
        // I don't know how to return std::string without copying data
        std::lock_guard<std::mutex> lock(m_IOLock);
        std::vector<char> result = {};
        FileHandle_t fh = m_InternalFS->Open(path.c_str(), "r", pathID);
        if (fh) {
            int fileSize = m_InternalFS->Size(fh);
            result.resize(fileSize);

            m_InternalFS->Read(result.data(), result.size(), fh);
            m_InternalFS->Close(fh);
        }
        return { result.data(), result.size() };
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
    Filesystem::FileFinder::iterator Filesystem::FileFinder::INVALID_ITERATOR = {};

    Filesystem::FileFinder::iterator& Filesystem::FileFinder::iterator::operator++() {
        if (!m_Filesystem || !valid_handle() || m_pFileName == NULL)
            return INVALID_ITERATOR;

        m_Filesystem->m_IOLock.lock();
        m_pFileName = m_Filesystem->m_InternalFS->FindNext(m_Handle);
        if (m_pFileName == NULL) {
            m_Filesystem->m_InternalFS->FindClose(m_Handle);
            m_Handle = 0;
        }
        m_Filesystem->m_IOLock.unlock();

        return m_pFileName != NULL ? *this : INVALID_ITERATOR;
    }

    Filesystem::FileFinder::iterator Filesystem::FileFinder::begin() {
        if (!m_Filesystem || m_SearchWildcard.empty())
            return end();
        
        FileFindHandle_t findHandle;
        m_Filesystem->m_IOLock.lock();
        const char* pFilename = m_Filesystem->m_InternalFS->FindFirstEx(m_SearchWildcard.c_str(), m_PathID.c_str(), &findHandle);

        while (pFilename != NULL && (strcmp(pFilename, ".") == 0 || strcmp(pFilename, "..") == 0)) {
            // Skip . and ..
            pFilename = m_Filesystem->m_InternalFS->FindNext(findHandle);
        }
        m_Filesystem->m_IOLock.unlock();

        if (pFilename == NULL) {
            return end();
        }

        return iterator(findHandle, m_Filesystem, pFilename);
    }
}