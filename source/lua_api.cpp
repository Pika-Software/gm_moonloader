#include "lua_api.hpp"
#include "config.hpp"
#include "global.hpp"
#include "core.hpp"
#include "utils.hpp"

#include <GarrysMod/Lua/Interface.h>
#include <moonengine/engine.hpp>
#include <yuescript/yue_compiler.h>

#if IS_SERVERSIDE
#include "compiler.hpp"
#include "filesystem.hpp"
#endif

using namespace MoonLoader;

inline void ParseYueConfig(GarrysMod::Lua::ILuaBase* LUA, yue::YueConfig& config, int index) {
    using namespace GarrysMod::Lua;

    LUA->GetField(index, "lint_global"); 
    if (LUA->IsType(-1, Type::Bool)) config.lintGlobalVariable = LUA->GetBool(-1); LUA->Pop();
    LUA->GetField(index, "implicit_return_root"); 
    if (LUA->IsType(-1, Type::Bool)) config.implicitReturnRoot = LUA->GetBool(-1); LUA->Pop();
    LUA->GetField(index, "reserve_line_number"); 
    if (LUA->IsType(-1, Type::Bool)) config.reserveLineNumber = LUA->GetBool(-1); LUA->Pop();
    LUA->GetField(index, "reserve_comment"); 
    if (LUA->IsType(-1, Type::Bool)) config.reserveComment = LUA->GetBool(-1); LUA->Pop();
    LUA->GetField(index, "space_over_tab"); 
    if (LUA->IsType(-1, Type::Bool)) config.useSpaceOverTab = LUA->GetBool(-1); LUA->Pop();
    LUA->GetField(index, "options");
    if (LUA->IsType(-1, Type::Table)) {
        LUA->PushNil();
        while (LUA->Next(-2) != 0) {
            auto key = std::string(Utils::GetString(LUA, -2));
            auto value = std::string(Utils::GetString(LUA, -1));
            config.options[key] = value;
            LUA->Pop();
        }
        LUA->Pop();
    }
    LUA->Pop();
    LUA->GetField(index, "line_offset");
    if (LUA->IsType(-1, Type::Number)) config.lineOffset = static_cast<int>(LUA->GetNumber(-1)); LUA->Pop();
    LUA->GetField(index, "module");
    if (LUA->IsType(-1, Type::String)) config.module = std::string(Utils::GetString(LUA, -1)); LUA->Pop();
}

namespace Functions {
    LUA_FUNCTION(EmptyFunc) {
        return 0;
    }

    LUA_FUNCTION(ToLua) {
        if (auto core = Core::Get(LUA); core && core->moonengine) {
            LUA->CheckType(1, GarrysMod::Lua::Type::String);
            unsigned int codeLen = 0;
            const char* code = LUA->GetString(1, &codeLen);

            auto info = core->moonengine->CompileString2({code, codeLen});
            if (info.error) {
                LUA->PushNil();
                LUA->PushString(info.error->display_msg.c_str());
            } else {
                // lua_code
                LUA->PushString(info.lua_code.c_str());

                // line_table
                LUA->CreateTable();
                for (auto& line : info.posmap) {
                    LUA->PushNumber(line.first);
                    LUA->PushNumber(line.second.offset);
                    LUA->SetTable(-3);
                }
            }

            return 2;
        }
        return 0;
    }

    LUA_FUNCTION(YueToLua) {
        LUA->CheckType(1, GarrysMod::Lua::Type::String);
        if (LUA->Top() >= 2) LUA->CheckType(2, GarrysMod::Lua::Type::Table);
        if (auto core = Core::Get(LUA)) { 
            auto input = Utils::GetString(LUA, 1);
            yue::YueConfig config;
            if (LUA->Top() >= 2) ParseYueConfig(LUA, config, 2);
            auto result = yue::YueCompiler().compile(input, config);
            if (result.error) LUA->PushNil();
            else Utils::PushString(LUA, result.codes);
            if (result.error) Utils::PushString(LUA, result.error->displayMessage);
            else LUA->PushNil();

            if (result.globals) {
                LUA->CreateTable();
                double i = 1;
                for (const auto& var : *result.globals) {
                    LUA->PushNumber(i);
                    LUA->CreateTable();

                    LUA->PushNumber(1);
                    Utils::PushString(LUA, var.name);
                    LUA->SetTable(-3);

                    LUA->PushNumber(2);
                    LUA->PushNumber(var.line);
                    LUA->SetTable(-3);

                    LUA->PushNumber(1);
                    LUA->PushNumber(var.col);
                    LUA->SetTable(-3);

                    LUA->SetTable(-3);
                    i++;
                }
            } else LUA->PushNil();
            return 3;
        }
        return 0;
    }

#if IS_SERVERSIDE
     LUA_FUNCTION(PreCacheDir) {
         if (auto core = Core::Get(LUA)) {
             core->lua_api->PreCacheDir(core->LUA, LUA->CheckString(1));
         }
         return 0;
     }

