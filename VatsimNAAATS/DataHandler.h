#pragma once

#include "vector"

#include "Structures.h"

#include "EuroScopePlugIn.h"

#include "Overlays.h"

#include "Utils.h"

#include "Logger.h"

#include <json.hpp>

#include <ctime>



using namespace std;

using namespace EuroScopePlugIn;



// NATTrak clearance status

enum class CNatTrakStatus {

	UNKNOWN,	// Not in NATTrak system (asterisk)

	PENDING,	// In system but not cleared (diamond with line)

	CLEARED		// Cleared (airplane icon)

};



// NATTrak clearance info

struct CNatTrakClearance {

	string Callsign;

	CNatTrakStatus Status;

	string Nat;				// Track letter (e.g., "A", "B", "RR" for random)

	string Fix;				// Entry fix

	string Level;			// Cleared flight level

	string Mach;			// Cleared Mach number

	string EstimatingTime;	// ETA over entry fix

	string ClearanceIssued;	// When clearance was issued

	string ExtraInfo;		// Pilot remarks

};



class CDataHandler

{

	public:

		// Check plugin version number

		static int CheckPluginVersion(CPlugIn* plugin);

		

		// Download nat track data (now from natTrak API)
		static int PopulateLatestTrackData(CPlugIn* plugin);
		static void PopulateLatestTrackDataAsync(CPlugIn* plugin);
		
		// Get flight data
		static CAircraftFlightPlan* GetFlightData(string callsign);
		static void GetFlightData(string callsign, CAircraftFlightPlan& fp);
		static map<string, CAircraftFlightPlan>& GetFlights() { return flights; }

		

		// Update a flight data object

		static int UpdateFlightData(CRadarScreen* screen, string callsign, bool updateRoute);

		

		// Create a new flight data object

		static int CreateFlightData(CRadarScreen* screen, string callsign);

		

		// Deletes a flight data object out of the flights map
		static int DeleteFlightData(string callsign);
		
		// Set route
		static int SetRoute(string callsign, vector<CWaypoint>* route, string track, CAircraftFlightPlan* copiedPlan = nullptr);
		
		// VATSIM Data API
		static void StartVatsimDataFetcher();
		static void StopVatsimDataFetcher();
		static string GetVatsimRoute(string callsign);

		// NATTrak clearance functions

		static int FetchNatTrakClearances(CPlugIn* plugin);

		static CNatTrakStatus GetNatTrakStatus(const string& callsign);

		static CNatTrakClearance* GetNatTrakClearance(const string& callsign);

		static time_t GetLastNatTrakFetch() { return lastNatTrakFetch; }

		

		// Session state management (for recovery after disconnect)

		static void StoreSessionState(const string& callsign);

		static bool RestoreSessionState(const string& callsign);

		static void CleanupExpiredSessions();

		

		// Transfer state management

		static void MarkAsTransferred(const string& callsign, double lat, double lon);

		static void ClearTransferState(const string& callsign);

		

		// Coordination flash state

		static void StartCoordinationFlash(const string& callsign);

		static void StopCoordinationFlash(const string& callsign);

		

		// vNAAATS network methods - REMOVED (defunct API)

		// These are kept as stubs for compatibility but do nothing

		static void DownloadNetworkAircraft(void* args);

		static void GetAllNetworkAircraft();

		static void PostNetworkAircraft(void* args);

		static void UpdateNetworkAircraft(void* args);



	private:

		// Helper to parse track waypoint coordinates

		static CPosition ParseTrackWaypoint(const string& waypoint);

		

		// Version URL (update to your repo if desired)

		static const string PluginVersion;

		

		// NAT Track URL - now uses natTrak

		static const string TrackURL;

		

		// NATTrak clearance API URL

		static const string NatTrakClearanceURL;

		

		// Flight data storage
		static map<string, CAircraftFlightPlan> flights;
		
		// VATSIM Data Cache
		static map<string, string> vatsimRouteCache;
		static mutex vatsimCacheMutex;
		static bool vatsimFetcherRunning;
		static thread vatsimFetcherThread;
		static void VatsimFetcherLoop();

		// NATTrak clearance storage

		static map<string, CNatTrakClearance> natTrakClearances;

		static time_t lastNatTrakFetch;

		

		// REMOVED - defunct vNAAATS API links (kept as empty for compatibility)

		static const string TrackSource;

		static const string GetSingleAircraft;

		static const string FlightDataUpdate;

		static const string PostSingleAircraft;

};

