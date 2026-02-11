#include "AuthManager.h"
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#include <thread>
#include <iostream>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

string CAuthManager::accessToken = "";
string CAuthManager::userCID = "";
bool CAuthManager::isAuthenticated = false;

// VATSIM OAuth Configuration (Placeholders - would need real client_id/secret)
const string AUTH_URL = "https://auth.vatsim.net/oauth/authorize";
const string TOKEN_URL = "https://auth.vatsim.net/oauth/token";
const string REDIRECT_URI = "http://localhost:8080/callback";
const string CLIENT_ID = "YOUR_CLIENT_ID"; 

#define WM_AUTH_COMPLETE (WM_USER + 1)

void CAuthManager::InitiateLogin(HWND hWnd) {
    // 1. Construct the authorization URL
    string url = AUTH_URL + "?client_id=" + CLIENT_ID + 
                 "&redirect_uri=" + REDIRECT_URI + 
                 "&response_type=code&scope=full_name+vatsim_details+email";

    // 2. Open the browser for the user to login
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);

    // 3. Start a temporary background thread to listen for the callback
    thread([hWnd]() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(8080);

        bind(listenSock, (sockaddr*)&addr, sizeof(addr));
        listen(listenSock, 1);

        SOCKET clientSock = accept(listenSock, NULL, NULL);
        if (clientSock != INVALID_SOCKET) {
            char buffer[4096];
            int bytes = recv(clientSock, buffer, sizeof(buffer), 0);
            if (bytes > 0) {
                string request(buffer, bytes);
                size_t codePos = request.find("code=");
                if (codePos != string::npos) {
                    size_t endPos = request.find(" ", codePos);
                    string code = request.substr(codePos + 5, endPos - (codePos + 5));

                    // Send success response to browser
                    string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                      "<html><body><h1>Login Successful</h1><p>You can close this window and return to the Oceanic Controller app.</p></body></html>";
                    send(clientSock, response.c_str(), response.length(), 0);

                    isAuthenticated = true;
                    userCID = "1234567"; // This would be fetched from /api/user
                    PostMessage(hWnd, WM_AUTH_COMPLETE, 1, 0);
                } else {
                    PostMessage(hWnd, WM_AUTH_COMPLETE, 0, 0);
                }
            }
            closesocket(clientSock);
        }
        closesocket(listenSock);
        WSACleanup();
    }).detach();
}

bool CAuthManager::IsAuthenticated() {
    return isAuthenticated;
}

string CAuthManager::GetUserCID() {
    return userCID;
}

string CAuthManager::GetAccessToken() {
    return accessToken;
}

