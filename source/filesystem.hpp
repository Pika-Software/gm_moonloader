#ifndef MOONLOADER_FILESYSTEM_HPP
#define MOONLOADER_FILESYSTEM_HPP

#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <iterator>
#include <vector>
#include <tuple>
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
    };

    class Filesystem::FileFinder {
    public:
        class Iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = std::tuple<std::string_view, bool>;
            using pointer           = value_type*;
            using reference         = value_type&;

            Iterator() = default;
            Iterator(Filesystem* filesystem, FileFindHandle_t handle, const char* pFileName)
                : m_Filesystem(filesystem), m_Handle(handle) { SetValue(pFileName); }

            Iterator(const Iterator&) = delete;
            Iterator& operator=(const Iterator&) = delete;
            Iterator(Iterator&&) = default;
            Iterator& operator=(Iterator&&) = default;

            reference operator*() { return m_Value; }
            pointer operator->() { return &m_Value; }

            Iterator& operator++();

            friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_Handle == b.m_Handle && a.m_Value == b.m_Value; };
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_Handle != b.m_Handle || a.m_Value != b.m_Value; };

        private:
            Filesystem* m_Filesystem = nullptr;
            FileFindHandle_t m_Handle = 0;
            value_type m_Value = {};

            bool SetValue(const char* pFileName);
        };

        FileFinder(Filesystem* filesystem, const std::string& wildcard, const std::string& pathID = {})
            : m_Filesystem(filesystem), m_SearchWildcard(wildcard), m_PathID(pathID) {}

        Iterator begin();
        Iterator end() const { return Iterator(); }

    private:
        Filesystem* m_Filesystem;
        std::string m_SearchWildcard;
        std::string m_PathID;
    };
}

#endif // MOONLOADER_FILESYSTEM_HPP
