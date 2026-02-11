#include "pch.h"
#include "FddWindow.h"
#include "Constants.h"
#include "Styles.h"
#include "DataHandler.h"
#include "Utils.h"
#include "RoutesHelper.h"
#include "RadarDisplay.h"
#include "WebServer.h"
#include <algorithm>
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <map>
#include <ctime>

using namespace Colours;
using namespace std;

// Helper to send command to EuroScope
void SendEuroScopeCommand(string command) {
	vector<INPUT> inputs;
	for (char c : command) {
		INPUT input = { 0 };
		input.type = INPUT_KEYBOARD;
		short vk = VkKeyScanA(c);
		input.ki.wVk = vk & 0xFF;

		if ((vk >> 8) & 1) {
			INPUT shiftDown = { 0 };
			shiftDown.type = INPUT_KEYBOARD;
			shiftDown.ki.wVk = VK_SHIFT;
			inputs.push_back(shiftDown);
		}

		inputs.push_back(input);
		input.ki.dwFlags = KEYEVENTF_KEYUP;
		inputs.push_back(input);

		if ((vk >> 8) & 1) {
			INPUT shiftUp = { 0 };
			shiftUp.type = INPUT_KEYBOARD;
			shiftUp.ki.wVk = VK_SHIFT;
			shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
			inputs.push_back(shiftUp);
		}
	}

	INPUT enter = { 0 };
	enter.type = INPUT_KEYBOARD;
	enter.ki.wVk = VK_RETURN;
	inputs.push_back(enter);
	enter.ki.dwFlags = KEYEVENTF_KEYUP;
	inputs.push_back(enter);

	SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}

const int CFddWindow::BTN_CLOSE = 100;
const int CFddWindow::BTN_TMI = 101;
const int CFddWindow::BTN_TRACK = 102;
const int CFddWindow::BTN_SELCAL = 103;
const int CFddWindow::BTN_WEB = 104;
const int CFddWindow::BTN_NATTRAK = 105;

CFddWindow::CFddWindow(POINT topLeft) : CBaseWindow(topLeft) {
	MakeWindowItems();
	IsClosed = true;
	SelectedCallsign = "";
	showingFLDropdown = false;
	showingMachDropdown = false;
	dropdownCallsign = "";
	dropdownScrollOffset = 0;
	InitializeColumns(WINSZ_FDD_WIDTH);
}

void CFddWindow::MakeWindowItems() {
	windowButtons[BTN_TMI] = CWinButton(BTN_TMI, WIN_FDD, "TMI", CInputState::INACTIVE);
	windowButtons[BTN_TRACK] = CWinButton(BTN_TRACK, WIN_FDD, "TRACK", CInputState::INACTIVE);
	windowButtons[BTN_SELCAL] = CWinButton(BTN_SELCAL, WIN_FDD, "SELCAL", CInputState::INACTIVE);
	// windowButtons[BTN_WEB] = CWinButton(BTN_WEB, WIN_FDD, "BROWSER", CInputState::INACTIVE);
	windowButtons[BTN_NATTRAK] = CWinButton(BTN_NATTRAK, WIN_FDD, "NATTRAK", CInputState::INACTIVE);
}

void CFddWindow::InitializeColumns(int windowWidth) {
	columns.CallsignX = 35;
	columns.TypeX = 120;
	columns.DepDestX = 170;
	columns.FlightLevelX = 260;
	columns.MachX = 320;
	columns.SelcalX = 390;
	columns.EntryFixX = 470;
	columns.EntryTimeX = 540;
	columns.ExitFixX = 610;
	columns.ExitTimeX = 680;
	columns.DiscardX = windowWidth - 25;
}

// Direction: true = Westbound (Blue), false = Eastbound (Yellow)
COLORREF CFddWindow::GetDirectionColor(bool isWestbound, bool isHeader) {
	if (isWestbound) {
		// Blue tones for Westbound
		return isHeader ? RGB(70, 100, 160) : RGB(180, 200, 230);
	} else {
		// Yellow/Amber tones for Eastbound
		return isHeader ? RGB(180, 150, 60) : RGB(255, 245, 200);
	}
}

COLORREF CFddWindow::GetStripBackgroundColor(bool isWestbound, bool isTracked, bool isSelected, int row) {
	if (isSelected) {
		return RGB(100, 180, 100); // Green highlight for selection
	}
	
	if (isWestbound) {
		// Blue tones for Westbound - Tracked aircraft are darker/more prominent
		if (isTracked) {
			return (row % 2 == 0) ? RGB(170, 190, 220) : RGB(155, 175, 205);
		} else {
			return (row % 2 == 0) ? RGB(200, 215, 240) : RGB(185, 200, 225);
		}
	} else {
		// Yellow/Amber tones for Eastbound - Tracked aircraft are darker/more prominent
		if (isTracked) {
			return (row % 2 == 0) ? RGB(245, 235, 180) : RGB(235, 225, 165);
		} else {
			return (row % 2 == 0) ? RGB(255, 250, 210) : RGB(250, 240, 195);
		}
	}
}

void CFddWindow::SelectAircraft(CRadarScreen* screen, string callsign) {
	CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());
	if (fp.IsValid()) {
		screen->GetPlugIn()->SetASELAircraft(fp);
		SelectedCallsign = callsign;
	}
}

void CFddWindow::ShowFLDropdown(string callsign, POINT position) {
	showingFLDropdown = true;
	showingMachDropdown = false;
	dropdownCallsign = callsign;
	dropdownPosition = position;
	dropdownScrollOffset = 0;
}

void CFddWindow::ShowMachDropdown(string callsign, POINT position) {
	showingFLDropdown = false;
	showingMachDropdown = true;
	dropdownCallsign = callsign;
	dropdownPosition = position;
	dropdownScrollOffset = 0;
}

