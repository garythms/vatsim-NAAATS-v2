#pragma once
#include "pch.h"
#include "EuroScopePlugIn.h"
#include "BaseWindow.h"
#include "HoppieClient.h"
#include "Constants.h"
#include <string>
#include <map>
#include <vector>
#include <gdiplus.h>
#include <future>

using namespace std;
using namespace Gdiplus;
using namespace EuroScopePlugIn;

class CCPDLCWindow : public CBaseWindow
{
private:
	// Selected items
	string selectedAircraft;
	int selectedMessageId;
	
	// Scroll positions
	int messagesScrollPos;
	int aircraftScrollPos;
	
	// Message compose
	string composeText;
	
	// Async polling
	std::future<void> m_pollFuture;

public:
	// Hoppie client instance (static so it persists)
	static CHoppieClient* hoppieClient;
	
	// Get Hoppie client instance
	static CHoppieClient* GetHoppieClient() { return hoppieClient; }
	
	// Connection state
	bool IsConnected;
	string CurrentStation;
	
	// Inherited methods
	CCPDLCWindow(POINT topLeft);
	virtual void MakeWindowItems();
	virtual void RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen);
	
	// Background tick (polling/cleanup). Runs even when window is closed.
	void Tick();
	
	// Render sub-panels
	void RenderLoginPanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area);
	void RenderMessagesPanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area);
	void RenderAircraftPanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area);
	void RenderComposePanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area);
	
	// Button clicks
	virtual void ButtonDown(int id);
	virtual void ButtonUp(int id, CRadarScreen* screen = nullptr);
	virtual void ButtonPress(int id);
	virtual void ButtonUnpress(int id);
	virtual void SetButtonState(int id, CInputState state);
	void SetTextValue(CRadarScreen* screen, int id, string content);
	CInputState GetInputState(int id);
	bool IsButtonPressed(int id);
	
	// CPDLC Operations
	void Connect(CRadarScreen* screen);
	void Disconnect();
	void SendMessage(const string& callsign, const string& message);
	void AcceptLogon(const string& callsign);
	void RejectLogon(const string& callsign);
	void RespondToMessage(int messageId, const string& response);
	
	// Selection
	void SelectAircraft(const string& callsign);
	void SelectMessage(int messageId);
	void OnOverDropDownItem(int id);

	// Pending release map (Callsign -> isPending)
	static map<string, bool> PendingRelease;
	static map<string, bool> PendingHandoff;

	// Station structure
	struct CPDLCStation {
		string Code;
		string Name;
		string Radio;
	};
	static map<string, CPDLCStation> Stations;
	static void InitializeStations();
	static CPDLCStation* GetStationByController(string controllerCallsign);

	// Handoff event
	void OnHandoffAccepted(CRadarScreen* screen, string callsign, string nextController);

	// Check for pending releases
	static void CheckForRelease(CRadarScreen* screen);

	// Cleanup static resources
	static void Cleanup();
	
	// Window size
	static const int WINSZ_CPDLC_WIDTH = 650;
	static const int WINSZ_CPDLC_HEIGHT = 500;
	
	// Button definitions
	static const int BTN_CLOSE = 0;
	static const int BTN_CONNECT = 1;
	static const int BTN_DISCONNECT = 2;
	static const int BTN_ACCEPT_LOGON = 3;
	static const int BTN_REJECT_LOGON = 4;
	static const int BTN_SEND = 5;
	static const int BTN_WILCO = 6;
	static const int BTN_ROGER = 7;
	static const int BTN_STANDBY = 8;
	static const int BTN_UNABLE = 9;
	static const int BTN_CLEAR_MSG = 10;
	static const int BTN_COMPOSE = 11;
	static const int BTN_MSG_SCROLL_UP = 12;
	static const int BTN_MSG_SCROLL_DN = 13;
	static const int BTN_AC_SCROLL_UP = 14;
	static const int BTN_AC_SCROLL_DN = 15;
	static const int BTN_POLL = 16;  // Manual poll button
	static const int BTN_DISCONNECT_AC = 17;  // Disconnect selected aircraft
	
	// Dropdown definitions
	static const int DRP_STATION = 200;
	
	// Text input definitions
	// NOTE: Popup edit FunctionId is global across the plugin, so these IDs MUST NOT
	// collide with other windows (e.g. FlightPlanWindow uses 100+).
	static const int TXT_LOGON_CODE = 5100;
	static const int TXT_COMPOSE = 5101;
	static const int TXT_STATION = 5102;  // Station callsign input
	
	// Checkbox definitions
	static const int CHK_AUTO_LOGIN = 300;
	
	// Message type buttons for compose
	static const int BTN_MSG_CLIMB = 20;
	static const int BTN_MSG_DESCEND = 21;
	static const int BTN_MSG_MAINTAIN = 22;
	static const int BTN_MSG_PROCEED = 23;
	static const int BTN_MSG_CONTACT = 24;
	static const int BTN_MSG_SQUAWK = 25;
	static const int BTN_MSG_CLEARED = 26;
	static const int BTN_MSG_CROSS = 27;
	static const int BTN_MSG_FREETEXT = 28;
};
