#include "pch.h"
#include "CPDLCWindow.h"
#include "Constants.h"
#include "Styles.h"
#include "Utils.h"
#include "CommonRenders.h"
#include "Logger.h"
#include "MenuBar.h"

using namespace Colours;

// Static member initialization
CHoppieClient* CCPDLCWindow::hoppieClient = nullptr;
map<string, bool> CCPDLCWindow::PendingRelease;
map<string, bool> CCPDLCWindow::PendingHandoff;
map<string, CCPDLCWindow::CPDLCStation> CCPDLCWindow::Stations;

CCPDLCWindow::CCPDLCWindow(POINT topLeft) : CBaseWindow(topLeft) {
	// Make buttons
	MakeWindowItems();
	
	// Close by default
	IsClosed = true;
	
	// Initialize state
	IsConnected = false;
	CurrentStation = "";
	selectedAircraft = "";
	selectedMessageId = -1;
	messagesScrollPos = 0;
	aircraftScrollPos = 0;
	composeText = "";
	
	// Create Hoppie client if not exists
	if (hoppieClient == nullptr) {
		hoppieClient = new CHoppieClient();
	}
}

void CCPDLCWindow::MakeWindowItems() {
	// Main buttons
	windowButtons[BTN_CLOSE] = CWinButton(BTN_CLOSE, WIN_CPDLC, "Close", CInputState::INACTIVE);
	windowButtons[BTN_CONNECT] = CWinButton(BTN_CONNECT, WIN_CPDLC, "Connect", CInputState::INACTIVE);
	windowButtons[BTN_DISCONNECT] = CWinButton(BTN_DISCONNECT, WIN_CPDLC, "Disconnect", CInputState::DISABLED);
	
	// Logon management buttons
	windowButtons[BTN_ACCEPT_LOGON] = CWinButton(BTN_ACCEPT_LOGON, WIN_CPDLC, "Accept", CInputState::DISABLED);
	windowButtons[BTN_REJECT_LOGON] = CWinButton(BTN_REJECT_LOGON, WIN_CPDLC, "Reject", CInputState::DISABLED);
	
	// Message response buttons
	windowButtons[BTN_WILCO] = CWinButton(BTN_WILCO, WIN_CPDLC, "WILCO", CInputState::DISABLED);
	windowButtons[BTN_ROGER] = CWinButton(BTN_ROGER, WIN_CPDLC, "ROGER", CInputState::DISABLED);
	windowButtons[BTN_STANDBY] = CWinButton(BTN_STANDBY, WIN_CPDLC, "STANDBY", CInputState::DISABLED);
	windowButtons[BTN_UNABLE] = CWinButton(BTN_UNABLE, WIN_CPDLC, "UNABLE", CInputState::DISABLED);
	windowButtons[BTN_CLEAR_MSG] = CWinButton(BTN_CLEAR_MSG, WIN_CPDLC, "Clear", CInputState::DISABLED);
	
	// Compose buttons
	windowButtons[BTN_SEND] = CWinButton(BTN_SEND, WIN_CPDLC, "Send", CInputState::DISABLED);
	windowButtons[BTN_COMPOSE] = CWinButton(BTN_COMPOSE, WIN_CPDLC, "Compose", CInputState::DISABLED);
	
	// Scroll buttons
	windowButtons[BTN_MSG_SCROLL_UP] = CWinButton(BTN_MSG_SCROLL_UP, WIN_CPDLC, "^", CInputState::INACTIVE);
	windowButtons[BTN_MSG_SCROLL_DN] = CWinButton(BTN_MSG_SCROLL_DN, WIN_CPDLC, "v", CInputState::INACTIVE);
	windowButtons[BTN_AC_SCROLL_UP] = CWinButton(BTN_AC_SCROLL_UP, WIN_CPDLC, "^", CInputState::INACTIVE);
	windowButtons[BTN_AC_SCROLL_DN] = CWinButton(BTN_AC_SCROLL_DN, WIN_CPDLC, "v", CInputState::INACTIVE);
	windowButtons[BTN_POLL] = CWinButton(BTN_POLL, WIN_CPDLC, "Poll", CInputState::DISABLED);
	
	// Message type buttons
	windowButtons[BTN_MSG_CLIMB] = CWinButton(BTN_MSG_CLIMB, WIN_CPDLC, "CLIMB", CInputState::DISABLED);
	windowButtons[BTN_MSG_DESCEND] = CWinButton(BTN_MSG_DESCEND, WIN_CPDLC, "DESCEND", CInputState::DISABLED);
	windowButtons[BTN_MSG_MAINTAIN] = CWinButton(BTN_MSG_MAINTAIN, WIN_CPDLC, "MAINTAIN", CInputState::DISABLED);
	windowButtons[BTN_MSG_PROCEED] = CWinButton(BTN_MSG_PROCEED, WIN_CPDLC, "DIRECT", CInputState::DISABLED);
	windowButtons[BTN_MSG_CONTACT] = CWinButton(BTN_MSG_CONTACT, WIN_CPDLC, "CONTACT", CInputState::DISABLED);
	windowButtons[BTN_MSG_SQUAWK] = CWinButton(BTN_MSG_SQUAWK, WIN_CPDLC, "SQUAWK", CInputState::DISABLED);
	windowButtons[BTN_MSG_CLEARED] = CWinButton(BTN_MSG_CLEARED, WIN_CPDLC, "CLEARED", CInputState::DISABLED);
	windowButtons[BTN_MSG_CROSS] = CWinButton(BTN_MSG_CROSS, WIN_CPDLC, "CROSS", CInputState::DISABLED);
	windowButtons[BTN_MSG_FREETEXT] = CWinButton(BTN_MSG_FREETEXT, WIN_CPDLC, "FREETEXT", CInputState::DISABLED);
	
	// Text inputs
	textInputs[TXT_LOGON_CODE] = CTextInput(TXT_LOGON_CODE, WIN_CPDLC, "Logon Code", "", 150, CInputState::ACTIVE);
	textInputs[TXT_COMPOSE] = CTextInput(TXT_COMPOSE, WIN_CPDLC, "", "", 280, CInputState::DISABLED);
	textInputs[TXT_STATION] = CTextInput(TXT_STATION, WIN_CPDLC, "Station", "NATX", 80, CInputState::ACTIVE);
	
	// Disconnect aircraft button
	windowButtons[BTN_DISCONNECT_AC] = CWinButton(BTN_DISCONNECT_AC, WIN_CPDLC, "Disc", CInputState::DISABLED);
}

