#define _USE_MATH_DEFINES
#include "pch.h"
#include<cmath>
#include <algorithm>
#include <cctype>
#include "Utils.h"
#include "Constants.h"
#include "Structures.h"
#include "RadarDisplay.h"

// Default values
int CUtils::InboundX = 1400;
int CUtils::InboundY = 170;
int CUtils::OthersX = 1200;
int CUtils::OthersY = 150;
int CUtils::ConflictX = 60;
int CUtils::ConflictY = 120;
int CUtils::RCLX = 600;
int CUtils::RCLY = 150;
int CUtils::TrackWindowX = 300;
int CUtils::TrackWindowY = 200;
int CUtils::AltFiltLow = 0;
int CUtils::AltFiltHigh = 700;
bool CUtils::GridEnabled = false;
bool CUtils::TagsEnabled = true;
bool CUtils::QckLookEnabled = false;
bool CUtils::OverlayEnabled = false;
int CUtils::AreaSelection = 802;
int CUtils::SelectedOverlay = 800;
int CUtils::PosType = 802;
char CUtils::DllPathFile[_MAX_PATH];
string CUtils::DllPath;
string CUtils::ScreenCount = "1 Screen";
string CUtils::HoppieCode = "";
bool CUtils::MenuScroll = true;
string CUtils::Station = "NATX";
bool CUtils::AutoLogin = false;

// Separation minima defaults (used by conflict logic / tools)
int CUtils::SepMinimaVertical = 1000;
int CUtils::SepMinimaLateral = 60;
int CUtils::SepMinimaLongitudinal = 10;

// SELCAL local storage (callsign -> code)
map<string, string> CUtils::SelcalStorage;

// Save plugin data
void CUtils::SavePluginData(CRadarScreen* screen) {

	// Inbound list
	screen->SaveDataToAsr(SET_INBNDX.c_str(), "X position of Inbound list.", to_string(InboundX).c_str());
	screen->SaveDataToAsr(SET_INBNDY.c_str(), "Y position of Inbound list.", to_string(InboundY).c_str());

	// Others list
	screen->SaveDataToAsr(SET_OTHERSX.c_str(), "X position of Other list.", to_string(OthersX).c_str());
	screen->SaveDataToAsr(SET_OTHERSY.c_str(), "Y position of Other list.", to_string(OthersY).c_str());

	// Altitude filter (TODO)
	screen->SaveDataToAsr(SET_ALTFILT_LOW.c_str(), "Lower level for altitude filter.", to_string(AltFiltLow).c_str());
	screen->SaveDataToAsr(SET_ALTFILT_HIGH.c_str(), "Upper level for altitude filter.", to_string(AltFiltHigh).c_str());

	// Misc display settings
	screen->SaveDataToAsr(SET_GRID.c_str(), "Grid enabled/disabled.", GridEnabled ? "true" : "false");
	screen->SaveDataToAsr(SET_TAGS.c_str(), "Tags enabled/disabled.", TagsEnabled ? "true" : "false");
	screen->SaveDataToAsr(SET_OVERLAY.c_str(), "Overlay enabled/disabled.", OverlayEnabled ? "true" : "false");
	screen->SaveDataToAsr(SET_QCKLOOK.c_str(), "Quick Look enabled/disabled.", QckLookEnabled ? "true" : "false");

	// Dropdown values
	screen->SaveDataToAsr(SET_AREASEL.c_str(), "Selected area ownership.", to_string(AreaSelection).c_str());
	screen->SaveDataToAsr(SET_OVERLAYSEL.c_str(), "Selected overlay.", to_string(SelectedOverlay).c_str());
	screen->SaveDataToAsr(SET_POSTYPESEL.c_str(), "Selected position type.", to_string(PosType).c_str());

	// New settings
	screen->SaveDataToAsr(SET_SCREEN_COUNT.c_str(), "Number of screens used.", ScreenCount.c_str());
	screen->SaveDataToAsr(SET_HOPPIE_CODE.c_str(), "Hoppie CPDLC code.", HoppieCode.c_str());
	screen->SaveDataToAsr(SET_MENU_SCROLL.c_str(), "Menu bar scroll enabled.", MenuScroll ? "true" : "false");
	screen->SaveDataToAsr(SET_STATION.c_str(), "Station identifier.", Station.c_str());
	screen->SaveDataToAsr(SET_AUTO_LOGIN.c_str(), "CPDLC auto-accept inbound logons.", AutoLogin ? "true" : "false");
}

bool CUtils::WrapText(CDC* dc, string textToWrap, char wrapChar, int contentWidth,  vector<string>* ptrWrappedText) {
	// Split string
	vector<string> tokens;
	StringSplit(textToWrap, wrapChar, &tokens);

	// Intermediate string to store line data whilst length is being calculated
	string intermediate = "";

	// Iterate through and calculate text extent along the way
	for (int i = 0; i < tokens.size(); i++) {
		// Variable to calculate text extent before adding it to the intermediate line
		string beforeAdd = intermediate + tokens[i] + wrapChar;

		// If the text extent of the temporary variable is now greater than the content width
		if (dc->GetTextExtent(beforeAdd.c_str()).cx > contentWidth) {
			// Add the intermediate string to the return vector and clear the intermediate
			ptrWrappedText->push_back(intermediate);
			intermediate = tokens[i] + wrapChar;
		}
		else {
			// Add the token to the intermediate string
			intermediate += tokens[i] + wrapChar;
		}
	}

	// Reached the end, there may be a bit of left over string, if so, add it
	if (intermediate != "") {
		ptrWrappedText->push_back(intermediate);
	}

	return 0;
}

