#include "pch.h"
#include "MenuBar.h"
#include "Constants.h"
#include "MenuBar.h"
#include "Styles.h"
#include "Utils.h"
#include "CommonRenders.h"
#include "Overlays.h"
#include "DataHandler.h"
#include <string>
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <fstream>

using namespace Colours;

// Static CPDLC alert state
bool CMenuBar::CpdlcAlert = false;
std::time_t CMenuBar::CpdlcAlertTime = 0;

CMenuBar::CMenuBar() {
	// Button defaults
	buttons[BTN_SETUP] = CWinButton(BTN_SETUP, MENBAR, "Setup", CInputState::INACTIVE, 46);
	buttons[BTN_NOTEPAD] = CWinButton(BTN_NOTEPAD, MENBAR, "NotePad", CInputState::DISABLED, 78);
	buttons[BTN_ADSC] = CWinButton(BTN_ADSC, MENBAR, "Flight Data", CInputState::INACTIVE, 80);
	buttons[BTN_TCKINFO] = CWinButton(BTN_TCKINFO, MENBAR, "Track Info", CInputState::INACTIVE, 78);
	buttons[BTN_MISC] = CWinButton(BTN_MISC, MENBAR, "Misc", CInputState::DISABLED, 41);
	buttons[BTN_MESSAGE] = CWinButton(BTN_MESSAGE, MENBAR, "Message", CInputState::DISABLED, 75);
	buttons[BTN_TAGS] = CWinButton(BTN_TAGS, MENBAR, "Tags", CInputState::ACTIVE, 46);
	buttons[BTN_FLIGHTPLAN] = CWinButton(BTN_FLIGHTPLAN, MENBAR, "Flight Plan", CInputState::DISABLED, 78);
	buttons[BTN_DETAILED] = CWinButton(BTN_DETAILED, MENBAR, "ASEL Dtld", CInputState::INACTIVE, 80);
	buttons[BTN_AREASEL] = CWinButton(BTN_AREASEL, MENBAR, "Area Sel", CInputState::DISABLED, 83);
	// BTN_TCKCTRL removed - not needed
	buttons[BTN_OVERLAYS] = CWinButton(BTN_OVERLAYS, MENBAR, "Overlays", CInputState::INACTIVE, 73);
	buttons[BTN_TYPESEL] = CWinButton(BTN_TYPESEL, MENBAR, "Select", CInputState::DISABLED, 68);
	buttons[BTN_ALTFILT] = CWinButton(BTN_ALTFILT, MENBAR, "Alt Filter", CInputState::INACTIVE, 86);
	buttons[BTN_HALO] = CWinButton(BTN_HALO, MENBAR, "Halo 5", CInputState::INACTIVE, 68, 0);
	buttons[BTN_RBL] = CWinButton(BTN_RBL, MENBAR, "RBL", CInputState::INACTIVE, 48);
	buttons[BTN_RINGS] = CWinButton(BTN_RINGS, MENBAR, "Rings 1", CInputState::DISABLED, 73, 0);
	buttons[BTN_QDM] = CWinButton(BTN_QDM, MENBAR, "QDM", CInputState::INACTIVE, 43);
	buttons[BTN_PTL] = CWinButton(BTN_PTL, MENBAR, "PTL 5", CInputState::INACTIVE, 68, 0);
	buttons[BTN_PIV] = CWinButton(BTN_PIV, MENBAR, "PIV", CInputState::INACTIVE, 48);
	buttons[BTN_GRID] = CWinButton(BTN_GRID, MENBAR, "Grid", CInputState::DISABLED, 48);
	buttons[BTN_SEP] = CWinButton(BTN_SEP, MENBAR, "Sep", CInputState::INACTIVE, 43);
	buttons[BTN_QCKLOOK] = CWinButton(BTN_QCKLOOK, MENBAR, "Qck Look", CInputState::DISABLED, 70);
	buttons[BTN_PSSR] = CWinButton(BTN_PSSR, MENBAR, "PSR_SYMBOL", CInputState::DISABLED, 40);
	buttons[BTN_EXT] = CWinButton(BTN_EXT, MENBAR, "Ext", CInputState::INACTIVE, 35);
	buttons[BTN_AUTOTAG] = CWinButton(BTN_AUTOTAG, MENBAR, "Auto Tag", CInputState::INACTIVE, 65);
	buttons[BTN_ALL] = CWinButton(BTN_ALL, MENBAR, "ALL", CInputState::DISABLED, 35);
	buttons[BTN_RTEDEL] = CWinButton(BTN_RTEDEL, MENBAR, "Rte Del", CInputState::INACTIVE, 60);
	buttons[BTN_CPDLC] = CWinButton(BTN_CPDLC, MENBAR, "CPDLC", CInputState::INACTIVE, 55);
	buttons[BTN_SELCAL] = CWinButton(BTN_SELCAL, MENBAR, "SELCAL", CInputState::INACTIVE, 55);
	buttons[BTN_FDD] = CWinButton(BTN_FDD, MENBAR, "FDD", CInputState::INACTIVE, 45);
	buttons[BTN_VACS] = CWinButton(BTN_VACS, MENBAR, "VACS", CInputState::INACTIVE, 45);
	buttons[BTN_SCROLL_LEFT] = CWinButton(BTN_SCROLL_LEFT, MENBAR, "<", CInputState::INACTIVE, 25);
	buttons[BTN_SCROLL_RIGHT] = CWinButton(BTN_SCROLL_RIGHT, MENBAR, ">", CInputState::INACTIVE, 25);

	// Text inputs
	textInputs[TXT_SEARCH] = CTextInput(TXT_SEARCH, MENBAR, "Search A/C: ", "", 100, CInputState::ACTIVE);

	/// Dropdown defaults
	map<string, bool> map;
	map.insert(make_pair("CZQX", false));
	map.insert(make_pair("EGGX", false));
	map.insert(make_pair("BDBX", false));
	dropDowns[DRP_AREASEL] = CDropDown(DRP_AREASEL, MENBAR, "CZQX", &map, CInputState::INACTIVE, 83);
	map.clear();
	dropDowns[DRP_TCKCTRL] = CDropDown(DRP_TCKCTRL, MENBAR, "None", &map, CInputState::INACTIVE, 88);
	map.clear();
	map.insert(make_pair("ALL_TCKS", false));
	map.insert(make_pair("TCKS_EAST", false));
	map.insert(make_pair("TCKS_WEST", false));
	map.insert(make_pair("TCKS_SEL", false));
	dropDowns[DRP_OVERLAYS] = CDropDown(DRP_OVERLAYS, MENBAR, "ALL_TCKS", &map, CInputState::INACTIVE, 113);
	map.clear();
	map.insert(make_pair("Delivery", false));
	map.insert(make_pair("OCA Enroute", false));
	map.insert(make_pair("Multi-role", false));
	dropDowns[DRP_TYPESEL] = CDropDown(DRP_TYPESEL, MENBAR, "Multi-role", &map, CInputState::INACTIVE, 143);

	// Clogger
	CLogger::Log(CLogType::NORM, "Finished instantiation.", "MENUBAR");
}

