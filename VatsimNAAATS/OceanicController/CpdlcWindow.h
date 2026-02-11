#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

using namespace std;
using namespace Gdiplus;

class CCpdlcWindow {
public:
    static HWND Create(HINSTANCE hInstance, HWND hParent);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static void Render(HWND hWnd, Graphics& g);
    static void RenderMessages(Graphics& g, Rect rect);
    static void RenderAircraftList(Graphics& g, Rect rect);
    static void RenderComposeArea(Graphics& g, Rect rect);
};
