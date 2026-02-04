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
#include <mutex>
#include <algorithm>

using json = nlohmann::json;

// Flight data map
map<string, CAircraftFlightPlan> CDataHandler::flights;
mutex CDataHandler::flightsMutex;

// NATTrak clearance storage
map<string, CNatTrakClearance> CDataHandler::natTrakClearances;
mutex CDataHandler::natTrakMutex;
bool CDataHandler::natTrakFetcherRunning = false;
time_t CDataHandler::lastNatTrakFetch = 0;

// Version check URL
const string CDataHandler::PluginVersion = "https://raw.githubusercontent.com/garythms/vatsim-NAAATS-v2/master/pluginversion.txt";

// NAT Track URL
const string CDataHandler::TrackURL = "https://nattrak.vatsim.net/api/tracks";

// NATTrak clearance API URL
const string CDataHandler::NatTrakClearanceURL = "https://nattrak.vatsim.net/api/plugins";

// DEFUNCT
const string CDataHandler::TrackSource = "";
const string CDataHandler::GetSingleAircraft = "";
const string CDataHandler::FlightDataUpdate = "";
const string CDataHandler::PostSingleAircraft = "";


// ---------------------------------------------------------
// VATSIM Data Fetcher - File Scope Implementation with RAII
// ---------------------------------------------------------

// RAII Thread Wrapper to ensure join on destruction
class CVatsimThreadGuard {
	std::thread t;
	std::atomic<bool>& runFlag;
public:
	CVatsimThreadGuard(std::atomic<bool>& flag) : runFlag(flag) {}
	~CVatsimThreadGuard() {
		runFlag = false; // Signal thread to stop
		if (t.joinable()) {
			t.join();
		}
	}
	void start(std::thread&& newT) {
		if (t.joinable()) t.join();
		t = std::move(newT);
	}
	void join() {
		if (t.joinable()) t.join();
	}
	bool joinable() const { return t.joinable(); }
};

// Static variables for VATSIM fetcher (internal linkage)
static map<string, string> g_vatsimRouteCache;
static mutex g_vatsimCacheMutex;
static atomic<bool> g_vatsimFetcherRunning(false);
static CVatsimThreadGuard g_vatsimFetcherThread(g_vatsimFetcherRunning);

// Helper function to fetch URL using WinINet with isolated session
// This prevents interference with EuroScope's authentication
static string FetchUrlWithWinInet(const string& url) {
	string result;
	
	// Create a NEW, ISOLATED internet session for this plugin
	// Using a unique agent name and INTERNET_OPEN_TYPE_PRECONFIG
	HINTERNET hInternet = InternetOpenA(
		"vNAAATS-DataFetcher/2.0",  // Unique agent name
		INTERNET_OPEN_TYPE_PRECONFIG,
		NULL, 
		NULL, 
		0
	);
	
	if (!hInternet) {
		return "";
	}
	
	// Set reasonable timeouts to prevent blocking
	DWORD timeout = 10000; // 10 seconds
	InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
	
	// Open the URL
	HINTERNET hUrl = InternetOpenUrlA(
		hInternet,
		url.c_str(),
		NULL,
		0,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE,
		0
	);
	
	if (hUrl) {
		char buffer[8192];
		DWORD bytesRead;
		
		while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
			buffer[bytesRead] = '\0';
			result.append(buffer, bytesRead);
		}
		
		InternetCloseHandle(hUrl);
	}
	
	// IMPORTANT: Close the session handle immediately after use
	// This ensures we don't hold any resources that could interfere with ES
	InternetCloseHandle(hInternet);
	
	return result;
}

