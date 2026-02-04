#pragma once
#include "pch.h"
#include "RadarDisplay.h"
#include "AcTargets.h"
#include "InboundList.h"
#include "Constants.h"
#include "DataHandler.h"
#include "Utils.h"
#include "ConflictDetection.h"
#include "WebServer.h"
#include "RoutesHelper.h"
#include <json.hpp>
#include <thread>

using json = nlohmann::json;
#include <gdiplus.h>
#include <ctype.h>
#include <iostream>

using namespace Gdiplus;
using namespace std;
using namespace EuroScopePlugIn;

CRadarDisplay::CRadarDisplay()
{
	//COverlays::ShowHideGridReference(this, false);
	inboundList = new CInboundList({ CUtils::InboundX, CUtils::InboundY });
	otherList = new COtherList({ CUtils::OthersX, CUtils::OthersY });
	//rclList = new CRCLList({ CUtils::RCLX, CUtils::RCLY }); // TODO: settings save
	conflictList = new CConflictList({ CUtils::ConflictX, CUtils::ConflictY }); // TODO: settings save
	trackWindow = new CTrackInfoWindow({ CUtils::TrackWindowX, CUtils::TrackWindowY });
	fltPlnWindow = new CFlightPlanWindow({ 1000, 300 }); // TODO: settings save
	msgWindow = new CMessageWindow({ 500, 500 }); // TODO: settings save
	npWindow = new CNotePad({ 300, 300 }, { 800, 200 }); // TODO: save settings
	cpdlcWindow = new CCPDLCWindow({ 600, 200 }); // TODO: save settings
	fddWindow = new CFddWindow({ 200, 200 }); // Initial position
	setupWindow = new CSetupWindow({ 400, 250 }); // Setup/profile
	menuBar = new CMenuBar();

	// Load settings
	CUtils::LoadPluginData(this);

	asel = GetPlugIn()->FlightPlanSelectASEL().GetCallsign();
	fiveSecondTimer = clock();
	tenSecondTimer = clock();
	thirtySecondTimer = clock();

	// Clogger
	CLogger::Log(CLogType::NORM, "Finished initialisation.", "CRadarDisplay");
}

CRadarDisplay::~CRadarDisplay()
{
	// Clean up
	appCursor->isESClosed;
	delete appCursor;
	delete trackWindow;
	delete fltPlnWindow;
	delete msgWindow;
	delete npWindow;
	delete cpdlcWindow;
	delete fddWindow;
	delete setupWindow;
	delete inboundList;
	delete otherList;
	delete conflictList;
	delete menuBar;
}

void CRadarDisplay::PopulateProgramData() {
	// Lists
	inboundList->MoveList({ CUtils::InboundX, CUtils::InboundY });
	otherList->MoveList({ CUtils::OthersX, CUtils::OthersY });

	// Dropdown values
	menuBar->SetDropDownValue(CMenuBar::DRP_AREASEL, CUtils::AreaSelection);
	menuBar->SetDropDownValue(CMenuBar::DRP_OVERLAYS, CUtils::SelectedOverlay);
	menuBar->SetDropDownValue(CMenuBar::DRP_TYPESEL, CUtils::PosType);

	// Buttons
	if (CUtils::TagsEnabled && !menuBar->IsButtonPressed(CMenuBar::BTN_TAGS)) {
		menuBar->SetButtonState(CMenuBar::BTN_TAGS, CInputState::ACTIVE);
	}
	if (CUtils::GridEnabled && !menuBar->IsButtonPressed(CMenuBar::BTN_GRID)) {
		menuBar->SetButtonState(CMenuBar::BTN_GRID, CInputState::ACTIVE);
	}
	if (CUtils::OverlayEnabled && !menuBar->IsButtonPressed(CMenuBar::BTN_OVERLAYS)) {
		menuBar->SetButtonState(CMenuBar::BTN_OVERLAYS, CInputState::ACTIVE);
	}
	if (CUtils::QckLookEnabled && !menuBar->IsButtonPressed(CMenuBar::BTN_QCKLOOK)) {
		menuBar->SetButtonState(CMenuBar::BTN_QCKLOOK, CInputState::ACTIVE);
	}

	// Parse track ID
	if (CUtils::SelectedOverlay == DRP_OVL_ALL) {
		COverlays::CurrentType = COverlayType::TCKS_ALL;
	}
	else if (CUtils::SelectedOverlay == DRP_OVL_EAST) {
		COverlays::CurrentType = COverlayType::TCKS_EAST;
	}
	else if (CUtils::SelectedOverlay == DRP_OVL_WEST) {
		COverlays::CurrentType = COverlayType::TCKS_WEST;
	}
	else {
		COverlays::CurrentType = COverlayType::TCKS_SEL;
	}

	// Download the tracks
	CDataHandler::PopulateLatestTrackDataAsync(GetPlugIn());

	// Set tracks in menu bar
	menuBar->MakeDropDownItems(menuBar->DRP_TCKCTRL);

	// Initialise fonts
	FontSelector::InitialiseFonts();

	// Start cursor update loop
	appCursor->screen = this;
	_beginthread(CursorStateUpdater, 0, (void*)appCursor);
	CLogger::Log(CLogType::NORM, "Firing cursor update sequence thread.", "CRadarDisplay::PopulateProgramData");

	// Check plugin version (Async)
	thread([](CPlugIn* plugin) {
		CDataHandler::CheckPluginVersion(plugin);
	}, GetPlugIn()).detach();
}

void CRadarDisplay::OpenFlightPlanWindow(string callsign) {
	if (fltPlnWindow) {
		fltPlnWindow->Instantiate(this, callsign);
	}
}

