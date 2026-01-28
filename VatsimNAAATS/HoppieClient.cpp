#include "pch.h"
#include "HoppieClient.h"
#include "Logger.h"
#include "MenuBar.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>

const string HOPPIE_URL = "https://www.hoppie.nl/acars/system/connect.html";

CHoppieClient::CHoppieClient() 
	: m_connected(false), m_nextMessageId(1), m_lastPollTime(0), m_soundEnabled(true), m_hInternet(nullptr) {
	// Default sound path - will be in plugin directory
	m_soundPath = "";
}

CHoppieClient::~CHoppieClient() {
	Disconnect();
	if (m_hInternet) {
		InternetCloseHandle(m_hInternet);
		m_hInternet = nullptr;
	}
}

void CHoppieClient::SetLogonCode(const string& code) {
	m_logonCode = code;
}

void CHoppieClient::SetCallsign(const string& callsign) {
	m_callsign = callsign;
}

void CHoppieClient::SetAutoLogin(bool enabled) {
	m_autoLogin = enabled;
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
	string encoded = "";
	char buf[4];
	
	for (char c : str) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			encoded += c;
		}
		else {
			sprintf_s(buf, "%%%02X", c);
			encoded += buf;
		}
	}
	
	return encoded;
}

bool CHoppieClient::IsLoggedOn(const string& callsign) {
	if (m_connectedAircraft.find(callsign) != m_connectedAircraft.end()) {
		return m_connectedAircraft[callsign].LoggedOn;
	}
	return false;
}