// Constants Definitions
const vector<string> pointsGander = { "AVPUT",
        "CLAVY",
        "EMBOK",
        "KETLA",
        "LIBOR",
        "MAXAR",
        "NIFTY",
        "PIDSO",
        "RADUN",
        "SAVRY",
        "TOXIT",
        "URTAK",
        "VESMI",
        "AVUTI",
        "BOKTO",
        "CUDDY",
        "DORYY",
        "ENNSO",
        "HOIST",
        "IRLOK",
        "JANJO",
        "KODIK",
        "LOMSI",
        "MELDI",
        "NEEKO",
        "PELTU",
        "RIKAL",
        "SAXAN",
        "TUDEP",
        "UMESI",
        "ALLRY",
        "BUDAR",
        "ELSIR",
        "IBERG",
        "JOOPY",
        "MUSAK",
        "NICSO",
        "OMSAT",
        "PORTI",
        "RELIC",
        "SUPRY",
        "RAFIN",
        "JAROM",
        "BOBTU" 
};

const vector<string> pointsShanwick = { "RATSU",
        "LUSEN",
        "ATSIX",
        "ORTAV",
        "BALIX",
        "ADODO",
        "ERAKA",
        "ETILO",
        "GOMUP",
        "AGORI",
        "SUNOT",
        "BILTO",
        "PIKIL",
        "ETARI",
        "RESNO",
        "VENER",
        "DOGAL",
        "NEBIN",
        "MALOT",
        "TOBOR",
        "LIMRI",
        "ADARA",
        "DINIM",
        "RODEL",
        "SOMAX",
        "KOGAD",
        "BEDRA",
        "NERTU",
        "NASBA",
        "OMOKO",
        "TAMEL",
        "GELPO",
        "LASNO",
        "ETIKI",
        "UMLER",
        "SEPAL",
        "BUNAV",
        "SIVIR",
        "BEGAS",
        "DIVAT",
        "DIXIS",
        "BERUX",
        "PITAX",
        "PASAS",
        "NILAV",
        "GONAN",
        "ATSUR"
};

const string PLUGIN_NAME = "vNAAATS";
const string PLUGIN_VERSION = "2.0.0";
const string PLUGIN_AUTHOR = "Gary Thomas (Original by Andrew Ogden)";
const string PLUGIN_COPYRIGHT = "(C) 2024-2026 Gary Thomas, Original (C) 2021 Andrew Ogden";

const string SET_INBNDX = "InboundX";
const string SET_INBNDY = "InboundY";
const string SET_OTHERSX = "OthersX";
const string SET_OTHERSY = "OthersY";
const string SET_ALTFILT_LOW = "AltFiltLow";
const string SET_ALTFILT_HIGH = "AltFiltHigh";
const string SET_GRID = "GridEnabled";
const string SET_TAGS = "TagsEnabled";
const string SET_QCKLOOK = "QckLookEnabled";
const string SET_OVERLAY = "OverlayEnabled";
const string SET_AREASEL = "SelectedArea";
const string SET_OVERLAYSEL = "SelectedOverlay";
const string SET_POSTYPESEL = "SelectedPosType";
const string SET_SCREEN_COUNT = "ScreenCount";
const string SET_HOPPIE_CODE = "HoppieCode";
const string SET_MENU_SCROLL = "MenuScroll";
const string SET_STATION = "Station";
const string SET_AUTO_LOGIN = "AutoLogin";

const vector<CWaypoint> NatSM = {
	CWaypoint("SM15W", 50.683, -15.0),
	CWaypoint("SM20W", 50.833, -20.0),
	CWaypoint("SM30W", 50.5, -30.0),
	CWaypoint("SM40W", 49.266, -40.0),
	CWaypoint("SM50W", 47.05, -50.0),
	CWaypoint("SM53W", 46.166, -53.0),
	CWaypoint("SM60W", 44.233, -60.0),
	CWaypoint("SM65W", 42.766, -65.0),
	CWaypoint("SM67W", 42.0, -67.0)
};
const vector<CWaypoint> NatSN = {
	CWaypoint("SN67W", 40.416667, -67.0),
	CWaypoint("SN65W", 41.666667, -65.0),
	CWaypoint("SN60W", 43.116667, -60.0),
	CWaypoint("SN525W", 45.166667, -52.5),
	CWaypoint("SN50W", 45.9, -50.0),
	CWaypoint("SN40W", 48.166667, -40.0),
	CWaypoint("SN30W", 49.433333, -30.0),
	CWaypoint("SN20W", 49.816667, -20.0),
	CWaypoint("SN15W", 49.683333, -15.0)
};
const vector<CWaypoint> NatSO = {
	CWaypoint("SO15W", 48.666667, -15.0),
	CWaypoint("SO20W", 48.8, -20.0),
	CWaypoint("SO30W", 48.366667, -30.0),
	CWaypoint("SO40W", 47.066667, -40.0),
	CWaypoint("SO50W", 44.75, -50.0),
	CWaypoint("SO52W", 44.166667, -52.0),
	CWaypoint("SO60W", 42.0, -60.0)
};
const vector<CWaypoint> NatSL = {
	CWaypoint("SL50W", 57.0, -50.0),
	CWaypoint("SL40W", 57.0, -40.0),
	CWaypoint("SL30W", 56.0, -30.0),
	CWaypoint("SL20W", 54.0, -20.0),
	CWaypoint("SL15W", 52.0, -15.0)
};
const vector<CWaypoint> NatSP = {
	CWaypoint("SP20W", 46.816667, -20.0),
	CWaypoint("SP238W", 45.0, -23.883333),
	CWaypoint("SP30W", 41.6, -30.0),
	CWaypoint("SP40W", 34.366667, -40.0),
	CWaypoint("SP477W", 27.0, -47.783333),
	CWaypoint("SP50W", 24.633333, -50.0),
	CWaypoint("SP556W", 18.0, -55.65)
};

