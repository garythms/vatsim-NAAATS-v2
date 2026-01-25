#include "pch.h"
#include "HoppieClient.h"
#include "Logger.h"
#include "MenuBar.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>

const string HOPPIE_URL = "http://www.hoppie.nl/acars/system/connect.html";

CHoppieClient::CHoppieClient() 
	: m_connected(false), m_nextMessageId(1), m_lastPollTime(0), m_soundEnabled(true) {
	// Default sound path - will be in plugin directory
	m_soundPath = "";
}

CHoppieClient::~CHoppieClient() {
	Disconnect();
}

void CHoppieClient::SetLogonCode(const string& code) {
	m_logonCode = code;
}

void CHoppieClient::SetCallsign(const string& callsign) {
	m_callsign = callsign;
}

string CHoppieClient::GetCurrentTimestamp() {
	time_t now = time(0);
	tm* gmt = gmtime(&now);
	stringstream ss;
	ss << setfill('0') << setw(2) << gmt->tm_hour 
	   << setw(2) << gmt->tm_min << "Z";
	return ss.str();
}

string CHoppieClient::UrlEncode(const string& str) {
	string result;
	for (char c : str) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			result += c;
		}
		else if (c == ' ') {
			result += '+';
		}
		else {
			stringstream ss;
			ss << '%' << uppercase << hex << setfill('0') << setw(2) << (int)(unsigned char)c;
			result += ss.str();
		}
	}
	return result;
}

string CHoppieClient::HttpPost(const string& data) {
	string response;
	
	HINTERNET hInternet = InternetOpenA("vNAAATS-CPDLC/2.0", 
										INTERNET_OPEN_TYPE_PRECONFIG, 
										NULL, NULL, 0);
	if (!hInternet) {
		DWORD error = GetLastError();
		CLogger::Log(CLogType::ERR, "Failed to open internet handle. Error: " + to_string(error), "CHoppieClient::HttpPost");
		return "error {connection failed - no internet handle}";
	}
	
	// Connect to the Hoppie server
	HINTERNET hConnect = InternetConnectA(hInternet,
										  "www.hoppie.nl",
										  INTERNET_DEFAULT_HTTP_PORT,
										  NULL, NULL,
										  INTERNET_SERVICE_HTTP,
										  0, 0);
	
	if (!hConnect) {
		DWORD error = GetLastError();
		CLogger::Log(CLogType::ERR, "Failed to connect to Hoppie server. Error: " + to_string(error), "CHoppieClient::HttpPost");
		InternetCloseHandle(hInternet);
		return "error {connection failed - error " + to_string(error) + "}";
	}
	
	// Open HTTP request
	HINTERNET hRequest = HttpOpenRequestA(hConnect,
										  "POST",
										  "/acars/system/connect.html",
										  NULL, NULL, NULL,
										  INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
										  0);
	
	if (!hRequest) {
		DWORD error = GetLastError();
		CLogger::Log(CLogType::ERR, "Failed to open HTTP request. Error: " + to_string(error), "CHoppieClient::HttpPost");
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return "error {request failed - error " + to_string(error) + "}";
	}
	
	// Set headers for POST
	string headers = "Content-Type: application/x-www-form-urlencoded";
	
	CLogger::Log(CLogType::NORM, "Sending POST to Hoppie: " + data, "CHoppieClient::HttpPost");
	
	// Send the request
	BOOL result = HttpSendRequestA(hRequest,
								   headers.c_str(),
								   (DWORD)headers.length(),
								   (LPVOID)data.c_str(),
								   (DWORD)data.length());
	
	if (result) {
		char buffer[8192];
		DWORD bytesRead;
		while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
			buffer[bytesRead] = '\0';
			response += buffer;
		}
		CLogger::Log(CLogType::NORM, "Response received: " + response, "CHoppieClient::HttpPost");
	}
	else {
		DWORD error = GetLastError();
		CLogger::Log(CLogType::ERR, "HTTP request failed. Error: " + to_string(error), "CHoppieClient::HttpPost");
		response = "error {request failed - error " + to_string(error) + "}";
	}
	
	InternetCloseHandle(hRequest);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);
	
	return response.empty() ? "error {no response}" : response;
}