void CCPDLCWindow::Tick() {
	// Check async status
	if (m_pollFuture.valid()) {
		if (m_pollFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			m_pollFuture.get();
		}
	}

	// Auto-poll for messages when connected (every 15 seconds).
	// This is intentionally independent of window visibility so CPDLC keeps working
	// even when the window is closed.
	if (IsConnected && hoppieClient != nullptr && hoppieClient->IsConnected()) {
		// If polling, don't do anything
		if (m_pollFuture.valid()) return;

		time_t now = time(0);
		time_t lastPoll = hoppieClient->GetLastPollTime();
		if (lastPoll == 0 || (now - lastPoll) >= 15) {
			// Run poll and cleanup in background
			m_pollFuture = std::async(std::launch::async, []() {
				hoppieClient->Poll();
				// Cleanup old acknowledged/closed messages (90 seconds)
				hoppieClient->CleanupOldMessages(90);
			});
			
			CLogger::Log(CLogType::NORM, "Auto-polling Hoppie for messages (Async)", "CCPDLCWindow::Tick");
		}
	}
}

void CCPDLCWindow::RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen) {
	// Always run periodic tasks first.
	Tick();
	
	// If window is closed, don't render but still poll above
	if (IsClosed) {
		return;
	}
	
	// Clear alert when window is visible (not closed)
	CMenuBar::CpdlcAlert = false;
	CMenuBar::CpdlcAlertTime = 0;
	
	// Save DC
	int sDC = dc->SaveDC();
	
	// Brushes
	CBrush darkerBrush(DarkBackground.ToCOLORREF());
	CBrush lighterBrush(WindowBorder.ToCOLORREF());
	
	// Main window rectangle
	// CBaseWindow stores the origin as `topLeft`
	CRect windowRect(topLeft.x, topLeft.y, topLeft.x + WINSZ_CPDLC_WIDTH, topLeft.y + WINSZ_CPDLC_HEIGHT);
	dc->FillRect(windowRect, &darkerBrush);
	dc->Draw3dRect(windowRect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	InflateRect(windowRect, -1, -1);
	dc->Draw3dRect(windowRect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	
	// Title bar
	CRect titleRect(windowRect.left + 2, windowRect.top + 2, windowRect.right - 2, windowRect.top + 22);

	// Add screen objects for window dragging (match other windows)
	screen->AddScreenObject(WINDOW, "WIN_CPDLC", windowRect, true, "");
	screen->AddScreenObject(WINDOW, "CPDLC", titleRect, true, "");
	dc->FillSolidRect(titleRect, ButtonPressed.ToCOLORREF());
	
	// Title text
	FontSelector::SelectNormalFont(15, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_CENTER);
	
	string title = "CPDLC";
	if (IsConnected) {
		title += " - " + CurrentStation + " (Connected)";
	}
	else {
		title += " - Not Connected";
	}
	dc->TextOutA(titleRect.left + titleRect.Width() / 2, titleRect.top + 3, title.c_str());
	
	// Close button
	CCommonRenders::RenderButton(dc, screen, { windowRect.right - 65, windowRect.top + 25 }, 60, 25, &windowButtons[BTN_CLOSE]);
	
	// Render panels based on connection state
	if (!IsConnected) {
		// Show login panel
		CRect loginArea(windowRect.left + 5, windowRect.top + 55, windowRect.right - 5, windowRect.bottom - 5);
		RenderLoginPanel(dc, g, screen, loginArea);
	}
	else {
		// Left panel - Messages (60% width)
		int leftWidth = (int)(WINSZ_CPDLC_WIDTH * 0.58);
		CRect messagesArea(windowRect.left + 5, windowRect.top + 55, windowRect.left + leftWidth, windowRect.bottom - 5);
		RenderMessagesPanel(dc, g, screen, messagesArea);
		
		// Right panel - Connected Aircraft (40% width)
		CRect aircraftArea(windowRect.left + leftWidth + 5, windowRect.top + 55, windowRect.right - 5, windowRect.bottom - 150);
		RenderAircraftPanel(dc, g, screen, aircraftArea);
		
		// Bottom right - Compose panel
		CRect composeArea(windowRect.left + leftWidth + 5, windowRect.bottom - 145, windowRect.right - 5, windowRect.bottom - 5);
		RenderComposePanel(dc, g, screen, composeArea);
	}
	
	// Restore DC
	dc->RestoreDC(sDC);
}

void CCPDLCWindow::RenderLoginPanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area) {
	// Panel background
	CBrush panelBrush(DarkBackground.ToCOLORREF());
	dc->FillRect(area, &panelBrush);
	dc->Draw3dRect(area, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());
	
	// Title
	FontSelector::SelectNormalFont(16, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);
	dc->TextOutA(area.left + 10, area.top + 10, "CPDLC Login");
	
	// Station input (text input instead of dropdown)
	FontSelector::SelectNormalFont(14, dc);
	dc->TextOutA(area.left + 10, area.top + 45, "Station:");
	CCommonRenders::RenderTextInput(dc, screen, { area.left + 80, area.top + 40 }, 100, 25, &textInputs[TXT_STATION]);
	
	// Logon code input
	dc->TextOutA(area.left + 10, area.top + 85, "Hoppie Code:");
	CCommonRenders::RenderTextInput(dc, screen, { area.left + 100, area.top + 80 }, 150, 25, &textInputs[TXT_LOGON_CODE]);
	
	// Connect button
	CCommonRenders::RenderButton(dc, screen, { area.left + 10, area.top + 130 }, 100, 30, &windowButtons[BTN_CONNECT]);
	
	// Instructions
	FontSelector::SelectNormalFont(12, dc);
	dc->SetTextColor(Disabled.ToCOLORREF());
	dc->TextOutA(area.left + 10, area.top + 180, "1. Enter your station callsign (e.g. NATX, EGGX, CZQX)");
	dc->TextOutA(area.left + 10, area.top + 200, "2. Enter your Hoppie ACARS logon code");
	dc->TextOutA(area.left + 10, area.top + 220, "3. Click Connect to go online");
	dc->TextOutA(area.left + 10, area.top + 260, "Get a logon code at: www.hoppie.nl/acars/");
}

void CCPDLCWindow::RenderMessagesPanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area) {
	// Panel background
	CBrush panelBrush(DarkBackground.ToCOLORREF());
	dc->FillRect(area, &panelBrush);
	dc->Draw3dRect(area, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());
	
	// Title
	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);
	dc->TextOutA(area.left + 5, area.top + 5, "Messages");
	
	// Poll button (enabled when connected)
	windowButtons[BTN_POLL].State = (hoppieClient != nullptr && hoppieClient->IsConnected()) ? CInputState::INACTIVE : CInputState::DISABLED;
	CCommonRenders::RenderButton(dc, screen, { area.right - 170, area.top + 2 }, 40, 22, &windowButtons[BTN_POLL]);
	
	// Disconnect button
	CCommonRenders::RenderButton(dc, screen, { area.right - 85, area.top + 2 }, 80, 22, &windowButtons[BTN_DISCONNECT]);
	
	// Message list area
	CRect listArea(area.left + 5, area.top + 30, area.right - 25, area.bottom - 80);
	dc->FillSolidRect(listArea, ButtonPressed.ToCOLORREF());
	dc->Draw3dRect(listArea, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());
	
	// Scroll buttons
	CCommonRenders::RenderButton(dc, screen, { area.right - 22, area.top + 30 }, 20, 20, &windowButtons[BTN_MSG_SCROLL_UP]);
	CCommonRenders::RenderButton(dc, screen, { area.right - 22, area.bottom - 100 }, 20, 20, &windowButtons[BTN_MSG_SCROLL_DN]);
	
	// Render messages (newest first)
	if (hoppieClient != nullptr) {
		auto& messages = hoppieClient->GetPendingMessages();
		
		int yPos = listArea.top + 5;
		int itemHeight = 50;
		int visibleItems = (listArea.Height() - 10) / itemHeight;
		int maxTextWidth = listArea.Width() - 15;  // For text truncation
		
		FontSelector::SelectNormalFont(12, dc);
		
		// Iterate in reverse order (newest first)
		int displayCount = 0;
		for (int i = (int)messages.size() - 1 - messagesScrollPos; i >= 0 && displayCount < visibleItems; i--) {
			CCpdlcMessage& msg = messages[i];
			
			// Message background
			CRect msgRect(listArea.left + 2, yPos, listArea.right - 4, yPos + itemHeight - 2);
			
			// Color based on status/type
			COLORREF bgColor;
			if (msg.Status == CpdlcMessageStatus::PENDING) {
				if (msg.Direction == CpdlcDirection::UPLINK) {
					bgColor = RGB(0, 100, 0);  // Green - needs response
				}
				else {
					bgColor = RGB(0, 0, 139);  // Blue - awaiting response
				}
			}
			else if (msg.Status == CpdlcMessageStatus::ACKNOWLEDGED) {
				bgColor = RGB(139, 90, 0);  // Orange/cream - acknowledged
			}
			else {
				bgColor = RGB(50, 50, 50);  // Grey - closed
			}
			
			dc->FillSolidRect(msgRect, bgColor);
			
			// Highlight selected
			if (msg.Id == selectedMessageId) {
				dc->Draw3dRect(msgRect, RGB(255, 255, 0), RGB(255, 255, 0));
			}
			
			// Add screen object for click
			string objName = "CPDLC_MSG_" + to_string(msg.Id);
			screen->AddScreenObject(WIN_CPDLC, objName.c_str(), msgRect, false, "");
			
			// Message header
			dc->SetTextColor(TextWhite.ToCOLORREF());
			string header = msg.Timestamp + " ";
			header += (msg.Direction == CpdlcDirection::UPLINK) ? "FROM " : "TO ";
			header += (msg.Direction == CpdlcDirection::UPLINK) ? msg.From : msg.To;
			dc->TextOutA(msgRect.left + 5, msgRect.top + 3, header.c_str());
			
			// Message content - truncate to fit within box
			string content = msg.Content;
			int maxChars = (msgRect.Width() - 10) / 6;  // Approximate char width
			if (maxChars < 10) maxChars = 10;
			if ((int)content.length() > maxChars) {
				content = content.substr(0, maxChars - 3) + "...";
			}
			dc->SetTextColor(Disabled.ToCOLORREF());
			dc->TextOutA(msgRect.left + 5, msgRect.top + 18, content.c_str());
			
			// Status
			string status;
			switch (msg.Status) {
				case CpdlcMessageStatus::PENDING: status = "[PENDING]"; break;
				case CpdlcMessageStatus::ACKNOWLEDGED: status = "[ACK]"; break;
				case CpdlcMessageStatus::CLOSED: status = "[CLOSED]"; break;
			}
			dc->TextOutA(msgRect.left + 5, msgRect.top + 33, status.c_str());
			
			yPos += itemHeight;
			displayCount++;
		}
	}
	
	// Response buttons (bottom of panel)
	int btnY = area.bottom - 70;
	int btnX = area.left + 5;
	int btnW = 55;
	int btnH = 25;
	int spacing = 5;
	
	FontSelector::SelectNormalFont(11, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->TextOutA(btnX, btnY - 15, "Quick Response:");
	
	// Enable response buttons only if a pending uplink message is selected
	bool canRespond = false;
	if (selectedMessageId > 0 && hoppieClient != nullptr) {
		for (auto& msg : hoppieClient->GetPendingMessages()) {
			if (msg.Id == selectedMessageId && 
				msg.Direction == CpdlcDirection::UPLINK &&
				msg.Status == CpdlcMessageStatus::PENDING) {
				canRespond = true;
				break;
			}
		}
	}
	
	windowButtons[BTN_WILCO].State = canRespond ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_ROGER].State = canRespond ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_STANDBY].State = canRespond ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_UNABLE].State = canRespond ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_CLEAR_MSG].State = (selectedMessageId > 0) ? CInputState::INACTIVE : CInputState::DISABLED;
	
	CCommonRenders::RenderButton(dc, screen, { btnX, btnY }, btnW, btnH, &windowButtons[BTN_WILCO]);
	CCommonRenders::RenderButton(dc, screen, { btnX + btnW + spacing, btnY }, btnW, btnH, &windowButtons[BTN_ROGER]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 2*(btnW + spacing), btnY }, btnW + 10, btnH, &windowButtons[BTN_STANDBY]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 3*(btnW + spacing) + 10, btnY }, btnW, btnH, &windowButtons[BTN_UNABLE]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 4*(btnW + spacing) + 10, btnY }, btnW, btnH, &windowButtons[BTN_CLEAR_MSG]);
	
	// Accept/Reject logon buttons
	btnY += 30;
	dc->TextOutA(btnX, btnY - 3, "Logon:");
	windowButtons[BTN_ACCEPT_LOGON].State = canRespond ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_REJECT_LOGON].State = canRespond ? CInputState::INACTIVE : CInputState::DISABLED;
	CCommonRenders::RenderButton(dc, screen, { btnX + 50, btnY - 5 }, 60, btnH, &windowButtons[BTN_ACCEPT_LOGON]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 115, btnY - 5 }, 60, btnH, &windowButtons[BTN_REJECT_LOGON]);
}

