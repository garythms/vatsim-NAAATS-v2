#pragma once
#include "pch.h"
#include "BaseWindow.h"
#include "EuroScopePlugIn.h"
#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace EuroScopePlugIn;
using namespace Gdiplus;

class CRadarDisplay;

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

	// Interaction
	void HandleButton(string id, CRadarDisplay* display);
	void DiscardAircraft(string callsign);
	
	// Select aircraft in EuroScope
	void SelectAircraft(CRadarScreen* screen, string callsign);
	
	// Dropdown handling
	void ShowFLDropdown(string callsign, POINT position);
	void ShowMachDropdown(string callsign, POINT position);
	void HideDropdowns();
	void HandleDropdownSelection(string selection, CRadarScreen* screen);

	// Button IDs
	static const int BTN_CLOSE;
	static const int BTN_TMI;
	static const int BTN_TRACK;
	static const int BTN_SELCAL;
	static const int BTN_WEB;
	
	// Window dimensions
	static const int WINSZ_FDD_WIDTH = 900;
	static const int WINSZ_FDD_HEIGHT = 550;

	// Hidden aircraft
	vector<string> HiddenAircraft;
	
	// Currently selected callsign in FDD
	string SelectedCallsign;

private:
	// Scroll variables
	double scrollAreaSize;
	double gripPosDelta = 0;
	double gripSize;
	CRect currentScrollPos = CRect(0, 0, 0, 0);
	int scrollWindowSize;
	int scrollOffset = 0;
	int totalContentHeight = 0;
	
	// Dropdown state
	bool showingFLDropdown;
	bool showingMachDropdown;
	string dropdownCallsign;
	POINT dropdownPosition;
	int dropdownScrollOffset;
	
	// Strip rendering
	void RenderStrip(CDC* dc, Graphics* g, CRadarScreen* screen, CAircraftFlightPlan* fp, CRect rect, bool isTracked, bool isSelected, int row);
	
	// Track section header rendering
	void RenderTrackHeader(CDC* dc, Graphics* g, CRadarScreen* screen, const string& trackId, CRect rect, int trackedCount, int untrackedCount, bool isWestbound);
	
	// Dropdown rendering
	void RenderFLDropdown(CDC* dc, Graphics* g, CRadarScreen* screen);
	void RenderMachDropdown(CDC* dc, Graphics* g, CRadarScreen* screen);
	
	// Column positions for waypoint times
	struct ColumnLayout {
		int CallsignX;
		int TypeX;
		int DepDestX;
		int FlightLevelX;
		int MachX;
		int SelcalX;
		int EntryFixX;
		int EntryTimeX;
		int ExitFixX;
		int ExitTimeX;
		int DiscardX;
	};
	ColumnLayout columns;
	
	// Initialize column layout
	void InitializeColumns(int windowWidth);
	
	// Color definitions for the FDD display
	// Direction: true = Westbound (Blue), false = Eastbound (Yellow)
	COLORREF GetDirectionColor(bool isWestbound, bool isHeader);
	COLORREF GetStripBackgroundColor(bool isWestbound, bool isTracked, bool isSelected, int row);
};
