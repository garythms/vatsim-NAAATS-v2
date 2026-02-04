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
		// Clear current tracks (thread-safe)
		{
			lock_guard<mutex> lock(CRoutesHelper::TracksMutex);
			if (!CRoutesHelper::CurrentTracks.empty()) {
				CRoutesHelper::CurrentTracks.clear();
			}
		}

		auto jsonArray = json::parse(responseString);
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
					currentTMI = validFrom.substr(8, 2); 
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
	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(callsign) != flights.end()) {
		return &flights.find(callsign)->second;
	}
	return nullptr;
}

void CDataHandler::GetFlightData(string callsign, CAircraftFlightPlan& fp) {
	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(callsign) != flights.end()) {
		fp = flights.find(callsign)->second;
	}
}

int CDataHandler::CreateFlightData(CRadarScreen* screen, string callsign) {
	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(callsign) == flights.end()) {
		flights[callsign] = CAircraftFlightPlan();
		flights[callsign].Callsign = callsign;
		flights[callsign].IsValid = true;
	}
	return 0;
}

int CDataHandler::DeleteFlightData(string callsign) {
	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(callsign) != flights.end()) {
		flights.erase(callsign);
	}
	return 0;
}

int CDataHandler::UpdateFlightData(CRadarScreen* screen, string callsign, bool updateRoute) {
	{
		lock_guard<mutex> lock(flightsMutex);
		if (flights.find(callsign) == flights.end()) {
			flights[callsign] = CAircraftFlightPlan();
			flights[callsign].Callsign = callsign;
			flights[callsign].IsValid = true;
		}
	}

	CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());
	if (!fp.IsValid()) return 1;

	lock_guard<mutex> lock(flightsMutex);
	if (flights.find(callsign) == flights.end()) return 1; // It was deleted?
	
	CAircraftFlightPlan& data = flights[callsign];
	const char* callsignStr = fp.GetCallsign();
	data.Callsign = callsignStr ? callsignStr : "";

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

	// Flight level
	int flVal = fp.GetControllerAssignedData().GetClearedAltitude();
	if (flVal == 0) flVal = fp.GetFlightPlanData().GetFinalAltitude();
	if (flVal == 0) {
		CRadarTarget rt = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (rt.IsValid()) {
			flVal = rt.GetPosition().GetFlightLevel();
		}
	}
	if (flVal > 1000) flVal /= 100;
	
	char flBuf[10];
	sprintf_s(flBuf, "%03d", flVal);
	data.FlightLevel = flBuf;

	// Mach
	int machVal = fp.GetControllerAssignedData().GetAssignedMach();
	if (machVal == 0) {
		// Try to get from filed TAS/Mach
		machVal = fp.GetFlightPlanData().GetTrueAirspeed();
	}
	
	// Normalize Mach to 2 or 3 digits (e.g., 82 for .82, 105 for 1.05)
	if (machVal >= 1000) machVal /= 10; // 8200 -> 820 or 820 -> 82? EuroScope usually uses Mach*100 or Mach*1000
	if (machVal >= 500) machVal /= 10;  // 820 -> 82
	
	char machBuf[10];
	sprintf_s(machBuf, "%03d", machVal);
	data.Mach = machBuf;
	
	// Sector
	const char* sectorId = fp.GetTrackingControllerId();
	data.Sector = sectorId ? sectorId : "";

	// Check NAT Track
		if (updateRoute) {
			const char* routeStr = fp.GetFlightPlanData().GetRoute();
			string route = routeStr ? routeStr : "";
			// If EuroScope route is empty, try VATSIM API
			if (route.empty()) {
				// GetVatsimRoute is thread safe (uses its own mutex)
				route = GetVatsimRoute(callsign);
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
		// Pass true for disableEuroScopeFetch because we already fetched/provided the route
		// OnNatTrack is thread safe (uses TracksMutex)
		string detectedTrack = CRoutesHelper::OnNatTrack(screen, callsign, route, true);
		
		if (!detectedTrack.empty()) {
			data.Track = detectedTrack;
		} else {
			// If no track detected in route, only set to RR if it's not already a valid NAT track
			// This preserves manual track assignments made via the Flight Plan window
			bool isExistingTrackValid = false;
			{
				lock_guard<mutex> trackLock(CRoutesHelper::TracksMutex);
				// A valid track is either in the current tracks map, or is a single/double letter ID (manual override)
				if (CRoutesHelper::CurrentTracks.find(data.Track) != CRoutesHelper::CurrentTracks.end()
					|| (data.Track.length() <= 2 && data.Track != "RR" && !data.Track.empty())) {
					isExistingTrackValid = true;
				}
			}

			if (!isExistingTrackValid) {
				data.Track = "RR";
			}
		}

		// Update direction if it's a known NAT track
		if (data.Track != "RR" && !data.Track.empty()) {
			lock_guard<mutex> trackLock(CRoutesHelper::TracksMutex);
			if (CRoutesHelper::CurrentTracks.find(data.Track) != CRoutesHelper::CurrentTracks.end()) {
				data.Direction = (CRoutesHelper::CurrentTracks[data.Track].Direction == CTrackDirection::WEST);
			}
		} else {
			// Determine direction for Random Route from current heading
			CRadarTarget rt = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
			if (rt.IsValid()) {
				int heading = rt.GetPosition().GetReportedHeading();
				// Westbound is roughly 180 to 360 degrees
				data.Direction = (heading > 180 && heading < 360);
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
void CDataHandler::FetchNatTrakClearancesAsync(void* args) {}
int CDataHandler::FetchNatTrakClearances(CPlugIn* plugin) { return 0; }
bool CDataHandler::GetNatTrakClearance(const string& callsign, CNatTrakClearance& outClearance) { return false; }
CNatTrakStatus CDataHandler::GetNatTrakStatus(const string& callsign) { return CNatTrakStatus::UNKNOWN; }

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