void CCPDLCWindow::RenderAircraftPanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area) {
	// Panel background
	CBrush panelBrush(DarkBackground.ToCOLORREF());
	dc->FillRect(area, &panelBrush);
	dc->Draw3dRect(area, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());
	
	// Title
	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);
	dc->TextOutA(area.left + 5, area.top + 5, "Connected Aircraft");
	
	// Disconnect aircraft button (enabled when aircraft is selected)
	windowButtons[BTN_DISCONNECT_AC].State = (!selectedAircraft.empty()) ? CInputState::INACTIVE : CInputState::DISABLED;
	CCommonRenders::RenderButton(dc, screen, { area.right - 50, area.top + 2 }, 45, 20, &windowButtons[BTN_DISCONNECT_AC]);
	
	// Aircraft list area
	CRect listArea(area.left + 5, area.top + 25, area.right - 25, area.bottom - 5);
	dc->FillSolidRect(listArea, ButtonPressed.ToCOLORREF());
	dc->Draw3dRect(listArea, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());
	
	// Scroll buttons
	CCommonRenders::RenderButton(dc, screen, { area.right - 22, area.top + 25 }, 20, 20, &windowButtons[BTN_AC_SCROLL_UP]);
	CCommonRenders::RenderButton(dc, screen, { area.right - 22, area.bottom - 25 }, 20, 20, &windowButtons[BTN_AC_SCROLL_DN]);
	
	// Render aircraft list
	if (hoppieClient != nullptr) {
		auto& aircraft = hoppieClient->GetConnectedAircraft();
		
		int yPos = listArea.top + 3;
		int itemHeight = 22;
		int idx = 0;
		
		FontSelector::SelectNormalFont(12, dc);
		
		for (auto& pair : aircraft) {
			if (idx < aircraftScrollPos) {
				idx++;
				continue;
			}
			
			if (yPos + itemHeight > listArea.bottom) break;
			
			CConnectedAircraft& ac = pair.second;
			
			// Aircraft row
			CRect acRect(listArea.left + 2, yPos, listArea.right - 2, yPos + itemHeight - 2);
			
			// Background color based on logon status
			COLORREF bgColor = ac.LoggedOn ? RGB(0, 100, 0) : RGB(139, 90, 0);
			dc->FillSolidRect(acRect, bgColor);
			
			// Highlight selected
			if (ac.Callsign == selectedAircraft) {
				dc->Draw3dRect(acRect, RGB(255, 255, 0), RGB(255, 255, 0));
			}
			
			// Add screen object for click
			string objName = "CPDLC_AC_" + ac.Callsign;
			screen->AddScreenObject(WIN_CPDLC, objName.c_str(), acRect, false, "");
			
			// Callsign and status
			dc->SetTextColor(TextWhite.ToCOLORREF());
			string text = ac.Callsign;
			if (!ac.LoggedOn) {
				text += " [PENDING]";
			}
			else if (!ac.LogonTime.empty()) {
				text += " (" + ac.LogonTime + ")";
			}
			dc->TextOutA(acRect.left + 5, acRect.top + 3, text.c_str());
			
			yPos += itemHeight;
			idx++;
		}
	}
}