// On radar screen refresh (modified to occur 4 times a second)
void CRadarDisplay::OnRefresh(HDC hDC, int Phase)
{
	// Create device context
	CDC dc;
	dc.Attach(hDC);

	// Graphics object
	Graphics g(hDC);

	try {

	// Check for CPDLC releases
	if (cpdlcWindow != nullptr) {
		cpdlcWindow->CheckForRelease(this);
	}

	// 5 second timer
	double fiveSecT = (double)(clock() - fiveSecondTimer) / ((double)CLOCKS_PER_SEC);
	// 10 second timer
	double tenSecT = (double)(clock() - tenSecondTimer) / ((double)CLOCKS_PER_SEC);

	// Web Server Sync (1 second interval)
	static clock_t lastWebSync = clock();
	if ((double)(clock() - lastWebSync) / ((double)CLOCKS_PER_SEC) >= 1.0) {
		lastWebSync = clock();

		// Check if we need to show FDD URL
		int runningPort = CWebServer::GetRunningPort();
		if (!fddUrlShown && runningPort > 0) {
			string url = "http://127.0.0.1:" + to_string(runningPort) + "/";
			GetPlugIn()->DisplayUserMessage("vNAAATS", "FDD", ("Flight Data Display available at " + url).c_str(), true, true, true, true, true);
			CLogger::Log(CLogType::NORM, "FDD URL displayed to user: " + url, "CRadarDisplay::OnRefresh");
			fddUrlShown = true;
		}

		// Async track update check (every 10 minutes)
		static clock_t lastTrackUpdate = 0;
		if (lastTrackUpdate == 0 || (double)(clock() - lastTrackUpdate) / ((double)CLOCKS_PER_SEC) >= 600.0) {
			lastTrackUpdate = clock();
			CDataHandler::PopulateLatestTrackDataAsync(GetPlugIn());
		}

		// 1. Push data to WebServer - only aircraft visible on scope
		// 1. Pull updates from WebServer first
	while (CWebServer::HasPendingUpdates()) {
		string updateStr = CWebServer::GetPendingUpdates();
		try {
			auto update = json::parse(updateStr);
			string callsign = update["callsign"];
			string field = update["field"];
			string value = update["value"];

			// Trim and uppercase callsign
			string csTrimmed = callsign;
			csTrimmed.erase(csTrimmed.find_last_not_of(" \n\r\t") + 1);
			for (auto& c : csTrimmed) c = toupper((unsigned char)c);

			CAircraftFlightPlan* fp = CDataHandler::GetFlightData(csTrimmed);
			if (fp && fp->IsValid) {
				if (field == "FlightLevel") {
					// Format to 3 digits
					try {
						int fl = stoi(value);
						char buf[10];
						sprintf_s(buf, "%03d", fl);
						value = buf;
					} catch (...) {}

					fp->FlightLevel = value;
					CFlightPlan esFp = GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
					if (esFp.IsValid()) {
						try {
							esFp.GetControllerAssignedData().SetClearedAltitude(stoi(value) * 100);
						} catch (...) {}
					}
				}
				else if (field == "Mach") {
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
					CFlightPlan esFp = GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
					if (esFp.IsValid()) {
						try {
							esFp.GetControllerAssignedData().SetAssignedMach(stoi(value) * 10);
						} catch (...) {}
					}
				}
				else if (field == "SELCAL") {
					fp->SELCAL = value;
					// Also update local storage so it persists across refreshes
					CUtils::SelcalStorage[csTrimmed] = value;
				}
			}

			// Handle system commands
			if (callsign == "SYSTEM" && field == "COMMAND") {
				if (value == "OPEN_CPDLC") {
					if (cpdlcWindow) {
						cpdlcWindow->IsClosed = false;
						if (menuBar) menuBar->SetButtonState(CMenuBar::BTN_CPDLC, CInputState::ACTIVE);
						CLogger::Log(CLogType::NORM, "Opening CPDLC window via FDD request", "CRadarDisplay::OnRefresh");
					}
				}
			}

			// Handle aircraft commands
			if (field == "COMMAND") {
				if (value == "SELECT") {
					// Select aircraft in EuroScope
					CFlightPlan esFp = GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
					if (esFp.IsValid()) {
						GetPlugIn()->SetASELAircraft(esFp);
					}
				}
				else if (value == "TRACK") {
					CFlightPlan esFp = GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
					if (esFp.IsValid()) {
						if (esFp.GetTrackingControllerIsMe()) {
							esFp.EndTracking();
						} else {
							esFp.StartTracking();
						}
					}
				}
				else if (value == "SELCAL") {
					CFlightPlan esFp = GetPlugIn()->FlightPlanSelect(csTrimmed.c_str());
					if (esFp.IsValid()) {
						string selcal = "";
						CAircraftFlightPlan* flightData = CDataHandler::GetFlightData(csTrimmed);
						if (flightData && flightData->IsValid && !flightData->SELCAL.empty() && flightData->SELCAL != "N/A") {
							selcal = flightData->SELCAL;
						}
						if (selcal.empty()) {
							selcal = CUtils::GetSelcalForAircraft(&esFp);
						}
						if (!selcal.empty() && selcal != "N/A") {
							// Trigger SELCAL via EuroScope command
							GetPlugIn()->DisplayUserMessage("SELCAL", csTrimmed.c_str(), ("Sending SELCAL: " + selcal).c_str(), true, true, false, true, false);
						}
					}
				}
			}
		}
		catch (...) {}
	}

	// 2. Build current state
	json root = json::array();
	int visibleFlights = 0;

	// Iterate through actual radar targets visible on scope
	for (CRadarTarget rt = GetPlugIn()->RadarTargetSelectFirst(); 
		 rt.IsValid(); 
		 rt = GetPlugIn()->RadarTargetSelectNext(rt)) {
			
			// Get the correlated flight plan
			CFlightPlan fp = rt.GetCorrelatedFlightPlan();
			if (!fp.IsValid()) continue;
			
			// Relevance filter: Only show if tracked by me, or within the oceanic region + buffer
			// Oceanic region roughly between 5W and 65W.
			double lon = rt.GetPosition().GetPosition().m_Longitude;
			bool isRelevantRegion = (lon < 5.0 && lon > -70.0);
			if (!fp.GetTrackingControllerIsMe() && !isRelevantRegion) continue;

			// Altitude filter - use configured filters from CUtils
			int flVal = rt.GetPosition().GetFlightLevel();
			if (flVal == 0) flVal = fp.GetFlightPlanData().GetFinalAltitude();
			
			// Default to FL200 if filters are not set (0)
			int lowFilter = CUtils::AltFiltLow > 0 ? CUtils::AltFiltLow * 100 : 20000;
			int highFilter = CUtils::AltFiltHigh > 0 ? CUtils::AltFiltHigh * 100 : 60000;

			if (flVal < lowFilter || flVal > highFilter) {
				if (flVal != 0) continue; // Allow 0 if it's unknown/ground
			}

			string callsign = fp.GetCallsign();
			
			// Get our stored flight data (if any)
			CAircraftFlightPlan* flight = CDataHandler::GetFlightData(callsign);
			
			// If data is missing or invalid, trigger an update
			if (!flight || !flight->IsValid || flight->Type.empty() || flight->Depart.empty()) {
				CDataHandler::UpdateFlightData(this, callsign, true);
				flight = CDataHandler::GetFlightData(callsign);
			}

			json f;
			f["Callsign"] = callsign;
			
			// Use flight plan data if available, otherwise use EuroScope data
			if (flight && flight->IsValid) {
				f["Type"] = flight->Type;
				f["Depart"] = flight->Depart;
				f["Dest"] = flight->Dest;
				f["Track"] = flight->Track;
				f["Mach"] = flight->Mach;
				f["SELCAL"] = flight->SELCAL;
				f["Direction"] = flight->Direction;
				f["IsEquipped"] = flight->IsEquipped;
				f["TMI"] = CRoutesHelper::CurrentTMI;
				
				// Get route details with estimates
				vector<CRoutePosition> rte;
				if (CRoutesHelper::GetRoute(this, &rte, callsign)) {
					json routeArray = json::array();
					for (const auto& pt : rte) {
						json point;
						point["name"] = pt.Fix;
						point["lat"] = pt.PositionRaw.m_Latitude;
						point["lon"] = pt.PositionRaw.m_Longitude;
						point["est"] = pt.Estimate;
						routeArray.push_back(point);
					}
					f["RouteDetails"] = routeArray;
				} else {
					f["RouteDetails"] = json::array();
				}
			} else {
				// Fallback to EuroScope data
				f["Type"] = fp.GetFlightPlanData().GetAircraftFPType();
				f["Depart"] = fp.GetFlightPlanData().GetOrigin();
				f["Dest"] = fp.GetFlightPlanData().GetDestination();
				f["Track"] = "";
				f["Mach"] = "";
				f["SELCAL"] = "";
				f["Direction"] = false;
				f["IsEquipped"] = false;
				f["TMI"] = CRoutesHelper::CurrentTMI;
				f["RouteDetails"] = json::array();
			}

			// Flight level from radar target or flight plan
			// Use flight plan value if available, otherwise radar
			string flStr = (flight && flight->IsValid) ? flight->FlightLevel : "";
			if (flStr == "000" || flStr.empty()) {
				int flV = rt.GetPosition().GetFlightLevel();
				if (flV > 1000) flV /= 100;
				char flBuf[10];
				sprintf_s(flBuf, "%03d", flV);
				flStr = flBuf;
			}
			f["FlightLevel"] = flStr;

			// Tracking info
			f["TrackedBy"] = fp.GetTrackingControllerCallsign();
			f["TrackedById"] = fp.GetTrackingControllerId();
			f["IsTrackedByMe"] = fp.GetTrackingControllerIsMe();
			f["IsCleared"] = (flight && flight->IsValid) ? flight->IsCleared : false;

			// NATTrak status
			CNatTrakClearance ntClearance;
			if (CDataHandler::GetNatTrakClearance(callsign, ntClearance)) {
				f["NatStatus"] = (int)ntClearance.Status;
				f["NatRequestId"] = ntClearance.RequestId;
			} else {
				f["NatStatus"] = (int)CNatTrakStatus::UNKNOWN;
				f["NatRequestId"] = 0;
			}

			root.push_back(f);
			visibleFlights++;
		}

		CLogger::Log(CLogType::NORM, "FDD Sync: Sending " + to_string(visibleFlights) + " visible flights to WebServer.", "CRadarDisplay::OnRefresh");
		CWebServer::SetData(root.dump());
	}

	// Run CPDLC background tasks (polling) regardless of window visibility.
	if (cpdlcWindow != nullptr) {
		cpdlcWindow->Tick();
	}

	// Clear lists if not empty and time is greater than 1 second
	if (fiveSecT >= 5 && !inboundList->AircraftList.empty()) {
		inboundList->AircraftList.clear();
		CLogger::Log(CLogType::NORM, "Refreshing Inbound list.", "CRadarDisplay::OnRefresh");
	}
	if (fiveSecT >= 5 && !otherList->AircraftList.empty()) {
		otherList->AircraftList.clear();
		CLogger::Log(CLogType::NORM, "Refreshing Other list.", "CRadarDisplay::OnRefresh");
	}

	// Online controllers
	if (fiveSecT >= 5) {
		// Fetch NATTrak clearances
		if (!CDataHandler::IsNatTrakFetcherRunning()) {
			_beginthread(CDataHandler::FetchNatTrakClearancesAsync, 0, (void*)GetPlugIn());
		}

		// Clear online controllers first
		if (!fltPlnWindow->onlineControllers.empty())
			fltPlnWindow->onlineControllers.clear();
		// Get controllers
		CController controller;
		for (controller = GetPlugIn()->ControllerSelectFirst(); controller.IsValid(); controller = GetPlugIn()->ControllerSelectNext(controller)) {
			if (string(controller.GetCallsign()).find("CTR") != string::npos || string(controller.GetCallsign()).find("FSS") != string::npos)
			fltPlnWindow->onlineControllers[controller.GetCallsign()] = controller;
		}
		CLogger::Log(CLogType::NORM, "Refreshing controller list.", "CRadarDisplay::OnRefresh");

		// Log cursor position every 5 seconds
		CLogger::Log(CLogType::NORM, "Current cursor position: (" + to_string(appCursor->position.x) + ", " + to_string(appCursor->position.y) + ")", "CRadarDisplay::OnRefresh");
	}

	// Show FDD URL once server is running
	if (!fddUrlShown) {
		int port = CWebServer::GetRunningPort();
		if (port != 0) {
			string url = "http://localhost:" + to_string(port);
			string msg = "FDD Server started on " + url;
			GetPlugIn()->DisplayUserMessage("vNAAATS", "FDD", msg.c_str(), true, true, true, true, true);
			
			// Automatically open the FDD page
			ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
			
			fddUrlShown = true;
		}
	}

	// Set the ASEL if it is different to the current one in program
	string cs = GetPlugIn()->FlightPlanSelectASEL().GetCallsign();
	if (GetPlugIn()->FlightPlanSelectASEL().GetCallsign() != asel) {
		asel = GetPlugIn()->FlightPlanSelectASEL().GetCallsign();
		CLogger::Log(CLogType::NORM, "Selected aircraft changed to " + asel + ".", "CRadarDisplay::OnRefresh");
	}

	// Set the flight plan button state
	CAircraftFlightPlan* fpAsel = CDataHandler::GetFlightData(asel);
	if (aircraftOnScreen.empty() || asel == "" || fpAsel == nullptr || !fpAsel->IsValid || !fpAsel->IsFirstUpdate || !GetPlugIn()->ControllerMyself().IsController()) {
		if (menuBar->GetButtonState(CMenuBar::BTN_FLIGHTPLAN) != CInputState::DISABLED)
			menuBar->SetButtonState(CMenuBar::BTN_FLIGHTPLAN, CInputState::DISABLED);
	}
	else {
		if (menuBar->GetButtonState(CMenuBar::BTN_FLIGHTPLAN) == CInputState::DISABLED && menuBar->GetButtonState(CMenuBar::BTN_FLIGHTPLAN) != CInputState::ACTIVE)
			menuBar->SetButtonState(CMenuBar::BTN_FLIGHTPLAN, CInputState::INACTIVE);
	}

	// Reset currently on screen list
	if (tenSecT >= 10 && !aircraftOnScreen.empty()) {
		CLogger::Log(CLogType::NORM, "Refreshing internal aircraft on screen list.", "CRadarDisplay::OnRefresh");
		// Loop on screen aircraft
		auto idx = aircraftOnScreen.begin();
		while(idx != aircraftOnScreen.end()) {
			// Check if valid
			CRadarTarget target = GetPlugIn()->RadarTargetSelect(idx->first.c_str());
			bool isValid = CUtils::IsAircraftRelevant(this, &target, menuBar->IsButtonPressed(CMenuBar::BTN_ALL));
			if (!target.IsValid() || !isValid) { // If not valid
				// Erase aircraft selections if they are an asel
				if (idx->first == aircraftSel1 || idx->first == aircraftSel2) { // We need to annul the selection and disable tool if activated
					aircraftSel1 = "";
					// Reset RBL (if active)
					if (menuBar->IsButtonPressed(CMenuBar::BTN_RBL)) {
						menuBar->SetButtonState(CMenuBar::BTN_RBL, CInputState::INACTIVE);
						CLogger::Log(CLogType::NORM, "Resetting RBL tool.", "CRadarDisplay::OnRefresh");
					}
					// Reset SEP (if active)
					if (menuBar->IsButtonPressed(CMenuBar::BTN_SEP)) {
						CLogger::Log(CLogType::NORM, "Resetting SEP tool.", "CRadarDisplay::OnRefresh");
						menuBar->SetButtonState(CMenuBar::BTN_SEP, CInputState::INACTIVE);
					}
					// Reset PIV (if active)
					if (menuBar->IsButtonPressed(CMenuBar::BTN_PIV)) {
						CLogger::Log(CLogType::NORM, "Resetting PIV tool.", "CRadarDisplay::OnRefresh");
						menuBar->SetButtonState(CMenuBar::BTN_PIV, CInputState::INACTIVE);
					}
				}
				// Erase ASEL
				if (idx->first == asel) {
					asel = "";
				}

				// Erase STCA if exists
				auto jdx = CConflictDetection::CurrentSTCA.begin();
				// Loop this way to avoid a vector overflow
				while (jdx != CConflictDetection::CurrentSTCA.end()) {
					if (idx->first == jdx->CallsignA || idx->first == jdx->CallsignB) {
						CLogger::Log(CLogType::NORM, "Erasing STCA for " + idx->first + ".", "CRadarDisplay::OnRefresh");
						jdx = CConflictDetection::CurrentSTCA.erase(jdx);						
					}
					else {
						jdx++;
					}
				}

				// Erase route if exists
				auto kdx = CRoutesHelper::ActiveRoutes.begin();
				// Loop this way to avoid a vector overflow
				while (kdx != CRoutesHelper::ActiveRoutes.end()) {
					if (idx->first == *kdx || idx->first == *kdx) {
						CLogger::Log(CLogType::NORM, "Erasing route for " + idx->first + ".", "CRadarDisplay::OnRefresh");
						kdx = CRoutesHelper::ActiveRoutes.erase(kdx);						
					}
					else {
						kdx++;
					}
				}

				CAircraftFlightPlan* fpDel = CDataHandler::GetFlightData(idx->first);
				if (fpDel && fpDel->IsValid) {
					CLogger::Log(CLogType::NORM, "Deleting flight data for " + idx->first + ".", "CRadarDisplay::OnRefresh");
					CDataHandler::DeleteFlightData(idx->first);					
				}

				// Erase flight plan window
				menuBar->SetButtonState(menuBar->BTN_FLIGHTPLAN, CInputState::DISABLED);

				// Finally erase the on screen reference
				CLogger::Log(CLogType::NORM, "Erasing reference for " + idx->first + ".", "CRadarDisplay");
				idx = aircraftOnScreen.erase(idx);
			}
			else {
				++idx; // We increment here to avoid a vector overflow
			}
		}
	}

	// Redo the PIV calculations every 5 seconds
	if (fiveSecT >= 5 && menuBar->IsButtonPressed(CMenuBar::BTN_PIV)) {
		CLogger::Log(CLogType::NORM, "Recalculating PIV between " + aircraftSel1 + " and " + aircraftSel2 + ".", "CRadarDisplay::OnRefresh");
		try {
			CConflictDetection::PIVLocations1.clear();
			CConflictDetection::PIVLocations2.clear();
			CConflictDetection::PIVSeparationStatuses.clear();
			CConflictDetection::PIVTool(this, aircraftSel1, aircraftSel2);
		}
		catch (exception & ex) {
			CLogger::Log(CLogType::ERR, "An error occurred when trying to recalculate PIV. " + string(ex.what()), "CRadarDisplay::OnRefresh");
		}
		
	}

	// Check if the altitude filter is on
	bool altFiltEnabled = false;
	if (menuBar->IsButtonPressed(CMenuBar::BTN_ALTFILT)) altFiltEnabled = true;

	// Get the radar area
	CRect RadarArea(GetRadarArea());
	RadarArea.top = RadarArea.top - 1;
	RadarArea.bottom = GetChatArea().bottom;

	if (Phase == REFRESH_PHASE_BEFORE_TAGS) {
		/// Write current lat lon on screen
		int sDC = dc.SaveDC();
		// Select font
		FontSelector::SelectMonoFont(12, &dc);
		dc.SetTextColor(TextWhite.ToCOLORREF());
		dc.SetTextAlign(TA_LEFT);
		// Get radar area and lat lon
		CRect radarBounds = this->GetRadarArea();
		dc.TextOutA(radarBounds.right - 185, radarBounds.bottom - dc.GetTextExtent("ABC").cy - 2, 
			CUtils::GetLatLonString(&appCursor->latLonPosition).c_str());
		dc.RestoreDC(sDC);

		// Draw overlays if enabled
		if (menuBar->IsButtonPressed(CMenuBar::BTN_OVERLAYS)) {
			COverlays::ShowCurrentOverlay(&dc, &g, this, menuBar);
		}

		// Draw adjacent sectors if online
		CCommonRenders::RenderAdjacentSectors(&dc, &g, this, fltPlnWindow);

		// Get first aircraft
		CRadarTarget ac;
		ac = GetPlugIn()->RadarTargetSelectFirst();

		// Get entry time and direction
		int entryMinutes;
		bool direction;

		// Draw routes
		if (!CRoutesHelper::ActiveRoutes.empty()) {
			CCommonRenders::RenderRoutes(&dc, &g, this);
		}

		// Loop all aircraft
		while (ac.IsValid()) {
			// The system plan
			CAircraftFlightPlan aircraftFlightPlan;
			CDataHandler::GetFlightData(cs.c_str(), aircraftFlightPlan);

			// Flight plan
			CFlightPlan fp = GetPlugIn()->FlightPlanSelect(ac.GetCallsign());

			// Route
			CFlightPlanExtractedRoute rte = fp.GetExtractedRoute();

			// Time and direction
			entryMinutes = fp.GetSectorEntryMinutes();
			direction = CUtils::GetAircraftDirection(ac.GetPosition().GetReportedHeading());
			
			//string debug = string(ac.GetCallsign()) + ":" + to_string(entryMinutes) + ":" + to_string(direction) + ":" + to_string(aircraftFlightPlan.ExitTime) + ":" + to_string(CUtils::GetTargetModeInt(ac.GetPosition().GetRadarFlags())) + "\n";

			//CLogger::LogAircraftDebugInfo(debug);

			// Check if they are a selected aircraft
			if (ac.GetCallsign() == CAcTargets::SearchedAircraft) {
				if (((double)(clock() - CAcTargets::fiveSecondTimer) / ((double)CLOCKS_PER_SEC)) >= 5) {
					CAcTargets::SearchedAircraft = "";
					CAcTargets::fiveSecondTimer = clock();
				}
				else {
					CAcTargets::RenderSelectionHalo(&g, this, &ac);
				}
			}

			// If PSSR button not pressed
			/*if (!menuBar->IsButtonPressed(CMenuBar::BTN_PSSR)) {
				// Check their PSSR state to hide all non ADS-B aircraft
				if (CUtils::GetTargetMode(ac.GetPosition().GetRadarFlags()) != CRadarTargetMode::ADS_B) {
					// Select the next target
					ac = GetPlugIn()->RadarTargetSelectNext(ac);
					if (aircraftOnScreen.find(ac.GetCallsign()) != aircraftOnScreen.end()) aircraftOnScreen.erase(ac.GetCallsign());
					continue;
				}
			}*/

			// Check their altitude, if they are outside the filter, skip them
			if (altFiltEnabled && !menuBar->IsButtonPressed(CMenuBar::BTN_ALL)) {
				if (ac.GetPosition().GetPressureAltitude() / 100 < CUtils::AltFiltLow || ac.GetPosition().GetPressureAltitude() / 100 > CUtils::AltFiltHigh) {
					// Select the next target
					ac = GetPlugIn()->RadarTargetSelectNext(ac);
					if (aircraftOnScreen.find(ac.GetCallsign()) != aircraftOnScreen.end()) aircraftOnScreen.erase(ac.GetCallsign());
					continue;
				}
			}

			// Check track filtering
			if (menuBar->GetButtonState(menuBar->BTN_TCKCTRL) == CInputState::ACTIVE && !menuBar->IsButtonPressed(CMenuBar::BTN_ALL)) {
				// Primed plan
				string cs = (string)fp.GetCallsign();
				vector<string> tracks;
				auto idx = find_if(CConflictDetection::CurrentSTCA.begin(), CConflictDetection::CurrentSTCA.end(), [&cs](const CSTCAStatus& obj) {return obj.CallsignA == cs || obj.CallsignB == cs; });
				if (idx == CConflictDetection::CurrentSTCA.end())
					menuBar->GetSelectedTracks(tracks);
				bool skipAircraft = tracks.empty() ? false : true;
				for (int i = 0; i < tracks.size(); i++) {
					if (aircraftFlightPlan.IsValid) {
						bool showAc = aircraftFlightPlan.Track == tracks[i];
						if (showAc) {
							skipAircraft = false;
							break;
						}						
					}
					else if (CRoutesHelper::OnNatTrack(this, string(fp.GetCallsign())) == tracks[i]) {
						skipAircraft = false;
						break;
					}
					else {
						continue;
					}
				}

				if (skipAircraft) {
					// Select the next target
					ac = GetPlugIn()->RadarTargetSelectNext(ac);
					continue;
				}
			}
			
			// Parse inbound & other
			bool filtersDisabled = menuBar->IsButtonPressed(CMenuBar::BTN_ALL);
			if (CUtils::IsAircraftRelevant(this, &ac, filtersDisabled)) {
				
				// If not there then add the status
				if (tagStatuses.find(fp.GetCallsign()) == tagStatuses.end()) {
					pair<bool, POINT> pt = make_pair(false, POINT{ 0, 0 });
					tagStatuses.insert(make_pair(string(fp.GetCallsign()), pt));
				}

				// STCA
				if (tenSecT >= 10) {
					CConflictDetection::CheckSTCA(this, &ac, &aircraftOnScreen);
				}

				if (fiveSecT >= 5) {
					// If going westbound
					if (!direction && entryMinutes > 0) {
						if (menuBar->GetDropDownValue(CMenuBar::DRP_AREASEL) == "CZQX") {
							int i;
							for (i = 0; i < rte.GetPointsNumber(); i++) {
								// Find out if 30 west is in their flight plan
								if (rte.GetPointPosition(i).m_Longitude == -30.0) {
									// Test flight time
									if (rte.GetPointDistanceInMinutes(i) > 0 && rte.GetPointDistanceInMinutes(i) < 60) {
										// Add if within
										inboundList->AircraftList.push_back(CInboundAircraft(ac.GetCallsign(), fp.GetFinalAltitude(), fp.GetClearedAltitude(),
												rte.GetPointName(i), CUtils::ParseZuluTime(false, -1, &fp, i), fp.GetFlightPlanData().GetDestination(), false));
										break;
									}
								}
							}
							if (i == rte.GetPointsNumber()) {
								// Add to 'others' list
								otherList->AircraftList.push_back(ac.GetCallsign());
							}
						}
						else {
							int i;
							for (i = 0; i < rte.GetPointsNumber(); i++) {
								// They are coming from land so check entry points
								if (CUtils::IsEntryPoint(rte.GetPointName(i), direction) || CUtils::IsExitPoint(rte.GetPointName(i), direction)) {
									// Add if within
									inboundList->AircraftList.push_back(CInboundAircraft(ac.GetCallsign(), fp.GetFinalAltitude(), fp.GetClearedAltitude(),
										rte.GetPointName(i), CUtils::ParseZuluTime(false, -1, &fp, i), fp.GetFlightPlanData().GetDestination(), false));
									break;
								}
							}
							if (i == rte.GetPointsNumber()) {
								// Add to 'others' list
								otherList->AircraftList.push_back(ac.GetCallsign());
							}
						}
					}
					else if (direction && entryMinutes > 0) {
						if (menuBar->GetDropDownValue(CMenuBar::DRP_AREASEL) == "EGGX") {
							int i;
							for (i = 0; i < rte.GetPointsNumber(); i++) {
								// Find out if 30 west is in their flight plan
								if (rte.GetPointPosition(i).m_Longitude == -30.0) {
									// Test flight time
									if (rte.GetPointDistanceInMinutes(i) > 0 && rte.GetPointDistanceInMinutes(i) < 60) {
										// Add if within
										inboundList->AircraftList.push_back(CInboundAircraft(ac.GetCallsign(), fp.GetFinalAltitude(), fp.GetClearedAltitude(),
											rte.GetPointName(i), CUtils::ParseZuluTime(false, -1, &fp, i), fp.GetFlightPlanData().GetDestination(), true));
										break;
									}
								}
							}
							if (i == rte.GetPointsNumber()) {
								// Add to 'others' list
								otherList->AircraftList.push_back(ac.GetCallsign());
							}
						}
						else {
							// They are coming from land so check entry points
							int i;
							for (i = 0; i < rte.GetPointsNumber(); i++) {
								if (CUtils::IsEntryPoint(rte.GetPointName(i), direction) || CUtils::IsExitPoint(rte.GetPointName(i), direction)) {
									// Add if within
									inboundList->AircraftList.push_back(CInboundAircraft(ac.GetCallsign(), fp.GetFinalAltitude(), fp.GetClearedAltitude(),
										rte.GetPointName(i), CUtils::ParseZuluTime(false, -1, &fp, i), fp.GetFlightPlanData().GetDestination(), true));
									break;
								}
							}
							if (i == rte.GetPointsNumber()) {
								// Add to 'others' list
								otherList->AircraftList.push_back(ac.GetCallsign());
							}
						}
					}
				}

				// Store whether detailed tags are enabled
				bool detailedEnabled = false;

				// Now we check if all the tags are selected as detailed
				if (menuBar->IsButtonPressed(CMenuBar::BTN_EXT)) {
					detailedEnabled = true; // Set detailed on

					// Unpress detailed if not already
					if (menuBar->IsButtonPressed(CMenuBar::BTN_DETAILED) && !aselDetailed) {
						menuBar->SetButtonState(CMenuBar::BTN_DETAILED, CInputState::INACTIVE);
					}
				}

				// Check if only one is set to detailed
				if (menuBar->IsButtonPressed(CMenuBar::BTN_DETAILED)) {
					if (fp.GetCallsign() == asel) {
						detailedEnabled = true; // Set detailed on
					}

					// Unpress extended if not already
					if (menuBar->IsButtonPressed(CMenuBar::BTN_EXT) && aselDetailed) {
						menuBar->SetButtonState(CMenuBar::BTN_EXT, CInputState::INACTIVE);
					}
				}

				bool ptl = false;
				bool halo = false;
				// Set PTL and HALO if they are on
				if (menuBar->IsButtonPressed(CMenuBar::BTN_PTL)) {
					ptl = true;
				}
				if (menuBar->IsButtonPressed(CMenuBar::BTN_HALO)) {
					halo = true;
				}

				// Get STCA so it can be drawn
				CSTCAStatus stcaStatus(ac.GetCallsign(), "", CConflictStatus::OK, -1, -1); // Create default
				auto idx = CConflictDetection::CurrentSTCA.begin();
				for (idx = CConflictDetection::CurrentSTCA.begin(); idx != CConflictDetection::CurrentSTCA.end(); idx++) {
					if (ac.GetCallsign() == idx->CallsignA || ac.GetCallsign() == idx->CallsignB) {
						stcaStatus = *idx;
						break; // Break for optimisation
					}
				}

				// Draw the tag and target with the information if tags are turned on and within altitude filter
				if (menuBar->IsButtonPressed(CMenuBar::BTN_TAGS)) {
					if (aircraftOnScreen.find(ac.GetCallsign()) == aircraftOnScreen.end()) aircraftOnScreen.insert(make_pair(ac.GetCallsign(), 0));
					auto kv = tagStatuses.find(fp.GetCallsign());
					kv->second.first = detailedEnabled; // Set detailed on
					CAcTargets::RenderTarget(&g, &dc, this, &ac, true, &menuBar->GetToggleButtons(), halo, ptl, &stcaStatus);
					POINT tagPosition = CAcTargets::RenderTag(&g, &dc, this, &ac, &kv->second, direction, &stcaStatus, asel);

					// If tracking dialog open
					if (CAcTargets::OpenTrackingDialog != "" && CAcTargets::OpenTrackingDialog == ac.GetCallsign()) {
						CAcTargets::RenderCoordTagItem(&dc, this, ac.GetCallsign(), tagPosition);
					}

				}
				else {
					if (aircraftOnScreen.find(ac.GetCallsign()) == aircraftOnScreen.end()) aircraftOnScreen.insert(make_pair(ac.GetCallsign(), 0));
					CAcTargets::RenderTarget(&g, &dc, this, &ac, false, &menuBar->GetToggleButtons(), halo, ptl, &stcaStatus);
				}
			}
			else { // If not there, and the aircraft was on the screen, then delete
				if (aircraftOnScreen.find(ac.GetCallsign()) != aircraftOnScreen.end()) aircraftOnScreen.erase(ac.GetCallsign());
			}

			// Select the next target
			ac = GetPlugIn()->RadarTargetSelectNext(ac);
		}


		// Clear ASELs if none of the range/separation tools are pressed
		if (!menuBar->IsButtonPressed(CMenuBar::BTN_PIV)
			&& !menuBar->IsButtonPressed(CMenuBar::BTN_RBL)
			&& !menuBar->IsButtonPressed(CMenuBar::BTN_SEP)) {
			// Reset ASELs if none enabled
			aircraftSel1 = "";
			aircraftSel2 = "";
		}

		/// RENDERING
		// Draw menu bar
		menuBar->RenderBar(&dc, &g, this, asel);

		// Draw lists
		inboundList->RenderList(&g, &dc, this);
		otherList->RenderList(&g, &dc, this);
		//rclList->RenderList(&g, &dc, this);
		conflictList->RenderList(&g, &dc, this);

		// SEP draw
		if (menuBar->IsButtonPressed(CMenuBar::BTN_SEP)) {
			// If both aircraft selected then draw
			if (aircraftSel1 != "" && aircraftSel2 != "") {
				CConflictDetection::SepTool(&dc, &g, this, aircraftSel1, aircraftSel2);
			}
		}

		// RBL draw
		if (menuBar->IsButtonPressed(CMenuBar::BTN_RBL)) {
			if (aircraftSel1 != "" && aircraftSel2 != "") {
				CConflictDetection::RBLTool(&dc, &g, this, aircraftSel1, aircraftSel2);
			}
		}

		// PIV draw
		if (menuBar->IsButtonPressed(CMenuBar::BTN_PIV)) {
			// If both aircraft selected then draw
			if (aircraftSel1 != "" && aircraftSel2 != "") {
				// Render
				CConflictDetection::RenderPIV(&dc, &g, this, aircraftSel1, aircraftSel2);
			}
		}

		// QDM draw
		if (menuBar->IsButtonPressed(CMenuBar::BTN_QDM)) {
			// Check if first position is set
			if (RulerPoint1.m_Latitude != 0.0 && RulerPoint1.m_Longitude != 0.0) {
				// Render
				CCommonRenders::RenderQDM(&dc, &g, this, &RulerPoint1, &RulerPoint2, appCursor->position, &appCursor->latLonPosition);
			}
		}

		// Draw track info window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_TCKINFO)) {
			trackWindow->RenderWindow(&dc, &g, this, menuBar);
		}

		// Draw message window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_MESSAGE)) {
			msgWindow->RenderWindow(&dc, &g, this);
		}

		// Draw CPDLC window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_CPDLC)) {
				// Ensure the CPDLC window is actually opened when the menu bar button is active.
				// The window defaults to closed, so without this it will never render.
				cpdlcWindow->IsClosed = false;
			cpdlcWindow->RenderWindow(&dc, &g, this);
		}

		// Draw FDD window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_FDD)) {
			if (fddWindow->IsClosed) fddWindow->IsClosed = false;
			fddWindow->RenderWindow(&dc, &g, this);
		}

		// Draw flight plan window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_FLIGHTPLAN)) {
			fltPlnWindow->RenderWindow(&dc, &g, this);
		}

		// Draw notepad window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_NOTEPAD)) {
			npWindow->RenderWindow(&dc, &g, this);
		}
		// Draw setup window if button pressed
		if (menuBar->IsButtonPressed(CMenuBar::BTN_SETUP)) {
			if (setupWindow->IsClosed) setupWindow->IsClosed = false;
			setupWindow->RenderWindow(&dc, &g, this);
		}

		// Draw active dropdown list last to ensure it's on top of everything
		menuBar->RenderActiveDropDown(&dc, &g, this);

		// Finally, reset the clocks if time has been exceeded
		if (fiveSecT >= 5) {
			fiveSecondTimer = clock();
		}
		if (tenSecT >= 10) {
			CLogger::Log(CLogType::NORM, "Recalculating STCA.", "CRadarDisplay::OnRefresh");
			tenSecondTimer = clock();
		}
		if (double twoSecT = (double)(clock() - CAcTargets::twoSecondTimer) / ((double)CLOCKS_PER_SEC) >= 2.2) { // Ac target and tag colours
			CAcTargets::twoSecondTimer = clock();
		}
	}
	}
	catch (std::exception& ex) {
		CLogger::Log(CLogType::ERR, "Exception in OnRefresh: " + string(ex.what()), "CRadarDisplay::OnRefresh");
	}
	catch (...) {
		CLogger::Log(CLogType::ERR, "Unknown exception in OnRefresh", "CRadarDisplay::OnRefresh");
	}

	// De-allocation
	dc.Detach();
}

