#ifndef MOONLOADER_UTILS_HPP
#define MOONLOADER_UTILS_HPP

#include <string>
#include <vector>
#include <platform.h>

namespace MoonLoader::Utils {
    std::vector<char> ReadBinaryFile(const std::string& path, const char* pathID = 0);
    bool WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len);

    uint64 Timestamp();

    namespace Path {
        std::string& FixSlashes(std::string& path);
        std::string& LowerCase(std::string& path);

        // Fixes slashes and lowercases the path
        std::string& Normalize(std::string& path);

        // Note: I recommend to you normalize your path before using these functions

        // dir/file.ext -> file.ext
        std::string_view FileName(std::string_view path);
        // dir/file.ext -> ext
        std::string_view FileExtension(std::string_view path);
        // dir/file.ext -> dir
        std::string& StripFileName(std::string& path);
        // dir/file.ext -> dir/file
        std::string& StripFileExtension(std::string& path);
        // dir/file.ext + .bak -> dir/file.bak
        std::string& SetFileExtension(std::string& path, std::string_view ext);
        // dir + subdir/file.ext -> dir/subdir/file.ext
        std::string Join(std::string_view path, std::string_view subPath);
    
        bool IsDirectory(const std::string& path, const char* pathID = 0);
        bool IsFile(const std::string& path, const char* pathID = 0);
        bool Exists(const std::string& path, const char* pathID = 0);
        void CreateDirs(const std::string& path, const char* pathID = 0);
    
        std::string RelativeToFullPath(const std::string& relativePath, const char* pathID);
        std::string FullToRelativePath(const std::string& fullPath, const char* pathID);
    }
}

#endif // MOONLOADER_UTILS_HPP