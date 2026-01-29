#include "pch.h"
#include <WinInet.h>
#pragma comment(lib, "wininet.lib")
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#include "DataHandler.h"



#include "RoutesHelper.h"



#include "Constants.h"

#include <sstream>
#include <thread>
#include <atomic>



using json = nlohmann::json;







// Flight data map
map<string, CAircraftFlightPlan> CDataHandler::flights;

// VATSIM Data Cache
map<string, string> CDataHandler::vatsimRouteCache;
mutex CDataHandler::vatsimCacheMutex;
bool CDataHandler::vatsimFetcherRunning = false;
thread CDataHandler::vatsimFetcherThread;

// NATTrak clearance storage



map<string, CNatTrakClearance> CDataHandler::natTrakClearances;



time_t CDataHandler::lastNatTrakFetch = 0;







// Version check URL (update to your own repo)



const string CDataHandler::PluginVersion = "https://raw.githubusercontent.com/garythms/vatsim-NAAATS-v2/master/pluginversion.txt";







// NAT Track URL - NOW USES NATTRAK API



const string CDataHandler::TrackURL = "https://nattrak.vatsim.net/api/tracks";







// NATTrak clearance API URL



const string CDataHandler::NatTrakClearanceURL = "https://nattrak.vatsim.net/api/plugins";







// DEFUNCT - kept as empty strings for compatibility



const string CDataHandler::TrackSource = "";



const string CDataHandler::GetSingleAircraft = "";



const string CDataHandler::FlightDataUpdate = "";



const string CDataHandler::PostSingleAircraft = "";







int CDataHandler::CheckPluginVersion(CPlugIn* plugin) {

	// Disabled: avoid disruptive update popups in EuroScope.

	// (Kept for compatibility with older code paths.)

	(void)plugin;

	return -1;





	// Try and get data and pass into string



	string responseString = "";



	try {



		// Convert URL to LPCSTR type



		LPCSTR lpcURL = CDataHandler::PluginVersion.c_str();







		// Delete cache data



		DeleteUrlCacheEntry(lpcURL);







		// Download data



		CComPtr<IStream> pStream;



		HRESULT hr = URLOpenBlockingStream(NULL, lpcURL, &pStream, 0, NULL);



		// If failed



		if (FAILED(hr)) {



			int code = (int)hr;



			// Show user message



			plugin->DisplayUserMessage("vNAAATS", "Error", "Failed to fetch version info. Code: " + code, true, true, true, true, true);



			// Clogger



			CLogger::Log(CLogType::ERR, "Could not fetch version info. Code: " + code, "CDataHandler::CheckPluginVersion");



			return -1;



		}



		// Put data into buffer



		char tempBuffer[16384];



		DWORD bytesRead = 0;



		hr = pStream->Read(tempBuffer, sizeof(tempBuffer), &bytesRead);



		// Put data into string



		for (int i = 0; i < bytesRead; i++) {



			responseString += tempBuffer[i];



		}



	}



	catch (exception & e) {



		// Log to ES



		plugin->DisplayUserMessage("vNAAATS", "Error", string("Failed to fetch version info: " + string(e.what())).c_str(), true, true, true, true, true);



		// Clogger



		CLogger::Log(CLogType::EXC, "Could not fetch version info: " + string(string(e.what())), "CDataHandler::CheckPluginVersion");



		return -1;



	}







	// Check version



	if (responseString != PLUGIN_VERSION) {



		// Display dialog if update available



		int msgBox = MessageBox(NULL, (LPCSTR)("A new version of vNAAATS (" + responseString + ") is now available. Your version: " + PLUGIN_VERSION +



			"\nPlease update as soon as possible to avoid possible compatibility issues.\nFind the new version at GitHub.").c_str(),



			(LPCSTR)"vNAAATS Version Notification", MB_ICONWARNING | MB_OK);







		if (msgBox == IDOK) {



			// Open the website



			ShellExecute(NULL, "open", "https://github.com/garythms/vatsim-NAAATS-v2", NULL, NULL, SW_SHOWNORMAL);



		}



		return msgBox;



	}







	return -1;



}







// Helper function to parse track waypoint strings into coordinates



// Formats: "ELSIR" (named), "50/50" (50N 50W), "5230/40" (52.5N 40W)