bool CUtils::StringSplit(string str, char splitBy, vector<string>* ptrTokens) {
	// Error if token does not exist
	if (str.find(splitBy) == string::npos) {
		if (str.size() != 0) {
			ptrTokens->push_back(str);
			return 1;
		}
		else {
			return 1;
		}
	}

	// String stream
	stringstream stream(str);

	// Intermediate value
	string intermediate;

	// Tokenise the string
	while (getline(stream, intermediate, splitBy))
	{
		ptrTokens->push_back(intermediate);
	}

	return 0;
}

// Phraseology parser
string CUtils::ParseToPhraseology(string rawInput, CMessageType type, string callsign) {
	try {
		// Split the string
		vector<string> splitString;
		StringSplit(rawInput, ':', &splitString);

		// CALLSIGN:LOG_ON:CONTROLLER
		if (type == CMessageType::LOG_ON) {
			return "REQ DATALINK LOG-ON STATION " + splitString[2];
		}
		// STATION_CALLSIGN:TRANSFER:AIRCRAFT_CALLSIGN
		else if (type == CMessageType::TRANSFER) {
			return "STATION " + splitString[0] + " REQUEST TRANSFER " + splitString[2] + " TO YOU";
		}
		// STATION_CALLSIGN:TRANSFER_ACCEPT:AIRCRAFT_CALLSIGN
		else if (type == CMessageType::TRANSFER_ACCEPT) {
			return "STATION " + splitString[0] + " ACCEPTED TRANSFER " + splitString[2];
		}
		// STATION_CALLSIGN:TRANSFER_REJECT:AIRCRAFT_CALLSIGN
		else if (type == CMessageType::TRANSFER_REJECT) {
			return "STATION " + splitString[0] + " REJECTED TRANSFER " + splitString[2];
		}
		// CALLSIGN:CLEARANCE_REQUEST:ICAO CODE:ROUTE STRING OR NULL:WAYPOINT:EST AS ZULU:LETTER OR RR:LEVEL:MACH:<FREETEXT>
		else if (type == CMessageType::CLEARANCE_REQ) {
			string returnString = "OCA CLR REQ: CLA TO " + splitString[2] + " VIA ";
			if (splitString[3] != "NULL") {
				returnString += "RANDOM ROUTING " + splitString[3] + ".";
			}
			else {
				returnString += "TRACK " + splitString[6] + ".";
			}
			// Flight level and mach
			returnString += " F" + splitString[7] + " M" + PadWithZeros(3, stoi(splitString[8]));
			// Estimated time
			returnString += " EST " + splitString[4] + " AT " + splitString[5];
			// Freetext
			//returnString += ". FREE: " + splitString[9];
			return returnString;
		}
		// CALLSIGN:CLEARANCE_ISSUE:ICAO CODE:ROUTE STRING OR NULL:WAYPOINT:EST AS ZULU:LETTER OR RR:LEVEL:MACH:ATC/:LCHG:MCHG:RERUTE
		else if (type == CMessageType::CLEARANCE_ISSUE) {
			string returnString = "CZQX CLRNCE: CLA TO " + splitString[2] + " VIA ";
			if (splitString[3] != "NULL") {
				returnString += "RANDOM ROUTING " + splitString[3] + ".";
			}
			else {
				returnString += splitString[4] + " NAT TRACK " + splitString[6] + ".";
			}
			// Estimate
			returnString += " FM " + splitString[4] + "/" + splitString[5];
			// Flight level and mach
			returnString += " MNTN F" + splitString[7] + " M" + PadWithZeros(3, stoi(splitString[8])) + " ";
			// Freetext
			//returnString += ". FREE: " + splitString[9];

			// Restrictions
			auto restrictionsIndex = std::find(splitString.begin(), splitString.end(), "ATC/");
			for (auto idx = restrictionsIndex; idx != splitString.end(); idx++) {
				if (*idx == "RERUTE")
					returnString += "ROUTE HAS BEEN CHANGED.";
			}
			return returnString;
		}
		// CALLSIGN:REVISION_REQ:MCHG:CONTENT:LCHG:CONTENT:RERUTE:CONTENT
		else if (type == CMessageType::REVISION_REQ) {
			string returnString = "REQUEST [";
			int counter = 0; // How many revisions (so that comma can be placed)
			for (int i = 0; i < splitString.size(); i++) {
				if (splitString[i] == "MCHG") {
					returnString += "M" + PadWithZeros(3, stoi(splitString[i + 1])) + "]";
				}
				if (splitString[i] == "LCHG") {
					returnString += "[F" + splitString[i + 1] + "] ";
				}
				if (splitString[i] == "RERUTE") {
					returnString += "[" + splitString[i + 1] + "] ";
				}
			}
			return returnString;
		}
		else if (type == CMessageType::REVISION_ISSUE) {
			// Flight data
			CAircraftFlightPlan* primedPlan = CDataHandler::GetFlightData(callsign);
			string returnString;
			// Split the current message
			vector<string> splitString;
			StringSplit(rawInput, ':', &splitString);
			int machChange = -1;
			int levelChange = -1;
			int rerute = -1;
			for (int i = 0; i < splitString.size(); i++) {
				if (splitString[i] == "ATC/")
					break;
				if (splitString[i] == "MCHG") {
					machChange = i + 1;
				}
				if (splitString[i] == "LCHG") {
					levelChange = i + 1;
				}
				if (splitString[i] == "RERUTE") {
					rerute = i + 1;
				}
			}

			bool isLevelRestriction = false;
			bool isMachRestriction = false;
			bool isRouteRestriction = false;
			vector<string> restrictions = { "LCHG, MCHG, ATA", "ATB", "XAT", "UNABLE", "INT" };

			// Restrictions
			auto restrictionsIndex = std::find(splitString.begin(), splitString.end(), "ATC/");
			for (auto idx = restrictionsIndex; idx != splitString.end(); idx++) {
				if (*idx == "LCHG") {
					isLevelRestriction = true;
					int isTime = false;
					returnString += "CLIMB TO AND MAINTAIN F" + splitString[levelChange] + " CROSS " + *(idx + 1) + " AT F" + splitString[levelChange] + " REPORT LEAVING F" + primedPlan->FlightLevel + " REPORT LEVEL F" + splitString[levelChange] + ". ";
				}
				else if (*idx == "MCHG") {
					isMachRestriction = true;
					returnString += "MAINTAIN MACH 0" + splitString[machChange] + ". CROSS " + *(idx + 1) + " AT " + splitString[machChange] + ". ";
				}
				else if (*idx == "EPC") {

				}
				else if (*idx == "RTD") {

				}
				else if (*idx == "UNABLE") {
					vector<string> unables;
					if (std::find(restrictions.begin(), restrictions.end(), *(idx + 1)) != restrictions.end()) {
						unables.push_back(*(idx + 1));
					}
					if (std::find(restrictions.begin(), restrictions.end(), *(idx + 2)) != restrictions.end()) {
						unables.push_back(*(idx + 2));
					}
					if (std::find(restrictions.begin(), restrictions.end(), *(idx + 3)) != restrictions.end()) {
						unables.push_back(*(idx + 3));
					}
					int counter = 0;
					for (int i = 0; i < unables.size(); i++) {
						if (unables[i] == "LCHG") {
							if (counter > 1)
								returnString += ", LEVEL CHANGE";
							else
								returnString += "UNABLE LEVEL CHANGE ";
						}
						else if (unables[i] == "MCHG") {
							if (counter > 1)
								returnString += ", SPD CHANGE";
							else
								returnString += "UNABLE SPD CHANGE ";
						}
						else {
							if (counter > 1)
								returnString += ", RERUTE";
							else
								returnString += "UNABLE RERUTE ";
						}
					}
					returnString += " ";
				}
				else if (*idx == "ATA") {
					returnString += "CROSS " + *(idx + 1) + " AFTER " + *(idx + 2) + ". ";
				}
				else if (*idx == "ATB") {
					returnString += "CROSS " + *(idx + 1) + " BEFORE " + *(idx + 2) + ". ";
				}
				else if (*idx == "XAT") {
					returnString += "CROSS " + *(idx + 1) + " AT " + *(idx + 2) + ". ";
				}
			}

			if (machChange != -1) {
				returnString += "MAINTAIN MACH 0" + splitString[machChange] + ". ";
			}
			if (levelChange != -1 && !isLevelRestriction) {
				if (!returnString.empty())
					returnString += " ";

				returnString += "CLIMB TO AND MAINTAIN F" + splitString[levelChange] + " REPORT LEAVING F" + primedPlan->FlightLevel + " REPORT LEVEL F" + splitString[levelChange] + ". ";
			}
			if (rerute != -1 && !isRouteRestriction) {
				if (!returnString.empty())
					returnString += " ";
				returnString += "ROUTE HAS BEEN CHANGED CLEARED " + splitString[rerute] + ". ";
			}

			return returnString;
		}
		// CALLSIGN:WILCO
		else if (type == CMessageType::WILCO) {
			return "WILCO LAST MSG";
		}
		// CALLSIGN:UNABLE
		else if (type == CMessageType::UNABLE) {
			return "UNABLE LAST REQUEST";
		}
		// CALLSIGN:ROGER
		else if (type == CMessageType::ROGER) {
			return "ROGER LAST MSG";
		}
		else { // Default
			return rawInput;
		}
	}
	catch(std::exception e) {
		return rawInput;
	}
}