void CMenuBar::RenderBar(CDC* dc, Graphics* g, CRadarScreen* screen, string asel) {
	// Save context for later
	int sDC = dc->SaveDC();

	// Brush to draw the bar
	CBrush brush(ScreenBlue.ToCOLORREF());

	// Get screen dimensions
	RECT radarArea = screen->GetRadarArea();
	LONG screenWidth = radarArea.right - radarArea.left;
	const int top = radarArea.top;

	// Row positions (aligned with date on row 1)
	const int kRow1Y = 10;   // Row 1 - aligned with date
	const int kRow2Y = 40;   // Row 2
	const int kDropY = 4;    // Dropdown row
	const int kDropH = 22;   // Dropdown height

	// Disable scroll if setting is off
	if (!CUtils::MenuScroll) {
		m_scrollOffset = 0;
	}

	// Compute max scroll based on total content width
	int totalPanelsWidth = RECT1_WIDTH + RECT2_WIDTH + RECT3_WIDTH + RECT4_WIDTH + RECT5_WIDTH + RECT7_WIDTH;
	m_maxScroll = max(0, totalPanelsWidth - (int)(radarArea.right - radarArea.left - 300));
	if (m_scrollOffset < 0) m_scrollOffset = 0;
	if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;

	// Panel boundaries (apply horizontal scroll offset)
	const int P1 = radarArea.left - m_scrollOffset;
	const int P2 = P1 + RECT1_WIDTH;
	const int P3 = P2 + RECT2_WIDTH;
	const int P4 = P3 + RECT3_WIDTH;
	const int P5 = P4 + RECT4_WIDTH;
	const int P6 = P5 + RECT5_WIDTH;
	const int P7 = P6 + RECT7_WIDTH;  // RECT6 is now 0
	const int P8 = P7;  // Panel 8 starts where 7 ends (spans to edge)

	// Create the base rectangle and the 3d bevel
	CRect baseMenuRectColour(radarArea.left, top, radarArea.left + screenWidth, top + MENBAR_HEIGHT);
	dc->FillRect(baseMenuRectColour, &brush);
	CRect baseMenuRect(radarArea.left, top, radarArea.left + screenWidth, top + MENBAR_HEIGHT);
	dc->Draw3dRect(baseMenuRect, ScreenBlue.ToCOLORREF(), BevelLight.ToCOLORREF());

	// Draw panels (only non-zero width panels)
	int panelWidths[] = { RECT1_WIDTH, RECT2_WIDTH, RECT3_WIDTH, RECT4_WIDTH, RECT5_WIDTH, RECT7_WIDTH };
	int menuOffsetX = radarArea.left - m_scrollOffset;
	for (int i = 0; i < 6; i++) {
		if (panelWidths[i] > 0) {
			CRect rect1(menuOffsetX, top + 1, menuOffsetX + panelWidths[i], top + MENBAR_HEIGHT - 2);
			dc->Draw3dRect(rect1, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
			InflateRect(rect1, -1, -1);
			dc->Draw3dRect(rect1, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
			menuOffsetX += panelWidths[i] + 1;
		}
	}
	// Final panel spans to screen edge
	CRect rectFinal(menuOffsetX, top + 1, radarArea.left + screenWidth - 2, top + MENBAR_HEIGHT - 2);
	dc->Draw3dRect(rectFinal, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	InflateRect(rectFinal, -1, -1);
	dc->Draw3dRect(rectFinal, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	const int P8_LEFT = menuOffsetX;

	/// ITEM RENDERING
	// Render zulu time (centered below menu bar)
	FontSelector::SelectNormalFont(30, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_CENTER);
	dc->TextOutA(radarArea.left + (screenWidth / 2), top + MENBAR_HEIGHT + 5, CUtils::ParseZuluTime(true).c_str());

	// Font selection
	FontSelector::SelectNormalFont(MEN_FONT_SIZE, dc);
	dc->SetTextColor(TextWhite.ToCOLORREF());
	dc->SetTextAlign(TA_CENTER);

	// Calculate and render date (Panel 1, Row 1)
	time_t now = time(0);
	tm* date = gmtime(&now);
	string strDate;
	strDate += to_string(date->tm_mon + 1);
	strDate += "-";
	strDate += to_string(date->tm_mday);
	strDate += "-";
	strDate += to_string(1900 + date->tm_year);
	dc->TextOutA(P1 + 60, top + kRow1Y + 7, strDate.c_str());

	// ===== PANEL 1: Buttons =====
	// Row 1: Setup, NotePad, Flight Data, Track Info
	int offsetX = P1 + 120;
	int offsetY = top + kRow1Y;
	dc->SetTextAlign(TA_LEFT);
	
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_SETUP].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_SETUP]);
	offsetX += buttons[BTN_SETUP].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_NOTEPAD].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_NOTEPAD]);
	offsetX += buttons[BTN_NOTEPAD].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_ADSC].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_ADSC]);
	offsetX += buttons[BTN_ADSC].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_TCKINFO].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_TCKINFO]);

	// Row 2: Misc, Message, Tags, Flight Plan, ASEL Dtld, Selected: xxx
	offsetX = P1 + 10;
	offsetY = top + kRow2Y;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_MISC].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_MISC]);
	offsetX += buttons[BTN_MISC].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_MESSAGE].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_MESSAGE]);
	offsetX += buttons[BTN_MESSAGE].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_TAGS].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_TAGS]);
	offsetX += buttons[BTN_TAGS].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_FLIGHTPLAN].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_FLIGHTPLAN]);
	offsetX += buttons[BTN_FLIGHTPLAN].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_DETAILED].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_DETAILED]);
	offsetX += buttons[BTN_DETAILED].Width + 4;
	string selText = "Selected: " + (asel == "" ? "None" : asel);
	dc->TextOutA(offsetX, offsetY + 7, selText.c_str());

	// ===== PANEL 2: Dropdowns and labels =====
	// Row 1: [CZQX] [None] [ALL_TCKS]
	offsetX = P2 + 10;
	offsetY = top + kDropY;
	dropDowns[DRP_AREASEL].LastRenderPos = { offsetX, offsetY };
	CCommonRenders::RenderDropDown(dc, g, screen, { offsetX, offsetY }, dropDowns[DRP_AREASEL].Width, kDropH, &dropDowns[DRP_AREASEL], false);
	offsetX += dropDowns[DRP_AREASEL].Width + 1;
	dropDowns[DRP_TCKCTRL].LastRenderPos = { offsetX, offsetY };
	CCommonRenders::RenderDropDown(dc, g, screen, { offsetX, offsetY }, dropDowns[DRP_TCKCTRL].Width, kDropH, &dropDowns[DRP_TCKCTRL], false);
	offsetX += dropDowns[DRP_TCKCTRL].Width + 1;
	dropDowns[DRP_OVERLAYS].LastRenderPos = { offsetX, offsetY };
	CCommonRenders::RenderDropDown(dc, g, screen, { offsetX, offsetY }, dropDowns[DRP_OVERLAYS].Width, kDropH, &dropDowns[DRP_OVERLAYS], false);

	// Row 2: Area Sel label, Overlays button (below ALL_TCKS)
	offsetX = P2 + 10;
	offsetY = top + kRow2Y;
	dc->SetTextAlign(TA_LEFT);
	dc->TextOutA(offsetX, offsetY + 7, "Area Sel");
	// Overlays button below ALL_TCKS dropdown
	offsetX = P2 + 10 + dropDowns[DRP_AREASEL].Width + 1 + dropDowns[DRP_TCKCTRL].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_OVERLAYS].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_OVERLAYS]);

	// ===== PANEL 3: OCA Enroute dropdown + Pos Type label =====
	// Row 1: [OCA Enroute]
	offsetX = P3 + 10;
	offsetY = top + kDropY;
	dropDowns[DRP_TYPESEL].LastRenderPos = { offsetX, offsetY };
	CCommonRenders::RenderDropDown(dc, g, screen, { offsetX, offsetY }, dropDowns[DRP_TYPESEL].Width, kDropH, &dropDowns[DRP_TYPESEL], false);
	// Row 2: Pos Type label (centered)
	dc->SetTextAlign(TA_CENTER);
	dc->TextOutA(P3 + (RECT3_WIDTH / 2), top + kRow2Y + 7, "Pos Type");

	// ===== PANEL 4: Alt Filter =====
	// Row 1: 200-700 box (centered)
	int altFiltWidth = 86;
	int altFiltLeft = P4 + (RECT4_WIDTH - altFiltWidth) / 2;
	CRect altFilt(altFiltLeft, top + kRow1Y, altFiltLeft + altFiltWidth, top + kRow1Y + MENBAR_BTN_HEIGHT);
	dc->FillSolidRect(altFilt, ButtonPressed.ToCOLORREF());
	dc->Draw3dRect(altFilt, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	InflateRect(altFilt, -1, -1);
	dc->Draw3dRect(altFilt, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
	// Text
	string lowAlt = CUtils::PadWithZeros(3, CUtils::AltFiltLow);
	string highAlt = CUtils::PadWithZeros(3, CUtils::AltFiltHigh);
	dc->SetTextAlign(TA_CENTER);
	dc->TextOutA(altFiltLeft + (altFiltWidth / 2), top + kRow1Y + 7, (lowAlt + "-" + highAlt).c_str());
	// Screen objects for clicking
	CSize rectSize = dc->GetTextExtent("000");
	CRect lowRect(altFilt.left + 14, altFilt.top + 6, altFilt.left + 16 + rectSize.cx, altFilt.top + 6 + rectSize.cy);
	screen->AddScreenObject(ALTFILT_TEXT, "ALTFILT_LOW", lowRect, false, "");
	CRect highRect(altFilt.right - 16 - rectSize.cx, altFilt.top + 6, altFilt.right - 14, altFilt.top + 6 + rectSize.cy);
	screen->AddScreenObject(ALTFILT_TEXT, "ALTFILT_HIGH", highRect, false, "");

	// ===== PANEL 5: Halo, RBL, QDM / PTL, PIV, Grid, Sep, Qck Look =====
	// Row 1: Halo 5, RBL, QDM
	offsetX = P5 + 10;
	offsetY = top + kRow1Y;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_HALO].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_HALO]);
	offsetX += buttons[BTN_HALO].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_RBL].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_RBL]);
	offsetX += buttons[BTN_RBL].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_QDM].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_QDM]);
	
	// Row 2: PTL 5, PIV, Grid, Sep, Qck Look
	offsetX = P5 + 10;
	offsetY = top + kRow2Y;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_PTL].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_PTL]);
	offsetX += buttons[BTN_PTL].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_PIV].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_PIV]);
	offsetX += buttons[BTN_PIV].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_GRID].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_GRID]);
	offsetX += buttons[BTN_GRID].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_SEP].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_SEP]);
	offsetX += buttons[BTN_SEP].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_QCKLOOK].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_QCKLOOK]);

	// ===== PANEL 6/7: Search A/C + Ext, Auto Tag / ALL, Rte Del =====
	// Row 1: Search A/C: [input] Ext Auto Tag
	offsetX = P6 + 10;
	offsetY = top + kRow1Y;
	dc->SetTextAlign(TA_LEFT);
	dc->TextOutA(offsetX, offsetY + 7, textInputs[TXT_SEARCH].Label.c_str());
	offsetX += dc->GetTextExtent(textInputs[TXT_SEARCH].Label.c_str()).cx + 2;
	CCommonRenders::RenderTextInput(dc, screen, { offsetX, offsetY }, textInputs[TXT_SEARCH].Width, MENBAR_BTN_HEIGHT, &textInputs[TXT_SEARCH]);
	offsetX += textInputs[TXT_SEARCH].Width + 5;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_EXT].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_EXT]);
	offsetX += buttons[BTN_EXT].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_AUTOTAG].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_AUTOTAG]);
	
	// Row 2: ALL, Rte Del
	offsetX = P6 + 10;
	offsetY = top + kRow2Y;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_ALL].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_ALL]);
	offsetX += buttons[BTN_ALL].Width + 1;
	CCommonRenders::RenderButton(dc, screen, { offsetX, offsetY }, buttons[BTN_RTEDEL].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_RTEDEL]);

	// ===== PANEL 8: CPDLC (tall), SELCAL (tall), FDD/VACS =====
	// CPDLC - spans both rows
	offsetX = P8_LEFT + 10;
	int tallBtnHeight = (kRow2Y - kRow1Y) + MENBAR_BTN_HEIGHT;  // Span both rows
	
	// Special rendering for CPDLC with flash capability
	if (CpdlcAlert) {
		time_t now = time(0);
		bool flashOn = ((now - CpdlcAlertTime) % 2) == 0;
		if (flashOn) {
			CRect btnRect(offsetX, top + kRow1Y, offsetX + buttons[BTN_CPDLC].Width, top + kRow1Y + tallBtnHeight);
			dc->FillSolidRect(btnRect, RGB(255, 255, 0));
			dc->Draw3dRect(btnRect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
			InflateRect(btnRect, -1, -1);
			dc->Draw3dRect(btnRect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());
			FontSelector::SelectNormalFont(MEN_FONT_SIZE, dc);
			dc->SetTextColor(RGB(0, 0, 0));
			dc->SetTextAlign(TA_CENTER);
			dc->TextOutA(offsetX + buttons[BTN_CPDLC].Width / 2, top + kRow1Y + tallBtnHeight / 2 - 6, buttons[BTN_CPDLC].Label.c_str());
			screen->AddScreenObject(buttons[BTN_CPDLC].Type, to_string(buttons[BTN_CPDLC].Id).c_str(), btnRect, false, "");
		} else {
			CCommonRenders::RenderButton(dc, screen, { offsetX, top + kRow1Y }, buttons[BTN_CPDLC].Width, tallBtnHeight, &buttons[BTN_CPDLC]);
		}
	} else {
		CCommonRenders::RenderButton(dc, screen, { offsetX, top + kRow1Y }, buttons[BTN_CPDLC].Width, tallBtnHeight, &buttons[BTN_CPDLC]);
	}
	offsetX += buttons[BTN_CPDLC].Width + 1;
	
	// SELCAL - spans both rows
	CCommonRenders::RenderButton(dc, screen, { offsetX, top + kRow1Y }, buttons[BTN_SELCAL].Width, tallBtnHeight, &buttons[BTN_SELCAL]);
	offsetX += buttons[BTN_SELCAL].Width + 5;
	
	// FDD - Row 1
	CCommonRenders::RenderButton(dc, screen, { offsetX, top + kRow1Y }, buttons[BTN_FDD].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_FDD]);
	// VACS - Row 2
	CCommonRenders::RenderButton(dc, screen, { offsetX, top + kRow2Y }, buttons[BTN_VACS].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_VACS]);

	// Scroll arrows fixed on far right
	if (CUtils::MenuScroll && m_maxScroll > 0) {
		int rightX = radarArea.left + screenWidth - 60;
		CCommonRenders::RenderButton(dc, screen, { rightX, top + kRow1Y }, buttons[BTN_SCROLL_LEFT].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_SCROLL_LEFT]);
		CCommonRenders::RenderButton(dc, screen, { rightX + buttons[BTN_SCROLL_LEFT].Width + 5, top + kRow1Y }, buttons[BTN_SCROLL_RIGHT].Width, MENBAR_BTN_HEIGHT, &buttons[BTN_SCROLL_RIGHT]);
	}

	// Reset text color
	dc->SetTextColor(TextWhite.ToCOLORREF());

	// Clean up


	// Restore
	dc->RestoreDC(sDC);
}

