#include "FddWindow.h"
#include "Version.h"
#include "DataBridge.h"
#include "Styles.h"
#include <vector>
#include <map>
#include <algorithm>

using namespace Gdiplus;

// Static members
HWND CFddWindow::hEditFindAC = NULL;
HWND CFddWindow::hEditSelcal = NULL;
HWND CFddWindow::hEditTMI = NULL;

string CFddWindow::dropdownCallsign = "";
string CFddWindow::dropdownField = "";
Rect CFddWindow::dropdownRect(0, 0, 0, 0);
vector<string> CFddWindow::dropdownItems;
string CFddWindow::selectedCallsign = "";
vector<string> CFddWindow::discardedCallsigns;

FontFamily* CFddWindow::fontFamily = NULL;
Font* CFddWindow::fontHeader = NULL;
Font* CFddWindow::fontBtn = NULL;
Font* CFddWindow::fontTrack = NULL;
Font* CFddWindow::fontMain = NULL;
Font* CFddWindow::fontSub = NULL;
Font* CFddWindow::fontWp = NULL;
Font* CFddWindow::fontTime = NULL;

void CFddWindow::InitResources() {
    if (fontFamily) return;
    fontFamily = new FontFamily(L"Segoe UI");
    FontFamily* monoFamily = new FontFamily(L"Consolas");
    if (monoFamily->GetLastStatus() != Ok) {
        delete monoFamily;
        monoFamily = new FontFamily(L"Courier New");
    }

    fontHeader = new Font(fontFamily, 14, FontStyleBold, UnitPixel);
    fontBtn = new Font(fontFamily, 11, FontStyleBold, UnitPixel);
    fontTrack = new Font(fontFamily, 16, FontStyleBold, UnitPixel);
    fontMain = new Font(fontFamily, 16, FontStyleBold, UnitPixel);
    fontSub = new Font(fontFamily, 13, FontStyleRegular, UnitPixel);
    fontWp = new Font(monoFamily, 14, FontStyleBold, UnitPixel);
    fontTime = new Font(monoFamily, 15, FontStyleBold, UnitPixel);
    
    delete monoFamily;
}

void CFddWindow::FreeResources() {
    delete fontHeader; fontHeader = NULL;
    delete fontBtn; fontBtn = NULL;
    delete fontTrack; fontTrack = NULL;
    delete fontMain; fontMain = NULL;
    delete fontSub; fontSub = NULL;
    delete fontWp; fontWp = NULL;
    delete fontTime; fontTime = NULL;
    delete fontFamily; fontFamily = NULL;
}

// Control IDs
#define IDC_FDD_FIND_AC 2001
#define IDC_FDD_SELCAL 2002
#define IDC_FDD_TMI 2003
#define IDC_FDD_BTN_FIND 2004
#define IDC_FDD_BTN_SELCAL 2005
#define IDC_FDD_BTN_NATTRAK 2006
#define IDC_FDD_BTN_DISCARD 2007

// Colors matching FDD plugin
const Color FDD_BG = Styles::DarkBackground;
const Color FDD_BORDER = Styles::WindowBorder;
const Color FDD_WEST_HEADER = Styles::FddWestHeader;
const Color FDD_EAST_HEADER = Styles::FddEastHeader;
const Color FDD_WEST_STRIP = Styles::FddWestStrip;
const Color FDD_EAST_STRIP = Styles::FddEastStrip;
const Color FDD_TEXT_WHITE(255, 255, 255);
const Color FDD_TEXT_BLACK(0, 0, 0);

int scrollPos = 0;
int maxScroll = 0;

