#ifndef MOONENGINE_ENGINE_HPP
#define MOONENGINE_ENGINE_HPP

#include <string>
#include <string_view>

struct lua_State;

namespace MoonEngine {
    class Engine {
        lua_State* m_State = nullptr;
        bool m_Initialized = false;

        int m_ToLuaRef = 0;
    public:
        Engine();
        ~Engine();

        bool IsInitialized() { return m_Initialized; }

        void RunLua(const char* luaCode);
        std::string CompileString(const char* moonCode, size_t len);
        std::string CompileString(std::string_view moonCode);
    };
}

#endif // MOONENGINE_ENGINE_HPP