void CCPDLCWindow::RenderComposePanel(CDC* dc, Graphics* g, CRadarScreen* screen, CRect area) {
	// Panel background
	CBrush panelBrush(DarkBackground.ToCOLORREF());
	dc->FillRect(area, &panelBrush);
	dc->Draw3dRect(area, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());
	
	// Title with selected aircraft
	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);
	
	string title = "Send Message";
	if (!selectedAircraft.empty()) {
		title += " to " + selectedAircraft;
	}
	dc->TextOutA(area.left + 5, area.top + 5, title.c_str());
	
	// Enable compose controls if aircraft is selected and logged on
	bool canCompose = false;
	if (!selectedAircraft.empty() && hoppieClient != nullptr) {
		CConnectedAircraft* ac = hoppieClient->GetAircraft(selectedAircraft);
		if (ac != nullptr && ac->LoggedOn) {
			canCompose = true;
		}
	}
	
	// Calculate button layout based on panel width
	int panelWidth = area.Width() - 10;  // 5px padding each side
	int btnH = 22;
	int spacing = 3;
	int btnW = (panelWidth - (2 * spacing)) / 3;  // 3 buttons per row
	
	int btnY = area.top + 25;
	int btnX = area.left + 5;
	
	windowButtons[BTN_MSG_CLIMB].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_DESCEND].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_MAINTAIN].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_PROCEED].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_CONTACT].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_SQUAWK].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_CLEARED].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_CROSS].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	windowButtons[BTN_MSG_FREETEXT].State = canCompose ? CInputState::INACTIVE : CInputState::DISABLED;
	
	// First row - CLIMB, DESCEND, MAINTAIN
	CCommonRenders::RenderButton(dc, screen, { btnX, btnY }, btnW, btnH, &windowButtons[BTN_MSG_CLIMB]);
	CCommonRenders::RenderButton(dc, screen, { btnX + btnW + spacing, btnY }, btnW, btnH, &windowButtons[BTN_MSG_DESCEND]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 2*(btnW + spacing), btnY }, btnW, btnH, &windowButtons[BTN_MSG_MAINTAIN]);
	
	// Second row - DIRECT, CONTACT, SQUAWK
	btnY += btnH + spacing;
	CCommonRenders::RenderButton(dc, screen, { btnX, btnY }, btnW, btnH, &windowButtons[BTN_MSG_PROCEED]);
	CCommonRenders::RenderButton(dc, screen, { btnX + btnW + spacing, btnY }, btnW, btnH, &windowButtons[BTN_MSG_CONTACT]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 2*(btnW + spacing), btnY }, btnW, btnH, &windowButtons[BTN_MSG_SQUAWK]);
	
	// Third row - CLEARED, CROSS, FREETEXT
	btnY += btnH + spacing;
	CCommonRenders::RenderButton(dc, screen, { btnX, btnY }, btnW, btnH, &windowButtons[BTN_MSG_CLEARED]);
	CCommonRenders::RenderButton(dc, screen, { btnX + btnW + spacing, btnY }, btnW, btnH, &windowButtons[BTN_MSG_CROSS]);
	CCommonRenders::RenderButton(dc, screen, { btnX + 2*(btnW + spacing), btnY }, btnW, btnH, &windowButtons[BTN_MSG_FREETEXT]);
	
	// Compose text input
	btnY += btnH + spacing + 5;
	textInputs[TXT_COMPOSE].State = canCompose ? CInputState::ACTIVE : CInputState::DISABLED;
	CCommonRenders::RenderTextInput(dc, screen, { area.left + 5, btnY }, area.Width() - 75, 25, &textInputs[TXT_COMPOSE]);
	
	// Send button
	windowButtons[BTN_SEND].State = (canCompose && !textInputs[TXT_COMPOSE].Content.empty()) ? CInputState::INACTIVE : CInputState::DISABLED;
	CCommonRenders::RenderButton(dc, screen, { area.right - 65, btnY }, 60, 25, &windowButtons[BTN_SEND]);
}