void CFddWindow::HideDropdowns() {
	showingFLDropdown = false;
	showingMachDropdown = false;
	dropdownCallsign = "";
}

void CFddWindow::HandleDropdownSelection(string selection, CRadarScreen* screen) {
	if (dropdownCallsign.empty()) return;
	
	CAircraftFlightPlan* fp = CDataHandler::GetFlightData(dropdownCallsign);
	if (!fp) return;
	
	if (showingFLDropdown) {
		// Update flight level
		fp->FlightLevel = selection;
		// Also update in EuroScope scratch pad or temp altitude
		CFlightPlan esFp = screen->GetPlugIn()->FlightPlanSelect(dropdownCallsign.c_str());
		if (esFp.IsValid()) {
			// Convert FL to feet and set temp altitude
			int fl = stoi(selection);
			esFp.GetControllerAssignedData().SetClearedAltitude(fl * 100);
		}
	}
	else if (showingMachDropdown) {
		// Update Mach
		fp->Mach = selection;
		// Could also update in EuroScope if there's a mechanism
	}
	
	HideDropdowns();
}

void CFddWindow::RenderFLDropdown(CDC* dc, Graphics* g, CRadarScreen* screen) {
	if (!showingFLDropdown) return;
	
	// FL options from FL390 to FL600 (in 10s)
	vector<string> flOptions;
	for (int fl = 390; fl <= 600; fl += 10) {
		flOptions.push_back(to_string(fl));
	}
	
	int itemHeight = 18;
	int dropdownWidth = 60;
	int visibleItems = 10;
	int dropdownHeight = visibleItems * itemHeight;
	
	CRect dropdownRect(dropdownPosition.x, dropdownPosition.y, 
		dropdownPosition.x + dropdownWidth, dropdownPosition.y + dropdownHeight);
	
	// Background
	CBrush bgBrush(RGB(255, 255, 255));
	dc->FillRect(dropdownRect, &bgBrush);
	dc->DrawEdge(dropdownRect, EDGE_RAISED, BF_RECT);
	
	FontSelector::SelectNormalFont(12, dc);
	dc->SetTextColor(RGB(0, 0, 0));
	dc->SetTextAlign(TA_LEFT);
	dc->SetBkMode(TRANSPARENT);
	
	int y = dropdownRect.top;
	for (size_t i = dropdownScrollOffset; i < flOptions.size() && y < dropdownRect.bottom; i++) {
		CRect itemRect(dropdownRect.left, y, dropdownRect.right, y + itemHeight);
		
		// Highlight on hover would need mouse tracking
		dc->TextOut(itemRect.left + 5, itemRect.top + 2, flOptions[i].c_str());
		
		string itemId = "FLSEL:" + dropdownCallsign + ":" + flOptions[i];
		screen->AddScreenObject(WIN_FDD, itemId.c_str(), itemRect, false, "");
		
		y += itemHeight;
	}
	
	// Scrollbar if needed
	if (flOptions.size() > visibleItems) {
		CRect scrollRect(dropdownRect.right - 12, dropdownRect.top, dropdownRect.right, dropdownRect.bottom);
		CBrush scrollBg(RGB(200, 200, 200));
		dc->FillRect(scrollRect, &scrollBg);
	}
}

void CFddWindow::RenderMachDropdown(CDC* dc, Graphics* g, CRadarScreen* screen) {
	if (!showingMachDropdown) return;
	
	// Mach options: M070 to M099, then M100 to M200 (in 1s for low, 5s for high)
	vector<string> machOptions;
	for (int m = 70; m <= 99; m++) {
		char buf[10];
		sprintf_s(buf, "%03d", m);
		machOptions.push_back(buf);
	}
	// Add 100, 105, ... 200
	for (int m = 100; m <= 200; m += 5) {
		char buf[10];
		sprintf_s(buf, "%03d", m);
		machOptions.push_back(buf);
	}
	
	int itemHeight = 18;
	int dropdownWidth = 55;
	int visibleItems = 12;
	int dropdownHeight = visibleItems * itemHeight;
	
	CRect dropdownRect(dropdownPosition.x, dropdownPosition.y, 
		dropdownPosition.x + dropdownWidth, dropdownPosition.y + dropdownHeight);
	
	// Background
	CBrush bgBrush(RGB(255, 255, 255));
	dc->FillRect(dropdownRect, &bgBrush);
	dc->DrawEdge(dropdownRect, EDGE_RAISED, BF_RECT);
	
	FontSelector::SelectNormalFont(12, dc);
	dc->SetTextColor(RGB(0, 0, 0));
	dc->SetTextAlign(TA_LEFT);
	dc->SetBkMode(TRANSPARENT);
	
	int y = dropdownRect.top;
	for (size_t i = dropdownScrollOffset; i < machOptions.size() && y < dropdownRect.bottom; i++) {
		CRect itemRect(dropdownRect.left, y, dropdownRect.right, y + itemHeight);
		
		dc->TextOut(itemRect.left + 5, itemRect.top + 2, machOptions[i].c_str());
		
		string itemId = "MACHSEL:" + dropdownCallsign + ":" + machOptions[i];
		screen->AddScreenObject(WIN_FDD, itemId.c_str(), itemRect, false, "");
		
		y += itemHeight;
	}
	
	// Scrollbar if needed
	if (machOptions.size() > visibleItems) {
		CRect scrollRect(dropdownRect.right - 12, dropdownRect.top, dropdownRect.right, dropdownRect.bottom);
		CBrush scrollBg(RGB(200, 200, 200));
		dc->FillRect(scrollRect, &scrollBg);
	}
}

