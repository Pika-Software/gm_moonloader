#include "utils.hpp"

#include <filesystem.h>
#include <algorithm>
#include <chrono>

namespace MoonLoader::Utils {
    std::vector<char> ReadBinaryFile(const std::string& path, const char* pathID) {
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

    bool WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len) {
        FileHandle_t fh = g_pFullFileSystem->Open(path.c_str(), "wb", pathID);
        if (!fh)
            return false;

        int written = g_pFullFileSystem->Write(data, len, fh);
        g_pFullFileSystem->Close(fh);
        return written == len;
    }

    uint64 Timestamp() {
        // Oh, yesss! I love one-liners!
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock().now().time_since_epoch()).count();
    }

    namespace Path {
        constexpr size_t PATH_BUFFER_SIZE = MAX_UNICODE_PATH;

        char* GetPathBuffer() {
            // I hate allocating big buffers on stack, but I don't want to use heap
            // So here is a thread_local static buffer
            // I pray that it is not allocated on stack
            thread_local static char pathBuffer[PATH_BUFFER_SIZE];
            return pathBuffer;
        }

        std::string& FixSlashes(std::string& path) {
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }

        std::string& LowerCase(std::string& path) {
            std::transform(path.begin(), path.end(), path.begin(), ::toupper);
            return path;
        }

        std::string& Normalize(std::string& path) {
            FixSlashes(path);
            LowerCase(path);
            return path;
        }

        std::string_view FileName(std::string_view path) {
            auto namePos = path.find_first_of('/');
            return namePos != std::string_view::npos ? path.substr(namePos + 1) : path;
        }

        std::string_view FileExtension(std::string_view path) {
            auto fileName = FileName(path);
            auto extPos = !fileName.empty() ? fileName.find_first_of('.') : std::string_view::npos;
            return extPos != std::string_view::npos ? fileName.substr(extPos + 1) : std::string_view{};
        }

        std::string& StripFileName(std::string& path) {
            auto namePos = path.find_last_of('/');
            if (namePos != std::string::npos)
                path.erase(namePos);
            return path;
        }

        std::string& StripFileExtension(std::string& path) {
            auto namePos = path.find_last_of('.');
            if (namePos != std::string::npos)
                path.erase(namePos);
            return path;
        }

        std::string& SetFileExtension(std::string& path, std::string_view ext) {
            StripFileExtension(path);
            if (!ext.empty() && ext.front() != '.')
                path += '.';
            return path;
        }

        std::string Join(std::string_view path, std::string_view subPath) {
            std::string finalPath = std::string(path);
            if (!finalPath.empty() && finalPath.back() != '/')
                finalPath += '/';

            finalPath += subPath;
            return finalPath;
        }

        // Probably it's a good idea to make thread-safe wrapper around IFileSystem
        bool IsDirectory(const std::string& path, const char* pathID) {
            return g_pFullFileSystem->IsDirectory(path.c_str(), pathID);
        }

        bool IsFile(const std::string& path, const char* pathID) {
            return !IsDirectory(path, pathID);
        }

        bool Exists(const std::string& path, const char* pathID) {
            return g_pFullFileSystem->FileExists(path.c_str(), pathID);
        }

        void CreateDirs(const std::string& path, const char* pathID) {
            g_pFullFileSystem->CreateDirHierarchy(path.c_str(), pathID);
        }

        std::string RelativeToFullPath(const std::string& path, const char* pathID) {
            auto buffer = GetPathBuffer();
            bool success = g_pFullFileSystem->RelativePathToFullPath(path.c_str(), pathID, buffer, PATH_BUFFER_SIZE) != NULL;
            return success ? std::string(buffer) : std::string{};
        }

        std::string FullToRelativePath(const std::string& fullPath, const char* pathID) {
            auto buffer = GetPathBuffer();
            bool success = g_pFullFileSystem->FullPathToRelativePathEx(fullPath.c_str(), pathID, buffer, PATH_BUFFER_SIZE);
            return success ? std::string(buffer) : std::string{};
        }
    }
}
