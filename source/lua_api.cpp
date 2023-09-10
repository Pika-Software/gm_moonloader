#include "lua_api.hpp"
#include "config.hpp"
#include "global.hpp"
#include "core.hpp"

#include <GarrysMod/Lua/Interface.h>
#include <moonengine/engine.hpp>

#if IS_SERVERSIDE
#include "compiler.hpp"
#include "utils.hpp"
#include "filesystem.hpp"
#endif

using namespace MoonLoader;

namespace Functions {
    LUA_FUNCTION(EmptyFunc) {
        return 0;
    }

    LUA_FUNCTION(ToLua) {
        if (auto core = Core::Get(LUA)) {
            LUA->CheckType(1, GarrysMod::Lua::Type::String);
            unsigned int codeLen = 0;
            const char* code = LUA->GetString(1, &codeLen);

            std::string result;
            MoonEngine::Engine::CompiledLines lines;
            bool success = core->moonengine->CompileStringEx(code, codeLen, result, &lines);

            if (success) {
                // lua_code
                LUA->PushString(result.c_str(), result.size());

                // line_table
                LUA->CreateTable();
                for (auto& line : lines) {
                    LUA->PushNumber(line.first);
                    LUA->PushNumber(line.second);
                    LUA->SetTable(-3);
                }
            } else {
                LUA->PushNil();
                LUA->PushString(result.c_str(), result.size());
            }

            return 2;
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
                core->compiler->CompileMoonScript(core->LUA, targetFile);
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
     return core->compiler->CompileMoonScript(LUA, path);
 }

 void LuaAPI::PreCacheDir(GarrysMod::Lua::ILuaInterface* LUA, const std::string& startPath) {
     for (auto file : core->fs->Find(Utils::JoinPaths(startPath, "*"), LUA->GetPathID())) {
         std::string path = Utils::JoinPaths(startPath, file);
         if (core->fs->IsDirectory(path, LUA->GetPathID())) {
             PreCacheDir(LUA, path);
         } else if (Utils::FileExtension(path) == "moon") {
             PreCacheFile(LUA, path);
         }
     }
 }
#endif

