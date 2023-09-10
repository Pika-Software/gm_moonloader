#ifndef MOONENGINE_ENGINE_HPP
#define MOONENGINE_ENGINE_HPP

#include <string>
#include <string_view>
#include <map>
#include <memory>

struct lua_State;

namespace MoonEngine {
    struct LuaStateDeleter {
        void operator()(lua_State* L);
    };

    class Engine {
    public:
        // Compiled line number = original char offset
        typedef std::map<size_t, size_t> CompiledLines;

    private:
        std::unique_ptr<lua_State, LuaStateDeleter> m_State;
        int m_ToLuaRef = 0;

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
    };
}

#endif // MOONENGINE_ENGINE_HPP