#pragma once
#include "EuroScopePlugIn.h"
#include "Constants.h"
#include "Structures.h"
#include "Utils.h"
#include <mutex>

using namespace std;
using namespace EuroScopePlugIn;

class CRoutesHelper
{
	public:
		// Current NAT tracks
		static map<string, CTrack> CurrentTracks;
		static mutex TracksMutex;

		// Current TMI
		static string CurrentTMI;

		// Active aircraft routes to draw
		static vector<string> ActiveRoutes;

		// Fix cache for thread-safe lookups
		static map<string, CPosition> FixCache;
		static mutex FixCacheMutex;
		static void InitialiseFixCache(CRadarScreen* screen);

		// Get a route
		static bool GetRoute(CRadarScreen* screen, vector<CRoutePosition>* routeVector, string callsign, CAircraftFlightPlan* copy = nullptr);

		// Initialise route
		static void InitialiseRoute(void* args);

		// Parse a raw route
		static int ParseRoute(CRadarScreen* screen, string callsign, string rawInput, bool isTrack = false, CAircraftFlightPlan* copy = nullptr);

		// Helper to check if a point is an entry/exit point based on active tracks
		static bool IsOceanicEntryPoint(string pointName, bool eastbound);
		static bool IsOceanicExitPoint(string pointName, bool eastbound);

		// Is on a NAT track
		static string OnNatTrack(CRadarScreen* screen, string callsign, string routeString = "", bool disableEuroScopeFetch = false);
};