HWND CFddWindow::Create(HINSTANCE hInstance, HWND hParent) {
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(20, 20, 20));
    wcex.lpszClassName = "FddWindowClass";

    RegisterClassEx(&wcex);
    InitResources();

    string title = "FDD - Flight Data Display v" + string(APP_VERSION) + " (Build " + to_string(BUILD_NUMBER) + ")";
    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, "FddWindowClass", title.c_str(), WS_OVERLAPPEDWINDOW | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700, hParent, NULL, hInstance, NULL);

    // Create Header Controls (Positioned but will be drawn over/integrated)
    hEditTMI = CreateWindowExA(0, "EDIT", "039", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL, 225, 17, 35, 20, hWnd, (HMENU)IDC_FDD_TMI, hInstance, NULL);
    hEditFindAC = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_UPPERCASE | ES_AUTOHSCROLL, 10, 10, 1, 1, hWnd, (HMENU)IDC_FDD_FIND_AC, hInstance, NULL); // Hidden edit
    hEditSelcal = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_UPPERCASE | ES_AUTOHSCROLL, 520, 17, 50, 20, hWnd, (HMENU)IDC_FDD_SELCAL, hInstance, NULL);

    // No more standard Win32 buttons in the header
    SetTimer(hWnd, 1, 1000, NULL); 

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return hWnd;
}