void CCPDLCWindow::ButtonUp(int id, CRadarScreen* screen) {
	switch (id) {
		case BTN_CLOSE:
			IsClosed = true;
			break;
			
		case BTN_CONNECT:
			Connect(screen);
			break;
			
		case BTN_DISCONNECT:
			Disconnect();
			break;
			
		case BTN_POLL:
			if (hoppieClient != nullptr && hoppieClient->IsConnected()) {
				hoppieClient->Poll();
				if (screen != nullptr) {
					screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Info", 
						"Polling Hoppie for messages...", true, true, false, false, false);
				}
			}
			break;
			
		case BTN_ACCEPT_LOGON:
			if (!selectedAircraft.empty()) {
				AcceptLogon(selectedAircraft);
			}
			break;
			
		case BTN_REJECT_LOGON:
			if (!selectedAircraft.empty()) {
				RejectLogon(selectedAircraft);
			}
			break;
			
		case BTN_DISCONNECT_AC:
			if (!selectedAircraft.empty() && hoppieClient != nullptr) {
				hoppieClient->DisconnectAircraft(selectedAircraft);
				selectedAircraft = "";
			}
			break;
			
		case BTN_WILCO:
			if (selectedMessageId > 0) {
				RespondToMessage(selectedMessageId, "WILCO");
			}
			break;
			
		case BTN_ROGER:
			if (selectedMessageId > 0) {
				RespondToMessage(selectedMessageId, "ROGER");
			}
			break;
			
		case BTN_STANDBY:
			if (selectedMessageId > 0) {
				RespondToMessage(selectedMessageId, "STANDBY");
			}
			break;
			
		case BTN_UNABLE:
			if (selectedMessageId > 0) {
				RespondToMessage(selectedMessageId, "UNABLE");
			}
			break;
			
		case BTN_CLEAR_MSG:
			if (selectedMessageId > 0 && hoppieClient != nullptr) {
				hoppieClient->CloseMessage(selectedMessageId);
				selectedMessageId = -1;
			}
			break;
			
		case BTN_SEND:
			if (!selectedAircraft.empty() && !textInputs[TXT_COMPOSE].Content.empty()) {
				SendMessage(selectedAircraft, textInputs[TXT_COMPOSE].Content);
				textInputs[TXT_COMPOSE].Content = "";
			}
			break;
			
		case BTN_MSG_SCROLL_UP:
			if (messagesScrollPos > 0) messagesScrollPos--;
			break;
			
		case BTN_MSG_SCROLL_DN:
			messagesScrollPos++;
			break;
			
		case BTN_AC_SCROLL_UP:
			if (aircraftScrollPos > 0) aircraftScrollPos--;
			break;
			
		case BTN_AC_SCROLL_DN:
			aircraftScrollPos++;
			break;
			
		// Message template buttons
		case BTN_MSG_CLIMB:
			textInputs[TXT_COMPOSE].Content = "CLIMB TO FL";
			break;
		case BTN_MSG_DESCEND:
			textInputs[TXT_COMPOSE].Content = "DESCEND TO FL";
			break;
		case BTN_MSG_MAINTAIN:
			textInputs[TXT_COMPOSE].Content = "MAINTAIN FL";
			break;
		case BTN_MSG_PROCEED:
			textInputs[TXT_COMPOSE].Content = "PROCEED DIRECT TO ";
			break;
		case BTN_MSG_CONTACT:
			textInputs[TXT_COMPOSE].Content = "CONTACT ";
			break;
		case BTN_MSG_SQUAWK:
			textInputs[TXT_COMPOSE].Content = "SQUAWK ";
			break;
		case BTN_MSG_CLEARED:
			textInputs[TXT_COMPOSE].Content = "CLRD TO  VIA  MAINTAIN FL MACH .";
			break;
		case BTN_MSG_CROSS:
			textInputs[TXT_COMPOSE].Content = "CROSS  AT ";
			break;
		case BTN_MSG_FREETEXT:
			textInputs[TXT_COMPOSE].Content = "";
			break;
	}

	// Release the pressed state so buttons behave like other windows
	ButtonUnpress(id);
}

