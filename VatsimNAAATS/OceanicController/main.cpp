#include <windows.h>
#include <string>
#include <shellapi.h>
#include <ctime>
#include <gdiplus.h>
#include <vector>
#include "Version.h"
#include "AuthManager.h"
#include "Launcher.h"
#include "ConfigManager.h"
#include "FddWindow.h"
#include "CpdlcWindow.h"
#include "Styles.h"

using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")

// Control IDs
#define IDC_EDIT_CID 1001
#define IDC_EDIT_PWD 1002
#define IDC_EDIT_ESPATH 1003
#define IDC_EDIT_VNAAATS 1004
#define IDC_EDIT_TOPSKY 1005
#define IDC_EDIT_HOPPIES 1006
#define IDC_BTN_BROWSE_ES 1007
#define IDC_BTN_BROWSE_VNAAATS 1008
#define IDC_BTN_BROWSE_TOPSKY 1009
#define WM_AUTH_COMPLETE (WM_USER + 1)

// Global variables
HINSTANCE hInst;
HWND hFddWnd = NULL;
HWND hCpdlcWnd = NULL;

enum class AppState {
    LOGO,
    LOGIN,
    SETUP_PATHS,
    SETUP_HOPPIES,
    MENU,
    VNAAATS_HUB,
    LAUNCHED
};
AppState currentState = AppState::LOGO;
clock_t logoStartTime;
ULONG_PTR gdiplusToken;

HWND hEditCID, hEditPWD, hEditESPath, hEditVNaaats, hEditTopSky, hEditHoppies;
HWND hBtnBrowseES, hBtnBrowseVNaaats, hBtnBrowseTopSky;

// Colors
Color FDD_Background = Styles::DarkBackground;
Color FDD_AccentBlue = Styles::ScreenBlue;
Color FDD_BoxBG = Styles::LightBackground;
Color FDD_TextWhite(255, 255, 255);
Color FDD_ErrorRed = Styles::CriticalRed;
Color FDD_Border = Styles::WindowBorder;

string errorMessage = "";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void UpdateControlVisibility(HWND hWnd) {
    int showLogin = (currentState == AppState::LOGIN) ? SW_SHOW : SW_HIDE;
    int showPaths = (currentState == AppState::SETUP_PATHS) ? SW_SHOW : SW_HIDE;
    int showHoppies = (currentState == AppState::SETUP_HOPPIES) ? SW_SHOW : SW_HIDE;

    ShowWindow(hEditCID, showLogin);
    ShowWindow(hEditPWD, showLogin);

    ShowWindow(hEditESPath, showPaths);
    ShowWindow(hEditVNaaats, showPaths);
    ShowWindow(hEditTopSky, showPaths);
    ShowWindow(hBtnBrowseES, showPaths);
    ShowWindow(hBtnBrowseVNaaats, showPaths);
    ShowWindow(hBtnBrowseTopSky, showPaths);

    ShowWindow(hEditHoppies, showHoppies);
}

string GetWindowTextStr(HWND hWnd) {
    int len = GetWindowTextLengthA(hWnd);
    if (len == 0) return "";
    string str(len, '\0');
    GetWindowTextA(hWnd, &str[0], len + 1);
    return str;
}