void CMenuBar::RenderActiveDropDown(CDC* dc, Graphics* g, CRadarScreen* screen) {
	if (ActiveDropDown != 0 && dropDowns.find(ActiveDropDown) != dropDowns.end()) {
		CDropDown* obj = &dropDowns[ActiveDropDown];
		if (obj->State == CInputState::ACTIVE) {
			CCommonRenders::RenderDropDown(dc, g, screen, obj->LastRenderPos, obj->Width, 22, obj, true);
		}
	}
}

bool CMenuBar::IsButtonPressed(int id) {
	// If button pressed
	if (id >= 100) { // dropdown
		if (dropDowns.find(id) != dropDowns.end() && dropDowns[id].State == CInputState::ACTIVE) return true;
	}
	else {
		if (buttons.find(id) != buttons.end() && buttons[id].State == CInputState::ACTIVE) return true;
	}

	return false; // Not pressed
}

string CMenuBar::GetDropDownValue(int id) {
	// Get the value
	return dropDowns[id].Value;
}

map<int, CWinButton> CMenuBar::GetToggleButtons() {
	map<int, CWinButton> map;
	map[BTN_PTL] = buttons[BTN_PTL];
	map[BTN_RINGS] = buttons[BTN_RINGS];
	map[BTN_HALO] = buttons[BTN_HALO];

	return map;
}

