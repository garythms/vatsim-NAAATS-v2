#pragma once
#include "BaseWindow.h"
class CSetupWindow : public CBaseWindow
{
	public:
		// Inherited methods
		CSetupWindow(POINT topLeft);
		virtual void RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen);
		virtual void MakeWindowItems();
		virtual void ButtonDown(int id);
		virtual void ButtonUp(int id, CRadarScreen* screen = nullptr);
		virtual void ButtonPress(int id);
		virtual void ButtonUnpress(int id);
		virtual void SetButtonState(int id, CInputState state);

		// Child methods
		void SetTextValue(CRadarScreen* screen, int id, string content);

		// Control IDs
		static const int BTN_CLOSE = 601;
		static const int BTN_SAVE = 602;
		static const int CHK_SINGLE = 603;
		static const int CHK_SCROLL = 604;
		static const int TXT_HOPPIE = 605;
		static const int TXT_STATION = 606;
};