void CFddWindow::RenderTrackHeader(CDC* dc, Graphics* g, CRadarScreen* screen, const string& trackId, CRect rect, int trackedCount, int untrackedCount, bool isWestbound) {
	// Fill with direction-based color
	COLORREF headerColor = GetDirectionColor(isWestbound, true);
	CBrush headerBrush(headerColor);
	dc->FillRect(rect, &headerBrush);
	
	// Draw bottom border
	dc->DrawEdge(rect, EDGE_ETCHED, BF_BOTTOM);
	
	// Text
	FontSelector::SelectBoldFont(14, dc);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_LEFT);
	dc->SetBkMode(TRANSPARENT);
	
	string headerText = "Track " + trackId;
	if (trackId == "RR") headerText = "Random Routes";
	
	// Add TMI info if this is a known NAT track
	if (CRoutesHelper::CurrentTracks.count(trackId)) {
		headerText += " (TMI " + CRoutesHelper::CurrentTracks.at(trackId).TMI + ")";
	}
	
	// Add direction indicator
	headerText += isWestbound ? " [WESTBOUND]" : " [EASTBOUND]";
	
	// Add counts
	headerText += " - " + to_string(trackedCount + untrackedCount) + " aircraft";
	if (trackedCount > 0) {
		headerText += " (" + to_string(trackedCount) + " tracked)";
	}
	
	dc->TextOut(rect.left + 10, rect.top + 4, headerText.c_str());
}

