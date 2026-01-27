#include "pch.h"
#include "FddWindow.h"
#include "Constants.h"
#include "Styles.h"
#include "DataHandler.h"
#include "Utils.h"

using namespace Colours;

const int CFddWindow::BTN_CLOSE = 100; // Arbitrary ID

CFddWindow::CFddWindow(POINT topLeft) : CBaseWindow(topLeft) {
	MakeWindowItems();
	IsClosed = true; // Closed by default
}

void CFddWindow::MakeWindowItems() {
	windowButtons[BTN_CLOSE] = CWinButton(BTN_CLOSE, WIN_FDD, "Close", CInputState::INACTIVE);
}

void CFddWindow::RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen) {
	// Save device context
	int iDC = dc->SaveDC();

	// Brushes
	CBrush darkerBrush(ScreenBlue.ToCOLORREF());
	CBrush lighterBrush(WindowBorder.ToCOLORREF());
	CBrush evenDarkerBrush(ButtonPressed.ToCOLORREF());

	// Window Dimensions
	CRect windowRect(topLeft.x, topLeft.y, topLeft.x + WINSZ_FDD_WIDTH, topLeft.y + WINSZ_FDD_HEIGHT);
	
	// Draw Background
	dc->FillRect(windowRect, &darkerBrush);

	// Title Bar
	CRect titleRect(windowRect.left, windowRect.top, windowRect.right, windowRect.top + WINSZ_TITLEBAR_HEIGHT);
	dc->FillRect(titleRect, &lighterBrush);
	dc->DrawEdge(titleRect, EDGE_RAISED, BF_BOTTOM);
	
	// Title Text
	FontSelector::SelectNormalFont(16, dc);
	dc->SetTextColor(Black.ToCOLORREF());
	dc->SetTextAlign(TA_CENTER);
	dc->TextOutA(titleRect.left + (WINSZ_FDD_WIDTH / 2), titleRect.top + 4, "Flight Data Display");

	// Button Bar (Bottom)
	CRect buttonBarRect(windowRect.left, windowRect.bottom - 40, windowRect.right, windowRect.bottom);
	dc->FillRect(buttonBarRect, &darkerBrush);
	dc->Draw3dRect(buttonBarRect, BevelLight.ToCOLORREF(), ScreenBlue.ToCOLORREF());

	// Add Screen Objects
	screen->AddScreenObject(WINDOW, "WIN_FDD", windowRect, true, "");
	screen->AddScreenObject(WINDOW, "FDD_TITLE", titleRect, true, "");

	// Render Close Button
	CCommonRenders::RenderButton(dc, screen, { buttonBarRect.right - 70, buttonBarRect.top + 5 }, 60, 30, &windowButtons.at(BTN_CLOSE));

	// Content Area
	CRect contentRect = windowRect;
	contentRect.top += WINSZ_TITLEBAR_HEIGHT;
	contentRect.bottom = buttonBarRect.top;
	
	// Collect Data
	vector<CAircraftFlightPlan*> trackedAircraft;
	for (CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelectFirst(); fp.IsValid(); fp = screen->GetPlugIn()->FlightPlanSelectNext(fp)) {
		// Only show tracked aircraft (or maybe all if configured, but user said "tracked")
		if (fp.GetTrackingControllerIsMe()) {
			CAircraftFlightPlan* data = CDataHandler::GetFlightData(fp.GetCallsign());
			if (data) {
				trackedAircraft.push_back(data);
			}
		}
	}

	// Scroll Logic
	int rowHeight = 25;
	int totalContentHeight = trackedAircraft.size() * rowHeight;
	scrollWindowSize = contentRect.Height();
	
	// Draw Scrollbar Track
	CRect scrollBarTrack(contentRect.right - 15, contentRect.top, contentRect.right, contentRect.bottom);
	dc->FillRect(scrollBarTrack, &evenDarkerBrush);
	screen->AddScreenObject(WIN_SCROLLBAR, "FDD_SCROLL", scrollBarTrack, true, "");

	// Calculate Scroll Grip
	if (totalContentHeight > 0) {
		double contentRatio = (double)scrollWindowSize / (double)totalContentHeight;
		gripSize = scrollWindowSize * contentRatio;
		if (gripSize > scrollWindowSize) gripSize = scrollWindowSize;
		if (gripSize < 20) gripSize = 20;

		int scrollableArea = totalContentHeight - scrollWindowSize;
		// Simple scroll implementation for now - improve if needed
		if (scrollableArea < 0) scrollableArea = 0;
	} else {
		gripSize = scrollWindowSize;
	}

	// Draw Content
	FontSelector::SelectMonoFont(14, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);

	int y = contentRect.top + 5; // Start with some padding
	// Clip to content area
	CRgn clipRgn;
	clipRgn.CreateRectRgnIndirect(&contentRect);
	dc->SelectClipRgn(&clipRgn);

	// Header
	int x = contentRect.left + 10;
	dc->TextOutA(x, y, "ACID"); x += 60;
	dc->TextOutA(x, y, "TYPE"); x += 50;
	dc->TextOutA(x, y, "DEP"); x += 50;
	dc->TextOutA(x, y, "DEST"); x += 50;
	dc->TextOutA(x, y, "RTE"); x += 150;
	dc->TextOutA(x, y, "FL"); x += 40;
	dc->TextOutA(x, y, "M"); x += 40;
	dc->TextOutA(x, y, "EST"); 
	y += rowHeight;

	dc->MoveTo(contentRect.left, y);
	dc->LineTo(contentRect.right - 15, y);
	y += 5;

	for (auto ac : trackedAircraft) {
		x = contentRect.left + 10;
		dc->TextOutA(x, y, ac->Callsign.c_str()); x += 60;
		dc->TextOutA(x, y, ac->Type.c_str()); x += 50;
		dc->TextOutA(x, y, ac->Depart.c_str()); x += 50;
		dc->TextOutA(x, y, ac->Dest.c_str()); x += 50;
		
		string routeStr = ac->Track;
		if (routeStr.empty() || routeStr == "RR") {
			routeStr = "";
			for (const auto& wp : ac->Route) {
				routeStr += wp.Name + " ";
			}
		}
		dc->TextOutA(x, y, routeStr.c_str()); x += 150;
		
		string flStr = ac->FlightLevel;
		// Check if it's in feet (e.g. 35000) or FL (350)
		try {
			int fl = stoi(flStr);
			if (fl > 1000) { // Likely feet
				flStr = to_string(fl / 100);
			}
		}
		catch (...) {}
		dc->TextOutA(x, y, flStr.c_str()); x += 40;
		
		dc->TextOutA(x, y, ac->Mach.c_str()); x += 40;
		// TODO: Add detailed estimates

		// Add screen object for interaction (using SCREEN_TAG_CS so it triggers selection logic)
		CRect rowRect(contentRect.left, y, contentRect.right - 15, y + rowHeight);
		screen->AddScreenObject(SCREEN_TAG_CS, ac->Callsign.c_str(), rowRect, false, "");

		y += rowHeight;
	}

	// Borders
	dc->SelectClipRgn(NULL); // Reset clip
	dc->DrawEdge(windowRect, EDGE_SUNKEN, BF_RECT);
	
	// Cleanup
	DeleteObject(darkerBrush);
	DeleteObject(lighterBrush);
	DeleteObject(evenDarkerBrush);
	dc->RestoreDC(iDC);
}

void CFddWindow::ButtonDown(int id) {
	if (windowButtons.find(id) != windowButtons.end())
		windowButtons[id].State = CInputState::ACTIVE;
}

void CFddWindow::ButtonUp(int id, CRadarScreen* screen) {
	if (id == BTN_CLOSE) {
		IsClosed = true;
	}
	if (windowButtons.find(id) != windowButtons.end())
		windowButtons[id].State = CInputState::INACTIVE;
}

void CFddWindow::ButtonPress(int id) {}

void CFddWindow::ButtonUnpress(int id) {}

void CFddWindow::SetButtonState(int id, CInputState state) {
	if (windowButtons.find(id) != windowButtons.end())
		windowButtons[id].State = state;
}

void CFddWindow::Scroll(CRect area, POINT mousePtr) {
	// TODO: Implement scrolling logic similar to TrackInfoWindow
}