string CHoppieClient::HttpPost(const string& data) {
	string response;
	int maxRetries = 1; // Retry once if session reset is needed
	
	for (int attempt = 0; attempt <= maxRetries; attempt++) {
		// Ensure internet handle is open
		if (!m_hInternet) {
			m_hInternet = InternetOpenA("vNAAATS-CPDLC/2.0", 
										INTERNET_OPEN_TYPE_PRECONFIG, 
										NULL, NULL, 0);
			if (!m_hInternet) {
				DWORD error = GetLastError();
				m_lastStatusMessage = "InternetOpen failed: " + to_string(error);
				CLogger::Log(CLogType::ERR, "Failed to open internet handle. Error: " + to_string(error), "CHoppieClient::HttpPost");
				return "error {connection failed - no internet handle}";
			}
			
			// Set timeouts (15 seconds)
			DWORD timeout = 15000;
			InternetSetOption(m_hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
			InternetSetOption(m_hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
			InternetSetOption(m_hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
		}
		
		// Connect to the Hoppie server
		HINTERNET hConnect = InternetConnectA(m_hInternet,
											  "www.hoppie.nl",
											  INTERNET_DEFAULT_HTTPS_PORT,
											  NULL, NULL,
											  INTERNET_SERVICE_HTTP,
											  0, 0);
		
		if (!hConnect) {
			DWORD error = GetLastError();
			
			// Handle resource exhaustion (Error 1450)
			if (error == ERROR_NO_SYSTEM_RESOURCES || error == 1450) {
				CLogger::Log(CLogType::ERR, "Resource exhaustion (1450) detected. Resetting Internet Session.", "CHoppieClient::HttpPost");
				if (m_hInternet) {
					InternetCloseHandle(m_hInternet);
					m_hInternet = nullptr; 
				}
				
				// Retry loop will handle re-opening
				if (attempt < maxRetries) {
					CLogger::Log(CLogType::NORM, "Retrying connection after session reset...", "CHoppieClient::HttpPost");
					continue;
				}
				
				return "error {resource exhaustion - session reset failed}";
			}

			m_lastStatusMessage = "InternetConnect failed: " + to_string(error);
			CLogger::Log(CLogType::ERR, "Failed to connect to Hoppie server. Error: " + to_string(error), "CHoppieClient::HttpPost");
			// Don't close m_hInternet here as it might be temporary network issue
			return "error {connection failed - error " + to_string(error) + "}";
		}
		
		// Open HTTP request
		HINTERNET hRequest = HttpOpenRequestA(hConnect,
											  "POST",
											  "/acars/system/connect.html",
											  NULL, NULL, NULL,
											  INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE,
											  0);
		
		if (!hRequest) {
			DWORD error = GetLastError();
			
			// Handle resource exhaustion (Error 1450)
			if (error == ERROR_NO_SYSTEM_RESOURCES || error == 1450) {
				CLogger::Log(CLogType::ERR, "Resource exhaustion (1450) detected in OpenRequest. Resetting Internet Session.", "CHoppieClient::HttpPost");
				InternetCloseHandle(hConnect);
				if (m_hInternet) {
					InternetCloseHandle(m_hInternet);
					m_hInternet = nullptr;
				}
				
				if (attempt < maxRetries) {
					CLogger::Log(CLogType::NORM, "Retrying request after session reset...", "CHoppieClient::HttpPost");
					continue;
				}
				return "error {resource exhaustion - session reset failed}";
			}

			m_lastStatusMessage = "HttpOpenRequest failed: " + to_string(error);
			CLogger::Log(CLogType::ERR, "Failed to open HTTP request. Error: " + to_string(error), "CHoppieClient::HttpPost");
			InternetCloseHandle(hConnect);
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
			
			// Success! Clean up handles and return response
			InternetCloseHandle(hRequest);
			InternetCloseHandle(hConnect);
			return response.empty() ? "error {no response}" : response;
		}
		else {
			DWORD error = GetLastError();
			m_lastStatusMessage = "HttpSendRequest failed: " + to_string(error);
			CLogger::Log(CLogType::ERR, "HTTP request failed. Error: " + to_string(error), "CHoppieClient::HttpPost");
			response = "error {request failed - error " + to_string(error) + "}";
			
			// Also check for 1450 here
			if (error == ERROR_NO_SYSTEM_RESOURCES || error == 1450) {
				InternetCloseHandle(hRequest);
				InternetCloseHandle(hConnect);
				if (m_hInternet) {
					InternetCloseHandle(m_hInternet);
					m_hInternet = nullptr;
				}
				
				if (attempt < maxRetries) {
					CLogger::Log(CLogType::NORM, "Retrying send after session reset...", "CHoppieClient::HttpPost");
					continue;
				}
				return "error {resource exhaustion - session reset failed}";
			}
		}
		
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		// Do NOT close m_hInternet here
		
		return response;
	}
	
	return "error {max retries exceeded}";
}

bool CHoppieClient::PingServer() {
	lock_guard<recursive_mutex> lock(m_mutex);

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
	
	if (!success) {
		if (response.find("error") == 0) {
			m_lastStatusMessage = "Hoppie Error: " + response;
		} else {
			m_lastStatusMessage = "Unknown server response: " + response;
		}
	} else {
		m_lastStatusMessage = "Connected successfully";
	}
	
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
	
	lock_guard<recursive_mutex> lock(m_mutex);

	try {
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
	catch (const std::exception& e) {
		CLogger::Log(CLogType::ERR, string("Exception in Poll: ") + e.what(), "CHoppieClient::Poll");
	}
	catch (...) {
		CLogger::Log(CLogType::ERR, "Unknown exception in Poll", "CHoppieClient::Poll");
	}
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
		bool shouldAlert = true;
		if (type == "cpdlc") {
			ParseCpdlcPacket(msg, from, content);
			
			// Check for logon request
			string upperContent = msg.Content; // Use parsed content
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
				
				if (m_autoLogin) {
					AcceptLogon(from);
					CLogger::Log(CLogType::NORM, "Auto-accepted logon from " + from, "CHoppieClient::ParseResponse");
					shouldAlert = false;
				}
				else if (m_onLogonRequest) {
					m_onLogonRequest(from);
				}
			}
			
			// Check for responses (WILCO, ROGER, UNABLE, STANDBY)
			if (upperContent.find("WILCO") != string::npos ||
				upperContent.find("ROGER") != string::npos ||
				upperContent.find("UNABLE") != string::npos ||
				upperContent.find("STANDBY") != string::npos) {
				msg.Status = CpdlcMessageStatus::ACKNOWLEDGED;
				
				// Check if this is a WILCO response to a CONTACT message
				// If so, auto-disconnect the aircraft
				if (upperContent.find("WILCO") != string::npos) {
					// Check if there's a pending CONTACT message for this aircraft
					for (auto& pendingMsg : m_pendingMessages) {
						if (pendingMsg.To == from && 
							pendingMsg.Direction == CpdlcDirection::DOWNLINK &&
							pendingMsg.Status == CpdlcMessageStatus::PENDING) {
							string pendingUpper = pendingMsg.Content;
							transform(pendingUpper.begin(), pendingUpper.end(), pendingUpper.begin(), ::toupper);
							if (pendingUpper.find("CONTACT") != string::npos) {
								// Mark original message as acknowledged
								pendingMsg.Status = CpdlcMessageStatus::ACKNOWLEDGED;
								// Auto-disconnect this aircraft
								DisconnectAircraft(from);
								CLogger::Log(CLogType::NORM, "Auto-disconnected " + from + " after WILCO to CONTACT message", 
											 "CHoppieClient::ParseResponse");
								break;
							}
							// Check for release message
							if (pendingUpper.find("NO FURTHER ATC AVAILABLE") != string::npos) {
								// Mark original message as acknowledged
								pendingMsg.Status = CpdlcMessageStatus::ACKNOWLEDGED;
								// Auto-disconnect this aircraft
								DisconnectAircraft(from);
								CLogger::Log(CLogType::NORM, "Auto-disconnected " + from + " after WILCO to RELEASE message", 
											 "CHoppieClient::ParseResponse");
								break;
							}
						}
					}
				}
			}
		}
		
		// Add to pending messages
		m_pendingMessages.push_back(msg);
		
		// Add to aircraft history if we know them
		if (m_connectedAircraft.find(from) != m_connectedAircraft.end()) {
			m_connectedAircraft[from].History.push_back(msg);
		}
		
		// Play notification sound
		if (shouldAlert) {
			PlayMessageSound();
			
			// Trigger CPDLC button flash alert on menu bar
			CMenuBar::CpdlcAlert = true;
			CMenuBar::CpdlcAlertTime = time(0);
		}
		
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

void CHoppieClient::ParseCpdlcPacket(CCpdlcMessage& msg, const string& from, const string& content) {
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
		
		// Update the message with parsed data
		try {
			msg.Id = stoi(min);
			if (!mrn.empty()) {
				msg.ReplyToId = stoi(mrn);
			}
		}
		catch (...) {
			// Parse error, ignore
		}
		msg.Content = message;
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
	
	lock_guard<recursive_mutex> lock(m_mutex);
	
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
	lock_guard<recursive_mutex> lock(m_mutex);
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

bool CHoppieClient::SendContactOnHandoff(const string& aircraft, const string& facility, const string& frequency) {
	// Check if aircraft is connected to CPDLC
	auto it = m_connectedAircraft.find(aircraft);
	if (it == m_connectedAircraft.end() || !it->second.LoggedOn) {
		CLogger::Log(CLogType::WARN, "Cannot send contact message - " + aircraft + " not connected to CPDLC", 
					 "CHoppieClient::SendContactOnHandoff");
		return false;
	}
	
	// Build and send the contact message
	string message = BuildContactFreq(facility, frequency);
	bool result = SendCpdlc(aircraft, message);
	
	if (result) {
		CLogger::Log(CLogType::NORM, "Sent contact message to " + aircraft + ": " + message, 
					 "CHoppieClient::SendContactOnHandoff");
	}
	
	return result;
}