CPosition CDataHandler::ParseTrackWaypoint(const string& waypoint) {



	CPosition pos;



	pos.m_Latitude = 0.0;



	pos.m_Longitude = 0.0;



	



	// Check if it contains a slash (coordinate format)



	size_t slashPos = waypoint.find('/');



	if (slashPos == string::npos) {



		// Named waypoint - return 0,0, RoutesHelper will resolve it



		return pos;



	}



	



	// Parse lat/lon from format like "50/50", "5230/40", "5330/30"



	string latPart = waypoint.substr(0, slashPos);



	string lonPart = waypoint.substr(slashPos + 1);



	



	try {



		double lat = 0.0;



		double lon = 0.0;



		



		// Parse latitude



		if (latPart.length() <= 2) {



			// Simple format: "50" = 50N



			lat = stod(latPart);



		}



		else if (latPart.length() == 4) {



			// Half-degree format: "5230" = 52 30' = 52.5N



			lat = stod(latPart.substr(0, 2)) + stod(latPart.substr(2, 2)) / 60.0;



		}



		else {



			lat = stod(latPart);



			if (lat > 90) lat /= 100.0;



		}



		



		// Parse longitude (always West in NAT, so negative)



		if (lonPart.length() <= 2) {



			// Simple format: "50" = 50W



			lon = -stod(lonPart);



		}



		else if (lonPart.length() == 4) {



			// Half-degree format: "5030" = 50 30' = 50.5W



			lon = -(stod(lonPart.substr(0, 2)) + stod(lonPart.substr(2, 2)) / 60.0);



		}



		else {



			lon = -stod(lonPart);



			if (lon < -180) lon /= 100.0;



		}



		



		pos.m_Latitude = lat;



		pos.m_Longitude = lon;



	}



	catch (...) {



		// Parse error - return 0,0



	}



	



	return pos;



}







int CDataHandler::PopulateLatestTrackData(CPlugIn* plugin) {



	// Try and get data and pass into string



	string responseString;



	try {



		// Convert URL to LPCSTR type



		LPCSTR lpcURL = TrackURL.c_str();







		// Delete cache data



		DeleteUrlCacheEntry(lpcURL);







		// Download data



		CComPtr<IStream> pStream;



		HRESULT hr = URLOpenBlockingStream(NULL, lpcURL, &pStream, 0, NULL);



		// If failed



		if (FAILED(hr)) {



			int code = (int)hr;



			// Show user message



			plugin->DisplayUserMessage("vNAAATS", "Error", "Track data download failed. Code: " + code, true, true, true, true, true);



			// Clogger



			CLogger::Log(CLogType::ERR, "Could not connect to natTrak tracks API. Code: " + code, "CDataHandler::PopulateLatestTrackData");



			return 1;



		}



		// Put data into buffer - increased size for natTrak response



		char tempBuffer[65536];



		DWORD bytesRead = 0;



		hr = pStream->Read(tempBuffer, sizeof(tempBuffer) - 1, &bytesRead);



		tempBuffer[bytesRead] = '\0';



		responseString = string(tempBuffer, bytesRead);



	}



	catch (exception & e) {



		// Log to ES



		plugin->DisplayUserMessage("vNAAATS", "Error", string("Failed to load NAT Track data: " + string(e.what())).c_str(), true, true, true, true, true);



		// Clogger



		CLogger::Log(CLogType::EXC, "Failed to load NAT Track data: " + string(string(e.what())), "CDataHandler::PopulateLatestTrackData");



		return 1;



	}







	// Parse the json



	try {



		// Clear old tracks



		if (!CRoutesHelper::CurrentTracks.empty()) {



			CRoutesHelper::CurrentTracks.clear();



		}







		// Parse JSON - natTrak returns array directly [...]



		auto jsonArray = json::parse(responseString);



		



		// TMI derived from valid_from date



		string currentTMI = "";



		



		for (size_t i = 0; i < jsonArray.size(); i++) {



			// Skip inactive tracks



			if (jsonArray[i].contains("active") && !jsonArray[i].at("active").get<bool>()) {



				continue;



			}



			



			// Make track



			CTrack track;







			// Identifier (natTrak uses "identifier" not "id")



			track.Identifier = jsonArray[i].at("identifier").get<string>();







			// TMI - derive from valid_from date



			if (jsonArray[i].contains("valid_from") && !jsonArray[i].at("valid_from").is_null()) {



				string validFrom = jsonArray[i].at("valid_from").get<string>();



				if (validFrom.length() >= 10) {



					currentTMI = validFrom.substr(8, 2); // Extract day



				}



			}



			track.TMI = currentTMI;



			CRoutesHelper::CurrentTMI = currentTMI;







			// Direction (natTrak uses string "east"/"west" not int)



			string dirStr = jsonArray[i].at("direction").get<string>();



			if (dirStr == "west") {



				track.Direction = CTrackDirection::WEST;



			}



			else if (dirStr == "east") {



				track.Direction = CTrackDirection::EAST;



			}



			else {



				track.Direction = CTrackDirection::UNKNOWN;



			}







			// Route - parse from "last_routeing" string



			// Format: "ELSIR 50/50 5230/40 5330/30 54/20 DOGAL BEXET"



			string routeStr = jsonArray[i].at("last_routeing").get<string>();



			vector<string> routeParts;



			CUtils::StringSplit(routeStr, ' ', &routeParts);



			



			for (const string& part : routeParts) {



				track.Route.push_back(part);



				CPosition pos = ParseTrackWaypoint(part);



				// Add position to RouteRaw (may be 0,0 for named waypoints)



				// RenderTracks will resolve named waypoints from sector file



				track.RouteRaw.push_back(pos);



			}







			// Flight levels - natTrak may return FL values directly (330, 340)



			// or in feet (33000, 34000) - handle both cases



			for (size_t j = 0; j < jsonArray[i].at("flight_levels").size(); j++) {



				int flValue = jsonArray[i].at("flight_levels")[j].get<int>();



				// Check if the value is in feet (> 1000) or already FL format



				if (flValue > 1000) {



					// It's in feet, convert to FL



					track.FlightLevels.push_back(flValue / 100);



				} else {



					// It's already in FL format



					track.FlightLevels.push_back(flValue);



				}



			}







			// Validity times (natTrak uses snake_case)



			if (jsonArray[i].contains("valid_from") && !jsonArray[i].at("valid_from").is_null()) {



				track.validFrom = jsonArray[i].at("valid_from").get<string>();



			}



			if (jsonArray[i].contains("valid_to") && !jsonArray[i].at("valid_to").is_null()) {



				track.validTo = jsonArray[i].at("valid_to").get<string>();



			}







			// Push track to tracks array



			CRoutesHelper::CurrentTracks.insert(make_pair(track.Identifier, track));



		}



		



		// Success message



		string message = "Track data loaded successfully from natTrak.";



		if (!CRoutesHelper::CurrentTMI.empty()) {



			message += " TMI is " + CRoutesHelper::CurrentTMI + ".";



		}



		message += " " + to_string(CRoutesHelper::CurrentTracks.size()) + " active tracks.";



		plugin->DisplayUserMessage("Message", "vNAAATS Plugin", message.c_str(), false, false, false, false, false);



		CLogger::Log(CLogType::NORM, message, "CDataHandler::PopulateLatestTrackData");



		return 0;



	}

	catch (exception & e) {
		plugin->DisplayUserMessage("vNAAATS", "Error", string("Failed to parse natTrak track JSON: " + string(e.what())).c_str(), true, true, true, true, true);
		CLogger::Log(CLogType::EXC, "Failed to parse natTrak track JSON: " + string(e.what()), "CDataHandler::PopulateLatestTrackData");
		return 1;
	}
}

