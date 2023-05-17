#ifndef MOONLOADER_COMPILER_HPP
#define MOONLOADER_COMPILER_HPP

#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <tier1/checksum_md5.h>

struct MD5Value_t;
class CUtlBuffer;

namespace MoonLoader {
    class Compiler {
    public:
        std::optional<MD5Value_t> GetFileHash(std::string path);
        MD5Value_t GetFileHash(CUtlBuffer& buffer);
        MD5Value_t GetFileHash(void* data, size_t len);

        bool CompileMoonScript(std::string path);
    };
}

#endif