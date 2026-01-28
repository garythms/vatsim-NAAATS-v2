#include "pch.h"
#include "SetupWindow.h"
#include "Constants.h"
#include "Styles.h"
#include "CommonRenders.h"
#include "Utils.h"
#include <fstream>

using namespace Colours;

CSetupWindow::CSetupWindow(POINT tl) : CBaseWindow(tl) {
	MakeWindowItems();
	IsClosed = true;
}

void CSetupWindow::MakeWindowItems() {
	CLogger::Log(CLogType::NORM, "MakeWindowItems running.", "CSetupWindow::MakeWindowItems");
	// Buttons
	windowButtons[BTN_CLOSE] = CWinButton(BTN_CLOSE, WIN_SETUP, "Close", CInputState::INACTIVE);
	windowButtons[BTN_SAVE] = CWinButton(BTN_SAVE, WIN_SETUP, "Save", CInputState::INACTIVE);

	// Checkboxes
	checkBoxes[CHK_SINGLE] = CCheckBox(CHK_SINGLE, WIN_SETUP, "Single Screen Mode", CUtils::ScreenCount == "1 Screen", CInputState::ACTIVE);
	checkBoxes[CHK_SCROLL] = CCheckBox(CHK_SCROLL, WIN_SETUP, "Enable Menu Scroll", CUtils::MenuScroll, CInputState::ACTIVE);

	// Text inputs
	textInputs[TXT_HOPPIE] = CTextInput(TXT_HOPPIE, WIN_SETUP, "Hoppie Code", CUtils::HoppieCode, 150, CInputState::ACTIVE);
}


void CSetupWindow::RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen) {
	if (IsClosed) return;

	// Safety check: ensure items are created
	if (windowButtons.empty()) {
		CLogger::Log(CLogType::NORM, "MakeWindowItems called from RenderWindow (safety check).", "CSetupWindow::RenderWindow");
		MakeWindowItems();
	}

	// Window rect
	CRect windowRect(topLeft.x, topLeft.y, topLeft.x + 360, topLeft.y + 220);
	CBrush bg(ScreenBlue.ToCOLORREF());
	dc->FillRect(windowRect, &bg);
	dc->Draw3dRect(windowRect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	InflateRect(windowRect, -1, -1);
	dc->Draw3dRect(windowRect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

	// Title
	CRect titleRect(windowRect.left + 2, windowRect.top + 2, windowRect.right - 2, windowRect.top + 22);
	dc->FillSolidRect(titleRect, ScreenBlue.ToCOLORREF());
	FontSelector::SelectNormalFont(15, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_CENTER);
	dc->TextOutA(titleRect.left + titleRect.Width() / 2, titleRect.top + 3, "Setup / Profile");

	// Allow dragging
	screen->AddScreenObject(WINDOW, "WIN_SETUP", windowRect, true, "");
	screen->AddScreenObject(WINDOW, "SETUP", titleRect, true, "");

	// Controls
	int x = windowRect.left + 10;
	int y = windowRect.top + 35;

	FontSelector::SelectNormalFont(14, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_LEFT);

	CRect r1 = CCommonRenders::RenderCheckBox(dc, g, screen, { x, y }, 22, &checkBoxes[CHK_SINGLE]);
	dc->TextOutA(r1.right + 5, r1.top + 4, checkBoxes[CHK_SINGLE].Label.c_str());
	y += 28;
	CRect r2 = CCommonRenders::RenderCheckBox(dc, g, screen, { x, y }, 22, &checkBoxes[CHK_SCROLL]);
	dc->TextOutA(r2.right + 5, r2.top + 4, checkBoxes[CHK_SCROLL].Label.c_str());

	y += 40;
	dc->TextOutA(x, y - 2, "Hoppie Code:");
	CCommonRenders::RenderTextInput(dc, screen, { x + 100, y - 6 }, 150, 25, &textInputs[TXT_HOPPIE]);

	// Buttons
	CCommonRenders::RenderButton(dc, screen, { windowRect.right - 65, windowRect.top + 25 }, 60, 25, &windowButtons[BTN_CLOSE]);
	CCommonRenders::RenderButton(dc, screen, { windowRect.right - 75, windowRect.bottom - 35 }, 70, 25, &windowButtons[BTN_SAVE]);
}

void CSetupWindow::ButtonDown(int id) {
	if (windowButtons.find(id) != windowButtons.end()) {
		if (windowButtons[id].State != CInputState::DISABLED) {
			windowButtons[id].State = CInputState::ACTIVE;
		}
	}
}

void CSetupWindow::ButtonUp(int id, CRadarScreen* screen) {
	if (id == BTN_CLOSE) {
		IsClosed = true;
	}
	if (id == BTN_SAVE) {
		// Update CUtils
		CUtils::ScreenCount = checkBoxes[CHK_SINGLE].IsChecked ? "1 Screen" : "2 Screens";
		CUtils::MenuScroll = checkBoxes[CHK_SCROLL].IsChecked;
		CUtils::HoppieCode = textInputs[TXT_HOPPIE].Content;

		// Save to ASR
		CUtils::SavePluginData(screen);

		// Feedback
		if (screen != nullptr) {
			screen->GetPlugIn()->DisplayUserMessage("Setup", "Saved", "Settings saved.", true, true, false, false, false);
		}
	}

	if (checkBoxes.find(id) != checkBoxes.end()) {
		checkBoxes[id].IsChecked = !checkBoxes[id].IsChecked;
	}

	if (windowButtons.find(id) != windowButtons.end()) {
		windowButtons[id].State = CInputState::INACTIVE;
	}
}

void CSetupWindow::ButtonPress(int id) {}
void CSetupWindow::ButtonUnpress(int id) {}

void CSetupWindow::SetButtonState(int id, CInputState state) {
	if (windowButtons.find(id) != windowButtons.end()) {
		windowButtons[id].State = state;
	}
}

void CSetupWindow::SetTextValue(CRadarScreen* screen, int id, string content) {
	if (textInputs.find(id) != textInputs.end()) {
		textInputs[id].Content = content;
	}
}