void CDataHandler::PopulateLatestTrackDataAsync(CPlugIn* plugin) {
	static atomic<bool> isTrackUpdateRunning(false);
	if (isTrackUpdateRunning) return;

	thread t([plugin]() {
		isTrackUpdateRunning = true;
		try {
			string responseString;
			LPCSTR lpcURL = TrackURL.c_str();
			DeleteUrlCacheEntry(lpcURL);

			CComPtr<IStream> pStream;
			HRESULT hr = URLOpenBlockingStream(NULL, lpcURL, &pStream, 0, NULL);

			if (FAILED(hr)) {
				CLogger::Log(CLogType::ERR, "Async: Could not connect to natTrak tracks API. Code: " + to_string((int)hr), "CDataHandler::PopulateLatestTrackDataAsync");
				isTrackUpdateRunning = false;
				return;
			}

			char tempBuffer[65536];
			DWORD bytesRead = 0;
			hr = pStream->Read(tempBuffer, sizeof(tempBuffer) - 1, &bytesRead);
			tempBuffer[bytesRead] = '\0';
			responseString = string(tempBuffer, bytesRead);

			// Parse
			auto jsonArray = json::parse(responseString);

			{
				lock_guard<mutex> lock(CRoutesHelper::TracksMutex);
				if (!CRoutesHelper::CurrentTracks.empty()) {
					CRoutesHelper::CurrentTracks.clear();
				}

				string currentTMI = "";
				for (size_t i = 0; i < jsonArray.size(); i++) {
					if (jsonArray[i].contains("active") && !jsonArray[i].at("active").get<bool>()) continue;

					CTrack track;
					track.Identifier = jsonArray[i].at("identifier").get<string>();

					if (jsonArray[i].contains("valid_from") && !jsonArray[i].at("valid_from").is_null()) {
						string validFrom = jsonArray[i].at("valid_from").get<string>();
						if (validFrom.length() >= 10) currentTMI = validFrom.substr(8, 2);
					}
					track.TMI = currentTMI;
					CRoutesHelper::CurrentTMI = currentTMI;

					string dirStr = jsonArray[i].at("direction").get<string>();
					if (dirStr == "west") track.Direction = CTrackDirection::WEST;
					else if (dirStr == "east") track.Direction = CTrackDirection::EAST;
					else track.Direction = CTrackDirection::UNKNOWN;

					string routeStr = jsonArray[i].at("last_routeing").get<string>();
					vector<string> routeParts;
					CUtils::StringSplit(routeStr, ' ', &routeParts);

					for (const string& part : routeParts) {
						track.Route.push_back(part);
						CPosition pos = ParseTrackWaypoint(part);
						track.RouteRaw.push_back(pos);
					}

					for (size_t j = 0; j < jsonArray[i].at("flight_levels").size(); j++) {
						int flValue = jsonArray[i].at("flight_levels")[j].get<int>();
						if (flValue > 1000) track.FlightLevels.push_back(flValue / 100);
						else track.FlightLevels.push_back(flValue);
					}

					if (jsonArray[i].contains("valid_from") && !jsonArray[i].at("valid_from").is_null())
						track.validFrom = jsonArray[i].at("valid_from").get<string>();
					if (jsonArray[i].contains("valid_to") && !jsonArray[i].at("valid_to").is_null())
						track.validTo = jsonArray[i].at("valid_to").get<string>();

					CRoutesHelper::CurrentTracks.insert(make_pair(track.Identifier, track));
				}
				CLogger::Log(CLogType::NORM, "Async: Track data loaded successfully. TMI " + CRoutesHelper::CurrentTMI + ". Tracks: " + to_string(CRoutesHelper::CurrentTracks.size()), "CDataHandler::PopulateLatestTrackDataAsync");
			}

		}
		catch (exception & e) {
			CLogger::Log(CLogType::EXC, "Async: Failed to load/parse NAT Track data: " + string(e.what()), "CDataHandler::PopulateLatestTrackDataAsync");
		}
		isTrackUpdateRunning = false;
	});
	t.detach();
}