// Raw format parser
string CUtils::ParseToRaw(string callsign, CMessageType type, CAircraftFlightPlan* copy) {
	// Flight data
	CAircraftFlightPlan* fp = copy != nullptr ? copy : CDataHandler::GetFlightData(callsign);

	if (type == CMessageType::LOG_ON_CONFIRM) {
		return fp->Callsign + ":LOG_ON_CONFIRM";
	}
	if (type == CMessageType::LOG_ON_REJECT) {
		return fp->Callsign + ":LOG_ON_REJECT";
	}
	if (type == CMessageType::TRANSFER_ACCEPT) {
		return fp->CurrentMessage->From + ":TRANSFER_ACCEPT:" + fp->Callsign;
	}
	if (type == CMessageType::TRANSFER_REJECT) {
		return fp->CurrentMessage->From + ":TRANSFER_REJECT:" + fp->Callsign;
	}
	if (type == CMessageType::CLEARANCE_ISSUE) {
		// Split the current message
		vector<string> splitString;
		StringSplit(fp->CurrentMessage->MessageRaw, ':', &splitString);

		// Get the route
		string routeString;
		if (fp->Track == "RR") {
			for (int i = 0; i < fp->RouteRaw.size(); i++)
				routeString += fp->RouteRaw[i] + " ";
			routeString = routeString.substr(0, routeString.size() - 1);
		}
		else {
			routeString = "NULL";
		}
		string returnString = fp->Callsign + ":CLEARANCE_ISSUE:" + fp->Dest + ":" + routeString + ":" + splitString[4] + ":" + splitString[5] + ":" + fp->Track + ":" + fp->FlightLevel + ":" + fp->Mach + ":" + "ATC/";
		if (fp->Restrictions.empty()) {
			returnString += "NULL";
		}
		else {
			for (int i = 0; i < fp->Restrictions.size(); i++) {
				if (fp->Restrictions[i].Type == CRestrictionType::LCHG) {
					returnString += ":LCHG:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::MCHG) {
					returnString += ":MCHG:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::EPC) {
					returnString += ":EPC:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::RERUTE) {
					returnString += ":RERUTE";
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::RTD) {
					returnString += ":RTD";
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::UNABLE) {
					returnString += ":UNABLE:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::ATA) {
					returnString += ":ATA:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::ATB) {
					returnString += ":ATB:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::XAT) {
					returnString += ":XAT:" + fp->Restrictions[i].Content;
				}
				else if (fp->Restrictions[i].Type == CRestrictionType::INT) {
					returnString += ":INT:" + fp->Restrictions[i].Content;
				}
			}
		}

		return returnString;
	}
	if (type == CMessageType::CLEARANCE_REJECT) {
		return fp->Callsign + ":CLEARANCE_REJECT";
	}
	if (type == CMessageType::REVISION_ISSUE) {
		// Flight data
		CAircraftFlightPlan* primedPlan = CDataHandler::GetFlightData(callsign);
		string returnString;
		string routeString;
		for (int i = 0; i < primedPlan->RouteRaw.size(); i++)
			routeString += primedPlan->RouteRaw[i] + " ";

		string copyRouteString;
		for (int i = 0; i < fp->RouteRaw.size(); i++)
			copyRouteString += fp->RouteRaw[i] + " ";
		
		returnString += fp->Callsign + ":REVISION_ISSUE";
		// Switch
		if (fp->Mach != primedPlan->Mach) {
			returnString += ":MCHG:" + fp->Mach;
		}
		if (fp->FlightLevel != primedPlan->FlightLevel) {
			returnString += ":LCHG:" + fp->FlightLevel;
		}
		if (copyRouteString != routeString) {
			returnString += ":RERUTE:" + routeString;
		}
		
		return returnString;
	}
	return "";
}