void CCPDLCWindow::Connect(CRadarScreen* screen) {
	if (hoppieClient == nullptr) {
		screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Error", 
			"CPDLC client not initialized", true, true, false, true, false);
		return;
	}
	
	// Get station from text input
	CurrentStation = textInputs[TXT_STATION].Content;
	
	// Get logon code from text input
	string logonCode = textInputs[TXT_LOGON_CODE].Content;
	
	// Validate station
	if (CurrentStation.empty()) {
		screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Error", 
			"Please enter a station callsign (e.g. NATX, EGGX, CZQX)", true, true, false, true, false);
		return;
	}
	
	// Debug: Show what we're trying to connect with
	string debugMsg = "Attempting connection - Station: " + CurrentStation + ", Code length: " + to_string(logonCode.length());
	CLogger::Log(CLogType::NORM, debugMsg, "CCPDLCWindow::Connect");
	
	if (logonCode.empty()) {
		screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Error", 
			"Please enter your Hoppie logon code", true, true, false, true, false);
		return;
	}
	
	// Configure and connect
	hoppieClient->SetCallsign(CurrentStation);
	hoppieClient->SetLogonCode(logonCode);
	
	// Set sound path - CPDLC.wav should be in the plugin directory
	string soundPath = CUtils::DllPath + "CPDLC.wav";
	hoppieClient->SetSoundPath(soundPath);
	
	// Log the attempt
	screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Info", 
		("Connecting to Hoppie as " + CurrentStation + "...").c_str(), 
		true, true, false, false, false);
	
	if (hoppieClient->Connect()) {
		IsConnected = true;
		windowButtons[BTN_CONNECT].State = CInputState::DISABLED;
		windowButtons[BTN_DISCONNECT].State = CInputState::INACTIVE;
		
		screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Connected", 
			("Connected to Hoppie ACARS as " + CurrentStation).c_str(), 
			true, true, false, false, false);
		
		CLogger::Log(CLogType::NORM, "CPDLC connected as " + CurrentStation, "CCPDLCWindow::Connect");
	}
	else {
		IsConnected = false;
		screen->GetPlugIn()->DisplayUserMessage("CPDLC", "Error", 
			"Failed to connect to Hoppie ACARS. Check your logon code and internet connection.", 
			true, true, false, true, false);
		CLogger::Log(CLogType::ERR, "CPDLC connection failed", "CCPDLCWindow::Connect");
	}
}