CAircraftFlightPlan* CDataHandler::GetFlightData(string callsign) {



	if (flights.find(callsign) != flights.end()) {



		return &flights.find(callsign)->second;



	}



	// Return invalid
	static CAircraftFlightPlan invalidFp;
	invalidFp.IsValid = false;
	return &invalidFp;
}







void CDataHandler::GetFlightData(string callsign, CAircraftFlightPlan& fp) {



	if (flights.find(callsign) != flights.end()) {



		fp = flights.find(callsign)->second;



		fp.IsValid = true;



		return;



	}



	fp.IsValid = false;



}







int CDataHandler::UpdateFlightData(CRadarScreen* screen, string callsign, bool updateRoute) {



	// Flight plan



	auto fp = flights.find(callsign);







	// If flight plan not found return non-success code



	if (fp == flights.end()) {



		return 1;



	}







	// Re-initialise the route if requested
	if (updateRoute) {

		// Ensure fix cache is populated (main thread)
		CRoutesHelper::InitialiseFixCache(screen);

		CUtils::CAsyncData* data = new CUtils::CAsyncData();

		data->Screen = screen;

		data->Callsign = callsign;

		// Fetch route here in main thread
		CFlightPlan fpES = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());
		
		// If direct selection fails or returns empty route, try correlated flight plan
		const char* routeStr = fpES.IsValid() ? fpES.GetFlightPlanData().GetRoute() : nullptr;
		bool routeMissing = !fpES.IsValid() || routeStr == nullptr || routeStr[0] == '\0';
		
		if (routeMissing) {
			CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
			if (target.IsValid()) {
				CFlightPlan correlatedFP = target.GetCorrelatedFlightPlan();
				if (correlatedFP.IsValid()) {
					fpES = correlatedFP;
					routeStr = fpES.GetFlightPlanData().GetRoute();
					if (routeStr != nullptr && routeStr[0] != '\0') {
						routeMissing = false;
						CLogger::Log(CLogType::NORM, "UpdateFlightData: Used correlated flight plan for " + callsign, "CDataHandler::UpdateFlightData");
					}
				}
			}
		}

		// If still missing, try VATSIM Data API
		if (routeMissing) {
			string vatsimRoute = GetVatsimRoute(callsign);
			if (!vatsimRoute.empty()) {
				data->RawRouteString = vatsimRoute;
				routeMissing = false;
				CLogger::Log(CLogType::NORM, "UpdateFlightData: Used VATSIM API data for " + callsign, "CDataHandler::UpdateFlightData");
			}
		}

		if (fpES.IsValid() && !routeMissing) {
			CFlightPlanExtractedRoute route = fpES.GetExtractedRoute();
			if (route.GetPointsNumber() > 0) {
				for (int i = 0; i < route.GetPointsNumber(); i++) {
					CWaypoint wp;
					wp.Name = route.GetPointName(i);
					wp.Position = route.GetPointPosition(i);
					data->ExtractedRoute.push_back(wp);
				}
				data->ExtractedRouteCalculatedIndex = route.GetPointsCalculatedIndex();
			} else {
				// If extracted route is empty but we have RawRouteString (from VATSIM or ES), 
				// try to populate ExtractedRoute using FixCache so InitialiseRoute works.
				CLogger::Log(CLogType::WARN, "UpdateFlightData: Extracted route empty for " + callsign, "CDataHandler::UpdateFlightData");
				
				if (!data->RawRouteString.empty()) {
					vector<string> parts;
					CUtils::StringSplit(data->RawRouteString, ' ', &parts);
					// Ensure cache is populated (already called InitialiseFixCache above)
					lock_guard<mutex> lock(CRoutesHelper::FixCacheMutex);
					for (const string& part : parts) {
						if (CRoutesHelper::FixCache.find(part) != CRoutesHelper::FixCache.end()) {
							CWaypoint wp;
							wp.Name = part;
							wp.Position = CRoutesHelper::FixCache[part];
							data->ExtractedRoute.push_back(wp);
						}
					}
					if (!data->ExtractedRoute.empty()) data->ExtractedRouteCalculatedIndex = 0;
				}
			}
			
			if (data->RawRouteString.empty()) {
				data->RawRouteString = fpES.GetFlightPlanData().GetRoute();
			}
		} else if (!data->RawRouteString.empty()) {
			// We have VATSIM data but no valid ES flight plan, proceed with what we have
			CLogger::Log(CLogType::NORM, "UpdateFlightData: Using VATSIM data without ES flight plan for " + callsign, "CDataHandler::UpdateFlightData");
			
			vector<string> parts;
			CUtils::StringSplit(data->RawRouteString, ' ', &parts);
			lock_guard<mutex> lock(CRoutesHelper::FixCacheMutex);
			for (const string& part : parts) {
				if (CRoutesHelper::FixCache.find(part) != CRoutesHelper::FixCache.end()) {
					CWaypoint wp;
					wp.Name = part;
					wp.Position = CRoutesHelper::FixCache[part];
					data->ExtractedRoute.push_back(wp);
				}
			}
			if (!data->ExtractedRoute.empty()) data->ExtractedRouteCalculatedIndex = 0;
		} else {
			CLogger::Log(CLogType::WARN, "UpdateFlightData: Flight plan invalid and no VATSIM data for " + callsign, "CDataHandler::UpdateFlightData");
		}
		
		// Copy safe data
		data->RouteRaw = fp->second.RouteRaw;
		data->Track = fp->second.Track;

		// Capture aircraft state safely in main thread
		CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (target.IsValid()) {
			data->Position = target.GetPosition().GetPosition();
			data->Direction = CUtils::GetAircraftDirection(target.GetPosition().GetReportedHeadingTrueNorth());
			data->PositionValid = true;
		} else {
			data->PositionValid = false;
			data->Direction = true; // Default
		}

		_beginthread(CRoutesHelper::InitialiseRoute, 0, (void*) data); // Async



	}







	// Success



	return 0;



}