bool CHoppieClient::PingServer() {
	if (m_logonCode.empty() || m_callsign.empty()) {
		CLogger::Log(CLogType::ERR, "PingServer failed: logon code or callsign empty", "CHoppieClient::PingServer");
		return false;
	}
	
	CLogger::Log(CLogType::NORM, "Pinging Hoppie server as " + m_callsign, "CHoppieClient::PingServer");
	
	string postData = "logon=" + UrlEncode(m_logonCode) +
					  "&from=" + UrlEncode(m_callsign) +
					  "&to=SERVER" +
					  "&type=ping" +
					  "&packet=";
	
	string response = HttpPost(postData);
	bool success = response.find("ok") == 0;
	
	CLogger::Log(success ? CLogType::NORM : CLogType::ERR, 
				 "Ping server response: " + response + " (success=" + (success ? "true" : "false") + ")", 
				 "CHoppieClient::PingServer");
	
	return success;
}

bool CHoppieClient::Connect() {
	if (m_logonCode.empty() || m_callsign.empty()) {
		CLogger::Log(CLogType::ERR, "Cannot connect: logon code or callsign not set", "CHoppieClient::Connect");
		return false;
	}
	
	m_connected = PingServer();
	
	if (m_connected) {
		CLogger::Log(CLogType::NORM, "Connected to Hoppie ACARS as " + m_callsign, "CHoppieClient::Connect");
	}
	
	return m_connected;
}

void CHoppieClient::Disconnect() {
	m_connected = false;
	CLogger::Log(CLogType::NORM, "Disconnected from Hoppie ACARS", "CHoppieClient::Disconnect");
}

void CHoppieClient::Poll() {
	if (!m_connected) {
		CLogger::Log(CLogType::WARN, "Poll called but not connected", "CHoppieClient::Poll");
		return;
	}
	
	CLogger::Log(CLogType::NORM, "Starting poll for " + m_callsign, "CHoppieClient::Poll");
	
	lock_guard<mutex> lock(m_mutex);
	
	string postData = "logon=" + UrlEncode(m_logonCode) +
					  "&from=" + UrlEncode(m_callsign) +
					  "&to=SERVER" +
					  "&type=poll" +
					  "&packet=";
	
	string response = HttpPost(postData);
	m_lastPollTime = time(0);
	
	CLogger::Log(CLogType::NORM, "Poll response: " + response, "CHoppieClient::Poll");
	
	ParseResponse(response);
}