void CFddWindow::RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen) {
	int iDC = dc->SaveDC();

	// Color definitions
	COLORREF titleBarColor = RGB(60, 70, 90);
	COLORREF buttonBarColor = RGB(50, 60, 80);
	COLORREF columnHeaderColor = RGB(40, 50, 70);

	CBrush titleBrush(titleBarColor);
	CBrush buttonBarBrush(buttonBarColor);
	CBrush colHeaderBrush(columnHeaderColor);

	// Window Dimensions
	CRect windowRect(topLeft.x, topLeft.y, topLeft.x + WINSZ_FDD_WIDTH, topLeft.y + WINSZ_FDD_HEIGHT);
	
	// Main background
	CBrush mainBgBrush(RGB(100, 110, 130));
	dc->FillRect(windowRect, &mainBgBrush);

	// Title Bar
	CRect titleRect(windowRect.left, windowRect.top, windowRect.right, windowRect.top + WINSZ_TITLEBAR_HEIGHT);
	dc->FillRect(titleRect, &titleBrush);
	dc->DrawEdge(titleRect, EDGE_RAISED, BF_BOTTOM);
	
	// Title Text
	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_CENTER);
	dc->SetBkMode(TRANSPARENT);
	dc->TextOut(titleRect.left + (WINSZ_FDD_WIDTH / 2), titleRect.top + 3, "Flight Data Display");

	// Close X Button
	CRect closeRect(titleRect.right - 18, titleRect.top + 2, titleRect.right - 2, titleRect.bottom - 2);
	dc->DrawFrameControl(closeRect, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
	screen->AddScreenObject(WIN_FDD, "CLOSE_X", closeRect, false, "");

	// Button Bar
	int btnBarY = titleRect.bottom;
	int btnBarHeight = 32;
	CRect btnBarRect(windowRect.left, btnBarY, windowRect.right, btnBarY + btnBarHeight);
	dc->FillRect(btnBarRect, &buttonBarBrush);
	
	// TMI Display (Non-interactive)
	string tmiLabel = "TMI: " + CRoutesHelper::CurrentTMI;
	if (CRoutesHelper::CurrentTMI.empty()) tmiLabel = "TMI: ---";
	
	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(RGB(200, 255, 200)); // Light green for TMI
	dc->SetTextAlign(TA_LEFT);
	dc->TextOut(windowRect.left + 10, btnBarY + 8, tmiLabel.c_str());
	
	// Current Time
	string timeStr = CUtils::ParseZuluTime(true);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->TextOut(windowRect.left + 110, btnBarY + 8, timeStr.c_str());

	// Interactive Buttons
	int btnX = windowRect.left + 200;
	int btnY = btnBarY + 4;
	int btnH = 24;
	int btnW = 80;
	
	// TRACK Button - Render manually with highlight color
	CRect trackBtnRect(btnX, btnY, btnX + btnW, btnY + btnH);
	CBrush trackBtnBrush(SelectedCallsign.empty() ? RGB(80, 90, 110) : RGB(0, 120, 0)); // Green when aircraft selected
	dc->FillRect(trackBtnRect, &trackBtnBrush);
	dc->DrawEdge(trackBtnRect, EDGE_RAISED, BF_RECT);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_CENTER);
	dc->TextOut(trackBtnRect.left + btnW/2, trackBtnRect.top + 4, "TRACK");
	screen->AddScreenObject(WIN_FDD, to_string(BTN_TRACK).c_str(), trackBtnRect, false, "");
	btnX += btnW + 10;
	
	// SELCAL Button - Render manually with highlight color
	CRect selcalBtnRect(btnX, btnY, btnX + btnW, btnY + btnH);
	CBrush selcalBtnBrush(SelectedCallsign.empty() ? RGB(80, 90, 110) : RGB(140, 0, 140)); // Purple when aircraft selected
	dc->FillRect(selcalBtnRect, &selcalBtnBrush);
	dc->DrawEdge(selcalBtnRect, EDGE_RAISED, BF_RECT);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_CENTER);
	dc->TextOut(selcalBtnRect.left + btnW/2, selcalBtnRect.top + 4, "SELCAL");
	screen->AddScreenObject(WIN_FDD, to_string(BTN_SELCAL).c_str(), selcalBtnRect, false, "");
	btnX += btnW + 10;

	// BROWSER Button (Removed)
	/*
	CRect webBtnRect(btnX, btnY, btnX + btnW, btnY + btnH);
	CBrush webBtnBrush(RGB(40, 60, 100)); // Blue for browser
	dc->FillRect(webBtnRect, &webBtnBrush);
	dc->DrawEdge(webBtnRect, EDGE_RAISED, BF_RECT);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_CENTER);
	dc->TextOut(webBtnRect.left + btnW/2, webBtnRect.top + 4, "BROWSER");
	screen->AddScreenObject(WIN_FDD, to_string(BTN_WEB).c_str(), webBtnRect, false, "");
	btnX += btnW + 10;
	*/

	// NATTRAK Button
	CRect natBtnRect(btnX, btnY, btnX + btnW, btnY + btnH);
	CBrush natBtnBrush(SelectedCallsign.empty() ? RGB(80, 90, 110) : RGB(20, 40, 160)); // Darker blue
	dc->FillRect(natBtnRect, &natBtnBrush);
	dc->DrawEdge(natBtnRect, EDGE_RAISED, BF_RECT);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_CENTER);
	dc->TextOut(natBtnRect.left + btnW/2, natBtnRect.top + 4, "NATTRAK");
	screen->AddScreenObject(WIN_FDD, to_string(BTN_NATTRAK).c_str(), natBtnRect, false, "");
	
	// Legend
	btnX = windowRect.right - 220;
	dc->SetTextColor(RGB(150, 180, 255));
	dc->TextOut(btnX, btnBarY + 8, "BLUE=West");
	btnX += 80;
	dc->SetTextColor(RGB(255, 230, 150));
	dc->TextOut(btnX, btnBarY + 8, "YELLOW=East");

	// Screen objects
	screen->AddScreenObject(WINDOW, "WIN_FDD", windowRect, true, "");
	screen->AddScreenObject(WINDOW, "FDD_TITLE", titleRect, true, "");

	// Column Header Area
	int colHeaderY = btnBarY + btnBarHeight;
	int colHeaderHeight = 22;
	CRect colHeaderRect(windowRect.left, colHeaderY, windowRect.right, colHeaderY + colHeaderHeight);
	dc->FillRect(colHeaderRect, &colHeaderBrush);
	dc->DrawEdge(colHeaderRect, EDGE_ETCHED, BF_BOTTOM);
	
	FontSelector::SelectNormalFont(12, dc);
	dc->SetTextColor(RGB(255, 255, 255));
	dc->SetTextAlign(TA_LEFT);
	
	// Column headers
	int x = windowRect.left;
	int y = colHeaderY + 4;
	dc->TextOut(x + columns.CallsignX, y, "CALLSIGN");
	dc->TextOut(x + columns.TypeX, y, "TYPE");
	dc->TextOut(x + columns.DepDestX, y, "DEP/ARR");
	dc->TextOut(x + columns.FlightLevelX, y, "FL");
	dc->TextOut(x + columns.MachX, y, "MACH");
	dc->TextOut(x + columns.SelcalX, y, "SELCAL");
	dc->TextOut(x + columns.EntryFixX, y, "ENTRY");
	dc->TextOut(x + columns.EntryTimeX, y, "ETA");
	dc->TextOut(x + columns.ExitFixX, y, "EXIT");
	dc->TextOut(x + columns.ExitTimeX, y, "ETO");

	// Content Area
	CRect contentRect = windowRect;
	contentRect.top = colHeaderY + colHeaderHeight;
	contentRect.bottom = windowRect.bottom;
	
	// Collect and organize aircraft data
	// Structure: Track -> Direction -> vector of (fp, isTracked, exitTime)
	struct AircraftEntry {
		CAircraftFlightPlan* fp;
		bool isTracked;
		int exitTime;
		bool isWestbound;
	};
	
	map<string, vector<AircraftEntry>> westboundByTrack;
	map<string, vector<AircraftEntry>> eastboundByTrack;
	vector<string> trackKeys;
	CPlugIn* plugin = screen->GetPlugIn();

	for (CFlightPlan fp = plugin->FlightPlanSelectFirst(); fp.IsValid(); fp = plugin->FlightPlanSelectNext(fp)) {
		CRadarTarget rt = fp.GetCorrelatedRadarTarget();
		if (!rt.IsValid()) continue;
		
		// Relevance filter: Only show if tracked by me, or within the oceanic region + buffer
		// Oceanic region roughly between 10W and 50W.
		// Let's use 5W to 60W as the active FDD range to keep it around ~40 aircraft.
		double lon = rt.GetPosition().GetPosition().m_Longitude;
		bool isRelevantRegion = (lon < -5.0 && lon > -60.0);
		
		if (!fp.GetTrackingControllerIsMe() && !isRelevantRegion) continue; 
		
		CAircraftFlightPlan* data = CDataHandler::GetFlightData(fp.GetCallsign());
		
		// If data is missing or invalid, trigger an update
		if (!data || !data->IsValid || data->Type.empty() || data->Depart.empty()) {
			CDataHandler::UpdateFlightData(screen, fp.GetCallsign(), true);
			data = CDataHandler::GetFlightData(fp.GetCallsign());
		}
		
		if (!data || !data->IsValid) continue;
		
		// FL200+ filter
		int fl = 0;
		try {
			fl = stoi(data->FlightLevel);
			if (fl > 1000) fl /= 100;
		} catch (...) { fl = 0; }
		if (fl < 200) continue;

		// Get route info for entry time calculation
		vector<CRoutePosition> route;
		CRoutesHelper::GetRoute(screen, &route, fp.GetCallsign());
		
		// 60 minute filter - skip if more than 60 mins from oceanic entry
		if (!route.empty() && !route[0].Estimate.empty() && route[0].Estimate != "--") {
			// Parse entry estimate time (format HHMM)
			try {
				string estStr = route[0].Estimate;
				if (estStr.length() >= 4) {
					int estHour = stoi(estStr.substr(0, 2));
					int estMin = stoi(estStr.substr(2, 2));
					int estMins = estHour * 60 + estMin;
					
					// Get current time
					time_t now = time(0);
					tm* gmt = gmtime(&now);
					int nowMins = gmt->tm_hour * 60 + gmt->tm_min;
					
					// Handle day wrap (if estimate is tomorrow)
					int minsUntilEntry = estMins - nowMins;
					if (minsUntilEntry < -720) minsUntilEntry += 1440; // Add 24 hours if it wrapped
					
					// Skip if more than 60 mins away
					if (minsUntilEntry > 60) continue;
				}
			} catch (...) {
				// If we can't parse, include the aircraft
			}
		}

		// Check if hidden
		bool isHidden = false;
		for (const auto& hidden : HiddenAircraft) {
			if (hidden == data->Callsign) {
				isHidden = true;
				break;
			}
		}
		if (isHidden) continue;
		
		string trackId = data->Track;
		if (trackId.empty()) trackId = "RR";
		
		bool isTracked = fp.GetTrackingControllerIsMe();
		bool isWestbound = data->Direction; // true = Westbound
		
		AircraftEntry entry = { data, isTracked, data->ExitTime, isWestbound };
		
		if (isWestbound) {
			westboundByTrack[trackId].push_back(entry);
		} else {
			eastboundByTrack[trackId].push_back(entry);
		}
		
		// Collect unique track keys
		bool found = false;
		for (const auto& k : trackKeys) {
			if (k == trackId) { found = true; break; }
		}
		if (!found) trackKeys.push_back(trackId);
	}

	// Sort track keys alphabetically (A, B, C... then RR at end)
	sort(trackKeys.begin(), trackKeys.end(), [](const string& a, const string& b) {
		if (a == "RR") return false;
		if (b == "RR") return true;
		return a < b;
	});

	// Sort aircraft within each track by exit time (earliest first = closest to exit)
	auto sortByExitTime = [](AircraftEntry& a, AircraftEntry& b) {
		// Put tracked aircraft first, then sort by exit time
		if (a.isTracked != b.isTracked) return a.isTracked > b.isTracked;
		// -1 means no exit time, put at end
		if (a.exitTime < 0) return false;
		if (b.exitTime < 0) return true;
		return a.exitTime < b.exitTime;
	};
	
	for (auto& kv : westboundByTrack) {
		sort(kv.second.begin(), kv.second.end(), sortByExitTime);
	}
	for (auto& kv : eastboundByTrack) {
		sort(kv.second.begin(), kv.second.end(), sortByExitTime);
	}

	// Calculate total content height
	totalContentHeight = 0;
	int stripHeight = 22;
	int sectionHeaderHeight = 24;
	
	for (const string& trackId : trackKeys) {
		// Westbound section
		if (westboundByTrack.count(trackId) && !westboundByTrack[trackId].empty()) {
			totalContentHeight += sectionHeaderHeight;
			totalContentHeight += (int)westboundByTrack[trackId].size() * stripHeight;
		}
		// Eastbound section
		if (eastboundByTrack.count(trackId) && !eastboundByTrack[trackId].empty()) {
			totalContentHeight += sectionHeaderHeight;
			totalContentHeight += (int)eastboundByTrack[trackId].size() * stripHeight;
		}
	}

	// Scroll Logic
	scrollWindowSize = contentRect.Height();
	if (scrollOffset < 0) scrollOffset = 0;
	if (scrollOffset > totalContentHeight - scrollWindowSize) scrollOffset = totalContentHeight - scrollWindowSize;
	if (totalContentHeight <= scrollWindowSize) scrollOffset = 0;

	// Clipping region
	CRgn listRgn;
	listRgn.CreateRectRgn(contentRect.left, contentRect.top, contentRect.right - 15, contentRect.bottom);
	dc->SelectClipRgn(&listRgn);

	// Render strips
	y = contentRect.top - scrollOffset;
	int rowCounter = 0;

	// 1. Render all Westbound sections first (Blue)
	for (const string& trackId : trackKeys) {
		if (westboundByTrack.count(trackId) && !westboundByTrack[trackId].empty()) {
			auto& aircraft = westboundByTrack[trackId];
			int trackedCount = 0;
			for (const auto& a : aircraft) if (a.isTracked) trackedCount++;
			
			// Section Header
			if (y + sectionHeaderHeight > contentRect.top && y < contentRect.bottom) {
				CRect headerRect(contentRect.left, y, contentRect.right - 15, y + sectionHeaderHeight);
				RenderTrackHeader(dc, g, screen, trackId, headerRect, trackedCount, (int)aircraft.size() - trackedCount, true);
			}
			y += sectionHeaderHeight;
			
			// Aircraft strips
			for (auto& entry : aircraft) {
				if (y > contentRect.bottom) break;
				if (y + stripHeight >= contentRect.top) {
					bool isSelected = (entry.fp->Callsign == SelectedCallsign);
					CRect stripRect(contentRect.left, y, contentRect.right - 15, y + stripHeight);
					RenderStrip(dc, g, screen, entry.fp, stripRect, entry.isTracked, isSelected, rowCounter);
				}
				y += stripHeight;
				rowCounter++;
			}
		}
	}
	
	// 2. Render all Eastbound sections (Yellow)
	for (const string& trackId : trackKeys) {
		if (eastboundByTrack.count(trackId) && !eastboundByTrack[trackId].empty()) {
			auto& aircraft = eastboundByTrack[trackId];
			int trackedCount = 0;
			for (const auto& a : aircraft) if (a.isTracked) trackedCount++;
			
			// Section Header
			if (y + sectionHeaderHeight > contentRect.top && y < contentRect.bottom) {
				CRect headerRect(contentRect.left, y, contentRect.right - 15, y + sectionHeaderHeight);
				RenderTrackHeader(dc, g, screen, trackId, headerRect, trackedCount, (int)aircraft.size() - trackedCount, false);
			}
			y += sectionHeaderHeight;
			
			// Aircraft strips
			for (auto& entry : aircraft) {
				if (y > contentRect.bottom) break;
				if (y + stripHeight >= contentRect.top) {
					bool isSelected = (entry.fp->Callsign == SelectedCallsign);
					CRect stripRect(contentRect.left, y, contentRect.right - 15, y + stripHeight);
					RenderStrip(dc, g, screen, entry.fp, stripRect, entry.isTracked, isSelected, rowCounter);
				}
				y += stripHeight;
				rowCounter++;
			}
		}
	}
	
	dc->SelectClipRgn(NULL);

	// Scrollbar
	if (totalContentHeight > scrollWindowSize) {
		double contentRatio = (double)scrollWindowSize / (double)totalContentHeight;
		gripSize = scrollWindowSize * contentRatio;
		if (gripSize < 20) gripSize = 20;
		
		double scrollRatio = (double)scrollOffset / (double)(totalContentHeight - scrollWindowSize);
		int gripPos = contentRect.top + (int)(scrollRatio * (scrollWindowSize - gripSize));

		CRect scrollBarRect(contentRect.right - 15, contentRect.top, contentRect.right, contentRect.bottom);
		CBrush scrollBgBrush(RGB(40, 50, 70));
		dc->FillRect(scrollBarRect, &scrollBgBrush);
		
		CRect gripRect(contentRect.right - 14, gripPos, contentRect.right - 1, gripPos + (int)gripSize);
		CBrush gripBrush(RGB(120, 140, 170));
		dc->FillRect(gripRect, &gripBrush);
		dc->DrawEdge(gripRect, EDGE_RAISED, BF_RECT);
		
		screen->AddScreenObject(WIN_SCROLLBAR, "FDD_SCROLL", scrollBarRect, true, "");
	}

	// Render dropdowns on top of everything
	RenderFLDropdown(dc, g, screen);
	RenderMachDropdown(dc, g, screen);

	dc->RestoreDC(iDC);
}

