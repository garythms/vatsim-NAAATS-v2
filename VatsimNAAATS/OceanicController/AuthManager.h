#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

using namespace std;

class CAuthManager {
public:
    static void InitiateLogin(HWND hWnd);
    static bool IsAuthenticated();
    static string GetUserCID();
    static string GetAccessToken();

private:
    static string accessToken;
    static string userCID;
    static bool isAuthenticated;
};
