#pragma once
#include <string>
#include <EuroScopePlugIn.h>

using namespace EuroScopePlugIn;

// Main plugin header
class CNAAATSPlugin : public CPlugIn
{
	public:
		// Constructor & destructor
		CNAAATSPlugin();
		virtual ~CNAAATSPlugin();

		// OnRadarScreenCreated event
		CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);

		// OnTimer event
		void OnTimer(int Counter);
	private:
		// Register the radar display
		void Register();
};