void CMenuBar::MakeDropDownItems(int id) {
	if (id == DRP_TCKCTRL) {
		map<string, bool> trackMap;
		{
			lock_guard<mutex> lock(CRoutesHelper::TracksMutex);
			for (auto const& kv : CRoutesHelper::CurrentTracks) {
				trackMap.insert(make_pair(kv.first, true));
			}
		}
		dropDowns[DRP_TCKCTRL].Items.clear();
		dropDowns[DRP_TCKCTRL].MakeItems(&trackMap);
	}
}

void CMenuBar::SetButtonState(int id, CInputState state) {
	// Set the state to the requested one (with failsafe check)
	if (id >= 100) { // dropdown
		if (dropDowns.find(id)->second.State != CInputState::DISABLED) {
			if (dropDowns.find(id) != dropDowns.end()) {
				// Remove the old dropdown stuff
				if (ActiveDropDownHover != 0) {
					dropDowns[ActiveDropDown].Items[ActiveDropDownHover].IsHovered = false;
					ActiveDropDownHover = 0;
				}
				if (ActiveDropDown != 0) {
					dropDowns[ActiveDropDown].State = CInputState::INACTIVE;
					ActiveDropDown = 0;
				}
				
				dropDowns[id].State = state;
				ActiveDropDown = state == CInputState::ACTIVE ? id : 0;
			}
		}
	}
	else { // normal button
		if (buttons.find(id) != buttons.end()) {
			buttons[id].State = state;
		}
	}
}