// Data updates
void CRadarDisplay::OnRadarTargetPositionUpdate(CRadarTarget RadarTarget)
{
	// Force filtersDisabled=true to ensure we track all aircraft in the sector for FSS/CPDLC purposes,
	// regardless of what the user is currently filtering on their radar screen.
	// CUtils::IsAircraftRelevant will still filter out invalid flight plans.
	if (CUtils::IsAircraftRelevant(this, &RadarTarget, true)) {
		// They are relevant so get the flight plan
		CAircraftFlightPlan* fp = CDataHandler::GetFlightData(RadarTarget.GetCallsign());


		// If not valid then it doesn't exist and we need to make it
		if (fp == nullptr || !fp->IsValid) {
			// Create it
			CDataHandler::CreateFlightData(this, RadarTarget.GetCallsign());
		}
		else {
			// Flight plan object
			CFlightPlan fpData = GetPlugIn()->FlightPlanSelect(RadarTarget.GetCallsign());

			// Update exit time
			int exitMinutes = fpData.GetSectorExitMinutes();
			if (exitMinutes != -1) {
				fp->ExitTime = exitMinutes;
			}

			// Update Direction and IsEquipped for Web Server
			fp->Direction = CUtils::GetAircraftDirection(RadarTarget.GetPosition().GetReportedHeadingTrueNorth());
			// IsEquipped logic - need to check where this comes from. 
			// netFP->IsEquipped is used later, but where is fp->IsEquipped set?
			// It seems it's set in DataHandler or here. 
			// Let's use CUtils::IsAircraftEquipped logic if possible or trust existing if set.
			// Actually, let's just ensure Direction is set for now.
			
			// Update selcal code (only if not currently editing in flight plan window)
			if (asel != fp->Callsign || fltPlnWindow->IsClosed) {
				string selcal = CUtils::GetSelcalForAircraft(&fpData);
				fp->SELCAL = selcal != "" ? selcal : "N/A";
			}

			if (!fp->IsFirstUpdate) fp->IsFirstUpdate = true;
			
			// Timer
			double thirtySecT = (double)(clock() - thirtySecondTimer) / ((double)CLOCKS_PER_SEC);

			// Send mach and FL to FSD from vNAAATS if it is not there
			try {
				if (fp->IsCleared && !fp->Mach.empty() && !fp->FlightLevel.empty() && isdigit(fp->Mach[0])) { // Basic validation
					if (fpData.GetTrackingControllerIsMe()) {
						if (stoi(fp->Mach) > 0 && fpData.GetControllerAssignedData().GetAssignedMach() / 10 != stoi(fp->Mach)) {
							// Assign
							fpData.GetControllerAssignedData().SetAssignedMach(stoi(fp->Mach) * 10);
						}
						if (stoi(fp->FlightLevel) > 0 && fpData.GetControllerAssignedData().GetFinalAltitude() / 100 != stoi(fp->FlightLevel)) {
							// Assign
							fpData.GetControllerAssignedData().SetFinalAltitude(stoi(fp->FlightLevel) * 100);
						}
					}
					else { // Back the other way, FSD to vNAAATS (only if not editing)
						if (asel != fp->Callsign || fltPlnWindow->IsClosed) {
							if (fpData.GetControllerAssignedData().GetAssignedMach() / 10 != stoi(fp->Mach)) {
								// Assign
								fp->Mach = to_string(fpData.GetControllerAssignedData().GetAssignedMach() / 10);
							}
							if (fpData.GetControllerAssignedData().GetFinalAltitude() / 100 != stoi(fp->FlightLevel)) {
								// Assign
								fp->FlightLevel = to_string(fpData.GetControllerAssignedData().GetFinalAltitude() / 100);
							}
						}
					}
				}				
			}
			catch (exception & ex) {
				CLogger::DebugLog(this, "An exception occurred when attempting to update FSD: " + string(ex.what()));
			}

			// Update if aircraft is cleared
			if (fp->IsCleared && fpData.GetTrackingControllerIsMe() && thirtySecT >= 30.0) {
				CNetworkFlightPlan* netFP = nullptr;
				try {
					// Set irrelevant			
					netFP = new CNetworkFlightPlan();
					netFP->Callsign = fp->Callsign;
					netFP->Type = fp->Type;
					netFP->AssignedLevel = stoi(fp->FlightLevel);
					netFP->AssignedMach = stoi(fp->Mach);
					netFP->Track = fp->Track;
					netFP->Departure = fp->Depart;
					netFP->Arrival = fp->Dest;
					netFP->Direction = CUtils::GetAircraftDirection(GetPlugIn()->RadarTargetSelect(fp->Callsign.c_str()).GetPosition().GetReportedHeadingTrueNorth());
					netFP->Selcal = fp->SELCAL == "N/A" ? "" : fp->SELCAL;
					netFP->DatalinkConnected = fp->DLStatus == "true" ? true : false;
					netFP->IsEquipped = fp->IsEquipped;
					netFP->State = fp->State;
					netFP->Etd = fp->Etd;
					netFP->Relevant = true;
					netFP->TargetMode = CUtils::GetTargetMode(GetPlugIn()->RadarTargetSelect(fp->Callsign.c_str()).GetPosition().GetRadarFlags());
					netFP->TrackedBy = GetPlugIn()->FlightPlanSelect(fp->Callsign.c_str()).GetTrackingControllerCallsign();
					netFP->TrackedById = GetPlugIn()->FlightPlanSelect(fp->Callsign.c_str()).GetTrackingControllerId();
					netFP->Route = ""; // Initialise
					netFP->RouteEtas = ""; // Initialise

					// Post data to the database
					DWORD activeCode;
					HANDLE hnd = CUtils::GetESProcess();
					GetExitCodeProcess(hnd, &activeCode);
					// Check if the app is still active
					if (activeCode == STILL_ACTIVE && GetPlugIn()->ControllerMyself().IsValid()) {
						if (netFP->Relevant != fp->IsRelevant) {
							fp->IsRelevant = netFP->Relevant;
							// UpdateNetworkAircraft removed (defunct API)
						}
					}
					delete netFP;
				}
				catch (std::exception & ex) {
					if (netFP) delete netFP;
					CLogger::DebugLog(this, "An exception occurred when attempting to update FSD: " + string(ex.what()));
				}
			}
				
				// Reset the clock
				thirtySecondTimer = clock();
		}
	} else { // Not relevant
		// Check if they have a flight plan data object
		CAircraftFlightPlan* fp = CDataHandler::GetFlightData(RadarTarget.GetCallsign());
		if (fp && fp->IsValid && RadarTarget.GetCallsign() != asel) {
			CLogger::Log(CLogType::WARN, "Aircraft " + string(fp->Callsign) + " outside relevant scope and will be erased. SectorExitMinutes: " + to_string(fp->ExitTime) + ".", "CRadarDisplay::OnRadarTargetPositionUpdate");
			// Delete the flight data object
			CDataHandler::DeleteFlightData(RadarTarget.GetCallsign());
		}
	}
}

