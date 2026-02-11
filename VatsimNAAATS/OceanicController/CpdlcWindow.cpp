#include "CpdlcWindow.h"
#include "DataBridge.h"
#include "Styles.h"
#include <algorithm>

using namespace Gdiplus;

// Colors matching CPDLC plugin
const Color CPDLC_BG = Styles::DarkBackground;
const Color CPDLC_PANEL(40, 40, 40);
const Color CPDLC_BORDER = Styles::WindowBorder;
const Color CPDLC_TEXT_WHITE(255, 255, 255);
const Color CPDLC_ACCENT_BLUE = Styles::ScreenBlue;

HWND CCpdlcWindow::Create(HINSTANCE hInstance, HWND hParent) {
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wcex.lpszClassName = "CpdlcWindowClass";

    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, "CpdlcWindowClass", "CPDLC - Controller Pilot Data Link Communication", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, hParent, NULL, hInstance, NULL);

    SetTimer(hWnd, 1, 1000, NULL);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return hWnd;
}

LRESULT CALLBACK CCpdlcWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        RECT rect;
        GetClientRect(hWnd, &rect);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        SelectObject(memDC, memBitmap);

        Graphics graphics(memDC);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
        
        Render(hWnd, graphics);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
    } break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CCpdlcWindow::Render(HWND hWnd, Graphics& g) {
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    SolidBrush bgBrush(CPDLC_BG);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Render Panels
    RenderAircraftList(g, Rect(10, 10, 200, height - 150));
    RenderMessages(g, Rect(220, 10, width - 230, height - 150));
    RenderComposeArea(g, Rect(10, height - 130, width - 20, 120));
}

void CCpdlcWindow::RenderAircraftList(Graphics& g, Rect rect) {
    SolidBrush panelBrush(CPDLC_PANEL);
    g.FillRectangle(&panelBrush, rect);
    
    Pen borderPen(CPDLC_BORDER, 1);
    g.DrawRectangle(&borderPen, rect);

    FontFamily fontFamily(L"Segoe UI");
    Font fontHeader(&fontFamily, 14, FontStyleBold, UnitPixel);
    Font fontText(&fontFamily, 13, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(CPDLC_TEXT_WHITE);

    g.DrawString(L"AIRCRAFT", -1, &fontHeader, PointF((REAL)rect.X + 5, (REAL)rect.Y + 5), &textBrush);

    auto data = CDataBridge::GetProcessedData();
    if (!data) return;

    int y = rect.Y + 30;
    for (auto const& [id, list] : data->tracks) {
        for (auto const& ac : list) {
            if (y + 20 > rect.Y + rect.Height) break;
            wstring wCs(ac.callsign.begin(), ac.callsign.end());
            g.DrawString(wCs.c_str(), -1, &fontText, PointF((REAL)rect.X + 10, (REAL)y), &textBrush);
            y += 20;
        }
    }
}

void CCpdlcWindow::RenderMessages(Graphics& g, Rect rect) {
    SolidBrush panelBrush(CPDLC_PANEL);
    g.FillRectangle(&panelBrush, rect);
    
    Pen borderPen(CPDLC_BORDER, 1);
    g.DrawRectangle(&borderPen, rect);

    FontFamily fontFamily(L"Segoe UI");
    Font fontHeader(&fontFamily, 14, FontStyleBold, UnitPixel);
    Font fontText(&fontFamily, 13, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(CPDLC_TEXT_WHITE);

    g.DrawString(L"MESSAGES", -1, &fontHeader, PointF((REAL)rect.X + 5, (REAL)rect.Y + 5), &textBrush);

    // Dummy messages
    g.DrawString(L"BAW123: LOGON REQUEST", -1, &fontText, PointF((REAL)rect.X + 10, (REAL)rect.Y + 30), &textBrush);
    g.DrawString(L"SYS: CONNECTED TO CZQX", -1, &fontText, PointF((REAL)rect.X + 10, (REAL)rect.Y + 50), &textBrush);
}

void CCpdlcWindow::RenderComposeArea(Graphics& g, Rect rect) {
    SolidBrush panelBrush(CPDLC_PANEL);
    g.FillRectangle(&panelBrush, rect);
    
    Pen borderPen(CPDLC_BORDER, 1);
    g.DrawRectangle(&borderPen, rect);

    FontFamily fontFamily(L"Segoe UI");
    Font fontHeader(&fontFamily, 14, FontStyleBold, UnitPixel);
    SolidBrush textBrush(CPDLC_TEXT_WHITE);

    g.DrawString(L"COMPOSE MESSAGE", -1, &fontHeader, PointF((REAL)rect.X + 5, (REAL)rect.Y + 5), &textBrush);
    
    // Send button placeholder
    SolidBrush accentBrush(CPDLC_ACCENT_BLUE);
    g.FillRectangle(&accentBrush, rect.X + rect.Width - 110, rect.Y + 45, 100, 35);
    g.DrawRectangle(&borderPen, rect.X + rect.Width - 110, rect.Y + 45, 100, 35);
    
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"SEND", -1, &fontHeader, RectF((REAL)rect.X + rect.Width - 110, (REAL)rect.Y + 45, 100, 35), &format, &textBrush);
}
