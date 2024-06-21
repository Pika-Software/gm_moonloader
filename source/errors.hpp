#ifndef MOONLOADER_ERRORS_HPP
#define MOONLOADER_ERRORS_HPP

#pragma once

#include <memory>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <optional>

namespace MoonLoader {
    class Core;

    class ErrorLine {
    public:
        std::string message;
        std::string source;
        int line;

        std::string to_string() {
            return source + ":" + std::to_string(line) + ": " + message;
        }

        GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry to_stack() {
            return {source, "", line};
        }
    };

    class Errors : public GarrysMod::Lua::ILuaGameCallback {
        std::shared_ptr<Core> core;
        GarrysMod::Lua::CLuaInterface* LUA = nullptr;
        GarrysMod::Lua::ILuaGameCallback* callback = nullptr;
    public:
        Errors(std::shared_ptr<Core> core);
        Errors(std::shared_ptr<Core> core, GarrysMod::Lua::ILuaInterface* LUA);
        ~Errors();

        void TransformStackEntry(std::string& source, int &line);
        inline void TransformStackEntry(GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry& entry) { return TransformStackEntry(entry.source, entry.line); }        
        inline void TransformStackEntry(ErrorLine& entry) { return TransformStackEntry(entry.source, entry.line); }
        std::optional<ErrorLine> TransformErrorMessage(std::string& err);
        void PrintSourceFile(std::string_view code, const ErrorLine& error);
        virtual void LuaError(const GarrysMod::Lua::ILuaGameCallback::CLuaError *error);

        // Default callbacks
        virtual GarrysMod::Lua::ILuaObject* CreateLuaObject() { return callback->CreateLuaObject(); }
        virtual void DestroyLuaObject(GarrysMod::Lua::ILuaObject *pObject) { callback->DestroyLuaObject(pObject); };
        virtual void ErrorPrint( const char *error, bool print ) { callback->ErrorPrint(error, print); }
        virtual void Msg( const char *msg, bool useless ) { callback->Msg(msg, useless); }
        virtual void MsgColour( const char *msg, const Color &color ) { callback->MsgColour(msg, color); }
        virtual void InterfaceCreated(GarrysMod::Lua::ILuaInterface *iface) { callback->InterfaceCreated(iface); }
    };
}

#endif //MOONLOADER_ERRORS_HPP
