#include "filesystem.hpp"

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
        std::transform(path.begin(), path.end(), path.begin(), ::toupper);
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
        bool success = g_pFullFileSystem->RelativePathToFullPath(path.c_str(), pathID, PathBuffer(), PathBufferSize()) != NULL;
        return success ? std::string(PathBuffer()) : std::string{};
    }
    std::string Filesystem::FullToRelativePath(const std::string& fullPath, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        bool success = g_pFullFileSystem->FullPathToRelativePathEx(fullPath.c_str(), pathID, PathBuffer(), PathBufferSize());
        return success ? std::string(PathBuffer()) : std::string{};
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
        g_pFullFileSystem->CreateDirHierarchy(path.c_str(), pathID);
    }

    std::vector<char> Filesystem::ReadBinaryFile(const std::string& path, const char* pathID) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        std::vector<char> result = {};
        FileHandle_t fh = g_pFullFileSystem->Open(path.c_str(), "rb", pathID);
        if (fh) {
            int fileSize = g_pFullFileSystem->Size(fh);
            result.resize(fileSize);

            g_pFullFileSystem->Read(result.data(), result.size(), fh);
            g_pFullFileSystem->Close(fh);
        }
        return result;
    }
    bool Filesystem::WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len) {
        std::lock_guard<std::mutex> lock(m_IOLock);
        FileHandle_t fh = g_pFullFileSystem->Open(path.c_str(), "wb", pathID);
        if (!fh)
            return false;

        int written = g_pFullFileSystem->Write(data, len, fh);
        g_pFullFileSystem->Close(fh);
        return written == len;
    }

    // ---------------------------- FileFinder ----------------------------
    Filesystem::FileFinder::iterator& Filesystem::FileFinder::iterator::operator++() {
        if (!m_Filesystem || !valid_handle() || m_pFileName == NULL)
            return end();

        m_Filesystem->m_IOLock.lock();
        m_pFileName = m_Filesystem->m_InternalFS->FindNext(m_Handle);
        if (m_pFileName == NULL) {
            m_Filesystem->m_InternalFS->FindClose(m_Handle);
            m_Handle = 0;
        }
        m_Filesystem->m_IOLock.unlock();

        return m_pFileName != NULL ? *this : end();
    }

    Filesystem::FileFinder::iterator Filesystem::FileFinder::begin() {
        if (!m_Filesystem || m_SearchWildcard.empty())
            return end();
        
        FileFindHandle_t findHandle;
        m_Filesystem->m_IOLock.lock();
        const char* pFilename = g_pFullFileSystem->FindFirstEx(m_SearchWildcard.c_str(), m_PathID.c_str(), &findHandle);

        while (pFilename != NULL && (strcmp(pFilename, ".") == 0 || strcmp(pFilename, "..") == 0)) {
            // Skip . and ..
            pFilename = g_pFullFileSystem->FindNext(findHandle);
        }
        m_Filesystem->m_IOLock.unlock();

        if (pFilename == NULL) {
            return end();
        }

        return iterator(findHandle, m_Filesystem, pFilename);
    }
}