void CRadarDisplay::OnFlightPlanDisconnect(CFlightPlan FlightPlan) {
	// Remove from data handler to prevent ghosts in FSS/CPDLC
	CDataHandler::DeleteFlightData(FlightPlan.GetCallsign());

	// Close the flight plan window immediately and cancel the ASEL so that we don't get a crash
	if (FlightPlan.GetCallsign() == asel) {
		menuBar->SetButtonState(CMenuBar::BTN_FLIGHTPLAN, CInputState::INACTIVE);
		asel = "";
	}
	// Erase any route drawing
	if (!CRoutesHelper::ActiveRoutes.empty()) {
		int found = -1; // Found flag so we can remove if needed
		for (size_t i = 0; i < CRoutesHelper::ActiveRoutes.size(); i++) {
			// If the route is currently on the screen
			if (CRoutesHelper::ActiveRoutes[i] == FlightPlan.GetCallsign()) {
				// Set to remove
				found = i;
				break;
			}
		}

		// Erase if the item was found, otherwise add
		if (found != -1) {
			CRoutesHelper::ActiveRoutes.erase(CRoutesHelper::ActiveRoutes.begin() + found);
		}

		// Set any vNAAATS network aircraft to irrelevant
		// Create network object
		try {
			CAircraftFlightPlan* primedPlan = CDataHandler::GetFlightData(FlightPlan.GetCallsign());
			if (primedPlan && primedPlan->IsValid && primedPlan->IsCleared) {
				CNetworkFlightPlan* netFP = new CNetworkFlightPlan();
				netFP->Callsign = primedPlan->Callsign;
				netFP->Type = primedPlan->Type;
				netFP->AssignedLevel = stoi(primedPlan->FlightLevel);
				netFP->AssignedMach = stoi(primedPlan->Mach);
				netFP->Track = primedPlan->Track;
				netFP->Departure = primedPlan->Depart;
				netFP->Arrival = primedPlan->Dest;
				netFP->Direction = CUtils::GetAircraftDirection(GetPlugIn()->RadarTargetSelect(primedPlan->Callsign.c_str()).GetPosition().GetReportedHeadingTrueNorth());
				netFP->Selcal = primedPlan->SELCAL == "N/A" ? "" : primedPlan->SELCAL;
				netFP->DatalinkConnected = primedPlan->DLStatus == "true" ? true : false;
				netFP->IsEquipped = primedPlan->IsEquipped;
				netFP->State = primedPlan->State;
				netFP->Etd = primedPlan->Etd;
				netFP->Relevant = true;
				netFP->TargetMode = CUtils::GetTargetMode(GetPlugIn()->RadarTargetSelect(primedPlan->Callsign.c_str()).GetPosition().GetRadarFlags());
				netFP->TrackedBy = GetPlugIn()->FlightPlanSelect(primedPlan->Callsign.c_str()).GetTrackingControllerCallsign();
				netFP->TrackedById = GetPlugIn()->FlightPlanSelect(primedPlan->Callsign.c_str()).GetTrackingControllerId();
				netFP->Route = ""; // Initialise
				netFP->RouteEtas = ""; // Initialise

				// Get routes and estimates
				vector<CRoutePosition> rte;
				CRoutesHelper::GetRoute(this, &rte, primedPlan->Callsign);
				for (size_t i = 0; i < rte.size(); i++) {
					if (i != rte.size() - 1) {
						netFP->Route += rte[i].Fix + " ";
						netFP->RouteEtas += rte[i].Estimate + " ";
					}
					else {
						netFP->Route += rte[i].Fix;
						netFP->RouteEtas += rte[i].Estimate;
					}
				}

				// Post data to the database
				DWORD activeCode;
				HANDLE hnd = CUtils::GetESProcess();
				GetExitCodeProcess(hnd, &activeCode);
				// Check if the app is still active
				if (activeCode == STILL_ACTIVE && GetPlugIn()->ControllerMyself().IsValid()) {
					if (netFP->Relevant != primedPlan->IsRelevant) {
						primedPlan->IsRelevant = netFP->Relevant;
						// UpdateNetworkAircraft removed (defunct API)
					}
				}
			}
		}
		catch (std::exception & ex) {
			CLogger::DebugLog(this, "An exception occurred. " + *ex.what());
		}
	}
}

