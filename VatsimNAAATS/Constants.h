#pragma once
#include <string>
#include <EuroScopePlugIn.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace EuroScopePlugIn;

/// WAYPOINTS
// Entry Waypoints
extern const vector<string> pointsGander;

extern const vector<string> pointsShanwick;


/// VALUES
// Plugin info
extern const string PLUGIN_NAME;
extern const string PLUGIN_VERSION;
extern const string PLUGIN_AUTHOR;
extern const string PLUGIN_COPYRIGHT;
const bool IS_ALPHA = false;
const bool DEBUG_MODE = false;
const bool ERROR_LOGGING = true; // Always log errors/crashes

// Sector file & geo constants
const int SECTELEMENT_COORD_IDX = 7;
const int RADIUS_EARTH_NM = 3440;

// Screen details
#define DISPLAY_NAME "vNAAATS Display"

// Text, margins and padding
const int MEN_FONT_SIZE = 16;
const int BTN_PAD_SIDE = 4;
const int BTN_PAD_TOP = 6;

// Lists
const int LIST_INBOUND_WIDTH = 400;
const int LIST_OTHERS_WIDTH = 110;
const int LIST_RCLS_WIDTH = 110;
const int LIST_CONFLICT_WIDTH = 150;

// Menu bar (height reduced by one row to sit higher; dropdowns on top row to avoid overlap)
const int MENBAR_HEIGHT = 80;
const int MENBAR_BTN_HEIGHT = 28;
const int RECT1_WIDTH = 500;
const int RECT2_WIDTH = 340;
const int RECT3_WIDTH = 160;
const int RECT4_WIDTH = 100;
const int RECT5_WIDTH = 300;
const int RECT6_WIDTH = 100;  // Removed - Search A/C moves to RECT7
const int RECT7_WIDTH = 310;  // Wider for Search A/C + Ext + Auto Tag + Rte Del
const int RECT8_WIDTH = 0;  // Will span to edge of screen

// Window sizes
const int WINSZ_TITLEBAR_HEIGHT = 20;
const int WINSZ_TCKINFO_WIDTH = 600;
const int WINSZ_TCKINFO_HEIGHT = 300;
const int WINSZ_FLTPLN_WIDTH = 540;
const int WINSZ_FLTPLN_HEIGHT_INIT = 175;
const int WINSZ_FLTPLN_HEIGHT_DATA = 190;
const int WINSZ_FLTPLN_HEIGHT_MANETRY = 175;
const int WINSZ_FLTPLN_HEIGHT_CPY = 190;
const int WINSZ_FLTPLN_HEIGHT_MSG = 175;
const int WINSZ_FLTPLN_HEIGHT_XTRA = 165;
const int WINSZ_FLTPLN_WIDTH_MDL = 375;
const int WINSZ_FLTPLN_WIDTH_COORD = 350;
const int WINSZ_FLTPLN_HEIGHT_COORD = 280;
const int WINSZ_FLTPLN_HEIGHT_ATCR = 260;
const int WINSZ_FLTPLN_HEIGHT_HIST = 250;
const int WINSZ_FLTPLN_WIDTH_HIST = 500;
const int WINSZ_FLTPLN_HEIGHT_TSFR = 260;
const int WINSZ_MSG_WIDTH = 550;
const int WINSZ_MSG_HEIGHT = 315;
const int WINSZ_NP_WIDTH = 250;
const int WINSZ_NP_HEIGHT = 300;

// Conflict
const int SEPTOOL_TIME = 2700; // 45 minutes
const int STCA_TIME = 480; // 8 minutes

/// OBJECT HANDLES
// Screen
const int RADAR_SCREEN = -1;
const int SCREEN_TAG = 1;
const int SCREEN_TAG_CS = 11;
const int SCREEN_TAG_CS_BTN = 12;
const int MENBAR = 2;
const int WINDOW = 3;