LRESULT CALLBACK CFddWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_FDD_BTN_FIND) {
            char buf[32];
            GetWindowTextA(hEditFindAC, buf, 32);
            string searchCs = buf;
            if (!searchCs.empty()) {
                CDataBridge::SendCommand("SELECT", searchCs);
                // Search in local list to scroll
                auto data = CDataBridge::GetProcessedData();
                if (data) {
                    int currentY = 10;
                    bool found = false;
                    for (auto& id : data->trackIds) {
                        currentY += 35; // Header
                        for (auto& ac : data->tracks[id]) {
                            if (ac.callsign == searchCs) {
                                scrollPos = max(0, currentY - 100);
                                found = true;
                                break;
                            }
                            currentY += 50;
                        }
                        if (found) break;
                        currentY += 15;
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
        } else if (wmId == IDC_FDD_BTN_SELCAL) {
            char buf[32];
            GetWindowTextA(hEditSelcal, buf, 32);
            string val = buf;
            if (!val.empty()) {
                if (!selectedCallsign.empty()) {
                    // Update the selected aircraft's SELCAL
                    CDataBridge::SendUpdate(selectedCallsign, "SELCAL", val);
                } else {
                    // Original behavior: search by SELCAL
                    auto data = CDataBridge::GetProcessedData();
                    if (data) {
                        for (auto const& [id, list] : data->tracks) {
                            for (auto& ac : list) {
                                if (ac.selcal == val) {
                                    CDataBridge::SendCommand("SELECT", ac.callsign);
                                    selectedCallsign = ac.callsign;
                                    InvalidateRect(hWnd, NULL, FALSE);
                                    return 0;
                                }
                            }
                        }
                    }
                }
            }
        } else if (wmId == IDC_FDD_BTN_NATTRAK) {
            ShellExecute(NULL, "open", "https://nattrak.vatsim.net/", NULL, NULL, SW_SHOWNORMAL);
        } else if (wmId == IDC_FDD_BTN_DISCARD) {
            if (!selectedCallsign.empty()) {
                string msg = "Confirm Discard this " + selectedCallsign + " strip?";
                if (MessageBoxA(hWnd, msg.c_str(), "Confirm Discard", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    discardedCallsigns.push_back(selectedCallsign);
                    CDataBridge::SendCommand("DISCARD", selectedCallsign);
                    selectedCallsign = "";
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            } else {
                MessageBoxA(hWnd, "Please select a flight strip first.", "No Selection", MB_OK | MB_ICONINFORMATION);
            }
        }
    } break;
    case WM_TIMER:
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (!dropdownCallsign.empty()) {
            // Dropdown scroll could be added here if needed
            return 0;
        }
        scrollPos -= (delta / 120) * 40;
        if (scrollPos < 0) scrollPos = 0;
        if (scrollPos > maxScroll) scrollPos = maxScroll;
        InvalidateRect(hWnd, NULL, FALSE);
    } break;
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // 1. Dropdown Interactions (Highest Priority)
        if (!dropdownItems.empty()) {
            if (dropdownRect.Contains(x, y)) {
                int itemIndex = (y - dropdownRect.Y) / 25;
                if (itemIndex >= 0 && itemIndex < (int)dropdownItems.size()) {
                    string selectedValue = dropdownItems[itemIndex];
                    if (dropdownField == "FL") {
                        CDataBridge::SendCommand("SET_TEMP_FL", dropdownCallsign, selectedValue);
                    } else if (dropdownField == "MACH") {
                        // Strip the '.' if present
                        string cleanVal = "";
                        for (char c : selectedValue) if (isdigit(c)) cleanVal += c;
                        CDataBridge::SendCommand("SET_TEMP_MACH", dropdownCallsign, cleanVal);
                    }
                }
            }
            dropdownItems.clear();
            dropdownCallsign = "";
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int width = clientRect.right - clientRect.left;

        // Header Buttons Hit Testing
        if (y >= 8 && y <= 46) {
            float rX = (float)width - 30;
            
            // Connected (Status only, no action)
            rX -= 90; 
            
            // CPDLC
            rX -= 75;
            if (x >= rX && x <= rX + 70) {
                CDataBridge::SendCommand("OPEN_CPDLC", "SYSTEM");
                return 0;
            }

            // NATTRAK
            rX -= 90;
            if (x >= rX && x <= rX + 85) {
                ShellExecute(NULL, "open", "https://nattrak.vatsim.net/", NULL, NULL, SW_SHOWNORMAL);
                return 0;
            }

            // SELCAL
            rX -= 75;
            if (x >= rX && x <= rX + 70) {
                SendMessage(hWnd, WM_COMMAND, IDC_FDD_BTN_SELCAL, 0);
                return 0;
            }

            // TRACK
            rX -= 70;
            if (x >= rX && x <= rX + 65) {
                if (!selectedCallsign.empty()) {
                    CDataBridge::SendCommand("TRACK", selectedCallsign);
                }
                return 0;
            }

            // Find AC
            rX -= 75;
            if (x >= rX && x <= rX + 70) {
                SendMessage(hWnd, WM_COMMAND, IDC_FDD_BTN_FIND, 0);
                return 0;
            }
        }

        if (y < 55) {
            return 0;
        }

        // 3. Scrolling Content Interactions
        int scrolledY = y + scrollPos - 55; // Account for fixed header

        auto data = CDataBridge::GetProcessedData();
        if (!data) return 0;

        int currentY = 10;
        for (size_t i = 0; i < data->trackIds.size(); ++i) {
            string trackId = data->trackIds[i];
            
            vector<AircraftData> filteredTracks;
            for (const auto& ac : data->tracks[trackId]) {
                if (find(discardedCallsigns.begin(), discardedCallsigns.end(), ac.callsign) == discardedCallsigns.end()) {
                    filteredTracks.push_back(ac);
                }
            }
            if (filteredTracks.empty()) continue;

            currentY += 35; // Header
            for (size_t j = 0; j < filteredTracks.size(); ++j) {
                const AircraftData& ac = filteredTracks[j];
                Rect stripRect(5, currentY, width - 20, 45);
                if (stripRect.Contains(x, scrolledY)) {
                    selectedCallsign = ac.callsign;
                    
                    if (x >= stripRect.X + 180 && x <= stripRect.X + 240) {
                        // Show FL Dropdown
                        dropdownCallsign = ac.callsign;
                        dropdownField = "FL";
                        dropdownItems.clear();
                        for (int fl = 300; fl <= 600; fl += 10) dropdownItems.push_back(to_string(fl));
                        dropdownRect = Rect(x - 30, y, 60, min(300, (int)dropdownItems.size() * 25)); // Cap height
                        if (dropdownRect.Y + dropdownRect.Height > clientRect.bottom) {
                            dropdownRect.Y = clientRect.bottom - dropdownRect.Height;
                        }
                    } else if (x >= stripRect.X + 245 && x <= stripRect.X + 305) {
                        // Show Mach Dropdown
                        dropdownCallsign = ac.callsign;
                        dropdownField = "MACH";
                        dropdownItems.clear();
                        for (int m = 75; m <= 99; m++) dropdownItems.push_back("." + to_string(m));
                        dropdownItems.push_back("1.00");
                        dropdownRect = Rect(x - 30, y, 60, min(300, (int)dropdownItems.size() * 25)); // Cap height
                        if (dropdownRect.Y + dropdownRect.Height > clientRect.bottom) {
                            dropdownRect.Y = clientRect.bottom - dropdownRect.Height;
                        }
                    } else if (x >= stripRect.X + 310 && x <= stripRect.X + 380) {
                        // Edit SELCAL - Pop up the header edit box or just focus it
                        selectedCallsign = ac.callsign;
                        SetWindowTextA(hEditSelcal, ac.selcal.c_str());
                        SetFocus(hEditSelcal);
                        // Show a tooltip or hint? 
                        InvalidateRect(hWnd, NULL, FALSE);
                    } else {
                        CDataBridge::SendCommand("SELECT", ac.callsign);
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                currentY += 50;
            }
            currentY += 15;
        }
    } break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        ShowWindow(hEditSelcal, selectedCallsign.empty() ? SW_HIDE : SW_SHOW);

        RECT rect;
        if (GetClientRect(hWnd, &rect) && rect.right > 0 && rect.bottom > 0) {
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

            Graphics graphics(memDC);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
            
            Render(hWnd, graphics);

            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
        }
        EndPaint(hWnd, &ps);
    } break;
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        HWND hwndEdit = (HWND)lParam;
        if (hwndEdit == hEditTMI) {
            SetTextColor(hdcEdit, RGB(255, 255, 255));
            SetBkColor(hdcEdit, RGB(20, 100, 20));
            static HBRUSH hBrushTMI = CreateSolidBrush(RGB(20, 100, 20));
            return (LRESULT)hBrushTMI;
        }
    } break;
    case WM_CLOSE: {
        if (MessageBoxA(hWnd, "Are you sure you want to close FDD?", "Confirm Close", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            DestroyWindow(hWnd);
        }
    } return 0;
    case WM_DESTROY:
        FreeResources();
        KillTimer(hWnd, 1);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CFddWindow::DrawDropdown(Graphics& g, Rect rect, vector<string> items) {
    SolidBrush bgBrush(Color(40, 40, 40));
    SolidBrush textBrush(Color::White);
    SolidBrush hoverBrush(Color(60, 60, 60));
    Pen borderPen(FDD_BORDER, 1);

    g.FillRectangle(&bgBrush, rect);
    g.DrawRectangle(&borderPen, rect);

    int itemHeight = 25;
    for (size_t i = 0; i < items.size(); ++i) {
        Rect itemRect(rect.X, rect.Y + (int)i * itemHeight, rect.Width, itemHeight);
        
        wstring wItem(items[i].begin(), items[i].end());
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        
        g.DrawString(wItem.c_str(), -1, fontSub, RectF((REAL)itemRect.X, (REAL)itemRect.Y, (REAL)itemRect.Width, (REAL)itemRect.Height), &format, &textBrush);
        g.DrawLine(&borderPen, itemRect.X, itemRect.Y + itemHeight, itemRect.X + itemRect.Width, itemRect.Y + itemHeight);
    }
}

void CFddWindow::Render(HWND hWnd, Graphics& g) {
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    SolidBrush bgBrush(FDD_BG);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // --- Modern Header (Matching Image) ---
    SolidBrush headerOuterBg(Color(20, 22, 32));
    g.FillRectangle(&headerOuterBg, 0, 0, width, 55);
    
    // Header Container (Squared area)
    SolidBrush headerInnerBg(Color(32, 35, 48));
    Pen headerBorder(Color(60, 60, 70), 1);
    g.FillRectangle(&headerInnerBg, 10, 8, width - 20, 38);
    g.DrawRectangle(&headerBorder, 10, 8, width - 20, 38);

    SolidBrush whiteText(Color(255, 255, 255));
    SolidBrush greyText(Color(180, 180, 195));
    
    // Left side: Title
    g.DrawString(L"Flight Data Display", -1, fontHeader, PointF(25, 16), &whiteText);
    
    // TMI Box
    SolidBrush tmiBrush(Color(30, 85, 55));
    Pen tmiBorder(Color(45, 110, 75), 1);
    g.FillRectangle(&tmiBrush, 185, 14, 85, 26);
    g.DrawRectangle(&tmiBorder, 185, 14, 85, 26);
    g.DrawString(L"TMI:", -1, fontBtn, PointF(192, 18), &whiteText);
    // TMI value is handled by hEditTMI which is at 225, 17
    
    // Selection Info Area
    if (!selectedCallsign.empty()) {
        SolidBrush selBg(Color(45, 50, 70));
        Pen selBorder(Color(80, 85, 105), 1);
        g.FillRectangle(&selBg, 360, 14, 220, 26);
        g.DrawRectangle(&selBorder, 360, 14, 220, 26);
        
        wstring wSel = L"SEL: " + wstring(selectedCallsign.begin(), selectedCallsign.end());
        g.DrawString(wSel.c_str(), -1, fontBtn, PointF(368, 18), &whiteText);
        
        g.DrawString(L"SELCAL:", -1, fontBtn, PointF(465, 18), &greyText);
        // hEditSelcal will be positioned at 515, 17
    }

    // Time
    time_t now = time(0);
    tm* gmt = gmtime(&now);
    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%MZ", gmt);
    wstring wTime(timeStr, timeStr + strlen(timeStr));
    wTime += L"  B" + to_wstring(BUILD_NUMBER);
    g.DrawString(wTime.c_str(), -1, fontBtn, PointF(285, 18), &greyText);

    // Right side: Buttons
    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);
    center.SetLineAlignment(StringAlignmentCenter);

    // Button colors
    SolidBrush btnDark(Color(42, 48, 62));
    SolidBrush btnBlue(Color(25, 40, 105));
    SolidBrush btnOrange(Color(215, 110, 32));
    SolidBrush btnGreen(Color(25, 80, 50));
    Pen btnBorder(Color(65, 75, 95), 1);

    // Positions (Right to Left)
    float rX = (float)width - 30;
    
    // Connected
    rX -= 90;
    g.FillRectangle(&btnGreen, rX, 14.0f, 85.0f, 26.0f);
    g.DrawRectangle(&btnBorder, rX, 14.0f, 85.0f, 26.0f);
    g.DrawString(CDataBridge::IsConnected() ? L"Connected" : L"Disconnected", -1, fontBtn, RectF(rX, 14, 85, 26), &center, &whiteText);

    // CPDLC
    rX -= 75;
    g.FillRectangle(&btnOrange, rX, 14.0f, 70.0f, 26.0f);
    g.DrawRectangle(&btnBorder, rX, 14.0f, 70.0f, 26.0f);
    g.DrawString(L"CPDLC", -1, fontBtn, RectF(rX, 14, 70, 26), &center, &whiteText);

    // NATTRAK
    rX -= 90;
    g.FillRectangle(&btnBlue, rX, 14.0f, 85.0f, 26.0f);
    g.DrawRectangle(&btnBorder, rX, 14.0f, 85.0f, 26.0f);
    g.DrawString(L"NATTRAK", -1, fontBtn, RectF(rX, 14, 85, 26), &center, &greyText);

    // SELCAL
    rX -= 75;
    g.FillRectangle(&btnDark, rX, 14.0f, 70.0f, 26.0f);
    g.DrawRectangle(&btnBorder, rX, 14.0f, 70.0f, 26.0f);
    g.DrawString(L"SELCAL", -1, fontBtn, RectF(rX, 14, 70, 26), &center, &greyText);

    // TRACK
    rX -= 70;
    g.FillRectangle(&btnDark, rX, 14.0f, 65.0f, 26.0f);
    g.DrawRectangle(&btnBorder, rX, 14.0f, 65.0f, 26.0f);
    g.DrawString(L"TRACK", -1, fontBtn, RectF(rX, 14, 65, 26), &center, &greyText);

    // Find AC
    rX -= 75;
    g.FillRectangle(&btnDark, rX, 14.0f, 70.0f, 26.0f);
    g.DrawRectangle(&btnBorder, rX, 14.0f, 70.0f, 26.0f);
    g.DrawString(L"Find AC", -1, fontBtn, RectF(rX, 14, 70, 26), &center, &whiteText);

    // --- Scrolling Content ---
    g.TranslateTransform(0, 55); // Offset for header
    g.TranslateTransform(0, (REAL)-scrollPos);

    auto data = CDataBridge::GetProcessedData();
    if (!data || data->trackIds.empty()) {
        SolidBrush statusTextBrush(Color(100, 100, 100));
        g.DrawString(L"Waiting for EuroScope data...", -1, fontHeader, RectF(0, 0, (REAL)width, (REAL)height - 40), &center, &statusTextBrush);
        return;
    }

    int y = 10;
    StringFormat format;
    format.SetLineAlignment(StringAlignmentCenter);

    for (size_t i = 0; i < data->trackIds.size(); ++i) {
        string trackId = data->trackIds[i];
        
        vector<AircraftData> filteredTracks;
        for (const auto& ac : data->tracks[trackId]) {
            if (find(discardedCallsigns.begin(), discardedCallsigns.end(), ac.callsign) == discardedCallsigns.end()) {
                filteredTracks.push_back(ac);
            }
        }
        if (filteredTracks.empty()) continue;

        bool isWest = filteredTracks[0].isWestbound;
        
        SolidBrush trackHeaderBrush(isWest ? FDD_WEST_HEADER : FDD_EAST_HEADER);
        g.FillRectangle(&trackHeaderBrush, 5, y, width - 20, 30);
        
        wstring wTrack(trackId.begin(), trackId.end());
        if (wTrack == L"RR" || wTrack == L"Random Routes") wTrack = L"RANDOM ROUTING";
        else wTrack = L"Track " + wTrack;
        
        g.DrawString(wTrack.c_str(), -1, fontTrack, RectF(15, (REAL)y, 250, 30), &format, &whiteText);
        
        // Draw Waypoint Names in Header (from first aircraft)
        if (!filteredTracks.empty()) {
            int wpX = 400;
            for (const auto& wp : filteredTracks[0].route) {
                if (wpX + 75 > width - 20) break;
                wstring wName(wp.name.begin(), wp.name.end());
                g.DrawString(wName.c_str(), -1, fontWp, PointF((REAL)wpX, (REAL)y + 8), &whiteText);
                wpX += 75;
            }
        }

        wstring wCount = to_wstring(filteredTracks.size()) + L" aircraft";
        StringFormat rightAlign;
        rightAlign.SetAlignment(StringAlignmentFar);
        rightAlign.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(wCount.c_str(), -1, fontTrack, RectF((REAL)width - 150, (REAL)y, 130, 30), &rightAlign, &whiteText);

        y += 35;

        for (size_t j = 0; j < filteredTracks.size(); ++j) {
            DrawStrip(g, Rect(5, y, width - 20, 45), filteredTracks[j], filteredTracks[j].callsign == selectedCallsign ? 1 : 0);
            y += 50;
        }
        y += 15;
    }

    // --- Draw active dropdown last so it's on top ---
    if (!dropdownItems.empty()) {
        g.ResetTransform();
        DrawDropdown(g, dropdownRect, dropdownItems);
    }

    maxScroll = max(0, y - height + 80);
}

void CFddWindow::DrawRoundedRect(Graphics& g, Pen* pen, Brush* brush, RectF rect, float radius) {
    GraphicsPath path;
    path.AddArc(rect.X, rect.Y, radius * 2, radius * 2, 180, 90);
    path.AddArc(rect.X + rect.Width - radius * 2, rect.Y, radius * 2, radius * 2, 270, 90);
    path.AddArc(rect.X + rect.Width - radius * 2, rect.Y + rect.Height - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();
    
    if (brush) g.FillPath(brush, &path);
    if (pen) g.DrawPath(pen, &path);
}

void CFddWindow::DrawStrip(Graphics& g, Rect rect, const AircraftData& ac, int status) {
    SolidBrush stripBrush(ac.isWestbound ? FDD_WEST_STRIP : FDD_EAST_STRIP);
    g.FillRectangle(&stripBrush, rect);

    Pen borderPen(status == 1 ? Color::Lime : Color(80, 80, 80), status == 1 ? 3 : 1); // Thicker selection
    g.DrawRectangle(&borderPen, rect);

    // NatTrak Status Indicator (Left Edge)
    Color statusColor = Color::Gray;
    if (ac.natStatus == 1) statusColor = Color::Yellow; // PENDING
    else if (ac.natStatus == 2) statusColor = Color::Lime; // CLEARED
    else if (ac.natStatus == 0) statusColor = Color::Red; // UNKNOWN
    SolidBrush statusBrush(statusColor);
    g.FillRectangle(&statusBrush, rect.X, rect.Y, 5, rect.Height);

    SolidBrush textBrush(FDD_TEXT_BLACK);
    SolidBrush whiteTextBrush(FDD_TEXT_WHITE);

    // Callsign & Type
    wstring wCs(ac.callsign.begin(), ac.callsign.end());
    wstring wType(ac.type.begin(), ac.type.end());
    g.DrawString(wCs.c_str(), -1, fontMain, PointF((REAL)rect.X + 10, (REAL)rect.Y + 5), &textBrush);
    g.DrawString(wType.c_str(), -1, fontSub, PointF((REAL)rect.X + 10, (REAL)rect.Y + 24), &textBrush);

    // Route
    wstring wRoute = wstring(ac.dep.begin(), ac.dep.end()) + L"/" + wstring(ac.dest.begin(), ac.dest.end());
    g.DrawString(wRoute.c_str(), -1, fontSub, PointF((REAL)rect.X + 100, (REAL)rect.Y + 14), &textBrush);
    
    // Add grey text brush for placeholders
    SolidBrush greyTextBrush(Color(180, 180, 195));

    // FL Box
    SolidBrush boxBrush(Color::Black);
    g.FillRectangle(&boxBrush, rect.X + 180, rect.Y + 7, 60, 30);
    g.DrawRectangle(&borderPen, rect.X + 180, rect.Y + 7, 60, 30);
    
    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);
    center.SetLineAlignment(StringAlignmentCenter);
    wstring wFL = ac.fl.empty() || ac.fl == "000" ? L"FL" : wstring(ac.fl.begin(), ac.fl.end());
    // Only gray out if it's the placeholder "FL"
    Brush* flBrush = (wFL == L"FL") ? &greyTextBrush : &whiteTextBrush;
    g.DrawString(wFL.c_str(), -1, fontMain, RectF((REAL)rect.X + 180, (REAL)rect.Y + 7, 60, 30), &center, flBrush);

    // Mach Box
    g.FillRectangle(&boxBrush, rect.X + 245, rect.Y + 7, 60, 30);
    g.DrawRectangle(&borderPen, rect.X + 245, rect.Y + 7, 60, 30);
    
    wstring wMach;
    bool isMachPlaceholder = false;
    if (ac.mach.empty() || ac.mach == "000") {
        wMach = L"MACH";
        isMachPlaceholder = true;
    } else {
        try {
            int m = stoi(ac.mach);
            if (m > 0 && m < 100) {
                wMach = L"." + to_wstring(m);
            } else if (m == 100) {
                wMach = L"1.00";
            } else {
                wMach = to_wstring(m); // Knots or already formatted
            }
        } catch (...) {
            wMach = wstring(ac.mach.begin(), ac.mach.end());
        }
    }
    Brush* machBrush = isMachPlaceholder ? &greyTextBrush : &whiteTextBrush;
    g.DrawString(wMach.c_str(), -1, fontMain, RectF((REAL)rect.X + 245, (REAL)rect.Y + 7, 60, 30), &center, machBrush);

    // SELCAL Box
    g.FillRectangle(&boxBrush, rect.X + 310, rect.Y + 7, 70, 30);
    g.DrawRectangle(&borderPen, rect.X + 310, rect.Y + 7, 70, 30);
    wstring wSelcal = ac.selcal.empty() ? L"SELCAL" : wstring(ac.selcal.begin(), ac.selcal.end());
    Brush* selcalBrush = (wSelcal == L"SELCAL") ? &greyTextBrush : &whiteTextBrush;
    g.DrawString(wSelcal.c_str(), -1, fontMain, RectF((REAL)rect.X + 310, (REAL)rect.Y + 7, 70, 30), &center, selcalBrush);

    // Waypoints (Estimates only, aligned with header)
    int wpX = rect.X + 400;
    for (size_t i = 0; i < ac.route.size(); ++i) {
        if (wpX + 75 > rect.X + rect.Width) break;
        
        wstring wTime(ac.route[i].estimate.begin(), ac.route[i].estimate.end());
        
        // Vertically center the time in the strip using the new bold robotic font
        g.DrawString(wTime.c_str(), -1, fontTime, PointF((REAL)wpX, (REAL)rect.Y + 12), &textBrush);
        wpX += 75;
    }
}