void CRadarDisplay::OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	// Move inbound list
	if (ObjectType == LIST_INBOUND) {
		inboundList->MoveList(Area);

		// To save
		CUtils::InboundX = Area.left;
		CUtils::InboundY = Area.top;
	}

	// Move other list
	if (ObjectType == LIST_OTHERS) {
		otherList->MoveList(Area);

		CUtils::OthersX = Area.left;
		CUtils::OthersY = Area.top;
	}

	// Move conflict list
	if (ObjectType == LIST_CONFLICT) {
		conflictList->MoveList(Area);

		// To save
		CUtils::ConflictX = Area.left;
		CUtils::ConflictY = Area.top;
	}

	// Move RCLs list
	if (ObjectType == LIST_RCLS) {
		//rclList->MoveList(Area);

		CUtils::RCLX = Area.left;
		CUtils::RCLY = Area.top;
	}

	// Move tag
	if (ObjectType == SCREEN_TAG || ObjectType == SCREEN_TAG_CS) {
		auto kv = tagStatuses.find(sObjectId);
		POINT acPosPix = ConvertCoordFromPositionToPixel(GetPlugIn()->RadarTargetSelect(sObjectId).GetPosition().GetPosition());
		kv->second.second = { Area.left - acPosPix.x, Area.top - acPosPix.y };
	}

	// Move window
	if (ObjectType == WINDOW) {
		if (string(sObjectId) == "TCKINFO")
		trackWindow->MoveWindow(Area);

		if (string(sObjectId) == "FLTPLN")
			fltPlnWindow->MoveWindow(Area);

		if (string(sObjectId) == "MSG")
			msgWindow->MoveWindow(Area);

		if (string(sObjectId) == "NOTEPAD")
			npWindow->MoveWindow(Area);

		if (string(sObjectId) == "CPDLC")
			cpdlcWindow->MoveWindow(Area);

		if (string(sObjectId) == "WIN_FDD")
			fddWindow->MoveWindow(Area);

		if (string(sObjectId) == "WIN_SETUP")
			setupWindow->MoveWindow(Area);

		CUtils::TrackWindowX = Area.left;
		CUtils::TrackWindowY = Area.top;
	}

	// Move subwindows for flight plan
	if (ObjectType == WIN_FLTPLN) {
		if (atoi(sObjectId) >= 400 && atoi(sObjectId) <= 420) {
			fltPlnWindow->MoveSubWindow(atoi(sObjectId), { Area.left, Area.top });
		}
		if (atoi(sObjectId) >= 500) {
			fltPlnWindow->Scroll(atoi(sObjectId), Pt, mousePointer);
		}
	}

	// Scrolling
	if (ObjectType == WIN_SCROLLBAR) {
		if (string(sObjectId) == "TCKINFO") trackWindow->Scroll(Area, mousePointer);
		if (string(sObjectId) == "520") msgWindow->Scroll(atoi(sObjectId), Pt, mousePointer);
		if (string(sObjectId) == "FDD_SCROLL") fddWindow->Scroll(Area, mousePointer);
	}

	// Mouse pointer
	mousePointer = Pt;

	// Refresh
	RequestRefresh();
}

