#ifndef MOONENGINE_ENGINE_HPP
#define MOONENGINE_ENGINE_HPP

#pragma once

#include <string>
#include <string_view>
#include <map>
#include <memory>
#include <utility>

struct lua_State;

namespace MoonEngine {
    struct LuaStateDeleter {
        void operator()(lua_State* L);
    };

    struct CompileInfo {
        struct Pos {
            Pos(int offset = 0, int line = 1, int col = 1) : offset(offset), line(line), col(col) {}
            int offset;
            int line;
            int col;
        };
        struct Error {
            std::string msg;
            std::string display_msg;
            Pos pos;
        };
        std::string lua_code;
        std::optional<Error> error;
        std::map<int, Pos> posmap; // lua line -> moonscript pos
        double parse_time = 0; // in millis
        double compile_time = 0; // in millis
        size_t memory_usage = 0; // in bytes

        static CompileInfo FromError(Error&& err) {
            CompileInfo info;
            info.error = std::move(err);
            return info;
        }
        static CompileInfo FromError(std::string_view msg, std::string_view display_msg = {}, Pos pos = {}) {
            Error err;
            err.msg = std::string(msg);
            err.display_msg = display_msg.empty() ? err.msg : std::string(display_msg);
            err.pos = std::move(pos);
            return FromError(std::move(err));
        }
    };

    struct CompileOptions {
        bool implicitly_return_root = true;
    };

    class Engine {
    public:
        // Compiled line number = original char offset
        typedef std::map<size_t, size_t> CompiledLines;

    private:
        std::unique_ptr<lua_State, LuaStateDeleter> m_State;
        int m_ToLuaRef = 0;
        int m_ParseStringRef = 0;
        int m_CompileTreeRef = 0;
        int m_CompileFormatErrorRef = 0;

    public:
        Engine();

        void RunLua(const char* luaCode);
        std::string CompileString(const char* moonCode, size_t len, CompiledLines* lineTable = nullptr);
        std::string CompileString(std::string_view moonCode, CompiledLines* lineTable = nullptr) {
            return CompileString(moonCode.data(), moonCode.size(), lineTable);
        }

        bool CompileStringEx(const char* moonCode, size_t len, std::string& output, CompiledLines* lineTable = nullptr);
        bool CompileStringEx(std::string_view moonCode, std::string& output, CompiledLines* lineTable = nullptr) {
            return CompileStringEx(moonCode.data(), moonCode.size(), output, lineTable);
        }

        CompileInfo CompileString2(std::string_view moonCode, const CompileOptions& options = {});

        // Converts char offset to line number and column
        static std::pair<int, int> OffsetToLine(std::string_view str, int pos);
        inline static CompileInfo::Pos OffsetToPos(std::string_view str, int pos) {
            auto [line, col] = OffsetToLine(str, pos);
            return CompileInfo::Pos(pos, line, col);
        }
    };
}

#endif // MOONENGINE_ENGINE_HPP