// Fetcher Loop Function - now uses isolated WinINet calls
static void VatsimFetcherLoop() {
	// Increased initial delay before first fetch - give EuroScope plenty of time to complete its auth (JWT download)
	// EuroScope's own downloader can fail if plugins are too aggressive with web requests during startup.
	for (int i = 0; i < 30; i++) {
		if (!g_vatsimFetcherRunning) return;
		this_thread::sleep_for(chrono::seconds(1));
	}
	
	while (g_vatsimFetcherRunning) {
		// Use isolated WinINet fetch instead of URLOpenBlockingStream
		string jsonStr = FetchUrlWithWinInet("https://data.vatsim.net/v3/vatsim-data.json");
		
		if (!jsonStr.empty()) {
			// Parse JSON
			try {
				auto j = json::parse(jsonStr);
				if (j.contains("pilots")) {
					lock_guard<mutex> lock(g_vatsimCacheMutex);
					g_vatsimRouteCache.clear();
					for (const auto& pilot : j["pilots"]) {
						if (pilot.contains("callsign") && pilot.contains("flight_plan") && !pilot["flight_plan"].is_null()) {
							string callsign = pilot["callsign"];
							string route = "";
							if (pilot["flight_plan"].contains("route") && !pilot["flight_plan"]["route"].is_null()) {
								route = pilot["flight_plan"]["route"].get<string>();
							}
							g_vatsimRouteCache[callsign] = route;
						}
					}
				}
			} catch (...) {
				// Ignore parsing errors
			}
		}

		// Sleep for 60 seconds (check every 1s for shutdown)
		for (int i = 0; i < 60; i++) {
			if (!g_vatsimFetcherRunning) break;
			this_thread::sleep_for(chrono::seconds(1));
		}
	}
}

// ---------------------------------------------------------
// CDataHandler Implementation
// ---------------------------------------------------------

int CDataHandler::CheckPluginVersion(CPlugIn* plugin) {
	// Stub
	return -1;
}

CPosition CDataHandler::ParseTrackWaypoint(const string& waypoint) {
	CPosition pos;
	pos.m_Latitude = 0.0;
	pos.m_Longitude = 0.0;
	
	try {
		size_t slashPos = waypoint.find('/');
		if (slashPos != string::npos) {
			string latStr = waypoint.substr(0, slashPos);
			string lonStr = waypoint.substr(slashPos + 1);
			
			double lat = stod(latStr);
			double lon = stod(lonStr);
			
			// Adjust for minutes if length > 2 (e.g. 5030 -> 50.5)
			if (latStr.length() > 2) {
				double deg = floor(lat / 100);
				double min = lat - (deg * 100);
				lat = deg + (min / 60.0);
			}
			if (lonStr.length() > 2) {
				double deg = floor(lon / 100);
				double min = lon - (deg * 100);
				lon = deg + (min / 60.0);
			}
			
			// Assuming North / West for NAT
			pos.m_Latitude = lat;
			pos.m_Longitude = -lon;
		} else {
			// Named fix lookup
			lock_guard<mutex> lock(CRoutesHelper::FixCacheMutex);
			if (CRoutesHelper::FixCache.find(waypoint) != CRoutesHelper::FixCache.end()) {
				pos = CRoutesHelper::FixCache[waypoint];
			}
		}
	} catch (...) {}
	
	return pos;
}