void CMenuBar::SetTextInput(int id, string value) {
	if (textInputs.find(id) != textInputs.end()) {
		textInputs[id].Content = value;
	}
}

CInputState CMenuBar::GetButtonState(int id) {
	if (id >= 100) { // dropdown
		if (dropDowns.find(id) != dropDowns.end()) {
			return dropDowns.find(id)->second.State;
		}
	}
	else {
		if (buttons.find(id) != buttons.end()) {
			return buttons.find(id)->second.State;
		}
	}
	return CInputState::DISABLED;  // Default return
}

void CMenuBar::OnOverDropDownItem(int id) {
	// Reset the hover state of the old one and set the state of new one
	if (ActiveDropDownHover != 0) {
		dropDowns[ActiveDropDown].Items[ActiveDropDownHover].IsHovered = false;
		dropDowns[ActiveDropDown].Items[id].IsHovered = true;
	}
	ActiveDropDownHover = id;
}

void CMenuBar::SetDropDownValue(int id, int value) {
	// Set the value
	dropDowns[id].Value = dropDowns[id].Items[value].Label;
}

void CMenuBar::ButtonDown(int id) {
	if (id == BTN_RTEDEL)
		SetButtonState(id, CInputState::ACTIVE);

	if (id == BTN_ADSC) 
		SetButtonState(id, CInputState::ACTIVE);

	if (id == BTN_AUTOTAG)
		SetButtonState(id, CInputState::ACTIVE);
}