// Convert coordinates to various type
string CUtils::ConvertCoordinateFormat(string coordinateString, int format) { // format = 0 (slash format), 1 (xxxxN), 2 (xxNxxxW)
	// Return var
	string returnFormat;
	try {
		// First we make sure there are numbers
		int isAllAlpha = true;
		for (int j = 0; j < coordinateString.size(); j++) {
			if (isdigit(coordinateString.at(j))) {
				isAllAlpha = false;
			}
		}
		// Check the current format of the input string
		int currentFormat = -1;
		if (coordinateString.find('/') != string::npos) {
			currentFormat = 0;
		}
		else if (coordinateString.find('W') == string::npos && coordinateString.find('/') == string::npos && coordinateString.size() == 5) {
			currentFormat = 1;
		}
		else if (coordinateString.find('W') != string::npos) {
			currentFormat = 2;
		}

		// Check the current format, if -1 or matches, just return the input string
		if (currentFormat == -1 || currentFormat == format || isAllAlpha || coordinateString.size() > 7) {
			return coordinateString;
		}

		// Otherwise, change the format
		if (format == 0) {
			if (currentFormat == 1) {
				returnFormat = coordinateString.substr(0, 2) + "/" + coordinateString.substr(2, 2);

			}
			else if (currentFormat == 2) {
				returnFormat = coordinateString.substr(0, 2) + "/" + coordinateString.substr(4, 2);
			}
		}
		else if (format == 1) {
			if (currentFormat == 0) {
				returnFormat = coordinateString.substr(0, 2) + coordinateString.substr(3, 2) + "N";
			}
			else if (currentFormat == 2) {
				returnFormat = coordinateString.substr(0, 2) + coordinateString.substr(4, 2) + "N";
			}
		}
		else {
			if (currentFormat == 0) {
				returnFormat = coordinateString.substr(0, 2) + "N0" + coordinateString.substr(3, 2) + "W";
			}
			else if (currentFormat == 1) {
				returnFormat = coordinateString.substr(0, 2) + "N0" + coordinateString.substr(2, 2) + "W";
			}
		}
	}
	catch (exception & ex) {
		// Return the old one if an exception occurs
		return coordinateString;
	}
	
	// Return the string
	return returnFormat;
}

