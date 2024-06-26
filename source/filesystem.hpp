#ifndef MOONLOADER_FILESYSTEM_HPP
#define MOONLOADER_FILESYSTEM_HPP

#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <iterator>
#include <vector>
#include <platform.h>

// SourceSDK filesystem
class IFileSystem;
typedef intp FileFindHandle_t;

namespace MoonLoader {
    class Filesystem {
        IFileSystem* m_InternalFS;
        std::mutex m_IOLock;

    public:
        class FileFinder;

        Filesystem(IFileSystem* fs); 

        static char* PathBuffer();
        static size_t PathBufferSize();

        // --- Path manipulation ---
        static std::string& Resolve(std::string& path);
        static std::string& FixSlashes(std::string& path);
        static std::string& LowerCase(std::string& path);
        // Fixes slashes and lowercases the path
        static std::string& Normalize(std::string& path);

        // dir/file.ext -> file.ext
        static std::string_view FileName(std::string_view path);
        // dir/file.ext -> ext
        static std::string_view FileExtension(std::string_view path);
        // dir/file.ext -> dir
        static std::string& StripFileName(std::string& path);
        // dir/file.ext -> dir/file
        static std::string& StripFileExtension(std::string& path);
        // dir/file.ext + .bak -> dir/file.bak
        static std::string& SetFileExtension(std::string& path, std::string_view ext);
        // dir + subdir/file.ext -> dir/subdir/file.ext
        static std::string Join(std::string_view path, std::string_view subPath);

        std::string RelativeToFullPath(const std::string& relativePath, const char* pathID);
        std::string FullToRelativePath(const std::string& fullPath, const char* pathID);
        std::string TransverseRelativePath(const std::string& relativePath, const char* fromPathID, const char* toPathID);
        // -------------------------

        bool Exists(const std::string& path, const char* pathID = 0);
        bool IsDirectory(const std::string& path, const char* pathID = 0);
        bool IsFile(const std::string& path, const char* pathID = 0) { return !IsDirectory(path, pathID) && Exists(path, pathID); }

        FileFinder Find(const std::string& wildcard, const char* pathID = 0);

        // Remove functions return how many files were removed
        int RemoveFile(const std::string& filename, const char* pathID = 0);
        int RemoveDir(const std::string& dir, const char* pathID = 0);
        int Remove(const std::string& path, const char* pathID = 0);

        void CreateDirs(const std::string& path, const char* pathID = 0);

        std::vector<char> ReadBinaryFile(const std::string& path, const char* pathID = 0);
        std::string ReadTextFile(const std::string& path, const char* pathID = 0);
        bool WriteToFile(const std::string& path, const char* pathID, const void* data, size_t len);

        size_t GetFileTime(const std::string& path, const char* pathID = 0);

        void AddSearchPath(const std::string& path, const char* pathID = 0, bool addToFront = false);
        void RemoveSearchPath(const std::string& path, const char* pathID = 0);

        void CreateDirectorySymlink(const std::string& target, const char* targetPathID, const std::string& link, const char* linkPathID);
        inline void CreateDirectorySymlink(const std::string& target, const std::string& link, const char* pathID = 0) { CreateDirectorySymlink(target, pathID, link, pathID); }
    };

    class Filesystem::FileFinder {
        Filesystem* m_Filesystem;
        std::string m_SearchWildcard;
        std::string m_PathID;

    public:
        class iterator : public std::iterator<
            std::input_iterator_tag,   // iterator_category
            const char*,                      // value_type
            const char*,                      // difference_type
            const char*,               // pointer
            const char*                       // reference
        > {
            FileFindHandle_t m_Handle = 0;
            Filesystem* m_Filesystem = nullptr;
            const char* m_pFileName = 0;

        public:
            iterator() {} // Empty constructor for end()
            explicit iterator(FileFindHandle_t handle, Filesystem* filesystem, const char* fileName)
                : m_Handle(handle), m_Filesystem(filesystem), m_pFileName(fileName) {}

            bool valid_handle() const { return m_Handle > 0; }
            iterator& operator++();
            bool operator==(const iterator& other) const { return m_Handle == other.m_Handle; }
            bool operator!=(const iterator& other) const { return !(*this == other); }
            reference operator*() const { return m_pFileName; }
        };

        static iterator INVALID_ITERATOR;

        FileFinder(Filesystem* filesystem, const std::string& wildcard, const char* pathID = 0)
            : m_Filesystem(filesystem), m_SearchWildcard(wildcard), m_PathID(pathID ? pathID : "") {}

        iterator begin();
        iterator end() const { return INVALID_ITERATOR; }
    };
}

#endif // MOONLOADER_FILESYSTEM_HPP
