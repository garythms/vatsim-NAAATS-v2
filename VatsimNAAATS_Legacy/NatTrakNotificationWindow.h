#pragma once
#include "pch.h"
#include "BaseWindow.h"
#include "DataHandler.h"

class CNatTrakNotificationWindow : public CBaseWindow {
public:
	CNatTrakNotificationWindow(POINT topLeft);
	virtual void RenderWindow(CDC* dc, Graphics* g, CRadarScreen* screen);
	virtual void MakeWindowItems();
	virtual void ButtonDown(int id);
	virtual void ButtonUp(int id, CRadarScreen* screen = nullptr);
	virtual void ButtonPress(int id);
	virtual void ButtonUnpress(int id);
	virtual void SetButtonState(int id, CInputState state);

	static const int BTN_ACK = 600;
	static const int BTN_CLOSE = 601;

private:
	clock_t flashTimer;
	bool flashState;
};
