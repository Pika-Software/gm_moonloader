#include "global.hpp"
#include "utils.hpp"
#include "config.hpp"

#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <steam/isteamhttp.h>

using namespace MoonLoader;

constexpr unsigned int MAX_VERSION_LENGTH = 32;

class VersionCheckCallback;
std::unique_ptr<VersionCheckCallback> g_pVersionCheckCallback;

class VersionCheckCallback {
public:
    void OnHTTPRequestComplete(HTTPRequestCompleted_t* pCallback, bool bIOFailure);
    CCallResult<VersionCheckCallback, HTTPRequestCompleted_t> m_HTTPRequestCompleted;
};

void VersionCheckCallback::OnHTTPRequestComplete(HTTPRequestCompleted_t* pCallback, bool bIOFailure) {
    if (pCallback->m_eStatusCode == k_EHTTPStatusCode200OK && pCallback->m_unBodySize < MAX_VERSION_LENGTH) {
        char buffer[MAX_VERSION_LENGTH] = {};
        SteamHTTP()->GetHTTPResponseBodyData(pCallback->m_hRequest, (uint8*)buffer, pCallback->m_unBodySize);

        int major, minor, patch;
        if (sscanf(buffer, "%d.%d.%d", &major, &minor, &patch) == 3) {
            if (MOONLOADER_VERSION_MAJOR < major || MOONLOADER_VERSION_MINOR < minor || MOONLOADER_VERSION_PATCH < patch) {
                Msg("[Mooloader] New version available: %d.%d.%d -> %d.%d.%d (%s)\n", 
                    MOONLOADER_VERSION_MAJOR, MOONLOADER_VERSION_MINOR, MOONLOADER_VERSION_PATCH,
                    major, minor, patch,
                    MOONLOADER_URL
                );
            }
        }
    }

    SteamHTTP()->ReleaseHTTPRequest(pCallback->m_hRequest);
    g_pVersionCheckCallback.release();
}

LUA_FUNCTION(CheckVersion) {
    SteamAPICall_t call;
    HTTPRequestHandle handle = SteamHTTP()->CreateHTTPRequest(EHTTPMethod::k_EHTTPMethodGET, "https://raw.githubusercontent.com/Pika-Software/gm_moonloader/main/VERSION");
    if (SteamHTTP()->SendHTTPRequest(handle, &call)) {
        g_pVersionCheckCallback = std::make_unique<VersionCheckCallback>();
        g_pVersionCheckCallback->m_HTTPRequestCompleted.Set(call, g_pVersionCheckCallback.get(), &VersionCheckCallback::OnHTTPRequestComplete);
    } else {
        SteamHTTP()->ReleaseHTTPRequest(handle);
        DevWarning("[Moonloader] Failed to send version check request\n");
    }

    return 0;
}

void MoonLoader::StartVersionCheck(GarrysMod::Lua::ILuaInterface* LUA) {
    Utils::FindValue(LUA, "timer.Simple");
    LUA->PushNumber(5); // Only check for an update after 5 seconds
    LUA->PushCFunction(CheckVersion);
    if (LUA->PCall(2, 0, 0) != 0) {
        DevWarning("[Moonloader] Failed to initialize version check: %s\n", LUA->GetString(-1));
        LUA->Pop();
    }
}