int CDataHandler::CreateFlightData(CRadarScreen* screen, string callsign) {



	try {



		// Euroscope flight plan and my flight plan



		CFlightPlan fpData = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());



		CAircraftFlightPlan fp;



		// Instantiate aircraft information



		fp.Callsign = callsign;



		fp.Type = fpData.GetFlightPlanData().GetAircraftFPType();



		fp.Depart = fpData.GetFlightPlanData().GetOrigin();



		fp.Dest = fpData.GetFlightPlanData().GetDestination();



		fp.Etd = CUtils::ParseZuluTime(false, atoi(fpData.GetFlightPlanData().GetEstimatedDepartureTime()));



		fp.ExitTime = fpData.GetSectorExitMinutes();



		fp.DLStatus = "false";



		fp.Sector = string(fpData.GetTrackingControllerId()) == "" ? "-1" : fpData.GetTrackingControllerId();



		fp.CurrentMessage = nullptr;



		fp.IsRelevant = fp.ExitTime != -1 ? true : false;

		fp.IsEquipped = CUtils::IsAircraftEquipped(fpData.GetFlightPlanData().GetRemarks(), fpData.GetFlightPlanData().GetAircraftInfo(), fpData.GetFlightPlanData().GetCapibilities());

		fp.Direction = CUtils::GetAircraftDirection(screen->GetPlugIn()->RadarTargetSelect(callsign.c_str()).GetPosition().GetReportedHeadingTrueNorth());

		fp.TargetMode = CUtils::GetTargetMode(screen->GetPlugIn()->RadarTargetSelect(callsign.c_str()).GetPosition().GetRadarFlags());







		// Scrape the selcal from the remarks



		string selcal = CUtils::GetSelcalCode(&fpData);



		fp.SELCAL = selcal != "" ? selcal : "N/A";







		// Get communication mode



		string comType;



		comType += toupper(fpData.GetFlightPlanData().GetCommunicationType());



		if (comType == "V") {



			comType = "VOX";



		}



		else if (comType == "T") {



			comType = "TXT";



		}



		else if (comType == "R") {



			comType = "RCV";



		}



		else {



			comType = "VOX";



		}



		fp.Communications = comType;







		// Set IsCleared



		fp.IsCleared = false;







		// Flight plan is valid



		fp.IsValid = true;



		// Add FP to map



		flights[callsign] = fp;







		// Generate route
		
		// Ensure fix cache is populated (main thread)
		CRoutesHelper::InitialiseFixCache(screen);

		CUtils::CAsyncData* data = new CUtils::CAsyncData();

		data->Screen = screen;

		data->Callsign = callsign;

		// Extract route
		CFlightPlanExtractedRoute route = fpData.GetExtractedRoute();
		for (int i = 0; i < route.GetPointsNumber(); i++) {
			CWaypoint wp;
			wp.Name = route.GetPointName(i);
			wp.Position = route.GetPointPosition(i);
			data->ExtractedRoute.push_back(wp);
		}
		data->ExtractedRouteCalculatedIndex = route.GetPointsCalculatedIndex();
		data->RawRouteString = fpData.GetFlightPlanData().GetRoute();

		// If route is missing, try VATSIM Data API
		if (data->RawRouteString.empty()) {
			string vatsimRoute = GetVatsimRoute(callsign);
			if (!vatsimRoute.empty()) {
				data->RawRouteString = vatsimRoute;
				CLogger::Log(CLogType::NORM, "CreateFlightData: Used VATSIM API data for " + callsign, "CDataHandler::CreateFlightData");
			}
		}

		// Copy safe data
		data->RouteRaw = fp.RouteRaw;
		data->Track = fp.Track;

		// Capture aircraft state safely in main thread
		CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (target.IsValid()) {
			data->Position = target.GetPosition().GetPosition();
			data->Direction = CUtils::GetAircraftDirection(target.GetPosition().GetReportedHeadingTrueNorth());
			data->PositionValid = true;
		} else {
			data->PositionValid = false;
			data->Direction = true; // Default
		}

		_beginthread(CRoutesHelper::InitialiseRoute, 0, (void*)data); // Async







		CLogger::Log(CLogType::NORM, "Flight data object for " + callsign + " generated successfully.", "CDataHandler::CreateFlightData");







		// Success



		return 0;



	}



	catch (exception & ex) {



		CLogger::Log(CLogType::EXC, "Flight data generation for " + callsign + " failed: " + string(ex.what()), "CDataHandler::CreateFlightData");



		return 1;



	}



}