// Helper for professional drawing
void DrawRoundedButton(Graphics& g, Rect rect, Color color, string text, Font* font, Color textColor = Color::White) {
    SolidBrush brush(color);
    g.FillRectangle(&brush, rect);
    
    Pen pen(FDD_Border, 1);
    g.DrawRectangle(&pen, rect);

    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    
    SolidBrush textBrush(textColor);
    wstring wtext(text.begin(), text.end());
    RectF rectF((REAL)rect.X, (REAL)rect.Y, (REAL)rect.Width, (REAL)rect.Height);
    g.DrawString(wtext.c_str(), -1, font, rectF, &format, &textBrush);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    CConfigManager::LoadConfig();
    CDataBridge::Start();
    if (CConfigManager::GetConfig().isSetupComplete) {
        currentState = AppState::MENU;
    }

    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wcex.lpszClassName = "OceanicControllerClass";

    if (!RegisterClassEx(&wcex)) return 1;

    HWND hWnd = CreateWindow("OceanicControllerClass", "Oceanic Controller", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 650, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 1;

    // Create controls
    hEditCID = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 200, 175, 300, 25, hWnd, (HMENU)IDC_EDIT_CID, hInstance, NULL);
    hEditPWD = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_PASSWORD | ES_AUTOHSCROLL, 200, 235, 300, 25, hWnd, (HMENU)IDC_EDIT_PWD, hInstance, NULL);
    
    hEditESPath = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 200, 175, 450, 25, hWnd, (HMENU)IDC_EDIT_ESPATH, hInstance, NULL);
    hEditVNaaats = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 200, 235, 450, 25, hWnd, (HMENU)IDC_EDIT_VNAAATS, hInstance, NULL);
    hEditTopSky = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 200, 295, 450, 25, hWnd, (HMENU)IDC_EDIT_TOPSKY, hInstance, NULL);
    
    hBtnBrowseES = CreateWindowA("BUTTON", "...", WS_CHILD, 660, 175, 30, 25, hWnd, (HMENU)IDC_BTN_BROWSE_ES, hInstance, NULL);
    hBtnBrowseVNaaats = CreateWindowA("BUTTON", "...", WS_CHILD, 660, 235, 30, 25, hWnd, (HMENU)IDC_BTN_BROWSE_VNAAATS, hInstance, NULL);
    hBtnBrowseTopSky = CreateWindowA("BUTTON", "...", WS_CHILD, 660, 295, 30, 25, hWnd, (HMENU)IDC_BTN_BROWSE_TOPSKY, hInstance, NULL);

    hEditHoppies = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL, 200, 175, 300, 25, hWnd, (HMENU)IDC_EDIT_HOPPIES, hInstance, NULL);

    // Set initial values from config
    UserConfig& config = CConfigManager::GetConfig();
    SetWindowTextA(hEditCID, config.vatsimCID.c_str());
    SetWindowTextA(hEditPWD, config.vatsimPassword.c_str());
    SetWindowTextA(hEditESPath, config.euroScopePath.c_str());
    SetWindowTextA(hEditVNaaats, config.vNaaatsAsrPath.c_str());
    SetWindowTextA(hEditTopSky, config.topSkyAsrPath.c_str());
    SetWindowTextA(hEditHoppies, config.hoppiesCode.c_str());

    UpdateControlVisibility(hWnd);

    logoStartTime = clock();
    SetTimer(hWnd, 1, 2000, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    UserConfig& config = CConfigManager::GetConfig();

    switch (message) {
    case WM_TIMER:
        if (currentState == AppState::LOGO) {
            KillTimer(hWnd, 1);
            if (!config.isSetupComplete) currentState = AppState::LOGIN;
            else currentState = AppState::MENU;
            UpdateControlVisibility(hWnd);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    case WM_AUTH_COMPLETE:
        if (wParam == 1) {
            config.vatsimCID = CAuthManager::GetUserCID();
            errorMessage = "";
            currentState = AppState::SETUP_PATHS;
            UpdateControlVisibility(hWnd);
        } else {
            errorMessage = "Login failed or was cancelled.";
        }
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_BTN_BROWSE_ES || wmId == IDC_BTN_BROWSE_VNAAATS || wmId == IDC_BTN_BROWSE_TOPSKY) {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = (wmId == IDC_BTN_BROWSE_ES) ? "Executables\0*.exe\0All\0*.*\0" : "ASR Files\0*.asr\0All\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn)) {
                if (wmId == IDC_BTN_BROWSE_ES) SetWindowTextA(hEditESPath, szFile);
                else if (wmId == IDC_BTN_BROWSE_VNAAATS) SetWindowTextA(hEditVNaaats, szFile);
                else if (wmId == IDC_BTN_BROWSE_TOPSKY) SetWindowTextA(hEditTopSky, szFile);
            }
        }
    } break;
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        if (currentState == AppState::LOGIN) {
            if (x > 300 && x < 500 && y > 300 && y < 350) {
                errorMessage = "Authenticating in browser...";
                InvalidateRect(hWnd, NULL, TRUE);
                CAuthManager::InitiateLogin(hWnd);
            }
        } else if (currentState == AppState::SETUP_PATHS) {
            if (x > 300 && x < 500 && y > 450 && y < 500) {
                config.euroScopePath = GetWindowTextStr(hEditESPath);
                config.vNaaatsAsrPath = GetWindowTextStr(hEditVNaaats);
                config.topSkyAsrPath = GetWindowTextStr(hEditTopSky);
                if (config.euroScopePath.empty() || config.vNaaatsAsrPath.empty() || config.topSkyAsrPath.empty()) {
                    errorMessage = "All paths are required.";
                } else {
                    errorMessage = "";
                    currentState = AppState::SETUP_HOPPIES;
                    UpdateControlVisibility(hWnd);
                }
                InvalidateRect(hWnd, NULL, TRUE);
            }
        } else if (currentState == AppState::SETUP_HOPPIES) {
            if (x > 300 && x < 500 && y > 300 && y < 350) {
                config.hoppiesCode = GetWindowTextStr(hEditHoppies);
                if (config.hoppiesCode.empty()) {
                    errorMessage = "Hoppies code is required.";
                } else {
                    errorMessage = "";
                    config.isSetupComplete = true;
                    CConfigManager::SaveConfig();
                    currentState = AppState::MENU;
                    UpdateControlVisibility(hWnd);
                }
                InvalidateRect(hWnd, NULL, TRUE);
            }
        } else if (currentState == AppState::MENU) {
            // 4 Boxes
            if (y > 100 && y < 300) {
                if (x > 75 && x < 375) {
                    currentState = AppState::VNAAATS_HUB;
                    CLauncher::LaunchEuroScope(EControllerVersion::VNAAATS);
                    UpdateControlVisibility(hWnd);
                    InvalidateRect(hWnd, NULL, TRUE);
                }
                else if (x > 425 && x < 725) {
                    CLauncher::LaunchEuroScope(EControllerVersion::TOPSKY);
                    if (hCpdlcWnd == NULL) hCpdlcWnd = CCpdlcWindow::Create(hInst, hWnd);
                }
            } else if (y > 330 && y < 530) {
                if (x > 75 && x < 375) ShellExecute(NULL, "open", "https://nattrak.vatsim.net/", NULL, NULL, SW_SHOWNORMAL);
                else if (x > 425 && x < 725) ShellExecute(NULL, "open", "https://ganderoceanic.ca/", NULL, NULL, SW_SHOWNORMAL);
            }
            else if (x > 300 && x < 500 && y > 550 && y < 590) {
                currentState = AppState::LOGIN;
                config.isSetupComplete = false;
                UpdateControlVisibility(hWnd);
                InvalidateRect(hWnd, NULL, TRUE);
            }
        } else if (currentState == AppState::VNAAATS_HUB) {
            // 3 Boxes: FDD, CPDLC, EXIT
            if (y > 150 && y < 350) {
                if (x > 50 && x < 280) { // FDD
                    if (hFddWnd == NULL) hFddWnd = CFddWindow::Create(hInst, hWnd);
                    else SetForegroundWindow(hFddWnd);
                } else if (x > 310 && x < 540) { // CPDLC
                    if (hCpdlcWnd == NULL) hCpdlcWnd = CCpdlcWindow::Create(hInst, hWnd);
                    else SetForegroundWindow(hCpdlcWnd);
                } else if (x > 570 && x < 750) { // EXIT
                    if (MessageBoxA(hWnd, "Are you sure you want to close?", "Confirm Exit", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        PostQuitMessage(0);
                    }
                }
            }
            // Back button
            else if (x > 20 && x < 120 && y > 20 && y < 60) {
                currentState = AppState::MENU;
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
    } break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

        FontFamily fontFamily(L"Segoe UI");
        Font fontHeader(&fontFamily, 24, FontStyleBold, UnitPixel);
        Font fontSub(&fontFamily, 16, FontStyleRegular, UnitPixel);
        Font fontBtn(&fontFamily, 14, FontStyleBold, UnitPixel);

        if (currentState == AppState::LOGO) {
            StringFormat format;
            format.SetAlignment(StringAlignmentCenter);
            format.SetLineAlignment(StringAlignmentCenter);
            SolidBrush brush(FDD_TextWhite);
            graphics.DrawString(L"OCEANIC CONTROLLER", -1, &fontHeader, RectF(0, 0, 800, 500), &format, &brush);
            
            wstring wVer = L"v" + wstring(APP_VERSION, APP_VERSION + strlen(APP_VERSION)) + L" (Build " + to_wstring(BUILD_NUMBER) + L")";
            graphics.DrawString(wVer.c_str(), -1, &fontSub, RectF(0, 50, 800, 500), &format, &brush);
        } else if (currentState == AppState::LOGIN) {
            SolidBrush brush(FDD_TextWhite);
            graphics.DrawString(L"VATSIM Login", -1, &fontHeader, PointF(50, 50), &brush);
            graphics.DrawString(L"Click the button below to authenticate with VATSIM.", -1, &fontSub, PointF(50, 100), &brush);
            
            if (!errorMessage.empty()) {
                SolidBrush errBrush(FDD_ErrorRed);
                wstring werr(errorMessage.begin(), errorMessage.end());
                graphics.DrawString(werr.c_str(), -1, &fontSub, PointF(50, 200), &errBrush);
            }
            DrawRoundedButton(graphics, Rect(300, 300, 200, 50), FDD_AccentBlue, "Login with VATSIM", &fontBtn);
        } else if (currentState == AppState::SETUP_PATHS) {
            SolidBrush brush(FDD_TextWhite);
            graphics.DrawString(L"First Time Setup", -1, &fontHeader, PointF(50, 50), &brush);
            graphics.DrawString(L"Configure your EuroScope and Plugin paths.", -1, &fontSub, PointF(50, 100), &brush);
            graphics.DrawString(L"EuroScope.exe:", -1, &fontSub, PointF(50, 180), &brush);
            graphics.DrawString(L"vNAAATS .asr:", -1, &fontSub, PointF(50, 240), &brush);
            graphics.DrawString(L"TopSky .asr:", -1, &fontSub, PointF(50, 300), &brush);

            if (!errorMessage.empty()) {
                SolidBrush errBrush(FDD_ErrorRed);
                wstring werr(errorMessage.begin(), errorMessage.end());
                graphics.DrawString(werr.c_str(), -1, &fontSub, PointF(200, 350), &errBrush);
            }
            DrawRoundedButton(graphics, Rect(300, 450, 200, 50), FDD_AccentBlue, "Next Step", &fontBtn);
        } else if (currentState == AppState::SETUP_HOPPIES) {
            SolidBrush brush(FDD_TextWhite);
            graphics.DrawString(L"CPDLC Setup", -1, &fontHeader, PointF(50, 50), &brush);
            graphics.DrawString(L"Enter your Hoppies CPDLC code.", -1, &fontSub, PointF(50, 100), &brush);
            graphics.DrawString(L"Hoppies Code:", -1, &fontSub, PointF(50, 180), &brush);

            if (!errorMessage.empty()) {
                SolidBrush errBrush(FDD_ErrorRed);
                wstring werr(errorMessage.begin(), errorMessage.end());
                graphics.DrawString(werr.c_str(), -1, &fontSub, PointF(200, 220), &errBrush);
            }
            DrawRoundedButton(graphics, Rect(300, 300, 200, 50), FDD_AccentBlue, "Complete Setup", &fontBtn);
        } else if (currentState == AppState::MENU) {
            SolidBrush brush(FDD_TextWhite);
            graphics.DrawString(L"Oceanic Controller Hub", -1, &fontHeader, PointF(50, 30), &brush);

            DrawRoundedButton(graphics, Rect(75, 100, 300, 200), FDD_BoxBG, "vNAAATS", &fontBtn, Color::Black);
            DrawRoundedButton(graphics, Rect(425, 100, 300, 200), FDD_BoxBG, "TopSky", &fontBtn, Color::Black);
            DrawRoundedButton(graphics, Rect(75, 330, 300, 200), FDD_BoxBG, "NatTrak", &fontBtn, Color::Black);
            DrawRoundedButton(graphics, Rect(425, 330, 300, 200), FDD_BoxBG, "Gander Oceanic", &fontBtn, Color::Black);

            DrawRoundedButton(graphics, Rect(300, 550, 200, 40), FDD_BoxBG, "Setup / Settings", &fontBtn, Color::Black);
        } else if (currentState == AppState::VNAAATS_HUB) {
            SolidBrush brush(FDD_TextWhite);
            graphics.DrawString(L"vNAAATS Controller Suite", -1, &fontHeader, PointF(50, 30), &brush);

            DrawRoundedButton(graphics, Rect(50, 150, 230, 200), FDD_BoxBG, "FDD", &fontBtn, Color::Black);
            DrawRoundedButton(graphics, Rect(310, 150, 230, 200), FDD_BoxBG, "CPDLC", &fontBtn, Color::Black);
            DrawRoundedButton(graphics, Rect(570, 150, 180, 200), FDD_ErrorRed, "EXIT", &fontBtn, Color::White);

            DrawRoundedButton(graphics, Rect(20, 20, 100, 40), FDD_BoxBG, "<- Back", &fontBtn, Color::Black);
        }
        
        EndPaint(hWnd, &ps);
    } break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}