void CRadarDisplay::OnOverScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area)
{
	mousePointer = Pt;
	// Dropdown
	if (ObjectType == MENBAR) {
		if (atoi(sObjectId) >= 800) {
			menuBar->OnOverDropDownItem(atoi(sObjectId));
		}
	}
	// CPDLC Window
	else if (ObjectType == WIN_CPDLC) {
		if (atoi(sObjectId) >= 800) {
			cpdlcWindow->OnOverDropDownItem(atoi(sObjectId));
		}
	}
	// If it is a message
	else if (ObjectType == ACTV_MESSAGE) {
		msgWindow->SelectedMessage = atoi(sObjectId);
	}
	// If it is a button on the tracking dialog
	if (ObjectType == SCREEN_TAG_CS_BTN) {
		CAcTargets::ButtonStates[sObjectId] = true;
	}

	// Setup window
	if (ObjectType == WIN_SETUP) {
		// Handled to ensure clickability
	}

	// TODO: button state reset
	// Refresh
	RequestRefresh();
}

void CRadarDisplay::OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	CLogger::Log(CLogType::NORM, "OnClickScreenObject: Type=" + to_string(ObjectType) + " ID=" + string(sObjectId), "CRadarDisplay::OnClickScreenObject");

	// If menu button
	if (ObjectType == MENBAR) {
		if (Button == BUTTON_RIGHT) { // Toggle buttons
			menuBar->ButtonPress(atoi(sObjectId), Button, this);
		}
		else {
			if (!menuBar->IsButtonPressed(atoi(sObjectId))) {
				// Increase refresh resolution for QDM
				if (atoi(sObjectId) == CMenuBar::BTN_QDM) {
					CLogger::Log(CLogType::NORM, "QDM enabled. Increasing refresh resolution to " + to_string(RefreshResolution) + ".", "CRadarDisplay::OnClickScreenObject");
					RefreshResolution = 0.04;
				}
				menuBar->ButtonPress(atoi(sObjectId), Button, this);

				// Open CPDLC window immediately when toggled on
				if (atoi(sObjectId) == CMenuBar::BTN_CPDLC && cpdlcWindow != nullptr) {
					cpdlcWindow->IsClosed = false;
				}
			}
			else {
				// Disable all QDM if it is the QDM button
				if (atoi(sObjectId) == CMenuBar::BTN_QDM) {
					CLogger::Log(CLogType::NORM, "QDM disabled. Resetting refresh resolution to " + to_string(RefreshResolution) + " and clearing lat/lon values.", "CRadarDisplay::OnClickScreenObject");
					RulerPoint1.m_Latitude = 0.0;
					RulerPoint1.m_Longitude = 0.0;
					RulerPoint2.m_Latitude = 0.0;
					RulerPoint2.m_Longitude = 0.0;

					// Reset refresh resolution
					RefreshResolution = 0.2;
				}
				menuBar->ButtonUnpress(atoi(sObjectId), Button, this);

				// Close CPDLC window when toggled off
				if (atoi(sObjectId) == CMenuBar::BTN_CPDLC && cpdlcWindow != nullptr) {
					cpdlcWindow->IsClosed = true;
				}
			}
		}
	} else if (ObjectType == WIN_FLTPLN) {
		if (!fltPlnWindow->IsButtonPressed(atoi(sObjectId))) {
			fltPlnWindow->ButtonPress(atoi(sObjectId));
		}
		else {
			fltPlnWindow->ButtonUnpress(atoi(sObjectId));
		}
	} else if (ObjectType == WIN_CPDLC) {
		std::string oid = sObjectId;
		// List selection (messages / aircraft)
		if (oid.rfind("CPDLC_MSG_", 0) == 0) {
			try { cpdlcWindow->SelectMessage(std::stoi(oid.substr(10))); } catch (...) {}
			return;
		}
		if (oid.rfind("CPDLC_AC_", 0) == 0) {
			cpdlcWindow->SelectAircraft(oid.substr(9));
			return;
		}

		// Text inputs (open edit popup)
		if (cpdlcWindow->IsTextInput(atoi(sObjectId))) {
			// EuroScope popup edit routes back to OnFunctionCall(FunctionId, ...)
			// FunctionId must be unique across the whole plugin.
			GetPlugIn()->OpenPopupEdit(Area, atoi(sObjectId), cpdlcWindow->GetTextValue(atoi(sObjectId)).c_str());
			return;
		}

		// Dropdown items / toggles (IDs >= 800)
		if (sObjectId && isdigit((unsigned char)sObjectId[0])) {
			int id = atoi(sObjectId);
			if (id >= 800) {
				if (!cpdlcWindow->IsButtonPressed(id)) cpdlcWindow->ButtonPress(id);
				else cpdlcWindow->ButtonUnpress(id);
				return;
			}
		}
	} else if (ObjectType == WIN_FDD) {
		fddWindow->HandleButton(sObjectId, this);
		if (sObjectId && isdigit((unsigned char)sObjectId[0])) {
			fddWindow->SetButtonState(atoi(sObjectId), CInputState::ACTIVE);
		}
		if (atoi(sObjectId) == CFddWindow::BTN_CLOSE) {
			fddWindow->IsClosed = true;
			menuBar->SetButtonState(CMenuBar::BTN_FDD, CInputState::INACTIVE);
		}
	}
	
	// Left button actions
	if (Button == BUTTON_LEFT) {
		// If screen object is a tag
		if (ObjectType == SCREEN_TAG || ObjectType == SCREEN_TAG_CS) {
			// Set FPW to re-instantiate if the aircraft is not the currently ASELed one
			bool newFP = false;
			if (asel != sObjectId && !fltPlnWindow->IsClosed) {
				newFP = true;
			}

			// Set the ASEL
			asel = sObjectId;
			CFlightPlan fp = GetPlugIn()->FlightPlanSelect(sObjectId);
			GetPlugIn()->SetASELAircraft(fp);
			CLogger::Log(CLogType::NORM, "Selected aircraft changed to " + asel + ".", "CRadarDisplay::OnClickScreenObject");
			
			// Re-instantiate FPW if needed
			if (newFP) {
				fltPlnWindow->Instantiate(this, asel);
			}

			// Probing tools
			if (menuBar->IsButtonPressed(CMenuBar::BTN_PIV)
				|| menuBar->IsButtonPressed(CMenuBar::BTN_RBL)
				|| menuBar->IsButtonPressed(CMenuBar::BTN_SEP)) {
				// Make sure flight plans are valid
				CAircraftFlightPlan* fpAsel = CDataHandler::GetFlightData(asel);
				if (aircraftSel1 == "" && fpAsel && fpAsel->IsValid) {
					aircraftSel1 = asel;
				}
				else if (aircraftSel2 == "" && aircraftSel1 != asel && fpAsel && fpAsel->IsValid) {
					aircraftSel2 = asel;
				}
			}

			if (fp.GetCallsign() == aircraftSel2 && menuBar->IsButtonPressed(CMenuBar::BTN_PIV)) {
				CConflictDetection::PIVTool(this, aircraftSel1, aircraftSel2);
			}
		}

		// Flight plan button
		if (atoi(sObjectId) == CMenuBar::BTN_FLIGHTPLAN && menuBar->IsButtonPressed(CMenuBar::BTN_FLIGHTPLAN)) {
			fltPlnWindow->Instantiate(this, asel);
		}

		if (!menuBar->IsButtonPressed(CMenuBar::BTN_FLIGHTPLAN)) {
			fltPlnWindow->IsClosed = true;
		}

		// Qck Look button
		if (atoi(sObjectId) == CMenuBar::BTN_EXT) {
			CLogger::Log(CLogType::NORM, "Extended tags enabled.", "CRadarDisplay::OnClickScreenObject");
			aselDetailed = false;
		}

		// Detailed button
		if (atoi(sObjectId) == CMenuBar::BTN_DETAILED) {
			CLogger::Log(CLogType::NORM, "Detailed tag enabled for currently selected aircraft. Current ASEL: " + asel + ".", "CRadarDisplay::OnClickScreenObject");
			aselDetailed = true;
		}

		// If the button is the PIV button
		if (atoi(sObjectId) == CMenuBar::BTN_PIV) {
			CLogger::Log(CLogType::NORM, "PIV tool activated.", "CRadarDisplay::OnClickScreenObject");
			// Erase RBL (if active)
			if (menuBar->IsButtonPressed(CMenuBar::BTN_RBL)) {
				menuBar->SetButtonState(CMenuBar::BTN_RBL, CInputState::INACTIVE);
				CLogger::Log(CLogType::NORM, "Deactivating RBL tool.", "CRadarDisplay::OnClickScreenObject");
			}

			// Erase SEP (if active)
			if (menuBar->IsButtonPressed(CMenuBar::BTN_SEP)) {
				CLogger::Log(CLogType::NORM, "Deactivating SEP tool.", "CRadarDisplay::OnClickScreenObject");
				menuBar->SetButtonState(CMenuBar::BTN_SEP, CInputState::INACTIVE);
			}

			// If PIV already active then clear everything
			if (!CConflictDetection::PIVSeparationStatuses.empty()) {
				CConflictDetection::PIVLocations1.clear();
				CConflictDetection::PIVLocations2.clear();
				CConflictDetection::PIVSeparationStatuses.clear();
				CLogger::Log(CLogType::NORM, "PIV already active. Clearing old PIV.", "CRadarDisplay::OnClickScreenObject");
			}
			// Reset ASELs
			aircraftSel1 = "";
			aircraftSel2 = "";
		}

		// If the button is the RBL button
		if (atoi(sObjectId) == CMenuBar::BTN_RBL) {
			CLogger::Log(CLogType::NORM, "RBL tool activated.", "CRadarDisplay::OnClickScreenObject");
			// Erase PIV (if active)
			if (menuBar->IsButtonPressed(CMenuBar::BTN_PIV)) {
				CLogger::Log(CLogType::NORM, "Deactivating PIV tool.", "CRadarDisplay::OnClickScreenObject");
				menuBar->SetButtonState(CMenuBar::BTN_PIV, CInputState::INACTIVE);
			}

			// Erase SEP (if active)
			if (menuBar->IsButtonPressed(CMenuBar::BTN_SEP)) {
				CLogger::Log(CLogType::NORM, "Deactivating SEP tool.", "CRadarDisplay::OnClickScreenObject");
				menuBar->SetButtonState(CMenuBar::BTN_SEP, CInputState::INACTIVE);
			}

			// Reset ASELs
			aircraftSel1 = "";
			aircraftSel2 = "";
		}

		// If the button is the SEP button
		if (atoi(sObjectId) == CMenuBar::BTN_SEP) {
			CLogger::Log(CLogType::NORM, "SEP tool activated.", "CRadarDisplay::OnClickScreenObject");
			// Erase RBL (if active)
			if (menuBar->IsButtonPressed(CMenuBar::BTN_RBL)) {
				CLogger::Log(CLogType::NORM, "Deactivating RBL tool.", "CRadarDisplay::OnClickScreenObject");
				menuBar->SetButtonState(CMenuBar::BTN_RBL, CInputState::INACTIVE);
			}

			// Erase PIV (if active)
			if (menuBar->IsButtonPressed(CMenuBar::BTN_PIV)) {
				CLogger::Log(CLogType::NORM, "Deactivating PIV tool.", "CRadarDisplay::OnClickScreenObject");
				menuBar->SetButtonState(CMenuBar::BTN_PIV, CInputState::INACTIVE);
			}

			// Reset ASELs
			aircraftSel1 = "";
			aircraftSel2 = "";
		}

		// If a menu text entry
		if (ObjectType == ALTFILT_TEXT) {
			// If the low altitude filter
			if (string(sObjectId) == "ALTFILT_LOW") {
				GetPlugIn()->OpenPopupEdit(Area, FUNC_ALTFILT_LOW, "");
			}
			// If the high altitude filter
			if (string(sObjectId) == "ALTFILT_HIGH") {
				GetPlugIn()->OpenPopupEdit(Area, FUNC_ALTFILT_HIGH, "");
			}
		}

		if (ObjectType == MENBAR) {
			if (string(sObjectId) == to_string(menuBar->TXT_SEARCH))
				GetPlugIn()->OpenPopupEdit(Area, atoi(sObjectId), "");
		}

		// Setup window
		if (ObjectType == WIN_SETUP) {
			CLogger::Log(CLogType::NORM, "WIN_SETUP Click detected. ID: " + string(sObjectId), "CRadarDisplay::OnClickScreenObject");
			if (setupWindow->IsTextInput(atoi(sObjectId))) {
				GetPlugIn()->OpenPopupEdit(Area, atoi(sObjectId), setupWindow->GetTextValue(atoi(sObjectId)).c_str());
			}
			else {
				CLogger::Log(CLogType::NORM, "WIN_SETUP Click on non-text object. ID: " + string(sObjectId), "CRadarDisplay::OnClickScreenObject");
			}
		}

		// If a flight plan window text entry
		if (ObjectType == WIN_FLTPLN) {
				// Only allow editing when the input is actually active.
				// The previous condition used (A != DISABLED || A != INACTIVE) which is always true.
				if (fltPlnWindow->IsTextInput(atoi(sObjectId)) &&
					(fltPlnWindow->GetInputState(atoi(sObjectId)) != CInputState::DISABLED &&
					 fltPlnWindow->GetInputState(atoi(sObjectId)) != CInputState::INACTIVE)) {
					GetPlugIn()->OpenPopupEdit(Area, atoi(sObjectId), fltPlnWindow->GetTextValue(atoi(sObjectId)).c_str());
				}
			}

		// If it is a hide show button for a list
		if (ObjectType == LIST_INBOUND) {
			if (string(sObjectId) == "HIDESHOW") {
				inboundList->HideShowButton = inboundList->HideShowButton == true ? false : true;
				CLogger::Log(CLogType::NORM, "Toggling Inbound list. Current status: " + to_string(inboundList->HideShowButton), "CRadarDisplay::OnClickScreenObject");
			}
		}
		if (ObjectType == LIST_OTHERS) {
			if (string(sObjectId) == "HIDESHOW") {
				otherList->HideShowButton = otherList->HideShowButton == true ? false : true;
				CLogger::Log(CLogType::NORM, "Toggling Other list. Current status: " + to_string(inboundList->HideShowButton), "CRadarDisplay::OnClickScreenObject");
			}
		}

		// If it is a message
		if (ObjectType == ACTV_MESSAGE) {
			msgWindow->SelectedMessage = atoi(sObjectId);
		}

		// If a coordination
		if (ObjectType == WIN_FLTPLN_TSFR) {
			if (GetPlugIn()->FlightPlanSelect(fltPlnWindow->primedPlan->Callsign.c_str()).GetTrackingControllerIsMe() &&
				string(GetPlugIn()->FlightPlanSelect(fltPlnWindow->primedPlan->Callsign.c_str()).GetHandoffTargetControllerCallsign()) == "") {
				fltPlnWindow->selectedAuthority = sObjectId;
			}
		}
	}

	if (Button == BUTTON_RIGHT) {
		if (ObjectType == SCREEN_TAG || ObjectType == SCREEN_TAG_CS || ObjectType == 2 || ObjectType == 3) {
			/// Set route drawing
			// Make sure flight plan exists otherwise it will crash, and also that they aren't PIV aircraft
			CAircraftFlightPlan* fpObj = CDataHandler::GetFlightData(string(sObjectId));
			if (fpObj && fpObj->IsValid && string(sObjectId) != aircraftSel1 && string(sObjectId) != aircraftSel2) {
				int found = -1; // Found flag so we can remove if needed
				for (int i = 0; i < CRoutesHelper::ActiveRoutes.size(); i++) {
					// If the route is currently on the screen
					if (CRoutesHelper::ActiveRoutes[i] == sObjectId) {
						// Set to remove
						found = i;
						break;
					}
				}

				// Erase if the item was found, otherwise add
				if (found != -1) {
					CLogger::Log(CLogType::NORM, "Erasing route draw for: " + string(sObjectId), "CRadarDisplay::OnClickScreenObject");
					CRoutesHelper::ActiveRoutes.erase(CRoutesHelper::ActiveRoutes.begin() + found);
				}
				else {
					CLogger::Log(CLogType::NORM, "Enabling route draw for: " + string(sObjectId), "CRadarDisplay::OnClickScreenObject");
					// Ensure route is calculated
					CAircraftFlightPlan* fp = CDataHandler::GetFlightData(string(sObjectId));
					if (fp && fp->Route.empty()) {
						CDataHandler::UpdateFlightData(this, string(sObjectId), true);
					}
					CRoutesHelper::ActiveRoutes.push_back(string(sObjectId));
				}
			}
		}
	}

	CLogger::Log(CLogType::NORM, "Screen refresh forced.", "CRadarDisplay::OnClickScreenObject");
	RequestRefresh();
}