int CDataHandler::DeleteFlightData(string callsign) {



	if (flights.find(callsign) != flights.end()) {



		flights.erase(callsign);



		CLogger::Log(CLogType::NORM, "Flight data object for " + callsign + " destroyed successfully.", "CDataHandler::DeleteFlightData");



		return 0;



	}



	else {



		return 1;



	}



}







int CDataHandler::SetRoute(string callsign, vector<CWaypoint>* route, string track, CAircraftFlightPlan* copiedPlan) {



	if (copiedPlan != nullptr) {



		copiedPlan->Route.clear();



		copiedPlan->Route = *route;



		copiedPlan->Route.shrink_to_fit();







		if (track != "")



			copiedPlan->Track = track;



		else



			copiedPlan->Track = "RR";







		return 0;



	}



	if (flights.find(callsign) != flights.end()) {



		flights.find(callsign)->second.Route.clear();



		flights.find(callsign)->second.Route = *route;



		flights.find(callsign)->second.Route.shrink_to_fit();



		if (track != "")



			flights.find(callsign)->second.Track = track;



		else



			flights.find(callsign)->second.Track = "RR";






		return 0;



	}



	else {
		return 1;
	}
}

// ============================================================================



// STUB FUNCTIONS - These called the defunct vNAAATS API



// Kept for compatibility but do nothing



// ============================================================================







void CDataHandler::DownloadNetworkAircraft(void* args) {



	// REMOVED - vNAAATS API is defunct



	// EuroScope already has aircraft data via VATSIM



	if (args) {



		delete args;



	}



}







void CDataHandler::GetAllNetworkAircraft() {



	// REMOVED - vNAAATS API is defunct



}







void CDataHandler::PostNetworkAircraft(void* args) {



	// REMOVED - vNAAATS API is defunct



	// Use natTrak for oceanic clearances



	if (args) {



		CUtils::CNetworkAsyncData* data = (CUtils::CNetworkAsyncData*)args;



		if (data->FP) delete data->FP;



		delete args;



	}



}







void CDataHandler::UpdateNetworkAircraft(void* args) {



	// REMOVED - vNAAATS API is defunct



	// Use natTrak for oceanic clearances



	if (args) {



		CUtils::CNetworkAsyncData* data = (CUtils::CNetworkAsyncData*)args;



		if (data->FP) delete data->FP;



		delete args;



	}



}







// Fetch NATTrak clearance data from API