// Window object handles (for buttons, etc)
const int WIN_TCKINFO = 101;
const int WIN_FLTPLN = 102;
const int WIN_SCROLLBAR = 103;
const int WIN_MSG = 104;
const int WIN_NOTEPAD = 105;
const int WIN_FLTPLN_TSFR = 106;
// CPDLC window object handle
const int WIN_CPDLC = 107;
// FDD window object handle
const int WIN_FDD = 108;
// Setup window object handle
const int WIN_SETUP = 109;

// Text inputs and functions
const int ALTFILT_TEXT = 200;
const int FUNC_ALTFILT_LOW = 201;
const int FUNC_ALTFILT_HIGH = 202;
const int ACTV_MESSAGE = 203;

// Lists
const int LIST_INBOUND = 300;
const int LIST_OTHERS = 301;
const int LIST_RCLS = 302;
const int LIST_CONFLICT = 303;

// Dropdown
const int DRP_AREA_EGGX = 801;
const int DRP_AREA_CZQX = 802;
const int DRP_AREA_BDBX = 803;
const int DRP_STATION = 810;
const int CHK_AUTO_LOGIN = 811;
const int DRP_OVL_ALL = 800;
const int DRP_OVL_EAST = 801;
const int DRP_OVL_WEST = 802;
const int DRP_OVL_SEL = 803;
const int DRP_TYPE_DEL = 801;
const int DRP_TYPE_ENR = 802;
const int DRP_TYPE_MULTI = 803;

/// SETTINGS VARIABLES
extern const string SET_INBNDX;
extern const string SET_INBNDY;
extern const string SET_OTHERSX;
extern const string SET_OTHERSY;
extern const string SET_ALTFILT_LOW;
extern const string SET_ALTFILT_HIGH;
extern const string SET_GRID;
extern const string SET_TAGS;
extern const string SET_QCKLOOK;
extern const string SET_OVERLAY;
extern const string SET_AREASEL;
extern const string SET_OVERLAYSEL;
extern const string SET_POSTYPESEL;
extern const string SET_SCREEN_COUNT;
extern const string SET_HOPPIE_CODE;
extern const string SET_MENU_SCROLL;
extern const string SET_STATION;
extern const string SET_AUTO_LOGIN;

/// ENUMS
// Path type enum
enum class CPathType {
    RTE,
    PIV,
    TCKS
};

// Track direction enum
enum class CTrackDirection {
    UNKNOWN,
    WEST,
    EAST
};

// Type of overlay
enum class COverlayType {
    TCKS_ALL,
    TCKS_EAST,
    TCKS_WEST,
    TCKS_ACTV,
    TCKS_SEL
};

// Conflict status
enum class CConflictStatus {
    OK,
    WARNING,
    CRITICAL
};

// Longitudinal trackstatus 
enum class CTrackStatus {
    NA,
    CROSSING,
    RECIPROCAL,
    OPPOSITE,
    SAME
};

// Button states
enum class CInputState {
    INACTIVE,
    ACTIVE,
    DISABLED
};

// Flight plan mode
enum class CFlightPlanMode {
    NOT_OWNED,
    INIT,
    DATA,
    DATA_COPY,
    CLEARANCE,
    PROBE
};

enum class CMessageType {
    LOG_ON,
    LOG_ON_CONFIRM,
    LOG_ON_REJECT,
    TRANSFER,
    TRANSFER_ACCEPT,
    TRANSFER_REJECT,
    CLEARANCE_REQ,
    CLEARANCE_ISSUE,
    CLEARANCE_REJECT,
    REVISION_REQ,
    REVISION_ISSUE,
    REVISION_REJECT,
    WILCO,
    ROGER,
    UNABLE
};

enum class CRestrictionType {
    LCHG,
    MCHG,
    EPC,
    RERUTE,
    UNABLE,
    RTD,
    ATA,
    ATB,
    XAT,
    INT
};

enum class CRadarTargetMode {
    PRIMARY, // Asterisk (0)
    SECONDARY_S, // Diamond with line (1)
    SECONDARY_C, // Star (2)
    ADS_B, // Airplane icon (also for cleared aircraft) (3)
};

enum class CLogType {
    INIT, // Initialisation message
    NORM, // General log item
    WARN, // Warning
    ERR, // Handled exception
    EXC, // Unhandled exception
};