void CRadarDisplay::OnButtonDownScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	CLogger::Log(CLogType::NORM, "OnButtonDownScreenObject: Type=" + to_string(ObjectType) + " ID=" + string(sObjectId), "CRadarDisplay::OnButtonDownScreenObject");

	// Track info window
	if (ObjectType == WIN_TCKINFO) {
		trackWindow->ButtonDown(atoi(sObjectId));
	}

	// FDD Window
	if (ObjectType == WIN_FDD) {
		fddWindow->ButtonDown(atoi(sObjectId));
	}

	// Flight plan window
	if (ObjectType == WIN_FLTPLN) {
		fltPlnWindow->ButtonDown(atoi(sObjectId));
	}

	// Note pad window
	if (ObjectType == WIN_NOTEPAD) {
		npWindow->ButtonDown(atoi(sObjectId));
	}

	// Message window
	if (ObjectType == WIN_MSG) {
		msgWindow->ButtonDown(atoi(sObjectId));
	}

	// CPDLC window (only numeric control IDs)
	if (ObjectType == WIN_CPDLC) {
		if (sObjectId && (isdigit((unsigned char)sObjectId[0]) || sObjectId[0] == '-'))
			cpdlcWindow->ButtonDown(atoi(sObjectId));
	}

	// Setup window
	if (ObjectType == WIN_SETUP) {
		CLogger::Log(CLogType::NORM, "WIN_SETUP ButtonDown detected. ID: " + string(sObjectId), "CRadarDisplay::OnButtonDownScreenObject");
		setupWindow->ButtonDown(atoi(sObjectId));
	}

	// Menu bar
	if (ObjectType == MENBAR) {
		menuBar->ButtonDown(atoi(sObjectId));
	}

	// Refresh
	CLogger::Log(CLogType::NORM, "Screen refresh forced.", "CRadarDisplay::OnButtonDownScreenObject");
	RequestRefresh();
}

void CRadarDisplay::OnButtonUpScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	// Track info window
	if (ObjectType == WIN_TCKINFO) {
		if (atoi(sObjectId) == CTrackInfoWindow::BTN_CLOSE) {
			// Close window if the close button
			menuBar->SetButtonState(CMenuBar::BTN_TCKINFO, CInputState::INACTIVE);
		}
		trackWindow->ButtonUp(atoi(sObjectId));
	}

	// Flight plan window
	if (ObjectType == WIN_FLTPLN) {
		if (atoi(sObjectId) == CFlightPlanWindow::BTN_CLOSE) {
			// Close window if the close button
			menuBar->SetButtonState(CMenuBar::BTN_FLIGHTPLAN, CInputState::INACTIVE);
		}
		if (atoi(sObjectId) < 200) {
			fltPlnWindow->ButtonUp(atoi(sObjectId), this);
		}
	}

	// Note pad window
	if (ObjectType == WIN_NOTEPAD) {
		if (atoi(sObjectId) == CNotePad::BTN_CLOSE) {
			// Close window if the close button
			menuBar->SetButtonState(CMenuBar::BTN_NOTEPAD, CInputState::INACTIVE);
		}
		npWindow->ButtonUp(atoi(sObjectId));
	}

	// Message window
	if (ObjectType == WIN_MSG) {
		if (atoi(sObjectId) == CMessageWindow::BTN_CLOSE) {
			// Close window if the close button
			menuBar->SetButtonState(CMenuBar::BTN_MESSAGE, CInputState::INACTIVE);
		}
		msgWindow->ButtonUp(atoi(sObjectId));
	}

	// FDD Window
	if (ObjectType == WIN_FDD) {
		string idStr = sObjectId;
		if (idStr.find(':') != string::npos) {
			fddWindow->HandleButton(idStr, this);
		}
		else {
			if (atoi(sObjectId) == CFddWindow::BTN_CLOSE) {
				menuBar->SetButtonState(CMenuBar::BTN_FDD, CInputState::INACTIVE);
			}
			fddWindow->ButtonUp(atoi(sObjectId), this);
		}
	}

	// CPDLC window (only numeric control IDs)
	if (ObjectType == WIN_CPDLC) {
		if (sObjectId && (isdigit((unsigned char)sObjectId[0]) || sObjectId[0] == '-')) {
			int id = atoi(sObjectId);
			if (id == CCPDLCWindow::BTN_CLOSE) {
				menuBar->SetButtonState(CMenuBar::BTN_CPDLC, CInputState::INACTIVE);
			}
			cpdlcWindow->ButtonUp(id, this);
		}
	}

	// Setup window
	if (ObjectType == WIN_SETUP) {
		CLogger::Log(CLogType::NORM, "WIN_SETUP ButtonUp detected. ID: " + string(sObjectId), "CRadarDisplay::OnButtonUpScreenObject");
		if (atoi(sObjectId) == CSetupWindow::BTN_CLOSE) {
			menuBar->SetButtonState(CMenuBar::BTN_SETUP, CInputState::INACTIVE);
		}
		setupWindow->ButtonUp(atoi(sObjectId), this);
	}

	// Menu bar
	if (ObjectType == MENBAR) {
		// Clear active routes
		if (atoi(sObjectId) == CMenuBar::BTN_RTEDEL) {
			CRoutesHelper::ActiveRoutes.clear();
		}
		if (atoi(sObjectId) == CMenuBar::BTN_AUTOTAG) {
			tagStatuses.clear();
		}
		menuBar->ButtonDown(atoi(sObjectId));

		// Launch FDD in browser if button activated
		if (atoi(sObjectId) == CMenuBar::BTN_FDD && menuBar->IsButtonPressed(CMenuBar::BTN_FDD)) {
			int port = CWebServer::GetRunningPort();
			if (port > 0) {
				string url = "http://localhost:" + to_string(port) + "/";
				ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
			}
		}
	}

	// Refresh
	CLogger::Log(CLogType::NORM, "Screen refresh forced.", "CRadarDisplay::OnButtonUpScreenObject");
	RequestRefresh();
}