int CDataHandler::PopulateLatestTrackData(CPlugIn* plugin) {
	// Use isolated WinINet fetch instead of URLOpenBlockingStream
	string responseString = FetchUrlWithWinInet(TrackURL);
	
	if (responseString.empty()) {
		// Fail silently or log
		return 1;
	}

	// Parse the json
	try {
		auto jsonArray = json::parse(responseString);
		if (!jsonArray.is_array()) return 1;

		// Clear tracks map before populating
		{
			lock_guard<mutex> lock(CRoutesHelper::TracksMutex);
			CRoutesHelper::CurrentTracks.clear();
		}

		string currentTMI = "";
		
		for (size_t i = 0; i < jsonArray.size(); i++) {
			if (jsonArray[i].contains("active") && !jsonArray[i].at("active").get<bool>()) {
				continue;
			}
			
			CTrack track;
			track.Identifier = jsonArray[i].at("identifier").get<string>();

			if (jsonArray[i].contains("valid_from") && !jsonArray[i].at("valid_from").is_null()) {
				string validFrom = jsonArray[i].at("valid_from").get<string>();
				if (validFrom.length() >= 10) {
					// Parse YYYY-MM-DD
					int year = stoi(validFrom.substr(0, 4));
					int month = stoi(validFrom.substr(5, 2));
					int day = stoi(validFrom.substr(8, 2));
					
					tm t = {0};
					t.tm_year = year - 1900;
					t.tm_mon = month - 1;
					t.tm_mday = day;
					mktime(&t);
					
					char buf[10];
					sprintf_s(buf, "%03d", t.tm_yday + 1);
					currentTMI = buf;
				}
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

			if (jsonArray[i].contains("valid_from") && !jsonArray[i].at("valid_from").is_null()) {
				track.validFrom = jsonArray[i].at("valid_from").get<string>();
			}
			if (jsonArray[i].contains("valid_to") && !jsonArray[i].at("valid_to").is_null()) {
				track.validTo = jsonArray[i].at("valid_to").get<string>();
			}

			// Add to map (thread-safe)
			{
				lock_guard<mutex> lock(CRoutesHelper::TracksMutex);
				CRoutesHelper::CurrentTracks.insert(make_pair(track.Identifier, track));
			}
		}
		
		return 0;
	}
	catch (exception & e) {
		plugin->DisplayUserMessage("vNAAATS", "Error", string("Failed to parse natTrak track JSON: " + string(e.what())).c_str(), true, true, true, true, true);
		return 1;
	}
}

void CDataHandler::PopulateLatestTrackDataAsync(CPlugIn* plugin) {
	static atomic<bool> isTrackUpdateRunning(false);
	if (isTrackUpdateRunning) return;

	thread t([plugin]() {
		isTrackUpdateRunning = true;
		// Initial delay to prevent interference with EuroScope's own auth/startup downloads
		this_thread::sleep_for(chrono::seconds(20));
		CDataHandler::PopulateLatestTrackData(plugin);
		isTrackUpdateRunning = false;
	});
	t.detach();
}

void CDataHandler::StartVatsimDataFetcher() {
	if (g_vatsimFetcherRunning) return;
	g_vatsimFetcherRunning = true;
	g_vatsimFetcherThread.start(thread(VatsimFetcherLoop));
}

void CDataHandler::StopVatsimDataFetcher() {
	g_vatsimFetcherRunning = false;
	g_vatsimFetcherThread.join();
}

string CDataHandler::GetVatsimRoute(string callsign) {
	lock_guard<mutex> lock(g_vatsimCacheMutex);
	if (g_vatsimRouteCache.find(callsign) != g_vatsimRouteCache.end()) {
		return g_vatsimRouteCache[callsign];
	}
	return "";
}

CAircraftFlightPlan* CDataHandler::GetFlightData(string callsign) {
	string cs = callsign;
	cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
	for (auto& c : cs) c = toupper((unsigned char)c);

	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(cs) != flights.end()) {
		return &flights.find(cs)->second;
	}
	return nullptr;
}

void CDataHandler::GetFlightData(string callsign, CAircraftFlightPlan& fp) {
	string cs = callsign;
	cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
	for (auto& c : cs) c = toupper((unsigned char)c);

	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(cs) != flights.end()) {
		fp = flights.find(cs)->second;
	}
}

int CDataHandler::CreateFlightData(CRadarScreen* screen, string callsign) {
	string cs = callsign;
	cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
	for (auto& c : cs) c = toupper((unsigned char)c);

	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(cs) == flights.end()) {
		flights[cs] = CAircraftFlightPlan();
		flights[cs].Callsign = cs;
		flights[cs].IsValid = true;
	}
	return 0;
}

int CDataHandler::DeleteFlightData(string callsign) {
	string cs = callsign;
	cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
	for (auto& c : cs) c = toupper((unsigned char)c);

	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(cs) != flights.end()) {
		flights.erase(cs);
	}
	return 0;
}