bool CUtils::GetAircraftDirection(int heading) {
	if ((heading <= 359) && (heading >= 180)) {
		return false; // Westbound
	}
	else if ((heading >= 0) && (heading <= 179)) {

		return true; // Eastbound
	}
	return false;  // Default - treat as westbound
}

bool CUtils::IsEntryPoint(string pointName, bool side) {
	if (side) { // Gander
		if (find(pointsGander.begin(), pointsGander.end(), pointName) != pointsGander.end()) {
			return true; // Match
		}
		else {
			return false; // No match
		}
	}
	else { // Shanwick
		if (find(pointsShanwick.begin(), pointsShanwick.end(), pointName) != pointsShanwick.end()) {
			return true; // Match
		}
		else {
			return false; // No match
		}
	}
}

bool CUtils::IsExitPoint(string pointName, bool side) {
	if (side) { // Gander
		if (find(pointsShanwick.begin(), pointsShanwick.end(), pointName) != pointsShanwick.end()) {
			return true; // Match
		}
		else {
			return false; // No match
		}
	}
	else { // Shanwick
		if (find(pointsGander.begin(), pointsGander.end(), pointName) != pointsGander.end()) {
			return true; // Match
		}
		else {
			return false; // No match
		}
	}
}

bool CUtils::IsAircraftRelevant(CRadarScreen* screen, CRadarTarget* target, bool filtersDisabled) {
	// Flag
	bool valid = true;

	// Flight plan & position
	CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(target->GetCallsign());
	CPosition pos = target->GetPosition().GetPosition();

	// If no flight plan, not relevant
	if (!fp.IsValid()) {
		return false;
	}

	// Filter by altitude (unless disabled)
	if (!filtersDisabled) {
		if (fp.GetControllerAssignedData().GetClearedAltitude() < (AltFiltLow * 100) || fp.GetControllerAssignedData().GetClearedAltitude() > (AltFiltHigh * 100)) {
			// Also check actual altitude
			if (target->GetPosition().GetPressureAltitude() < (AltFiltLow * 100) || target->GetPosition().GetPressureAltitude() > (AltFiltHigh * 100)) {
				return false;
			}
		}

		// Check if in airspace only if filters are enabled
		// TODO: Better check
		if (pos.m_Longitude > -65.0 && pos.m_Longitude < -10.0 && pos.m_Latitude > 40.0 && pos.m_Latitude < 67.0) {
			return true;
		}
		else {
			return false;
		}
	}

	// If filters disabled, show everything
	return true;
}

bool CUtils::IsAircraftEquipped(string rawRemarks, string rawAcInfo, char equipCode) {
	// Check if the aircraft is equipped
	if (rawAcInfo.find(equipCode) != string::npos) {
		return true;
	}
	return false;
}

CPosition CUtils::PositionFromLatLon(double lat, double lon) {
	CPosition pos;
	pos.m_Latitude = lat;
	pos.m_Longitude = lon;
	return pos;
}

int CUtils::GetMach(int groundSpeed, int speedSound) {
	return (int)round(((double)groundSpeed / (double)speedSound) * 100);
}

CRadarTargetMode CUtils::GetTargetMode(int radarFlags) {
	switch (radarFlags) {
		case 0: return CRadarTargetMode::PRIMARY;
		case 1: return CRadarTargetMode::SECONDARY_S;
		case 2: return CRadarTargetMode::SECONDARY_C;
		case 3: return CRadarTargetMode::ADS_B;
		default: return CRadarTargetMode::PRIMARY;
	}
}

int CUtils::GetTargetModeInt(int radarFlags) {
	return radarFlags;
}

string CUtils::PadWithZeros(int width, int number) {
	stringstream ss;
	ss << setw(width) << setfill('0') << number;
	return ss.str();
}

bool CUtils::IsAllAlpha(string str) {
	for (int i = 0; i < str.size(); i++) {
		if (!isalpha(str[i])) {
			return false;
		}
	}
	return true;
}

string CUtils::GetSelcalCode(CFlightPlan* fpData) {
	string remarks = fpData->GetFlightPlanData().GetRemarks();
	size_t found = remarks.find(string("SEL/"));
	if (found != string::npos) {
		return remarks.substr(found + 4, 4);
	}
	return "";
}