void CRadarDisplay::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
	// Set low alt filter
	if (FunctionId == FUNC_ALTFILT_LOW) {
		// Validation (range & type)
		// Check if it is a string
		bool isNumber = true;
		for (size_t i = 0; i < strlen(sItemString); i++) { // Check if string
			if (!isdigit((unsigned char)sItemString[i])) isNumber = false;
		}
		if (isNumber && (atoi(sItemString) < 1000 && atoi(sItemString) > 0)) {
			CUtils::AltFiltLow = atoi(sItemString); // Return if in range
		}
	}
	// Set high alt filter
	if (FunctionId == FUNC_ALTFILT_HIGH) {
		// Validation (range & type)
		bool isNumber = true;
		for (size_t i = 0; i < strlen(sItemString); i++) { // Check if string
			if (!isdigit((unsigned char)sItemString[i])) isNumber = false;
		}
		if (isNumber && (atoi(sItemString) < 999 && atoi(sItemString) > 0)) {
			CUtils::AltFiltHigh = atoi(sItemString); // Return if in range
		}
	}
	if (FunctionId == CMenuBar::TXT_SEARCH) {
		string itemString = string(sItemString);
		string value;
		for (size_t i = 0; i < itemString.size(); i++) {
			char c = itemString[i];
			if (isalpha((unsigned char)c))
				c = toupper((unsigned char)c);
			value += c;
		}
		if (GetPlugIn()->RadarTargetSelect(value.c_str()).IsValid()) {
			CAcTargets::SearchedAircraft = value;
			CAcTargets::fiveSecondTimer = clock();
			GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(value.c_str()));
		}
	}

	// If it is a flight plan window text input
	if (fltPlnWindow->IsTextInput(FunctionId)) {
		fltPlnWindow->SetTextValue(this, FunctionId, string(sItemString));
	}

	// CPDLC window text input
	if (cpdlcWindow->IsTextInput(FunctionId)) {
		cpdlcWindow->SetTextValue(this, FunctionId, string(sItemString));
	}

	// Setup window text input
	if (setupWindow->IsTextInput(FunctionId)) {
		setupWindow->SetTextValue(this, FunctionId, string(sItemString));
	}
}

void CRadarDisplay::OnDoubleClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	// If it is a message
	if (ObjectType == ACTV_MESSAGE) {
		msgWindow->ButtonDoubleClick(this, atoi(sObjectId), fltPlnWindow);
	}
	// If it concerns the flight plan window
	if (ObjectType == WIN_FLTPLN) {
		fltPlnWindow->ButtonDoubleClick(atoi(sObjectId));
	}
	// If it is an aircraft callsign
	if (ObjectType == SCREEN_TAG_CS) {
		//CAcTargets::OpenTrackingDialog = sObjectId;
	}
}

void CRadarDisplay::OnAsrContentToBeSaved(void)
{
	// Buttons
	CUtils::TagsEnabled = menuBar->IsButtonPressed(CMenuBar::BTN_TAGS) ? true : false;
	CUtils::GridEnabled = menuBar->IsButtonPressed(CMenuBar::BTN_GRID) ? true : false;
	CUtils::OverlayEnabled = menuBar->IsButtonPressed(CMenuBar::BTN_OVERLAYS) ? true : false;
	CUtils::QckLookEnabled = menuBar->IsButtonPressed(CMenuBar::BTN_QCKLOOK) ? true : false; // TODO: Change this to Ext
	
	// Save
	CUtils::SavePluginData(this);
	CLogger::Log(CLogType::NORM, "Saving plugin data...", "CRadarDisplay::OnAsrContentToBeSaved");
}

void CRadarDisplay::OnAsrContentLoaded(bool Loaded)
{
	if (!Loaded)
		return;

	// Load the plugin data
	CUtils::LoadPluginData(this);

	// Populate it
	PopulateProgramData();
}

// TODO: Break into individual methods or create ScreenFunctions class/namespace
void CRadarDisplay::CursorStateUpdater(void* args)
{
	// Pointer to cursor
	CAppCursor* cursor = (CAppCursor*)args;

	try {
		// Timer
		clock_t hundredmsTimer = clock();
		clock_t refreshTimer = clock();

		// Monitor information for proper positioning
		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(MONITORINFO);

		// Get the process information - infinite loop in separate thread = bad unless you manually break the loop on application close
		DWORD activeCode;
		while (!cursor->isESClosed) {
			HANDLE hnd = CUtils::GetESProcess();
			GetExitCodeProcess(hnd, &activeCode);
			// Check if the app is still active
			if (cursor->isESClosed || activeCode != STILL_ACTIVE) {
				CLogger::Log(CLogType::NORM, "ES quitting. Thread destroyed.");
				break; // Break if not
			}

			// Ok so the app is still active let's get the cursor data
			if (((double)(clock() - hundredmsTimer) / ((double)CLOCKS_PER_SEC)) >= 0.04) { // Greater than or equal to 40ms
				// Get cursor position (only if previous cursor position is inside the radar area)
				bool isCursorInsideRadarArea = false;
				CRect radarArea = cursor->screen->GetRadarArea();

				// Get the position and monitor in which the point lies
				GetCursorPos(&cursor->position);

				// Get the monitor
				HMONITOR monitor = MonitorFromPoint(cursor->position, MONITOR_DEFAULTTONEAREST);
				GetMonitorInfo(monitor, &monitorInfo);

				// Get the monitor resolution
				int monResX = abs(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);
				int monResY = abs(monitorInfo.rcMonitor.top - monitorInfo.rcMonitor.bottom);

				// Get the relative cursor position
				if (cursor->position.x > monResX) { // greater than (x)
					cursor->position.x = cursor->position.x % monResX;
				}
				if (cursor->position.y > monResY) { // greater than (y)
					cursor->position.y = cursor->position.y % monResY;
				}
				if (cursor->position.x < 0) { // less than (x)
					cursor->position.x = monResX - (abs(cursor->position.x) % monResX);
				}
				if (cursor->position.y < 0) { // less than (y)
					cursor->position.y = monResY - (abs(cursor->position.y) % monResY);
				}

				// Get button presses
				bool leftBtnPressed = (GetAsyncKeyState(VK_LBUTTON) & (1 << 15)) != 0;
				bool rightBtnPressed = (GetAsyncKeyState(VK_RBUTTON) & (1 << 15)) != 0;

				// Check if cursor inside radar area
				if (cursor->position.x > radarArea.left&&
					cursor->position.x < radarArea.right &&
					cursor->position.y > radarArea.top + MENBAR_HEIGHT && // We want *our* radar screen so we add the vNAAATS menu bar height
					cursor->position.y < radarArea.bottom) {
					isCursorInsideRadarArea = true;
				}

				// Lat/lon position
				cursor->latLonPosition = cursor->screen->ConvertCoordFromPixelToPosition(cursor->position);

				// Check button presses
				if ((!leftBtnPressed && !rightBtnPressed) || !isCursorInsideRadarArea) {
					cursor->button = 0;
				}

				/// Events!
				// On left click
				if (leftBtnPressed && cursor->button == 0) {
					// QDM button
					if (cursor->screen->menuBar->IsButtonPressed(CMenuBar::BTN_QDM) && isCursorInsideRadarArea) {
						bool pointSet = false;
						// Activate QDM, first check if first ruler point already filled
						if (cursor->screen->RulerPoint1.m_Latitude == 0.0 && cursor->screen->RulerPoint1.m_Longitude == 0.0) {
							// It isn't filled so set it
							cursor->screen->RulerPoint1.m_Latitude = cursor->latLonPosition.m_Latitude;
							cursor->screen->RulerPoint1.m_Longitude = cursor->latLonPosition.m_Longitude;

							// So that we don't accidently set the 2nd point at the same time
							pointSet = true;
						}
						// Check the 2nd point
						if (!pointSet && (cursor->screen->RulerPoint2.m_Latitude == 0.0 && cursor->screen->RulerPoint2.m_Longitude == 0.0)) {
							// It isn't filled so set it
							cursor->screen->RulerPoint2.m_Latitude = cursor->latLonPosition.m_Latitude;
							cursor->screen->RulerPoint2.m_Longitude = cursor->latLonPosition.m_Longitude;

							// So we dont cancel the QDM automatically
							pointSet = true;
						}

						// Both points are down so we need to reset
						if (!pointSet && (cursor->screen->RulerPoint2.m_Latitude != 0.0 && cursor->screen->RulerPoint2.m_Longitude != 0.0)) {
							cursor->screen->RulerPoint1.m_Latitude = 0.0;
							cursor->screen->RulerPoint1.m_Longitude = 0.0;
							cursor->screen->RulerPoint2.m_Latitude = 0.0;
							cursor->screen->RulerPoint2.m_Longitude = 0.0;
						}
					}

					// Set the button so the event doesn't fire again
					cursor->button = 2;
				}
				// On right click
				if (rightBtnPressed && cursor->button == 0) {

					// Set the button so the event doesn't fire again
					cursor->button = 1;
				}

				// Reset clock
				hundredmsTimer = clock();
			}

			// Call refresh sequence more often
			if (((double)(clock() - refreshTimer) / ((double)CLOCKS_PER_SEC)) >= cursor->screen->RefreshResolution) {
				// Refresh the radar screen
				cursor->screen->RequestRefresh();

				// Reset clock
				refreshTimer = clock();
			}

		}

		// Check if the app is still active
		if (cursor->isESClosed) {
			CLogger::Log(CLogType::NORM, "ES quitting. Thread destroyed.");
		}

		
	}
	catch (exception & ex) {
		CLogger::DebugLog(cursor->screen, "An exception occurred in the CursorStateUpdater. " + *ex.what());
		CLogger::Log(CLogType::ERR, "An error occurred. \nCursor position: " + to_string(cursor->latLonPosition.m_Latitude) + "," + to_string(cursor->latLonPosition.m_Longitude)
			+ "\nRefresh Resolution: " + to_string(cursor->screen->RefreshResolution) + "\nVerbose details: " + *ex.what(), "CRadarDisplay::CursorStateUpdater");
	}

	// Clean up and return
	delete args;

	return;
}