void CHoppieClient::ParseResponse(const string& response) {
	// Response format: ok {FROM TYPE {CONTENT}}{FROM TYPE {CONTENT}}...
	// or: ok (no messages)
	// or: error {reason}
	
	CLogger::Log(CLogType::NORM, "Parsing response: " + response, "CHoppieClient::ParseResponse");
	
	if (response.find("ok") != 0) {
		if (response.find("error") == 0) {
			CLogger::Log(CLogType::ERR, "Hoppie error: " + response, "CHoppieClient::ParseResponse");
		}
		return;
	}
	
	// Check if there are any messages (ok with no braces means no messages)
	if (response.find("{") == string::npos) {
		CLogger::Log(CLogType::NORM, "No messages in response", "CHoppieClient::ParseResponse");
		return;
	}
	
	// Find first message block
	size_t pos = response.find("{", 2);
	
	while (pos != string::npos) {
		// Extract: FROM TYPE {CONTENT}
		size_t spacePos = response.find(" ", pos + 1);
		if (spacePos == string::npos) break;
		
		string from = response.substr(pos + 1, spacePos - pos - 1);
		
		size_t typeEnd = response.find(" ", spacePos + 1);
		if (typeEnd == string::npos) break;
		
		string type = response.substr(spacePos + 1, typeEnd - spacePos - 1);
		
		size_t contentStart = response.find("{", typeEnd);
		if (contentStart == string::npos) break;
		
		size_t contentEnd = response.find("}", contentStart);
		if (contentEnd == string::npos) break;
		
		string content = response.substr(contentStart + 1, contentEnd - contentStart - 1);
		
		CLogger::Log(CLogType::NORM, "Parsed message - From: " + from + ", Type: " + type + ", Content: " + content, "CHoppieClient::ParseResponse");
		
		// Create message
		CCpdlcMessage msg;
		msg.Id = m_nextMessageId++;
		msg.From = from;
		msg.To = m_callsign;
		msg.Content = content;
		msg.Timestamp = GetCurrentTimestamp();
		msg.ReceivedTime = time(0);  // For cleanup tracking
		msg.Direction = CpdlcDirection::UPLINK;
		msg.Status = CpdlcMessageStatus::PENDING;
		msg.ReplyToId = -1;
		
		// Parse CPDLC-specific format
		if (type == "cpdlc") {
			ParseCpdlcPacket(from, content);
			
			// Check for logon request
			string upperContent = content;
			transform(upperContent.begin(), upperContent.end(), upperContent.begin(), ::toupper);
			
			if (upperContent.find("REQUEST LOGON") != string::npos ||
				upperContent.find("LOGON") != string::npos) {
				// This is a logon request
				if (m_connectedAircraft.find(from) == m_connectedAircraft.end()) {
					CConnectedAircraft ac;
					ac.Callsign = from;
					ac.LoggedOn = false;  // Not yet accepted
					ac.LogonTime = "";
					m_connectedAircraft[from] = ac;
				}
				
				if (m_onLogonRequest) {
					m_onLogonRequest(from);
				}
			}
			
			// Check for responses (WILCO, ROGER, UNABLE, STANDBY)
			if (upperContent.find("WILCO") != string::npos ||
				upperContent.find("ROGER") != string::npos ||
				upperContent.find("UNABLE") != string::npos ||
				upperContent.find("STANDBY") != string::npos) {
				msg.Status = CpdlcMessageStatus::ACKNOWLEDGED;
			}
		}
		
		// Add to pending messages
		m_pendingMessages.push_back(msg);
		
		// Add to aircraft history if we know them
		if (m_connectedAircraft.find(from) != m_connectedAircraft.end()) {
			m_connectedAircraft[from].History.push_back(msg);
		}
		
		// Play notification sound
		PlayMessageSound();
		
		// Trigger CPDLC button flash alert on menu bar
		CMenuBar::CpdlcAlert = true;
		CMenuBar::CpdlcAlertTime = time(0);
		
		// Callback
		if (m_onMessageReceived) {
			m_onMessageReceived(msg);
		}
		
		CLogger::Log(CLogType::NORM, "Received " + type + " from " + from + ": " + content, 
					 "CHoppieClient::ParseResponse");
		
		// Find next message block
		pos = response.find("{", contentEnd + 2);
	}
}

void CHoppieClient::ParseCpdlcPacket(const string& from, const string& content) {
	// CPDLC format: /data2/<min>/<mrn>/<message>
	if (content.find("/data2/") == 0) {
		size_t firstSlash = 7;  // After "/data2/"
		size_t secondSlash = content.find("/", firstSlash);
		if (secondSlash == string::npos) return;
		
		size_t thirdSlash = content.find("/", secondSlash + 1);
		if (thirdSlash == string::npos) return;
		
		string min = content.substr(firstSlash, secondSlash - firstSlash);
		string mrn = content.substr(secondSlash + 1, thirdSlash - secondSlash - 1);
		string message = content.substr(thirdSlash + 1);
		
		// Update the last pending message with parsed data
		if (!m_pendingMessages.empty()) {
			CCpdlcMessage& lastMsg = m_pendingMessages.back();
			try {
				lastMsg.Id = stoi(min);
				if (!mrn.empty()) {
					lastMsg.ReplyToId = stoi(mrn);
				}
			}
			catch (...) {
				// Parse error, ignore
			}
			lastMsg.Content = message;
		}
	}
}

bool CHoppieClient::SendCpdlc(const string& to, const string& message, int replyTo) {
	if (!m_connected) return false;
	
	lock_guard<mutex> lock(m_mutex);
	
	// Build CPDLC packet format: /data2/<min>/<mrn>/<message>
	string mrn = (replyTo > 0) ? to_string(replyTo) : "";
	int msgId = m_nextMessageId++;
	string packet = "/data2/" + to_string(msgId) + "/" + mrn + "/" + message;
	
	string postData = "logon=" + UrlEncode(m_logonCode) +
					  "&from=" + UrlEncode(m_callsign) +
					  "&to=" + UrlEncode(to) +
					  "&type=cpdlc" +
					  "&packet=" + UrlEncode(packet);
	
	string response = HttpPost(postData);
	
	if (response.find("ok") == 0) {
		// Store sent message
		CCpdlcMessage msg;
		msg.Id = msgId;
		msg.From = m_callsign;
		msg.To = to;
		msg.Content = message;
		msg.Timestamp = GetCurrentTimestamp();
		msg.ReceivedTime = time(0);  // For cleanup tracking
		msg.Direction = CpdlcDirection::DOWNLINK;
		msg.Status = CpdlcMessageStatus::PENDING;
		msg.ReplyToId = replyTo;
		
		m_pendingMessages.push_back(msg);
		
		if (m_connectedAircraft.find(to) != m_connectedAircraft.end()) {
			m_connectedAircraft[to].History.push_back(msg);
		}
		
		CLogger::Log(CLogType::NORM, "Sent CPDLC to " + to + ": " + message, "CHoppieClient::SendCpdlc");
		return true;
	}
	
	CLogger::Log(CLogType::ERR, "Failed to send CPDLC to " + to + ": " + response, "CHoppieClient::SendCpdlc");
	return false;
}

