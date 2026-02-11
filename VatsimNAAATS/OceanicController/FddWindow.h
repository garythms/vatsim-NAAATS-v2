#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

using namespace std;
using namespace Gdiplus;

#include "DataBridge.h"

class CFddWindow {
public:
    static HWND Create(HINSTANCE hInstance, HWND hParent);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static void Render(HWND hWnd, Graphics& g);
    static void DrawStrip(Graphics& g, Rect rect, const AircraftData& ac, int status);
    static void DrawDropdown(Graphics& g, Rect rect, vector<string> items);
    static void DrawRoundedRect(Graphics& g, Pen* pen, Brush* brush, RectF rect, float radius);
    
    static HWND hEditFindAC;
    static HWND hEditSelcal;
    static HWND hEditTMI;

    // Dropdown state
    static string dropdownCallsign;
    static string dropdownField; // "FL" or "MACH"
    static Rect dropdownRect;
    static vector<string> dropdownItems;

    // Selection state
    static string selectedCallsign;
    static vector<string> discardedCallsigns;

    // Cached GDI+ resources
    static FontFamily* fontFamily;
    static Font* fontHeader;
    static Font* fontBtn;
    static Font* fontTrack;
    static Font* fontMain;
    static Font* fontSub;
    static Font* fontWp;
    static Font* fontTime;
    
    static void InitResources();
    static void FreeResources();
};
