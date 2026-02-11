#pragma once
#include "pch.h"
#include "NAAATS.h"
#include <iostream>

#include "Constants.h"
#include "RadarDisplay.h"
#include "Utils.h"
#include "DataHandler.h"

CNAAATSPlugin::CNAAATSPlugin() : CPlugIn(COMPATIBILITY_CODE, PLUGIN_NAME.c_str(), PLUGIN_VERSION.c_str(), PLUGIN_AUTHOR.c_str(), PLUGIN_COPYRIGHT.c_str())
{
	// Register the display
	this->Register();
}

CNAAATSPlugin::~CNAAATSPlugin() {
	// Stop VATSIM data fetcher
	CDataHandler::StopVatsimDataFetcher();

	// Cleanup static resources
	CCPDLCWindow::Cleanup();
}

CRadarScreen* CNAAATSPlugin::OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated) 
{
	// Create new display if the display name matches the constant
	if (!strcmp(sDisplayName, DISPLAY_NAME)) {
		return new CRadarDisplay();
	}

	return nullptr;
}

void CNAAATSPlugin::OnTimer(int Counter)
{
	// Check connection status to start/stop VATSIM data fetcher
	// This is more reliable than checking for radar targets and avoids interference with startup auth
	static bool vatsimFetcherStarted = false;
	int connType = GetConnectionType();

	if (!vatsimFetcherStarted) {
		// Start if connected directly or via proxy
		if (connType == CONNECTION_TYPE_DIRECT || connType == CONNECTION_TYPE_VIA_PROXY) {
			CDataHandler::StartVatsimDataFetcher();
			vatsimFetcherStarted = true;
			CLogger::Log(CLogType::NORM, "VATSIM data fetcher started (connection detected via OnTimer)", "CNAAATSPlugin::OnTimer");
		}
	} else {
		// Stop if disconnected
		if (connType == CONNECTION_TYPE_NO) {
			CDataHandler::StopVatsimDataFetcher();
			vatsimFetcherStarted = false;
			CLogger::Log(CLogType::NORM, "VATSIM data fetcher stopped (disconnected via OnTimer)", "CNAAATSPlugin::OnTimer");
		}
	}
}

void CNAAATSPlugin::Register() {
	// Register the display type and prevent normal EuroScope traffic from rendering
	RegisterDisplayType(DISPLAY_NAME, false, true, true, true);
}