int CDataHandler::UpdateFlightData(CRadarScreen* screen, string callsign, bool updateRoute) {
	string csLookup = callsign;
	csLookup.erase(csLookup.find_last_not_of(" \n\r\t") + 1);
	for (auto& c : csLookup) c = toupper((unsigned char)c);

	{
		lock_guard<mutex> lock(flightsMutex);
		if (flights.find(csLookup) == flights.end()) {
			flights[csLookup] = CAircraftFlightPlan();
			flights[csLookup].Callsign = csLookup;
			flights[csLookup].IsValid = true;
		}
	}

	CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());
	if (!fp.IsValid()) return 1;

	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(csLookup) == flights.end()) return 1; // It was deleted?
	
	CAircraftFlightPlan& data = flights[csLookup];
	const char* callsignStr = fp.GetCallsign();
	data.Callsign = csLookup; // Use trimmed uppercase version

	const char* typeStr = fp.GetFlightPlanData().GetAircraftFPType();
	data.Type = typeStr ? typeStr : "";

	const char* originStr = fp.GetFlightPlanData().GetOrigin();
	data.Depart = originStr ? originStr : "";

	const char* destStr = fp.GetFlightPlanData().GetDestination();
	data.Dest = destStr ? destStr : "";
	
	// Format ETD
	const char* etdStr = fp.GetFlightPlanData().GetEstimatedDepartureTime();
	if (etdStr) {
		data.Etd = etdStr;
	} else {
		data.Etd = "0000";
	}

	// Flight level - only update if EuroScope has a valid value
	int flVal = fp.GetControllerAssignedData().GetClearedAltitude();
	if (flVal == 0) flVal = fp.GetFlightPlanData().GetFinalAltitude();
	if (flVal == 0) {
		CRadarTarget rt = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (rt.IsValid()) {
			flVal = rt.GetPosition().GetFlightLevel();
		}
	}
	
	if (flVal > 0) {
		if (flVal > 1000) flVal /= 100;
		char flBuf[10];
		sprintf_s(flBuf, "%03d", flVal);
		
		// Only update if current is empty or if EuroScope value changed significantly
		if (data.FlightLevel.empty() || data.FlightLevel == "000" || abs(stoi(data.FlightLevel) - flVal) > 5) {
			data.FlightLevel = flBuf;
		}
	}

	// Mach - only update if EuroScope has a valid value
	int machVal = fp.GetControllerAssignedData().GetAssignedMach();
	bool isMach = true;
	if (machVal == 0) {
		machVal = fp.GetFlightPlanData().GetTrueAirspeed();
		isMach = false;
	}
	
	if (machVal > 0) {
		// Normalize Mach
		if (machVal >= 1000) machVal /= 10;
		if (machVal >= 500) machVal /= 10; 
		
		char machBuf[10];
		sprintf_s(machBuf, "%03d", machVal);
		
		// Logic to prevent overwriting Mach with TAS
		// If current value looks like Mach (e.g. 70-99) and new value looks like TAS (>100), don't overwrite
		bool currentIsMach = !data.Mach.empty() && stoi(data.Mach) < 100;
		bool newIsMach = machVal < 100;

		if (data.Mach.empty() || (currentIsMach && newIsMach) || (!currentIsMach)) {
			data.Mach = machBuf;
		}
	}
	
	// Sector
	const char* sectorId = fp.GetTrackingControllerId();
	data.Sector = sectorId ? sectorId : "";

	// SELCAL - Update if EuroScope has it and we don't
	string esSelcal = CUtils::GetSelcalCode(&fp);
	if (!esSelcal.empty() && (data.SELCAL.empty() || data.SELCAL == "N/A")) {
		data.SELCAL = esSelcal;
	}

	// Check NAT Track
		if (updateRoute) {
			const char* routeStr = fp.GetFlightPlanData().GetRoute();
			string route = routeStr ? routeStr : "";
			// If EuroScope route is empty, try VATSIM API
			if (route.empty()) {
				route = GetVatsimRoute(csLookup);
			}

			// Populate RouteRaw
			data.RouteRaw.clear();
			string s = route;
			string delimiter = " ";
			size_t pos = 0;
			string token;
			while ((pos = s.find(delimiter)) != string::npos) {
				token = s.substr(0, pos);
				if (!token.empty()) data.RouteRaw.push_back(token);
				s.erase(0, pos + delimiter.length());
			}
			if (!s.empty()) data.RouteRaw.push_back(s);

			// Determine track from route
		string detectedTrack = CRoutesHelper::OnNatTrack(screen, csLookup, route, true);
		
		if (!detectedTrack.empty()) {
			data.Track = detectedTrack;
		} else {
			bool isExistingTrackValid = false;
			{
				lock_guard<mutex> trackLock(CRoutesHelper::TracksMutex);
				if (CRoutesHelper::CurrentTracks.find(data.Track) != CRoutesHelper::CurrentTracks.end()
					|| (data.Track.length() <= 2 && data.Track != "RR" && !data.Track.empty())) {
					isExistingTrackValid = true;
				}
			}

			if (!isExistingTrackValid) {
				data.Track = "RR";
			}
		}

		// Update direction
		if (data.Track != "RR" && !data.Track.empty()) {
			lock_guard<mutex> trackLock(CRoutesHelper::TracksMutex);
			if (CRoutesHelper::CurrentTracks.find(data.Track) != CRoutesHelper::CurrentTracks.end()) {
				data.Direction = (CRoutesHelper::CurrentTracks[data.Track].Direction == CTrackDirection::WEST);
			}
		} else {
			// Heuristic for direction based on Departure/Destination
			bool determined = false;
			string dep = data.Depart;
			string dest = data.Dest;
			
			if (dep.length() >= 2 && dest.length() >= 2) {
				// Common European/East prefixes
				string eastPrefixes[] = {"EB", "ED", "EE", "EF", "EG", "EH", "EI", "EK", "EL", "EN", "EP", "ES", "ET", "EU", "EV", "EY", "LF", "LS", "LO", "LH", "LI"};
				// Common North American/West prefixes
				string westPrefixes[] = {"C", "K", "M", "P", "T", "S"};

				bool depEast = false;
				for (const string& p : eastPrefixes) if (dep.substr(0, 2) == p) { depEast = true; break; }
				bool destEast = false;
				for (const string& p : eastPrefixes) if (dest.substr(0, 2) == p) { destEast = true; break; }
				
				bool depWest = false;
				for (const string& p : westPrefixes) if (dep.substr(0, 1) == p) { depWest = true; break; }
				bool destWest = false;
				for (const string& p : westPrefixes) if (dest.substr(0, 1) == p) { destWest = true; break; }

				if (depEast && destWest) { data.Direction = true; determined = true; } // Westbound
				else if (depWest && destEast) { data.Direction = false; determined = true; } // Eastbound
			}

			if (!determined) {
				CRadarTarget rt = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
				if (rt.IsValid()) {
					int heading = rt.GetPosition().GetReportedHeading();
					data.Direction = (heading > 180 && heading < 360);
				}
			}
		}
	}

	return 0;
}

