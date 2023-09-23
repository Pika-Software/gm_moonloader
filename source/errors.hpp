#ifndef MOONLOADER_ERRORS_HPP
#define MOONLOADER_ERRORS_HPP

#pragma once

#include <memory>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <GarrysMod/Lua/LuaInterface.h>

namespace MoonLoader {
    class Core;

    class Errors : public GarrysMod::Lua::ILuaGameCallback {
        std::shared_ptr<Core> core;
        GarrysMod::Lua::CLuaInterface* LUA = nullptr;
        GarrysMod::Lua::ILuaGameCallback* callback = nullptr;
    public:
        Errors(std::shared_ptr<Core> core);
        Errors(std::shared_ptr<Core> core, GarrysMod::Lua::ILuaInterface* LUA);
        ~Errors();

        void TransformStackEntry(GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry& entry);
        std::optional<GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry> TransformErrorMessage(std::string& message);
        void PrintSourceFile(std::string_view code, int line);
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