void CCPDLCWindow::Disconnect() {
	if (hoppieClient != nullptr) {
		hoppieClient->Disconnect();
		hoppieClient->ClearAll();
	}
	
	IsConnected = false;
	CurrentStation = "";
	selectedAircraft = "";
	selectedMessageId = -1;
	
	windowButtons[BTN_CONNECT].State = CInputState::INACTIVE;
	windowButtons[BTN_DISCONNECT].State = CInputState::DISABLED;
	
	CLogger::Log(CLogType::NORM, "CPDLC disconnected", "CCPDLCWindow::Disconnect");
}

void CCPDLCWindow::SendMessage(const string& callsign, const string& message) {
	if (hoppieClient != nullptr && hoppieClient->IsConnected()) {
		if (hoppieClient->SendCpdlc(callsign, message)) {
			CLogger::Log(CLogType::NORM, "Sent CPDLC to " + callsign + ": " + message, "CCPDLCWindow::SendMessage");
		}
	}
}

void CCPDLCWindow::AcceptLogon(const string& callsign) {
	if (hoppieClient != nullptr && hoppieClient->IsConnected()) {
		hoppieClient->AcceptLogon(callsign);
	}
}

void CCPDLCWindow::RejectLogon(const string& callsign) {
	if (hoppieClient != nullptr && hoppieClient->IsConnected()) {
		hoppieClient->RejectLogon(callsign);
		selectedAircraft = "";
	}
}