int CDataHandler::SetRoute(string callsign, vector<CWaypoint>* route, string track, CAircraftFlightPlan* copiedPlan) {
	// If copying to a provided plan, update it directly
	if (copiedPlan != nullptr) {
		copiedPlan->Route = *route;
		copiedPlan->Track = track;
		return 0;
	}
	
	// Otherwise update the flight in the map
	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(callsign) != flights.end()) {
		flights[callsign].Route = *route;
		flights[callsign].Track = track;
		return 0;
	}
	
	return 1; // Flight not found
}

// NATTrak Stubs
void CDataHandler::FetchNatTrakClearancesAsync(void* args) {
	CPlugIn* plugin = (CPlugIn*)args;
	if (natTrakFetcherRunning) return;
	natTrakFetcherRunning = true;
	
	thread t([plugin]() {
		while (natTrakFetcherRunning) {
			FetchNatTrakClearances(plugin);
			// Poll every 30 seconds
			for (int i = 0; i < 30 && natTrakFetcherRunning; ++i) {
				this_thread::sleep_for(chrono::seconds(1));
			}
		}
	});
	t.detach();
}

int CDataHandler::FetchNatTrakClearances(CPlugIn* plugin) {
	string response = FetchUrlWithWinInet(NatTrakClearanceURL);
	if (response.empty()) {
		CLogger::Log(CLogType::NORM, "NATTrack API: Empty response or fetch failed.", "CDataHandler::FetchNatTrakClearances");
		return 1;
	}

	try {
		json jsonArray = json::parse(response);
		if (!jsonArray.is_array()) {
			CLogger::Log(CLogType::NORM, "NATTrack API: Response is not an array.", "CDataHandler::FetchNatTrakClearances");
			return 1;
		}

		lock_guard<mutex> lock(natTrakMutex);
		// Instead of clearing, we mark all as "old" or just overwrite. 
		// Actually, clearing is fine if the API returns the FULL list. 
		// If it's partial, we might want to merge.
		natTrakClearances.clear();

		int count = 0;
		for (auto& item : jsonArray) {
			CNatTrakClearance clearance;
			string cs = item.at("callsign").get<string>();
			cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
			for (auto& c : cs) c = toupper((unsigned char)c);
			
			clearance.Callsign = cs;
			clearance.RequestId = item.at("id").get<int>();
			
			// NATTrack status parsing
			if (item.at("status").is_string()) {
				string statusStr = item.at("status").get<string>();
				if (statusStr == "PENDING") clearance.Status = CNatTrakStatus::PENDING;
				else if (statusStr == "CLEARED") clearance.Status = CNatTrakStatus::CLEARED;
				else clearance.Status = CNatTrakStatus::UNKNOWN;
			} else {
				int status = item.at("status").get<int>();
				if (status == 0) clearance.Status = CNatTrakStatus::PENDING;
				else if (status == 1) clearance.Status = CNatTrakStatus::CLEARED;
				else clearance.Status = CNatTrakStatus::UNKNOWN;
			}

			// Optional fields
			if (item.contains("nat") && !item.at("nat").is_null()) clearance.Nat = item.at("nat").get<string>();
			else if (item.contains("track") && !item.at("track").is_null()) clearance.Nat = item.at("track").get<string>();
			
			if (item.contains("fix") && !item.at("fix").is_null()) clearance.Fix = item.at("fix").get<string>();

			if (item.contains("level") && !item.at("level").is_null()) {
				if (item.at("level").is_number()) {
					clearance.Level = to_string(item.at("level").get<int>());
				} else {
					clearance.Level = item.at("level").get<string>();
				}
			}

			if (item.contains("mach") && !item.at("mach").is_null()) {
				if (item.at("mach").is_number()) {
					clearance.Mach = to_string(item.at("mach").get<double>());
				} else {
					clearance.Mach = item.at("mach").get<string>();
				}
			}

			if (item.contains("estimating_time") && !item.at("estimating_time").is_null()) clearance.EstimatingTime = item.at("estimating_time").get<string>();
			if (item.contains("clearance_issued") && !item.at("clearance_issued").is_null()) clearance.ClearanceIssued = item.at("clearance_issued").get<string>();
			if (item.contains("extra_info") && !item.at("extra_info").is_null()) clearance.ExtraInfo = item.at("extra_info").get<string>();
			
			natTrakClearances[clearance.Callsign] = clearance;
			count++;
		}
		lastNatTrakFetch = time(0);
		CLogger::Log(CLogType::NORM, "NATTrack API: Successfully parsed " + to_string(count) + " clearances.", "CDataHandler::FetchNatTrakClearances");
		return 0;
	}
	catch (exception &ex) {
		CLogger::Log(CLogType::ERR, "NATTrack API: Error parsing JSON: " + string(ex.what()), "CDataHandler::FetchNatTrakClearances");
		return 1;
	}
}