string CUtils::GetSelcalForAircraft(CFlightPlan* fp) {
    if (!fp) return "";

    string fromRemarks = GetSelcalCode(fp);
    if (fromRemarks.size() == 4) return fromRemarks;

    const string cs = fp->GetCallsign();
    auto it = SelcalStorage.find(cs);
    if (it != SelcalStorage.end()) return it->second;

    return "";
}

void CUtils::StoreSelcal(const string& callsign, const string& code) {
	SelcalStorage[callsign] = code;
}

void CUtils::ClearStoredSelcal(const string& callsign) {
	if (SelcalStorage.find(callsign) != SelcalStorage.end()) {
		SelcalStorage.erase(callsign);
	}
}

string CUtils::ParseZuluTime(bool delimit, int deltaTime, CFlightPlan* fp, int fix) {
	time_t now = time(0);
	tm* zuluTime = gmtime(&now);
	int deltaMinutes = 0;
	if (deltaTime != -1) {
		deltaMinutes = deltaTime;
	}
	if (fix != -1 && fp) {
		deltaMinutes = fp->GetExtractedRoute().GetPointDistanceInMinutes(fix);
	}
	int hours = zuluTime->tm_hour;
	int minutes = zuluTime->tm_min + deltaMinutes;

	if (minutes >= 60) {
		int minRemainder = minutes % 60;
		hours += (minutes - minRemainder) / 60;
		minutes = minRemainder;
	}

	if (hours >= 24) {
		hours = hours - 24;
	}

	string strHours = hours < 10 ? (hours == 0 ? "00" : "0" + to_string(hours)) : to_string(hours);
	string strMinutes = minutes < 10 ? (minutes == 0 ? "00" : "0" + to_string(minutes)) : to_string(minutes);

	return delimit ? (strHours + ":" + strMinutes) : (strHours + strMinutes);
}

int CUtils::GetDistanceBetweenPoints(POINT p1, POINT p2) {
	return (int)sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
}

POINT CUtils::GetMidPoint(POINT p1, POINT p2) {
	POINT p;
	p.x = (p1.x + p2.x) / 2;
	p.y = (p1.y + p2.y) / 2;
	return p;
}

int CUtils::GetTimeDistanceSpeed(int distanceNM, int speedGS) {
	// Time = Distance / Speed
	if (speedGS <= 0) return 0; // Prevent division by zero
	return (int)round(((double)distanceNM / (double)speedGS) * 60);
}

double CUtils::GetDistanceSpeedTime(int speedGS, int timeSec) {
	// Distance = Speed * Time
	return (double)speedGS * ((double)timeSec / 3600);
}

double CUtils::MetresToNauticalMiles(double metres) {
	return metres * 0.000539957;
}

double CUtils::ToRadians(double degrees) {
	return degrees * (M_PI / 180);
}

double CUtils::ToDegrees(double radians) {
	return radians * (180 / M_PI);
}

string CUtils::RoundDecimalPlaces(double num, int precision) {
	stringstream ss;
	ss << fixed << setprecision(precision) << num;
	return ss.str();
}

double CUtils::GetPathAngle(double hdg1, double hdg2) {
	// Convert to radians
	double hdg1Rad = ToRadians(hdg1);
	double hdg2Rad = ToRadians(hdg2);

	// Get vectors
	double u[] = { sin(hdg1Rad), cos(hdg1Rad) };
	double v[] = { sin(hdg2Rad), cos(hdg2Rad) };

	// Dot product
	double dotProduct = u[0] * v[0] + u[1] * v[1];

	// Angle
	double angle = ToDegrees(acos(dotProduct));

	return angle;
}

CPosition CUtils::GetPointDistanceBearing(CPosition position, int distanceMetres, int heading) {
	// Earth radius
	double R = 6371e3;

	// Convert to radians
	double lat1 = ToRadians(position.m_Latitude);
	double lon1 = ToRadians(position.m_Longitude);
	double brng = ToRadians(heading);

	// Calculate
	double lat2 = asin(sin(lat1) * cos(distanceMetres / R) + cos(lat1) * sin(distanceMetres / R) * cos(brng));
	double lon2 = lon1 + atan2(sin(brng) * sin(distanceMetres / R) * cos(lat1), cos(distanceMetres / R) - sin(lat1) * sin(lat2));

	// Return
	CPosition pos;
	pos.m_Latitude = ToDegrees(lat2);
	pos.m_Longitude = ToDegrees(lon2);
	return pos;
}

POINT CUtils::GetIntersectionFromPointBearing(POINT position1, POINT position2, double bearing1, double bearing2) {
	// Convert to radians
	double b1 = ToRadians(bearing1);
	double b2 = ToRadians(bearing2);

	// Calculate
	double x1 = position1.x;
	double y1 = position1.y;
	double x2 = position2.x;
	double y2 = position2.y;

	// Slopes
	double m1 = tan(M_PI / 2 - b1);
	double m2 = tan(M_PI / 2 - b2);

	// Intercepts
	double c1 = y1 - m1 * x1;
	double c2 = y2 - m2 * x2;

	// Intersection
	double x = (c2 - c1) / (m1 - m2);
	double y = m1 * x + c1;

	// Return
	POINT p;
	p.x = (long)x;
	p.y = (long)y;
	return p;
}