void CCPDLCWindow::RespondToMessage(int messageId, const string& response) {
	if (hoppieClient == nullptr || !hoppieClient->IsConnected()) return;
	
	// Find the message
	for (auto& msg : hoppieClient->GetPendingMessages()) {
		if (msg.Id == messageId && msg.Direction == CpdlcDirection::UPLINK) {
			hoppieClient->SendCpdlc(msg.From, response, messageId);
			hoppieClient->AcknowledgeMessage(messageId);
			break;
		}
	}
}

void CCPDLCWindow::SelectAircraft(const string& callsign) {
	selectedAircraft = callsign;
}

void CCPDLCWindow::SelectMessage(int messageId) {
	selectedMessageId = messageId;
	
	// Also select the aircraft associated with this message
	if (hoppieClient != nullptr) {
		for (auto& msg : hoppieClient->GetPendingMessages()) {
			if (msg.Id == messageId) {
				selectedAircraft = (msg.Direction == CpdlcDirection::UPLINK) ? msg.From : msg.To;
				break;
			}
		}
	}
}

void CCPDLCWindow::ButtonDown(int id) {
	if (windowButtons.find(id) != windowButtons.end()) {
		if (windowButtons[id].State != CInputState::DISABLED) {
			windowButtons[id].State = CInputState::ACTIVE;
		}
	}
}

void CCPDLCWindow::ButtonPress(int id) {
	// Handle button press
}

void CCPDLCWindow::ButtonUnpress(int id) {
	if (windowButtons.find(id) != windowButtons.end()) {
		if (windowButtons[id].State == CInputState::ACTIVE) {
			windowButtons[id].State = CInputState::INACTIVE;
		}
	}
}

void CCPDLCWindow::SetButtonState(int id, CInputState state) {
	if (windowButtons.find(id) != windowButtons.end()) {
		windowButtons[id].State = state;
	}
}

void CCPDLCWindow::SetTextValue(CRadarScreen* screen, int id, string content) {
	if (textInputs.find(id) != textInputs.end()) {
		textInputs[id].Content = content;
	}
}

CInputState CCPDLCWindow::GetInputState(int id) {
	if (windowButtons.find(id) != windowButtons.end()) {
		return windowButtons[id].State;
	}
	if (textInputs.find(id) != textInputs.end()) {
		return textInputs[id].State;
	}
	return CInputState::DISABLED;
}

bool CCPDLCWindow::IsButtonPressed(int id) {
	if (windowButtons.find(id) != windowButtons.end()) {
		return windowButtons[id].State == CInputState::ACTIVE;
	}
	return false;
}

void CCPDLCWindow::CheckForRelease(CRadarScreen* screen) {
	vector<string> toRemove;
	for (auto const& item : PendingRelease) {
		string callsign = item.first;
		// Check if the aircraft is still being tracked by us
		CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (target.IsValid()) {
			CFlightPlan fp = target.GetCorrelatedFlightPlan();
			if (fp.IsValid()) {
				// If tracking controller is empty or not us, it's released
				string trackingController = fp.GetTrackingControllerId();
				string myId = screen->GetPlugIn()->ControllerMyself().GetCallsign();
				
				if (trackingController != myId) {
					toRemove.push_back(callsign);
				}
			}
		} else {
			// Target invalid (logged off?), remove
			toRemove.push_back(callsign);
		}
	}
	
	for (const string& cs : toRemove) {
		PendingRelease.erase(cs);
	}
}

void CCPDLCWindow::OnOverDropDownItem(int id) {
	// Handle dropdown hover if needed
}

void CCPDLCWindow::Cleanup() {
	if (hoppieClient != nullptr) {
		delete hoppieClient;
		hoppieClient = nullptr;
	}
}
