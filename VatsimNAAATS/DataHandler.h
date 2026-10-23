#pragma once
#include "vector"
#include "Structures.h"
#include "EuroScopePlugIn.h"
#include "Overlays.h"
#include "Utils.h"
#include "Logger.h"
#include <json.hpp>
#include <mutex>
#include <atomic>

using namespace std;
using namespace EuroScopePlugIn;

struct CAselPush { string Me, Callsign; }; // reported to the server when the selected aircraft changes

class CDataHandler
{
	public:
	// Check plugin version number
	static int CheckPluginVersion(CPlugIn* plugin);

	// Download nat track data
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

	/// vNAAATS network methods
	// Download aircraft data (single)
	static void DownloadNetworkAircraft(void* args);

	// Download all aircraft data
	static void GetAllNetworkAircraft();

	// Post new aircraft data
	static void PostNetworkAircraft(void* args);

	// Update aircraft data
	static void UpdateNetworkAircraft(void* args);

	// Read vnaaats.cfg (server address, API key, squawk option) from the DLL folder
	static void LoadSettings();
	static bool AutoType; // autotype=1: type .selcal / .contactme / .msg into EuroScope when the FDD asks
	static bool SquawkOnClearance; // assign 2000 when a clearance is sent
	static string ControllerCallsign; // my EuroScope callsign, sent with changes
	static string BetaUrl; // download page (server + /beta)
	static string FddUrl; // flight data display address (server + /fdd)
	static string ScratchUrl; // server + /PluginScratch?code=KEY
	static string SyncScratch(CFlightPlan& esFp); // two-way sync of the scratchpad note between EuroScope and the server; returns the note
	static void PushScratch(void* args);
	static string AselUrl; // server + /PluginAsel?code=KEY
	static void ReportAsel(void* args); // tells the server which aircraft was clicked in EuroScope
	static string UrlEncode(const string& value); // percent-encode a query value
	// Commands sent from the web flight data display (track / untrack / transfer)
	struct CRemoteCommand {
		int Id = 0;
		string Type, Callsign, Level, Mach, Selcal, Target, Freq, Text;
	};
	static void PollRemoteCommands(void* args); // async: asks the server for waiting commands
	static void ReportRemoteCommand(void* args); // async: tells the server how a command went
	static vector<CRemoteCommand> TakeRemoteCommands(); // commands collected so far (called from the EuroScope thread)
	static void ReportRemoteResult(int id, bool ok, const string& msg);
	static std::atomic<bool> RemotePollBusy;
	static clock_t RemoteLastPoll;
	static void PostCpdlcHandoff(void* args); // tell the server a handoff was started (it messages the pilot if on CPDLC)
	
	private:
		// Methods
		static int GetTrackSource(CPlugIn* plugin); // Event tracks or not

		// Version URL
		static const string PluginVersion;

		// NAT Track URL
		static string TrackURL;
		static map<string, CAircraftFlightPlan> flights;

		// vNAAATS API Links
		static string TrackSource;
		static string GetSingleAircraft;
		static string FlightDataUpdate;
		static string PostSingleAircraft;
		static string CpdlcHandoffUrl;
		static string CommandsUrl;
		static string CommandDoneUrl;
		static std::mutex remoteLock;
		static vector<CRemoteCommand> remoteQueue;
};
