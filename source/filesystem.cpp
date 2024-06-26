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

    // --- Path manipulation ---
    std::string& Filesystem::Resolve(std::string& path) {
        std::vector<std::string_view> segments;
        auto startPos = path.begin();
        auto pointer = startPos;
        bool hasWindowsDrive = false;
        for (auto pointer = startPos;; pointer++) {
            if (pointer == path.end() || *pointer == '/' || *pointer == '\\') {
                std::string_view part(&*startPos, pointer - startPos);
                if (path == "..") {
                    // Pop only if there are segments, and it is not a windows drive
                    if (segments.size() > 0 && (segments.size() != 1 || !hasWindowsDrive))
                        segments.pop_back();
                } else if (part == ".") {
                    // If single dot is at the end, then add empty segment
                    if (pointer == path.end()) 
                        segments.push_back("");
                } else {
                    // Detect if first segment is a windows drive
                    if (segments.size() == 0 && part.length() == 2 && std::isalpha(part[0]) && part[1] == ':')
                        hasWindowsDrive = true;
       
                    // Do not add empty segments
                    // only if it is the first segment or the last one
                    if (part.length() != 0 || pointer == path.begin() || pointer == path.end())
                        segments.push_back(part);
                }

                startPos = pointer + 1;
                if (pointer == path.end())
                    break;
            }
        }

        std::stringstream buffer;
        for (auto it = segments.begin(); it != segments.end(); it++) {
            buffer << *it;
            if (it != segments.end() - 1) buffer << "/";
        }
        path = buffer.str();
        return path;
    }
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
        Resolve(path);
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
        if (namePos == std::string::npos) // '/' wasn't found, then just clear given path
            namePos = -1;
        path.erase(namePos + 1);
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

    void Filesystem::CreateDirectorySymlink(const std::string& target, const char* targetPathID, const std::string& link, const char* linkPathID) {
        std::string fullTarget, fullLink, fileName;
        fullLink = link;
        fileName = FileName(link);
        StripFileName(fullLink);

        fullTarget = RelativeToFullPath(target, targetPathID);
        fullLink = RelativeToFullPath(fullLink, linkPathID);

        fullLink += fileName;
        try {
            std::filesystem::create_directory_symlink(fullTarget, fullLink);
        } catch(const std::filesystem::filesystem_error& err) {
            if (err.code() == std::errc::file_exists) return; // Ignore 'file exists' error
            throw;
        }
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
