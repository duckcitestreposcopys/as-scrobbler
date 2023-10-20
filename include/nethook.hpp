/**
 * This file:
 * - hooks wininet functions
 * - rewrites parts of wininet functions
 * - emulates a last.fm server to submit scrobbles to your own local servers
 */

#include <Windows.h>
#include <WinInet.h>
#include <stdexcept>
#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>

#include "scrobbler.hpp"

namespace as_scrobbler
{
    namespace nethook
    {
        SafetyHookInline g_getserverunicode_hook{};
        SafetyHookInline g_getserver_hook{};
        SafetyHookInline g_internetconnecta_hook{};
        SafetyHookInline g_httpsendrequesta_hook{};
        SafetyHookInline g_httpendrequesta_hook{};
        SafetyHookInline g_internetreadfile_hook{};
        SafetyHookInline g_httpsendrequestexa_hook{};
        SafetyHookInline g_internetwritefile_hook{};

        scrobble scrobbleFunc;

        bool recentAuth = false;

        void replaceAll(std::string& source, const std::string& from, const std::string& to)
        {
            // safe replace all: https://stackoverflow.com/questions/2896600/how-to-replace-all-occurrences-of-a-character-in-string
            std::string newString;
            newString.reserve(source.length());  // avoids a few memory allocations

            std::string::size_type lastPos = 0;
            std::string::size_type findPos;

            while (std::string::npos != (findPos = source.find(from, lastPos)))
            {
                newString.append(source, lastPos, findPos - lastPos);
                newString += to;
                lastPos = findPos + from.length();
            }

            // Care for the rest after last occurrence
            newString += source.substr(lastPos);

            source.swap(newString);
        }