CNatTrakStatus CDataHandler::GetNatTrakStatus(const string& callsign) {
	string cs = callsign;
	// Trim trailing spaces
	cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
	// Convert to uppercase
	for (auto& c : cs) c = toupper((unsigned char)c);

	lock_guard<mutex> lock(natTrakMutex);
	if (natTrakClearances.find(cs) != natTrakClearances.end()) {
		return natTrakClearances[cs].Status;
	}
	return CNatTrakStatus::UNKNOWN;
}

bool CDataHandler::GetNatTrakClearance(const string& callsign, CNatTrakClearance& outClearance) {
	string cs = callsign;
	// Trim trailing spaces
	cs.erase(cs.find_last_not_of(" \n\r\t") + 1);
	// Convert to uppercase
	for (auto& c : cs) c = toupper((unsigned char)c);

	lock_guard<mutex> lock(natTrakMutex);
	if (natTrakClearances.find(cs) != natTrakClearances.end()) {
		outClearance = natTrakClearances[cs];
		return true;
	}
	return false;
}

// Session Stubs
void CDataHandler::StoreSessionState(const string& callsign) {}
bool CDataHandler::RestoreSessionState(const string& callsign) { return false; }
void CDataHandler::CleanupExpiredSessions() {}
void CDataHandler::MarkAsTransferred(const string& callsign, double lat, double lon) {}
void CDataHandler::ClearTransferState(const string& callsign) {}
void CDataHandler::StartCoordinationFlash(const string& callsign) {}
void CDataHandler::StopCoordinationFlash(const string& callsign) {}

// Network Stubs
void CDataHandler::DownloadNetworkAircraft(void* args) {}
void CDataHandler::GetAllNetworkAircraft() {}
void CDataHandler::PostNetworkAircraft(void* args) {}
void CDataHandler::UpdateNetworkAircraft(void* args) {}