bool CHoppieClient::SendTelex(const string& to, const string& message) {
	if (!m_connected) return false;
	
	lock_guard<mutex> lock(m_mutex);
	
	string postData = "logon=" + UrlEncode(m_logonCode) +
					  "&from=" + UrlEncode(m_callsign) +
					  "&to=" + UrlEncode(to) +
					  "&type=telex" +
					  "&packet=" + UrlEncode(message);
	
	string response = HttpPost(postData);
	
	if (response.find("ok") == 0) {
		CLogger::Log(CLogType::NORM, "Sent TELEX to " + to + ": " + message, "CHoppieClient::SendTelex");
		return true;
	}
	
	CLogger::Log(CLogType::ERR, "Failed to send TELEX to " + to + ": " + response, "CHoppieClient::SendTelex");
	return false;
}

bool CHoppieClient::AcceptLogon(const string& aircraft) {
	if (SendCpdlc(aircraft, "LOGON ACCEPTED")) {
		if (m_connectedAircraft.find(aircraft) != m_connectedAircraft.end()) {
			m_connectedAircraft[aircraft].LoggedOn = true;
			m_connectedAircraft[aircraft].LogonTime = GetCurrentTimestamp();
		}
		else {
			CConnectedAircraft ac;
			ac.Callsign = aircraft;
			ac.LoggedOn = true;
			ac.LogonTime = GetCurrentTimestamp();
			m_connectedAircraft[aircraft] = ac;
		}
		CLogger::Log(CLogType::NORM, "Accepted CPDLC logon from " + aircraft, "CHoppieClient::AcceptLogon");
		return true;
	}
	return false;
}

bool CHoppieClient::RejectLogon(const string& aircraft, const string& reason) {
	string msg = reason.empty() ? "LOGON REJECTED" : "LOGON REJECTED - " + reason;
	if (SendCpdlc(aircraft, msg)) {
		// Remove from connected aircraft if present
		m_connectedAircraft.erase(aircraft);
		CLogger::Log(CLogType::NORM, "Rejected CPDLC logon from " + aircraft, "CHoppieClient::RejectLogon");
		return true;
	}
	return false;
}

// Quick response methods
bool CHoppieClient::SendWilco(const string& to, int replyTo) {
	return SendCpdlc(to, "WILCO", replyTo);
}

bool CHoppieClient::SendRoger(const string& to, int replyTo) {
	return SendCpdlc(to, "ROGER", replyTo);
}

bool CHoppieClient::SendStandby(const string& to, int replyTo) {
	return SendCpdlc(to, "STANDBY", replyTo);
}

bool CHoppieClient::SendUnable(const string& to, int replyTo, const string& reason) {
	string msg = reason.empty() ? "UNABLE" : "UNABLE - " + reason;
	return SendCpdlc(to, msg, replyTo);
}

// Message builders for oceanic operations
string CHoppieClient::BuildClearedMessage(const string& dest, const string& route,
										  const string& level, const string& mach) {
	stringstream ss;
	ss << "CLRD TO " << dest << " VIA " << route;
	if (!level.empty()) {
		ss << " MAINTAIN FL" << level;
	}
	if (!mach.empty()) {
		ss << " MACH ." << mach;
	}
	return ss.str();
}

string CHoppieClient::BuildClimbTo(const string& level) {
	return "CLIMB TO FL" + level;
}

string CHoppieClient::BuildDescendTo(const string& level) {
	return "DESCEND TO FL" + level;
}

string CHoppieClient::BuildMaintainMach(const string& mach) {
	return "MAINTAIN M." + mach;
}