void CFddWindow::RenderStrip(CDC* dc, Graphics* g, CRadarScreen* screen, CAircraftFlightPlan* fp, CRect rect, bool isTracked, bool isSelected, int row) {
	bool isWestbound = fp->Direction;
	
	// Background color based on direction
	COLORREF bgColor = GetStripBackgroundColor(isWestbound, isTracked, isSelected, row);
	CBrush bgBrush(bgColor);
	dc->FillRect(rect, &bgBrush);

	// NATTrack status indicator on the left
	CNatTrakStatus ntStatus = CDataHandler::GetNatTrakStatus(fp->Callsign);
	COLORREF ntColor = 0;
	bool drawIndicator = true;
	if (ntStatus == CNatTrakStatus::PENDING) ntColor = RGB(255, 255, 0); // Yellow
	else if (ntStatus == CNatTrakStatus::CLEARED) ntColor = RGB(0, 255, 0); // Green
	else if (ntStatus == CNatTrakStatus::UNKNOWN) ntColor = RGB(255, 0, 0); // Red
	else drawIndicator = false; // Other - no indicator

	if (drawIndicator) {
		CRect indicatorRect(rect.left, rect.top, rect.left + 15, rect.bottom - 1);
		CBrush ntBrush(ntColor);
		dc->FillRect(indicatorRect, &ntBrush);
	}
	
	// Bottom border
	CPen borderPen(PS_SOLID, 1, RGB(100, 100, 100));
	CPen* oldPen = dc->SelectObject(&borderPen);
	dc->MoveTo(rect.left, rect.bottom - 1);
	dc->LineTo(rect.right, rect.bottom - 1);
	dc->SelectObject(oldPen);

	dc->SetBkMode(TRANSPARENT);
	dc->SetTextAlign(TA_LEFT);
	FontSelector::SelectNormalFont(12, dc);
	
	int x = rect.left;
	int y = rect.top + 4;
	
	// Callsign - Click to select
	COLORREF textColor = isTracked ? RGB(0, 100, 0) : RGB(0, 0, 0);
	dc->SetTextColor(textColor);
	dc->TextOut(x + columns.CallsignX, y, fp->Callsign.c_str());
	CRect csRect(x + columns.CallsignX, rect.top, x + columns.TypeX - 5, rect.bottom);
	screen->AddScreenObject(WIN_FDD, ("SELECT:" + fp->Callsign).c_str(), csRect, false, "");

	// Type
	dc->SetTextColor(RGB(0, 0, 0));
	dc->TextOut(x + columns.TypeX, y, fp->Type.c_str());

	// Departure/Destination
	string depDest = fp->Depart + "/" + fp->Dest;
	dc->TextOut(x + columns.DepDestX, y, depDest.c_str());

	// Flight Level - CLICKABLE for dropdown
	dc->SetTextColor(RGB(0, 0, 180)); // Blue for clickable
	dc->TextOut(x + columns.FlightLevelX, y, fp->FlightLevel.c_str());
	CRect flRect(x + columns.FlightLevelX, rect.top, x + columns.MachX - 5, rect.bottom);
	screen->AddScreenObject(WIN_FDD, ("FL:" + fp->Callsign).c_str(), flRect, false, "");

	// Mach - CLICKABLE for dropdown
	dc->TextOut(x + columns.MachX, y, fp->Mach.c_str());
	CRect machRect(x + columns.MachX, rect.top, x + columns.SelcalX - 5, rect.bottom);
	screen->AddScreenObject(WIN_FDD, ("MACH:" + fp->Callsign).c_str(), machRect, false, "");

	// SELCAL - Clickable
	string selcalText = fp->SELCAL;
	if (!selcalText.empty() && selcalText != "N/A") {
		dc->SetTextColor(RGB(140, 0, 140)); // Purple for SELCAL
		dc->TextOut(x + columns.SelcalX, y, selcalText.c_str());
		CRect selcalRect(x + columns.SelcalX, rect.top, x + columns.EntryFixX - 5, rect.bottom);
		screen->AddScreenObject(WIN_FDD, ("SELCAL:" + fp->Callsign).c_str(), selcalRect, false, "");
	}

	// Entry/Exit fix and time information
	dc->SetTextColor(RGB(0, 0, 0));
	
	// Get route for entry/exit info
	vector<CRoutePosition> route;
	CRoutesHelper::GetRoute(screen, &route, fp->Callsign);
	
	if (!route.empty()) {
		// Entry fix (first point)
		dc->TextOut(x + columns.EntryFixX, y, route[0].Fix.c_str());
		dc->TextOut(x + columns.EntryTimeX, y, route[0].Estimate.c_str());
		
		// Exit fix (last point)
		if (route.size() > 1) {
			dc->TextOut(x + columns.ExitFixX, y, route.back().Fix.c_str());
			dc->TextOut(x + columns.ExitTimeX, y, route.back().Estimate.c_str());
		}
	}
	
	// Exit time if available (format HHMM)
	if (fp->ExitTime > 0) {
		char exitBuf[10];
		sprintf_s(exitBuf, "%04d", fp->ExitTime);
		dc->SetTextColor(RGB(100, 0, 0));
		dc->TextOut(x + columns.ExitTimeX, y, exitBuf);
	}

	// Discard button (X)
	CRect discardRect(rect.right - 18, rect.top + 3, rect.right - 3, rect.bottom - 3);
	dc->DrawFrameControl(discardRect, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
	screen->AddScreenObject(WIN_FDD, ("DISCARD:" + fp->Callsign).c_str(), discardRect, false, "");
}

void CFddWindow::ButtonDown(int id) {
	if (windowButtons.find(id) != windowButtons.end())
		windowButtons[id].State = CInputState::ACTIVE;
}

void CFddWindow::ButtonUp(int id, CRadarScreen* screen) {
	if (id == BTN_CLOSE) {
		IsClosed = true;
	}
	else if (id == BTN_TMI) {
		// TMI is display-only
	}
	else if (id == BTN_TRACK) {
		if (!SelectedCallsign.empty() && screen) {
			CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(SelectedCallsign.c_str());
			if (fp.IsValid()) {
				if (fp.GetTrackingControllerIsMe()) {
					fp.EndTracking();
				} else {
					fp.StartTracking();
				}
			}
		}
	}
	else if (id == BTN_SELCAL) {
		if (!SelectedCallsign.empty() && screen) {
			CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(SelectedCallsign.c_str());
			if (fp.IsValid()) {
				CAircraftFlightPlan* flightData = CDataHandler::GetFlightData(SelectedCallsign);
				string selcal = "";
				
				if (flightData && flightData->IsValid && flightData->SELCAL != "N/A" && !flightData->SELCAL.empty()) {
					selcal = flightData->SELCAL;
				}
				
				if (selcal.empty()) {
					selcal = CUtils::GetSelcalForAircraft(&fp);
				}
				
				if (!selcal.empty() && selcal != "N/A") {
					SendEuroScopeCommand(".selcal " + selcal);
				} else {
					screen->GetPlugIn()->DisplayUserMessage("SELCAL", SelectedCallsign.c_str(), "No SELCAL code found.", true, true, false, true, false);
				}
			}
		}
	}
	/*
	else if (id == BTN_WEB) {
		int port = CWebServer::GetRunningPort();
		if (port != 0) {
			string url = "http://localhost:" + to_string(port);
			ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
		}
	}
	*/
	else if (id == BTN_NATTRAK) {
		if (!SelectedCallsign.empty()) {
			CNatTrakClearance ntClearance;
			if (CDataHandler::GetNatTrakClearance(SelectedCallsign, ntClearance) && ntClearance.RequestId != 0) {
				string url = "https://nattrak.vatsim.net/controllers/clx/rcl-msg/" + to_string(ntClearance.RequestId);
				ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
			} else {
				ShellExecuteA(NULL, "open", "https://nattrak.vatsim.net/controllers/clx", NULL, NULL, SW_SHOWNORMAL);
			}
		}
	}

	if (windowButtons.find(id) != windowButtons.end())
		windowButtons[id].State = CInputState::INACTIVE;
}

void CFddWindow::ButtonPress(int id) {}
void CFddWindow::ButtonUnpress(int id) {}

void CFddWindow::SetButtonState(int id, CInputState state) {
	if (windowButtons.find(id) != windowButtons.end()) {
		windowButtons[id].State = state;
	}
}

void CFddWindow::Scroll(CRect area, POINT mousePtr) {
	int relY = mousePtr.y - area.top;
	double ratio = (double)relY / (double)area.Height();
	scrollOffset = (int)(ratio * (totalContentHeight - scrollWindowSize));
}

void CFddWindow::HandleButton(string id, CRadarDisplay* display) {
	if (id == "CLOSE_X") {
		IsClosed = true;
		HideDropdowns();
		return;
	}

	// Check for dropdown selections first
	if (id.substr(0, 6) == "FLSEL:") {
		// Format: FLSEL:callsign:value
		size_t firstColon = id.find(':');
		size_t secondColon = id.find(':', firstColon + 1);
		string callsign = id.substr(firstColon + 1, secondColon - firstColon - 1);
		string value = id.substr(secondColon + 1);
		
		// Trim callsign for EuroScope
		string csTrimmed = callsign;
		csTrimmed.erase(csTrimmed.find_last_not_of(" \n\r\t") + 1);

		CAircraftFlightPlan* fp = CDataHandler::GetFlightData(callsign);
		if (fp) {
			// Format to 3 digits
			try {
				int fl = stoi(value);
				char buf[10];
				sprintf_s(buf, "%03d", fl);
				value = buf;
			} catch (...) {}

			fp->FlightLevel = value;
			CFlightPlan esFp = display->GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
			if (esFp.IsValid()) {
				int fl = stoi(value);
				esFp.GetControllerAssignedData().SetClearedAltitude(fl * 100);
			}
		}
		HideDropdowns();
		return;
	}
	
	if (id.substr(0, 8) == "MACHSEL:") {
		// Format: MACHSEL:callsign:value
		size_t firstColon = id.find(':');
		size_t secondColon = id.find(':', firstColon + 1);
		string callsign = id.substr(firstColon + 1, secondColon - firstColon - 1);
		string value = id.substr(secondColon + 1);
		
		// Trim callsign for EuroScope
		string csTrimmed = callsign;
		csTrimmed.erase(csTrimmed.find_last_not_of(" \n\r\t") + 1);

		CAircraftFlightPlan* fp = CDataHandler::GetFlightData(callsign);
		if (fp) {
			// Strip any non-numeric characters (like M or .)
			string cleanValue = "";
			for (char c : value) {
				if (isdigit(c)) cleanValue += c;
			}
			value = cleanValue;

			// Format to 3 digits
			try {
				int m = stoi(value);
				char buf[10];
				sprintf_s(buf, "%03d", m);
				value = buf;
			} catch (...) {}

			fp->Mach = value;
			CFlightPlan esFp = display->GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
			if (esFp.IsValid()) {
				try {
					esFp.GetControllerAssignedData().SetAssignedMach(stoi(value) * 10);
				} catch (...) {}
			}
		}
		HideDropdowns();
		return;
	}

	if (id.substr(0, 10) == "SELCALSEL:") {
		// Format: SELCALSEL:callsign:value
		size_t firstColon = id.find(':');
		size_t secondColon = id.find(':', firstColon + 1);
		string callsign = id.substr(firstColon + 1, secondColon - firstColon - 1);
		string value = id.substr(secondColon + 1);

		CAircraftFlightPlan* fp = CDataHandler::GetFlightData(callsign);
		if (fp) {
			fp->SELCAL = value;
		}
		HideDropdowns();
		return;
	}

	string type = id.substr(0, id.find(':'));
	string callsign = id.substr(id.find(':') + 1);
	
	if (type == "SELECT") {
		CFlightPlan fp = display->GetPlugIn()->FlightPlanSelect(callsign.c_str());
		if (fp.IsValid()) {
			display->GetPlugIn()->SetASELAircraft(fp);
			SelectedCallsign = callsign;
		}
		HideDropdowns();
	}
	else if (type == "DISCARD") {
		DiscardAircraft(callsign);
		HideDropdowns();
	}
	else if (type == "FL") {
		// Show FL dropdown
		CFlightPlan fp = display->GetPlugIn()->FlightPlanSelect(callsign.c_str());
		if (fp.IsValid()) {
			display->GetPlugIn()->SetASELAircraft(fp);
			SelectedCallsign = callsign;
		}
		// Get mouse position for dropdown - using approximate position
		POINT pos = { topLeft.x + columns.FlightLevelX, topLeft.y + 100 };
		ShowFLDropdown(callsign, pos);
	}
	else if (type == "MACH") {
		// Show Mach dropdown
		CFlightPlan fp = display->GetPlugIn()->FlightPlanSelect(callsign.c_str());
		if (fp.IsValid()) {
			display->GetPlugIn()->SetASELAircraft(fp);
			SelectedCallsign = callsign;
		}
		POINT pos = { topLeft.x + columns.MachX, topLeft.y + 100 };
		ShowMachDropdown(callsign, pos);
	}
	else if (type == "SELCAL") {
		CFlightPlan fp = display->GetPlugIn()->FlightPlanSelect(callsign.c_str());
		if (fp.IsValid()) {
			display->GetPlugIn()->SetASELAircraft(fp);
			SelectedCallsign = callsign;
			
			CAircraftFlightPlan* flightData = CDataHandler::GetFlightData(callsign);
			string selcal = "";
			if (flightData && flightData->IsValid && flightData->SELCAL != "N/A" && !flightData->SELCAL.empty()) {
				selcal = flightData->SELCAL;
			}
			if (selcal.empty()) {
				selcal = CUtils::GetSelcalForAircraft(&fp);
			}
			
			if (!selcal.empty() && selcal != "N/A") {
				SendEuroScopeCommand(".selcal " + selcal);
			} else {
				display->GetPlugIn()->DisplayUserMessage("SELCAL", callsign.c_str(), "No SELCAL code found.", true, true, false, true, false);
			}
		}
		HideDropdowns();
	}
}

void CFddWindow::DiscardAircraft(string callsign) {
	bool exists = false;
	for (const auto& hidden : HiddenAircraft) {
		if (hidden == callsign) {
			exists = true;
			break;
		}
	}
	
	if (!exists) {
		HiddenAircraft.push_back(callsign);
	}
}
