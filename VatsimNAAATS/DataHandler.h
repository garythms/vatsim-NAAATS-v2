#pragma once
#include "vector"
#include "Structures.h"
#include "EuroScopePlugIn.h"
#include "Overlays.h"
#include "Utils.h"
#include "Logger.h"
#include <json.hpp>

using namespace std;
using namespace EuroScopePlugIn;

class CDataHandler
{
	public:
		// Check plugin version number
		static int CheckPluginVersion(CPlugIn* plugin);
		
		// Download nat track data (now from natTrak API)
		static int PopulateLatestTrackData(CPlugIn* plugin);
		
		// Get flight data
		static CAircraftFlightPlan* GetFlightData(string callsign);
		static void GetFlightData(string callsign, CAircraftFlightPlan& fp);
		
		// Update a flight data object
		static int UpdateFlightData(CRadarScreen* screen, string callsign, bool updateRoute);
		
		// Create a new flight data object
		static int CreateFlightData(CRadarScreen* screen, string callsign);
		
		// Deletes a flight data object out of the flights map
		static int DeleteFlightData(string callsign);
		
		// Set route
		static int SetRoute(string callsign, vector<CWaypoint>* route, string track, CAircraftFlightPlan* copiedPlan = nullptr);
		
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
		
		// Flight data storage
		static map<string, CAircraftFlightPlan> flights;
		
		// REMOVED - defunct vNAAATS API links (kept as empty for compatibility)
		static const string TrackSource;
		static const string GetSingleAircraft;
		static const string FlightDataUpdate;
		static const string PostSingleAircraft;
};
