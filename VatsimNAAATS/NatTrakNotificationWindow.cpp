#include "pch.h"
#include "NatTrakNotificationWindow.h"
#include "Constants.h"
#include "Styles.h"
#include "Utils.h"
#include "CommonRenders.h"

using namespace Colours;


CNatTrakNotificationWindow::CNatTrakNotificationWindow(POINT topLeft) : CBaseWindow(topLeft) {
	MakeWindowItems();
	IsClosed = false; // Start open but hidden if no notifications
	flashTimer = clock();
	flashState = false;
}

void CNatTrakNotificationWindow::MakeWindowItems() {
	windowButtons[BTN_ACK] = CWinButton(BTN_ACK, WIN_NATTRAK_NOTIF, "ACK", CInputState::INACTIVE);
	windowButtons[BTN_CLOSE] = CWinButton(BTN_CLOSE, WIN_NATTRAK_NOTIF, "X", CInputState::INACTIVE);
}

void CNatTrakNotificationWindow::RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen) {
	vector<string> notifications = CDataHandler::GetNatTrakNotifications();
	if (notifications.empty()) return;

	// Save device context
	int iDC = dc->SaveDC();

	// Flash logic
	if ((double)(clock() - flashTimer) / CLOCKS_PER_SEC >= 0.5) {
		flashState = !flashState;
		flashTimer = clock();
	}

	// Brushes
	CBrush bgBrush(flashState ? RGB(150, 0, 0) : ScreenBlue.ToCOLORREF());
	CBrush borderBrush(WindowBorder.ToCOLORREF());

	// Window rectangle
	CRect windowRect(topLeft.x, topLeft.y, topLeft.x + WINSZ_NATTRAK_NOTIF_WIDTH, topLeft.y + WINSZ_NATTRAK_NOTIF_HEIGHT);
	dc->FillRect(windowRect, &bgBrush);
	dc->Draw3dRect(windowRect, WindowBorder.ToCOLORREF(), WindowBorder.ToCOLORREF());

	// Title bar
	CRect titleRect(windowRect.left, windowRect.top, windowRect.right, windowRect.top + WINSZ_TITLEBAR_HEIGHT);
	dc->FillRect(titleRect, &borderBrush);
	
	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(Black.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);
	dc->TextOut(titleRect.left + 5, titleRect.top + 2, "NATTRAK REQUESTS");

	// Close button
	CCommonRenders::RenderButton(dc, screen, { titleRect.right - 20, titleRect.top + 2 }, 18, 16, &windowButtons.at(BTN_CLOSE));

	// Render notifications
	FontSelector::SelectNormalFont(16, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	int yOffset = titleRect.bottom + 10;
	
	// Only show first 5
	int count = 0;
	for (const auto& callsign : notifications) {
		if (count >= 4) {
			dc->TextOut(windowRect.left + 10, yOffset, "... more");
			break;
		}
		string text = callsign + " RCL PENDING";
		dc->TextOut(windowRect.left + 10, yOffset, text.c_str());
		
		// Render ACK button for each? No, let's just have one global ACK for the top one
		if (count == 0) {
			CCommonRenders::RenderButton(dc, screen, { windowRect.right - 50, yOffset - 2 }, 40, 20, &windowButtons.at(BTN_ACK));
		}
		
		yOffset += 22;
		count++;
	}

	// Add screen objects
	screen->AddScreenObject(WINDOW, "WIN_NATTRAK_NOTIF", windowRect, true, "");
	screen->AddScreenObject(WINDOW, "NATTRAK_NOTIF", titleRect, true, "");

	dc->RestoreDC(iDC);
}

void CNatTrakNotificationWindow::ButtonDown(int id) {}
void CNatTrakNotificationWindow::ButtonUp(int id, CRadarScreen* screen) {
	if (id == BTN_ACK) {
		vector<string> notifications = CDataHandler::GetNatTrakNotifications();
		if (!notifications.empty()) {
			CDataHandler::AcknowledgeNatTrakNotification(notifications[0]);
		}
	}
	else if (id == BTN_CLOSE) {
		// Just clear all for now if they hit X
		CDataHandler::SetNewNatTrakNotification(false);
		// Or maybe hide it? User said "plays a sound when it identifies a new request"
		// Let's clear notifications
		for (auto n : CDataHandler::GetNatTrakNotifications()) {
			CDataHandler::AcknowledgeNatTrakNotification(n);
		}
	}
}
void CNatTrakNotificationWindow::ButtonPress(int id) {}
void CNatTrakNotificationWindow::ButtonUnpress(int id) {}
void CNatTrakNotificationWindow::SetButtonState(int id, CInputState state) {
	if (windowButtons.find(id) != windowButtons.end()) {
		windowButtons.at(id).State = state;
	}
}