int CDataHandler::FetchNatTrakClearances(CPlugIn* plugin) {



	string responseString = "";



	try {



		// Convert URL to LPCSTR type



		LPCSTR lpcURL = CDataHandler::NatTrakClearanceURL.c_str();







		// Delete cache data



		DeleteUrlCacheEntry(lpcURL);







		// Download data



		CComPtr<IStream> pStream;



		HRESULT hr = URLOpenBlockingStream(NULL, lpcURL, &pStream, 0, NULL);



		



		if (FAILED(hr)) {



			int code = (int)hr;



			CLogger::Log(CLogType::ERR, "Could not connect to NATTrak clearance API. Code: " + to_string(code), "CDataHandler::FetchNatTrakClearances");



			return 1;



		}



		



		// Put data into buffer



		char tempBuffer[65536];  // Large buffer for clearance data



		DWORD bytesRead = 0;



		while (true) {



			hr = pStream->Read(tempBuffer, sizeof(tempBuffer) - 1, &bytesRead);



			if (bytesRead == 0) break;



			tempBuffer[bytesRead] = '\0';



			responseString += tempBuffer;



		}



		



		// Parse JSON



		json j = json::parse(responseString);



		



		// Clear existing clearances



		natTrakClearances.clear();



		



		// Iterate through clearances



		for (auto& item : j) {



			CNatTrakClearance clx;



			



			// Callsign



			if (item.contains("callsign") && !item["callsign"].is_null()) {



				clx.Callsign = item["callsign"].get<string>();



			} else {



				continue;  // Skip if no callsign



			}



			



			// Status



			if (item.contains("status") && !item["status"].is_null()) {



				string status = item["status"].get<string>();



				if (status == "CLEARED") {



					clx.Status = CNatTrakStatus::CLEARED;



				} else if (status == "PENDING") {



					clx.Status = CNatTrakStatus::PENDING;



				} else {



					clx.Status = CNatTrakStatus::UNKNOWN;



				}



			} else {



				clx.Status = CNatTrakStatus::UNKNOWN;



			}



			



			// NAT track



			if (item.contains("nat") && !item["nat"].is_null()) {



				clx.Nat = item["nat"].get<string>();



			}



			



			// Entry fix



			if (item.contains("fix") && !item["fix"].is_null()) {



				clx.Fix = item["fix"].get<string>();



			}



			



			// Flight level



			if (item.contains("level") && !item["level"].is_null()) {



				clx.Level = item["level"].get<string>();



			}



			



			// Mach number



			if (item.contains("mach") && !item["mach"].is_null()) {



				clx.Mach = item["mach"].get<string>();



			}



			



			// Estimating time



			if (item.contains("estimating_time") && !item["estimating_time"].is_null()) {



				clx.EstimatingTime = item["estimating_time"].get<string>();



			}



			



			// Clearance issued time



			if (item.contains("clearance_issued") && !item["clearance_issued"].is_null()) {



				clx.ClearanceIssued = item["clearance_issued"].get<string>();



			}



			



			// Extra info/remarks



			if (item.contains("extra_info") && !item["extra_info"].is_null()) {



				clx.ExtraInfo = item["extra_info"].get<string>();



			}



			



			// Store in map



			natTrakClearances[clx.Callsign] = clx;



		}



		



		// Update last fetch time



		lastNatTrakFetch = time(0);



		



		CLogger::Log(CLogType::NORM, "Fetched " + to_string(natTrakClearances.size()) + " NATTrak clearances", "CDataHandler::FetchNatTrakClearances");



		



		return 0;



	}



	catch (exception& e) {



		CLogger::Log(CLogType::EXC, "Failed to fetch NATTrak clearances: " + string(e.what()), "CDataHandler::FetchNatTrakClearances");



		return 1;



	}



}







// Get NATTrak status for an aircraft



CNatTrakStatus CDataHandler::GetNatTrakStatus(const string& callsign) {



	auto it = natTrakClearances.find(callsign);



	if (it != natTrakClearances.end()) {



		return it->second.Status;



	}



	return CNatTrakStatus::UNKNOWN;



}







// Get full NATTrak clearance info for an aircraft



CNatTrakClearance* CDataHandler::GetNatTrakClearance(const string& callsign) {



	auto it = natTrakClearances.find(callsign);



	if (it != natTrakClearances.end()) {



		return &it->second;



	}



	return nullptr;



}



// Store current session state for recovery

void CDataHandler::StoreSessionState(const string& callsign) {

	auto it = flights.find(callsign);

	if (it != flights.end()) {

		CAircraftFlightPlan& fp = it->second;

		fp.StoredFlightLevel = fp.FlightLevel;

		fp.StoredMach = fp.Mach;

		fp.StoredSELCAL = fp.SELCAL;

		fp.StoredTime = time(0);

		CLogger::Log(CLogType::NORM, "Stored session state for " + callsign + 

					 " (FL:" + fp.FlightLevel + " M:" + fp.Mach + " SELCAL:" + fp.SELCAL + ")", 

					 "CDataHandler::StoreSessionState");

	}

}



// Restore session state if within 10 minutes

bool CDataHandler::RestoreSessionState(const string& callsign) {

	auto it = flights.find(callsign);

	if (it != flights.end()) {

		CAircraftFlightPlan& fp = it->second;

		

		// Check if stored data exists and is within 10 minutes

		if (fp.StoredTime > 0) {

			time_t now = time(0);

			double elapsedMinutes = difftime(now, fp.StoredTime) / 60.0;

			

			if (elapsedMinutes <= 10.0) {

				// Restore the data

				if (!fp.StoredFlightLevel.empty()) fp.FlightLevel = fp.StoredFlightLevel;

				if (!fp.StoredMach.empty()) fp.Mach = fp.StoredMach;

				if (!fp.StoredSELCAL.empty()) fp.SELCAL = fp.StoredSELCAL;

				

				CLogger::Log(CLogType::NORM, "Restored session state for " + callsign + 

							 " (FL:" + fp.FlightLevel + " M:" + fp.Mach + " SELCAL:" + fp.SELCAL + ")", 

							 "CDataHandler::RestoreSessionState");

				

				// Clear stored data after restore

				fp.StoredTime = 0;

				return true;

			} else {

				// Data expired, clear it

				fp.StoredTime = 0;

				CLogger::Log(CLogType::NORM, "Session state expired for " + callsign + " (was " + 

							 to_string((int)elapsedMinutes) + " minutes old)", 

							 "CDataHandler::RestoreSessionState");

			}

		}

	}

	return false;

}