string CHoppieClient::BuildMaintainLevel(const string& level) {
	return "MAINTAIN FL" + level;
}

string CHoppieClient::BuildProceedDirect(const string& fix) {
	return "PROCEED DIRECT TO " + fix;
}

string CHoppieClient::BuildCrossAt(const string& fix, const string& time) {
	return "CROSS " + fix + " AT " + time;
}

string CHoppieClient::BuildCrossAtLevel(const string& fix, const string& level) {
	return "CROSS " + fix + " AT FL" + level;
}

string CHoppieClient::BuildContactFreq(const string& facility, const string& freq) {
	return "CONTACT " + facility + " " + freq;
}

string CHoppieClient::BuildSquawk(const string& code) {
	return "SQUAWK " + code;
}

// Data access methods
vector<CCpdlcMessage>& CHoppieClient::GetPendingMessages() {
	return m_pendingMessages;
}

vector<CCpdlcMessage> CHoppieClient::GetMessagesForAircraft(const string& callsign) {
	vector<CCpdlcMessage> result;
	for (auto& msg : m_pendingMessages) {
		if (msg.From == callsign || msg.To == callsign) {
			result.push_back(msg);
		}
	}
	return result;
}

map<string, CConnectedAircraft>& CHoppieClient::GetConnectedAircraft() {
	return m_connectedAircraft;
}

CConnectedAircraft* CHoppieClient::GetAircraft(const string& callsign) {
	auto it = m_connectedAircraft.find(callsign);
	if (it != m_connectedAircraft.end()) {
		return &it->second;
	}
	return nullptr;
}

void CHoppieClient::AcknowledgeMessage(int messageId) {
	for (auto& msg : m_pendingMessages) {
		if (msg.Id == messageId) {
			msg.Status = CpdlcMessageStatus::ACKNOWLEDGED;
			break;
		}
	}
}

void CHoppieClient::CloseMessage(int messageId) {
	for (auto& msg : m_pendingMessages) {
		if (msg.Id == messageId) {
			msg.Status = CpdlcMessageStatus::CLOSED;
			break;
		}
	}
}

void CHoppieClient::SetOnMessageReceived(MessageCallback callback) {
	m_onMessageReceived = callback;
}

void CHoppieClient::SetOnLogonRequest(LogonRequestCallback callback) {
	m_onLogonRequest = callback;
}

void CHoppieClient::ClearAll() {
	lock_guard<mutex> lock(m_mutex);
	m_pendingMessages.clear();
	m_connectedAircraft.clear();
	m_nextMessageId = 1;
}

void CHoppieClient::SetSoundPath(const string& path) {
	m_soundPath = path;
	CLogger::Log(CLogType::NORM, "CPDLC sound path set to: " + path, "CHoppieClient::SetSoundPath");
}

void CHoppieClient::SetSoundEnabled(bool enabled) {
	m_soundEnabled = enabled;
}

void CHoppieClient::PlayMessageSound() {
	if (!m_soundEnabled) return;
	
	if (!m_soundPath.empty()) {
		// Play the specified sound file asynchronously
		PlaySoundA(m_soundPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
		CLogger::Log(CLogType::NORM, "Playing CPDLC notification sound", "CHoppieClient::PlayMessageSound");
	}
	else {
		// Fall back to system notification sound
		PlaySoundA("SystemNotification", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
	}
}

void CHoppieClient::DisconnectAircraft(const string& callsign) {
	// Remove aircraft from connected list
	auto it = m_connectedAircraft.find(callsign);
	if (it != m_connectedAircraft.end()) {
		m_connectedAircraft.erase(it);
		CLogger::Log(CLogType::NORM, "Disconnected aircraft: " + callsign, "CHoppieClient::DisconnectAircraft");
	}
}

void CHoppieClient::CleanupOldMessages(int maxAgeSeconds) {
	time_t now = time(0);
	
	// Remove messages that are CLOSED or ACKNOWLEDGED and older than maxAgeSeconds
	auto it = m_pendingMessages.begin();
	while (it != m_pendingMessages.end()) {
		if ((it->Status == CpdlcMessageStatus::CLOSED || it->Status == CpdlcMessageStatus::ACKNOWLEDGED) &&
			(now - it->ReceivedTime) >= maxAgeSeconds) {
			it = m_pendingMessages.erase(it);
		}
		else {
			++it;
		}
	}
}
