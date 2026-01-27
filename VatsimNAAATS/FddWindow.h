#pragma once
#include "pch.h"
#include "BaseWindow.h"
#include "EuroScopePlugIn.h"
#include <vector>
#include <string>

using namespace std;
using namespace EuroScopePlugIn;
using namespace Gdiplus;

class CFddWindow : public CBaseWindow {
public:
	CFddWindow(POINT topLeft);
	virtual void RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen);
	virtual void MakeWindowItems();
	virtual void ButtonDown(int id);
	virtual void ButtonUp(int id, CRadarScreen* screen = nullptr);
	virtual void ButtonPress(int id);
	virtual void ButtonUnpress(int id);
	virtual void SetButtonState(int id, CInputState state);
	
	// Scroll handling
	void Scroll(CRect area, POINT mousePtr);

	// Constants
	static const int BTN_CLOSE;
	static const int WINSZ_FDD_WIDTH = 800;
	static const int WINSZ_FDD_HEIGHT = 400;

private:
	// Scroll variables
	double scrollAreaSize;
	double gripPosDelta = 0;
	double gripSize;
	CRect currentScrollPos = CRect(0, 0, 0, 0);
	int scrollWindowSize;
};