void CMenuBar::ButtonUp(int id) {
	if (id == BTN_RTEDEL)
		SetButtonState(id, CInputState::INACTIVE);

	if (id == BTN_ADSC)
		SetButtonState(id, CInputState::INACTIVE);

	if (id == BTN_AUTOTAG)
		SetButtonState(id, CInputState::INACTIVE);
}

void CMenuBar::ButtonPress(int id, int button, CRadarScreen* screen = nullptr) {
	if (button == EuroScopePlugIn::BUTTON_LEFT) {
		// Check if dropdown
		if (id >= 800) {
			if (!dropDowns[ActiveDropDown].Items[id].IsCheckItem) {
				// Set value
				dropDowns[ActiveDropDown].Value = dropDowns[ActiveDropDown].Items[id].Label;
				// Close drop down
				dropDowns[ActiveDropDown].Items[ActiveDropDownHover].IsHovered = false;
				ActiveDropDownHover = 0;
				dropDowns[ActiveDropDown].State = CInputState::INACTIVE;
			} else {
				dropDowns[ActiveDropDown].Items[ActiveDropDownHover].IsHovered = false;
				ActiveDropDownHover = 0;
				dropDowns[ActiveDropDown].Items[id].State = dropDowns[ActiveDropDown].Items[id].State == CInputState::INACTIVE ? CInputState::ACTIVE : CInputState::INACTIVE;
			}

			// Save values
			if (ActiveDropDown == DRP_AREASEL) {
				CUtils::AreaSelection = id;
			} else if (ActiveDropDown == DRP_OVERLAYS) {
				CUtils::SelectedOverlay = id;

				switch (id) {
					case 800: // ALL_TCKS
						COverlays::CurrentType = COverlayType::TCKS_ALL;
						break;
					case 801: // TCKS_EAST
						COverlays::CurrentType = COverlayType::TCKS_EAST;
						break;
					case 802: // TCKS_SEL
						COverlays::CurrentType = COverlayType::TCKS_SEL;
						break;
					case 803: // TCKS_WEST
						COverlays::CurrentType = COverlayType::TCKS_WEST;
						break;
				}
			}
			else if (ActiveDropDown == DRP_TYPESEL) {
				CUtils::PosType = id;
			}

			// Reset drop down
			if (!dropDowns[ActiveDropDown].Items[id].IsCheckItem)
				ActiveDropDown = 0;
		}
		else {
			// Press the button (but not for instant-action buttons)
			if (GetButtonState(id) != CInputState::DISABLED && id != BTN_RTEDEL && id != BTN_SELCAL && id != BTN_VACS)
				SetButtonState(id, CInputState::ACTIVE);

			// Scroll handling
			if (CUtils::MenuScroll) {
				if (id == BTN_SCROLL_LEFT) {
					m_scrollOffset -= 200;
					if (m_scrollOffset < 0) m_scrollOffset = 0;
				}
				if (id == BTN_SCROLL_RIGHT) {
					m_scrollOffset += 200;
					if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;
				}
			}

			// Grid
			if (id == BTN_GRID) {
				COverlays::ShowHideGridReference(screen, true);
			}
			
			// SELCAL - instant action on click
			if (id == BTN_SELCAL && screen != nullptr) {
				CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelectASEL();
				if (fp.IsValid()) {
					// Get SELCAL from flight data first
					CAircraftFlightPlan* flightData = CDataHandler::GetFlightData(fp.GetCallsign());
					string selcal = "";
					
					if (flightData != nullptr && flightData->IsValid && flightData->SELCAL != "N/A" && !flightData->SELCAL.empty()) {
						selcal = flightData->SELCAL;
					}
					
					// Fallback to Utils method
					if (selcal.empty()) {
						selcal = CUtils::GetSelcalForAircraft(&fp);
					}
					
					if (!selcal.empty() && selcal != "N/A") {
						string command = ".selcal " + selcal;
						
						// Send input to EuroScope command line
						vector<INPUT> inputs;
						for (char c : command) {
							INPUT input = { 0 };
							input.type = INPUT_KEYBOARD;
							short vk = VkKeyScanA(c);
							input.ki.wVk = vk & 0xFF;
							
							// Handle shift
							if ((vk >> 8) & 1) {
								INPUT shiftDown = { 0 };
								shiftDown.type = INPUT_KEYBOARD;
								shiftDown.ki.wVk = VK_SHIFT;
								inputs.push_back(shiftDown);
							}

							inputs.push_back(input); // Key down

							input.ki.dwFlags = KEYEVENTF_KEYUP;
							inputs.push_back(input); // Key up

							if ((vk >> 8) & 1) {
								INPUT shiftUp = { 0 };
								shiftUp.type = INPUT_KEYBOARD;
								shiftUp.ki.wVk = VK_SHIFT;
								shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
								inputs.push_back(shiftUp);
							}
						}

						// Enter
						INPUT enter = { 0 };
						enter.type = INPUT_KEYBOARD;
						enter.ki.wVk = VK_RETURN;
						inputs.push_back(enter);
						enter.ki.dwFlags = KEYEVENTF_KEYUP;
						inputs.push_back(enter);

						SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));

						screen->GetPlugIn()->DisplayUserMessage("SELCAL", fp.GetCallsign(), 
							(selcal + " - Command executed.").c_str(),
							true, true, false, false, false);
					}
					else {
						screen->GetPlugIn()->DisplayUserMessage("SELCAL", fp.GetCallsign(), 
							"No SELCAL code found. Enter SELCAL in Flight Plan window first.",
							true, true, false, true, false);
					}
				}
				else {
					screen->GetPlugIn()->DisplayUserMessage("SELCAL", "Error", 
						"No aircraft selected - click on an aircraft first",
						true, true, false, true, false);
				}
			}
			
			// VACS - instant action on click
			if (id == BTN_VACS) {
				// Try to load saved path
				static string vacsPath = "";
				if (vacsPath.empty()) {
					// Try to read from settings file
					ifstream settingsFile("vNAAATS_settings.txt");
					if (settingsFile.is_open()) {
						string line;
						while (getline(settingsFile, line)) {
							if (line.find("VACS_PATH=") == 0) {
								vacsPath = line.substr(10);
								break;
							}
						}
						settingsFile.close();
					}
				}
				
				// If still no path, open file dialog
				if (vacsPath.empty()) {
					OPENFILENAMEA ofn;
					char szFile[260] = {0};
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = NULL;
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.lpstrFilter = "Executable Files\0*.exe\0All Files\0*.*\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrTitle = "Select VACS.exe";
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
					
					if (GetOpenFileNameA(&ofn)) {
						vacsPath = ofn.lpstrFile;
						// Save to settings file
						ofstream outFile("vNAAATS_settings.txt", ios::app);
						if (outFile.is_open()) {
							outFile << "VACS_PATH=" << vacsPath << endl;
							outFile.close();
						}
					}
				}
				
				// Launch VACS if we have a path
				if (!vacsPath.empty()) {
					ShellExecuteA(NULL, "open", vacsPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
				}
			}
		}
	}
	else if (button == EuroScopePlugIn::BUTTON_RIGHT) {
		if (id == BTN_HALO) {
			// Get the toggle button
			auto haloBtn = buttons.find(BTN_HALO);

			// Increment if less than or equal 7 (100 mile halos max)
			if (haloBtn->second.Cycle < 7) {
				haloBtn->second.Cycle++;
			}
			else {
				haloBtn->second.Cycle = 0;
			}

			switch (haloBtn->second.Cycle) {
			case 0:
				haloBtn->second.Label = "Halo 5";
				break;
			case 1:
				haloBtn->second.Label = "Halo 10";
				break;
			case 2:
				haloBtn->second.Label = "Halo 15";
				break;
			case 3:
				haloBtn->second.Label = "Halo 20";
				break;
			case 4:
				haloBtn->second.Label = "Halo 25";
				break;
			case 5:
				haloBtn->second.Label = "Halo 30";
				break;
			case 6:
				haloBtn->second.Label = "Halo 60";
				break;
			case 7:
				haloBtn->second.Label = "Halo 100";
				break;
			}
		}

		if (id == BTN_PTL) {
			// Get the toggle button
			auto ptlBtn = buttons.find(BTN_PTL);

			// Increment if less than or equal 6 (60 minute lines max)
			if (ptlBtn->second.Cycle < 6) {
				ptlBtn->second.Cycle++;
			}
			else {
				ptlBtn->second.Cycle = 0;
			}
			switch (ptlBtn->second.Cycle) {
			case 0:
				ptlBtn->second.Label = "PTL 5";
				break;
			case 1:
				ptlBtn->second.Label = "PTL 10";
				break;
			case 2:
				ptlBtn->second.Label = "PTL 15";
				break;
			case 3:
				ptlBtn->second.Label = "PTL 20";
				break;
			case 4:
				ptlBtn->second.Label = "PTL 25";
				break;
			case 5:
				ptlBtn->second.Label = "PTL 30";
				break;
			case 6:
				ptlBtn->second.Label = "PTL 60";
				break;
			}
		}

		if (id == BTN_RINGS) {
			// Get the toggle button
			auto ringsBtn = buttons.find(BTN_RINGS);

			// Increment if less than or equal 4 (5 rings max)
			if (ringsBtn->second.Cycle < 4) {
				ringsBtn->second.Cycle++;
			}
			else {
				ringsBtn->second.Cycle = 0;
			}

			switch (ringsBtn->second.Cycle) {
			case 0:
				ringsBtn->second.Label = "Rings 1";
				break;
			case 1:
				ringsBtn->second.Label = "Rings 2";
				break;
			case 2:
				ringsBtn->second.Label = "Rings 3";
				break;
			case 3:
				ringsBtn->second.Label = "Rings 4";
				break;
			case 4:
				ringsBtn->second.Label = "Rings 5";
				break;
			}
		}
	}
}

void CMenuBar::ButtonUnpress(int id, int button, CRadarScreen* screen) {
	// Don't unpress instant-action buttons (they don't show pressed state)
	if (id != BTN_SELCAL && id != BTN_VACS) {
		SetButtonState(id, CInputState::INACTIVE);
	}

	// Grid
	if (id == BTN_GRID) {
		COverlays::ShowHideGridReference(screen, false);
	}

	// CPDLC - clear alert when clicked
	if (id == BTN_CPDLC) {
		CpdlcAlert = false;
		CpdlcAlertTime = 0;
	}
}

void CMenuBar::GetSelectedTracks(vector<string>& tracksVector) {
	for (auto idx : dropDowns[DRP_TCKCTRL].Items) {
		if (idx.second.State == CInputState::ACTIVE) {
			tracksVector.push_back(idx.second.Label);
		}
	}
}