// Cleanup expired session states

void CDataHandler::CleanupExpiredSessions() {

	time_t now = time(0);

	for (auto& kv : flights) {

		if (kv.second.StoredTime > 0) {

			double elapsedMinutes = difftime(now, kv.second.StoredTime) / 60.0;

			if (elapsedMinutes > 10.0) {

				kv.second.StoredTime = 0;

				kv.second.StoredFlightLevel.clear();

				kv.second.StoredMach.clear();

				kv.second.StoredSELCAL.clear();

			}

		}

	}

}



// Mark aircraft as transferred

void CDataHandler::MarkAsTransferred(const string& callsign, double lat, double lon) {

	auto it = flights.find(callsign);

	if (it != flights.end()) {

		it->second.IsTransferred = true;

		it->second.TransferTime = time(0);

		it->second.TransferPositionLat = lat;

		it->second.TransferPositionLon = lon;

		CLogger::Log(CLogType::NORM, "Marked " + callsign + " as transferred", 

					 "CDataHandler::MarkAsTransferred");

	}

}



// Clear transfer state

void CDataHandler::ClearTransferState(const string& callsign) {

	auto it = flights.find(callsign);

	if (it != flights.end()) {

		it->second.IsTransferred = false;

		it->second.TransferTime = 0;

		it->second.TransferPositionLat = 0.0;

		it->second.TransferPositionLon = 0.0;

	}

}



// Start coordination flash for aircraft

void CDataHandler::StartCoordinationFlash(const string& callsign) {

	auto it = flights.find(callsign);

	if (it != flights.end()) {

		it->second.IsCoordinationFlashing = true;

		it->second.CoordinationTime = time(0);

		CLogger::Log(CLogType::NORM, "Started coordination flash for " + callsign, 

					 "CDataHandler::StartCoordinationFlash");

	}

}



// Stop coordination flash for aircraft

void CDataHandler::StopCoordinationFlash(const string& callsign) {
	auto it = flights.find(callsign);
	if (it != flights.end()) {
		it->second.IsCoordinationFlashing = false;
		it->second.CoordinationTime = 0;
	}
}

// VATSIM Data Fetcher Implementation
void CDataHandler::StartVatsimDataFetcher() {
	if (vatsimFetcherRunning) return;
	vatsimFetcherRunning = true;
	vatsimFetcherThread = thread(VatsimFetcherLoop);
}

void CDataHandler::StopVatsimDataFetcher() {
	vatsimFetcherRunning = false;
	if (vatsimFetcherThread.joinable()) {
		vatsimFetcherThread.join();
	}
}

void CDataHandler::VatsimFetcherLoop() {
	while (vatsimFetcherRunning) {
		// Fetch data
		IStream* stream = nullptr;
		HRESULT hr = URLOpenBlockingStream(NULL, "https://data.vatsim.net/v3/vatsim-data.json", &stream, 0, NULL);
		if (SUCCEEDED(hr) && stream) {
			// Read stream
			string jsonStr;
			char buffer[1024];
			ULONG bytesRead;
			while (stream->Read(buffer, sizeof(buffer), &bytesRead) == S_OK && bytesRead > 0) {
				jsonStr.append(buffer, bytesRead);
			}
			stream->Release();

			// Parse JSON
			try {
				auto j = json::parse(jsonStr);
				if (j.contains("pilots")) {
					lock_guard<mutex> lock(vatsimCacheMutex);
					vatsimRouteCache.clear();
					for (const auto& pilot : j["pilots"]) {
						if (pilot.contains("callsign") && pilot.contains("flight_plan") && !pilot["flight_plan"].is_null()) {
							string callsign = pilot["callsign"];
							string route = "";
							if (pilot["flight_plan"].contains("route") && !pilot["flight_plan"]["route"].is_null()) {
								route = pilot["flight_plan"]["route"].get<string>();
							}
							vatsimRouteCache[callsign] = route;
						}
					}
					CLogger::Log(CLogType::NORM, "Fetched VATSIM data. Cached " + to_string(vatsimRouteCache.size()) + " routes.", "CDataHandler::VatsimFetcherLoop");
				}
			} catch (exception& e) {
				CLogger::Log(CLogType::ERR, "Failed to parse VATSIM data: " + string(e.what()), "CDataHandler::VatsimFetcherLoop");
			}
		} else {
				CLogger::Log(CLogType::ERR, "Failed to download VATSIM data.", "CDataHandler::VatsimFetcherLoop");
		}

		// Sleep for 60 seconds (check every 1s for shutdown)
		for (int i = 0; i < 60; i++) {
			if (!vatsimFetcherRunning) break;
			this_thread::sleep_for(chrono::seconds(1));
		}
	}
}

string CDataHandler::GetVatsimRoute(string callsign) {
	lock_guard<mutex> lock(vatsimCacheMutex);
	auto it = vatsimRouteCache.find(callsign);
	if (it != vatsimRouteCache.end()) {
		return it->second;
	}
	return "";
}