string CUtils::GetLatLonString(CPosition* pos, bool space, int precision, bool showDecimal) {
	// Latitude
	string lat = "";
	double latVal = pos->m_Latitude;
	if (latVal < 0) {
		latVal = abs(latVal);
		lat = "S";
	}
	else {
		lat = "N";
	}
	if (latVal < 10) {
		lat += "0";
	}
	lat += to_string((int)latVal);
	if (space) {
		lat += " ";
	}
	// Minutes
	double latMin = (latVal - (int)latVal) * 60;
	if (latMin < 10) {
		lat += "0";
	}
	lat += to_string((int)latMin);
	if (showDecimal) {
		// Seconds
		double latSec = (latMin - (int)latMin) * 60;
		lat += ".";
		if (latSec < 10) {
			lat += "0";
		}
		lat += to_string((int)latSec);
	}
	
	// Longitude
	string lon = "";
	double lonVal = pos->m_Longitude;
	if (lonVal < 0) {
		lonVal = abs(lonVal);
		lon = "W";
	}
	else {
		lon = "E";
	}
	if (lonVal < 100) {
		lon += "0";
	}
	if (lonVal < 10) {
		lon += "0";
	}
	lon += to_string((int)lonVal);
	if (space) {
		lon += " ";
	}
	// Minutes
	double lonMin = (lonVal - (int)lonVal) * 60;
	if (lonMin < 10) {
		lon += "0";
	}
	lon += to_string((int)lonMin);
	if (showDecimal) {
		// Seconds
		double lonSec = (lonMin - (int)lonMin) * 60;
		lon += ".";
		if (lonSec < 10) {
			lon += "0";
		}
		lon += to_string((int)lonSec);
	}

	return lat + " " + lon;
}

HANDLE CUtils::GetESProcess() {
	// Get the process ID
	DWORD pid = GetCurrentProcessId();

	// Get the handle
	return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
}

void CUtils::LoadPluginData(CRadarScreen* screen) {
	// Strings to parse data
	const char* stra;
	const char* strb;

	// Inbound list
	stra = screen->GetDataFromAsr(SET_INBNDX.c_str());
	strb = screen->GetDataFromAsr(SET_INBNDY.c_str());
	if (stra != NULL && strb != NULL) {
		InboundX = stoi(stra);
		InboundY = stoi(strb);
	}

	// Others list
	stra = screen->GetDataFromAsr(SET_OTHERSX.c_str());
	strb = screen->GetDataFromAsr(SET_OTHERSY.c_str());
	if (stra != NULL && strb != NULL) {
		OthersX = stoi(stra);
		OthersY = stoi(strb);
	}

	// Others list
	stra = screen->GetDataFromAsr(SET_ALTFILT_LOW.c_str());
	strb = screen->GetDataFromAsr(SET_ALTFILT_HIGH.c_str());
	if (stra != NULL && strb != NULL) {
		AltFiltLow = stoi(stra);
		AltFiltHigh = stoi(strb);
	}
	// Grid enabled
	stra = screen->GetDataFromAsr(SET_GRID.c_str());
	if (stra != NULL) {
		GridEnabled = (stra[0] == 't');
	}
	
	// Tags enabled
	stra = screen->GetDataFromAsr(SET_TAGS.c_str());
	if (stra != NULL) {
		TagsEnabled = (stra[0] == 't');
	}

	// Quick look enabled
	stra = screen->GetDataFromAsr(SET_QCKLOOK.c_str());
	if (stra != NULL) {
		QckLookEnabled = (stra[0] == 't');
	}

	// Overlay enabled
	stra = screen->GetDataFromAsr(SET_OVERLAY.c_str());
	if (stra != NULL) {
		OverlayEnabled = (stra[0] == 't');
	}

	// Area selection
	stra = screen->GetDataFromAsr(SET_AREASEL.c_str());
	if (stra != NULL) {
		AreaSelection = stoi(stra);
	}

	// Overlay selection
	stra = screen->GetDataFromAsr(SET_OVERLAYSEL.c_str());
	if (stra != NULL) {
		SelectedOverlay = stoi(stra);
	}

	// Position type selection
	stra = screen->GetDataFromAsr(SET_POSTYPESEL.c_str());
	if (stra != NULL) {
		PosType = stoi(stra);
	}

	// Screen Count
	stra = screen->GetDataFromAsr(SET_SCREEN_COUNT.c_str());
	if (stra != NULL) {
		ScreenCount = stra;
	}

	// Hoppie Code
	stra = screen->GetDataFromAsr(SET_HOPPIE_CODE.c_str());
	if (stra != NULL) {
		HoppieCode = stra;
	}

	// Menu Scroll
	stra = screen->GetDataFromAsr(SET_MENU_SCROLL.c_str());
	if (stra != NULL) {
		MenuScroll = (stra[0] == 't');
	}

	// Station
	stra = screen->GetDataFromAsr(SET_STATION.c_str());
	if (stra != NULL) {
		Station = stra;
	}

	// Auto Login
	stra = screen->GetDataFromAsr(SET_AUTO_LOGIN.c_str());
	if (stra != NULL) {
		AutoLogin = (stra[0] == 't');
	}
}