        static std::string base64_encode(const std::string& in) {
            // b64 https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c and its comment
            std::string out;

            int val = 0, valb = -6;
            for (unsigned char c : in) {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0) {
                    out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
            while (out.size() % 4) out.push_back('=');
            return out;
        }


        static std::string base64_decode(const std::string& in) {

            std::string out;

            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

            int val = 0, valb = -8;
            for (unsigned char c : in) {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    out.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return out;
        }


        char* rewriteTargetServer(char* server)
        {
            if (strstr(server, "post.audioscrobbler.com") || strstr(server, "post2.audioscrobbler.com"))
                server = _strdup("0.0.0.0");
            return server;
        }

        char* __fastcall GetTargetServerUnicodeHook(void* thisptr, uintptr_t edx)
        {
            char* server = g_getserverunicode_hook.thiscall<char*>(thisptr);
            spdlog::debug("Rewriting server (Unicode): {0}", server);
            return rewriteTargetServer(server);
        }

        char* __fastcall GetTargetServerHook(void* thisptr, uintptr_t edx)
        {
            char* server = g_getserver_hook.thiscall<char*>(thisptr);
            spdlog::debug("Rewriting server: {0}", server);
            return rewriteTargetServer(server);
        }

        char* debugUrl(HINTERNET hInternet, std::string functionName) {
            DWORD bufferSize = 0;
            InternetQueryOptionA(hInternet, INTERNET_OPTION_URL, NULL, &bufferSize);
            char* url = new char[bufferSize];
            BOOL result = InternetQueryOptionA(hInternet, INTERNET_OPTION_URL, (LPVOID)url, &bufferSize);
            url[BUFSIZ] = 0;
            return url;
        }

        HINTERNET WINAPI InternetConnectHook(HINTERNET hInternet,
            LPCSTR lpszServerName,
            INTERNET_PORT nServerPort,
            LPCSTR lpszUserName,
            LPCSTR lpszPassword,
            DWORD dwService,
            DWORD dwFlags,
            DWORD_PTR dwContext)
        {

            if (lstrcmp(lpszServerName, "www.audio-surf.com") == 0) {
                return g_internetconnecta_hook.stdcall<HINTERNET>(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
            }
            else {
                nServerPort = 9999;
            }

            return g_internetconnecta_hook.stdcall<HINTERNET>(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
        }


        BOOL WINAPI HttpSendRequestAHook(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength) {
            

            std::string urlString = debugUrl(hRequest, "SendRequestA");
            std::string httpsUrl = "https://0.0.0.0:9999/?hs";
            BOOL shouldReplace = urlString.starts_with(httpsUrl);

            if (shouldReplace) {
                return true;
            }


            std::string text = "";
            if (lpOptional != NULL) {
                text = reinterpret_cast<const char*>(lpOptional);
                BOOL isProtocolUrl = urlString.ends_with("protocol_1.2");
                if (isProtocolUrl) {
                    spdlog::debug("Redirecting protocol URL");
                    std::string url = as_scrobbler::config::scrobble_server + ":" + std::to_string(as_scrobbler::config::server_port);
                    scrobbleFunc(text.c_str(), url.c_str(), as_scrobbler::config::api_key.c_str());
                    return true;
                }
            }

            BOOL ret = g_httpsendrequesta_hook.stdcall<BOOL>(hRequest, lpszHeaders, dwHeadersLength, (text == "") ? NULL : text.c_str(), dwOptionalLength);
            return ret;
        }


        BOOL WINAPI InternetReadFileHook(_In_ HINTERNET hFile, _Out_ LPVOID lpBuffer, _In_ DWORD dwNumberOfBytesToRead, _Out_ LPDWORD lpdwNumberOfBytesRead) {
            std::string urlString = debugUrl(hFile, "InternetReadFile");
            std::string authUrl = "https://0.0.0.0:9999/?hs";
            BOOL isAuthUrl = urlString.starts_with(authUrl);
            BOOL isProtocolUrl = urlString.ends_with("protocol_1.2");

            std::string text = "";

            // https://github.com/badstreff/DotaRain/blob/master/SteamWebAPI.cpp
            DWORD dwBytesRead = 0;
            do {
                char* MYlpBuffer = new char[2000];
                ZeroMemory(MYlpBuffer, 2000);
                g_internetreadfile_hook.stdcall<BOOL>(hFile, (LPVOID)MYlpBuffer, 2000, &dwBytesRead);
                text += MYlpBuffer;
                delete[] MYlpBuffer;
                MYlpBuffer = NULL;
            } while (dwBytesRead);

            
            if (isAuthUrl) {
                std::string auth_text = base64_decode("T0sKYWFiYjAwYmJhYWJiMDBiYmFhYmIwMGJiYWFiYjAwYmIKaHR0cDovL3Bvc3QuYXVkaW9zY3JvYmJsZXIuY29tOjgwL25wXzEuMgpodHRwOi8vcG9zdDIuYXVkaW9zY3JvYmJsZXIuY29tOjgwL3Byb3RvY29sXzEuMgo=");
                if (!recentAuth)  {
                    recentAuth = true;
                    text = auth_text;
                }   
            }
            else {
                recentAuth = false;
            }

            // ???: https://stackoverflow.com/a/65414961
            if (dwNumberOfBytesToRead < text.size()) {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }

            memcpy(lpBuffer, text.data(), text.size());
            *lpdwNumberOfBytesRead = text.size();

            SetLastError(ERROR_SUCCESS);
            

            return true;
        }

        void init(scrobble locatedScrobbleFunc)
        {
            spdlog::info("Attaching networking hooks");
            FARPROC targetServerUnicodeHandle = GetProcAddress(GetModuleHandleA("HTTP_Fetch_Unicode.dll"), "?GetTargetServer@HTTP_Fetch_Unicode@@UAEPADXZ");
            FARPROC targetServerHandle = GetProcAddress(GetModuleHandleA("17C5B19F-4273-423C-A158-CA6F73046D43.dll"), "?GetTargetServer@Aco_HTTP_Fetch@@UAEPADXZ");
            g_getserverunicode_hook = safetyhook::create_inline((void*)targetServerUnicodeHandle, (void*)GetTargetServerUnicodeHook);
            g_getserver_hook = safetyhook::create_inline((void*)targetServerHandle, (void*)GetTargetServerHook);
            g_internetconnecta_hook = safetyhook::create_inline((void*)InternetConnectA, (void*)InternetConnectHook);
            g_httpsendrequesta_hook = safetyhook::create_inline((void*)HttpSendRequestA, (void*)HttpSendRequestAHook);
            g_internetreadfile_hook = safetyhook::create_inline((void*)InternetReadFile, (void*)InternetReadFileHook);
            scrobbleFunc = locatedScrobbleFunc;
            recentAuth = false;
            if (!g_getserver_hook || !g_getserverunicode_hook || !g_internetconnecta_hook)
            {
                spdlog::error("Failed to attach hook(s). Hook addresses: {0:p} {1:p} {2:p}", fmt::ptr(&g_getserver_hook), fmt::ptr(&g_getserverunicode_hook), fmt::ptr(&g_internetconnecta_hook));
                throw std::runtime_error("Hook failed");
            }
        }
    }
}