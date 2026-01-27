#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <functional>
#include <mutex>
#include <WinInet.h>
#include <mmsystem.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winmm.lib")

using namespace std;

// Message status
enum class CpdlcMessageStatus {
	PENDING,		// Awaiting response
	ACKNOWLEDGED,	// Got WILCO/ROGER/etc
	CLOSED			// No longer active
};

// Message direction
enum class CpdlcDirection {
	UPLINK,			// From pilot to us
	DOWNLINK		// From us to pilot
};

// CPDLC Message structure
struct CCpdlcMessage {
	int Id;							// Message ID (MIN)
	int ReplyToId;					// Reference ID (MRN) - what this replies to
	string From;					// Sender callsign
	string To;						// Recipient callsign
	string Content;					// Message text
	string Timestamp;				// When received/sent (display string)
	time_t ReceivedTime;			// When received (for cleanup)
	CpdlcDirection Direction;		// Uplink or downlink
	CpdlcMessageStatus Status;		// Current status
	string Response;				// Response received (if any)
};

// Connected aircraft structure
struct CConnectedAircraft {
	string Callsign;
	bool LoggedOn;					// CPDLC session active
	string LogonTime;				// When they logged on
	vector<CCpdlcMessage> History;	// Message history with this aircraft
};

// Callback types
typedef function<void(const CCpdlcMessage&)> MessageCallback;
typedef function<void(const string&)> LogonRequestCallback;

class CHoppieClient {
public:
	CHoppieClient();
	~CHoppieClient();
	
	// Configuration
	void SetLogonCode(const string& code);
	void SetCallsign(const string& callsign);
	string GetCallsign() const { return m_callsign; }
	string GetLogonCode() const { return m_logonCode; }
	
	// Connection
	bool Connect();
	void Disconnect();
	bool IsConnected() const { return m_connected; }
	bool PingServer();
	
	// Polling - call this periodically
	void Poll();
	
	// Get last poll time for rate limiting
	time_t GetLastPollTime() const { return m_lastPollTime; }
	
	// CPDLC Operations
	bool AcceptLogon(const string& aircraft);
	bool RejectLogon(const string& aircraft, const string& reason = "");
	bool SendCpdlc(const string& to, const string& message, int replyTo = -1);
	bool SendTelex(const string& to, const string& message);
	
	// Quick response methods
	bool SendWilco(const string& to, int replyTo);
	bool SendRoger(const string& to, int replyTo);
	bool SendStandby(const string& to, int replyTo);
	bool SendUnable(const string& to, int replyTo, const string& reason = "");
	
	// Message builders for oceanic operations
	string BuildClearedMessage(const string& dest, const string& route, 
							   const string& level, const string& mach);
	string BuildClimbTo(const string& level);
	string BuildDescendTo(const string& level);
	string BuildMaintainMach(const string& mach);
	string BuildMaintainLevel(const string& level);
	string BuildProceedDirect(const string& fix);
	string BuildCrossAt(const string& fix, const string& time);
	string BuildCrossAtLevel(const string& fix, const string& level);
	string BuildContactFreq(const string& facility, const string& freq);
	string BuildSquawk(const string& code);
	
	// Data access
	vector<CCpdlcMessage>& GetPendingMessages();
	vector<CCpdlcMessage> GetMessagesForAircraft(const string& callsign);
	map<string, CConnectedAircraft>& GetConnectedAircraft();
	CConnectedAircraft* GetAircraft(const string& callsign);
	
	// Mark message as read/acknowledged
	void AcknowledgeMessage(int messageId);
	void CloseMessage(int messageId);
	
	// Disconnect aircraft
	void DisconnectAircraft(const string& callsign);
	
	// Send contact message when handoff initiated (auto-disconnects on WILCO)
	bool SendContactOnHandoff(const string& aircraft, const string& facility, const string& frequency);
	
	// Cleanup old closed/acknowledged messages (called periodically)
	void CleanupOldMessages(int maxAgeSeconds = 90);
	
	// Callbacks
	void SetOnMessageReceived(MessageCallback callback);
	void SetOnLogonRequest(LogonRequestCallback callback);
	
	// Sound settings
	void SetSoundPath(const string& path);
	void PlayMessageSound();
	void SetSoundEnabled(bool enabled);
	
	// Clear all data
	void ClearAll();
	
private:
	string m_logonCode;
	string m_callsign;
	bool m_connected;
	int m_nextMessageId;
	time_t m_lastPollTime;
	
	// Sound
	string m_soundPath;
	bool m_soundEnabled;
	
	vector<CCpdlcMessage> m_pendingMessages;
	map<string, CConnectedAircraft> m_connectedAircraft;
	
	MessageCallback m_onMessageReceived;
	LogonRequestCallback m_onLogonRequest;
	
	mutex m_mutex;  // Thread safety for polling
	
	// Internal methods
	string HttpPost(const string& data);
	string HttpGet(const string& url);
	void ParseResponse(const string& response);
	void ParseCpdlcPacket(const string& from, const string& content);
	string GetCurrentTimestamp();
	string UrlEncode(const string& str);
};