     LUA_FUNCTION(PreCacheFile) {
         if (auto core = Core::Get(LUA)) {
             LUA->PushBool(core->lua_api->PreCacheFile(core->LUA, LUA->CheckString(1)));
             return 1;
         }
         return 0;
     }

     LUA_FUNCTION(AddCSLuaFile) {
         if (auto core = Core::Get(LUA)) {
             core->lua_api->AddCSLuaFile(core->LUA);
         }
         return 0;
     }
#endif
}

void LuaAPI::Initialize(GarrysMod::Lua::ILuaInterface* LUA) {
    LUA->CreateTable();
    LUA->PushString("gm_moonloader"); LUA->SetField(-2, "_NAME");
    LUA->PushString("Pika-Software"); LUA->SetField(-2, "_AUTHORS");
    LUA->PushString(MOONLOADER_FULL_VERSION); LUA->SetField(-2, "_VERSION");
    LUA->PushNumber(MOONLOADER_VERSION_MAJOR); LUA->SetField(-2, "_VERSION_MAJOR");
    LUA->PushNumber(MOONLOADER_VERSION_MINOR); LUA->SetField(-2, "_VERSION_MINOR");
    LUA->PushNumber(MOONLOADER_VERSION_PATCH); LUA->SetField(-2, "_VERSION_PATCH");
    LUA->PushString(MOONLOADER_URL); LUA->SetField(-2, "_URL");

    LUA->PushCFunction(Functions::ToLua); LUA->SetField(-2, "ToLua");
    LUA->PushCFunction(Functions::EmptyFunc); LUA->SetField(-2, "PreCacheDir");
    LUA->PushCFunction(Functions::EmptyFunc); LUA->SetField(-2, "PreCacheFile");

    LUA->CreateTable();
    LUA->PushCFunction(Functions::YueToLua); LUA->SetField(-2, "ToLua");
    LUA->SetField(-2, "yue");

#if IS_SERVERSIDE
     LUA->PushCFunction(Functions::PreCacheDir); LUA->SetField(-2, "PreCacheDir");
     LUA->PushCFunction(Functions::PreCacheFile); LUA->SetField(-2, "PreCacheFile");
#endif
    LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "moonloader");

#if IS_SERVERSIDE
    if (LUA->IsServer()) {
        if (!AddCSLuaFile_ref) {
            LUA->GetField(GarrysMod::Lua::INDEX_GLOBAL, "AddCSLuaFile");
            if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) AddCSLuaFile_ref = GarrysMod::Lua::AutoReference(LUA);
            else LUA->Pop();
        }

         LUA->PushCFunction(Functions::AddCSLuaFile);
         LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "AddCSLuaFile");
    }

    BeginVersionCheck(LUA);
#endif
}

void LuaAPI::Deinitialize() {
#if IS_SERVERSIDE
    AddCSLuaFile_ref.Free();
#endif
}

#if IS_SERVERSIDE
void LuaAPI::AddCSLuaFile(GarrysMod::Lua::ILuaInterface* LUA) {
    if (AddCSLuaFile_ref.Push()) {
        if (!LUA->IsType(1, GarrysMod::Lua::Type::String)) {
            LUA->Call(0, 0);
        } else {
            std::string targetFile = LUA->GetString(1);
            if (Utils::FindMoonScript(core->LUA, targetFile)) {
                core->compiler->CompileFile(targetFile);
                Utils::SetFileExtension(targetFile, "lua");
            }

            LUA->PushString(targetFile.c_str());
            LUA->Call(1, 0);
        }
    } else {
        Warning("Oh no! Did moonloader broke AddCSLuaFile? That's not good!\n");
    }
}

 bool LuaAPI::PreCacheFile(GarrysMod::Lua::ILuaInterface* LUA, const std::string& path) {
     DevMsg("[Moonloader] Precaching %s\n", path.c_str());
     return core->compiler->CompileFile(path);
 }

 void LuaAPI::PreCacheDir(GarrysMod::Lua::ILuaInterface* LUA, const std::string& startPath) {
     for (auto file : core->fs->Find(Utils::JoinPaths(startPath, "*"), LUA->GetPathID())) {
         std::string path = Utils::JoinPaths(startPath, file);
         if (core->fs->IsDirectory(path, LUA->GetPathID())) {
             PreCacheDir(LUA, path);
         } else if (Utils::FileExtension(path) == "moon" || Utils::FileExtension(path) == "yue") {
             PreCacheFile(LUA, path);
         }
     }
 }
#endif

