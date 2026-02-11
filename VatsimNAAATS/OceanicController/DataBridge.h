#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "json.hpp"

#include <map>
#include <algorithm>

#include <memory>

using namespace std;
using json = nlohmann::json;

struct Waypoint {
    string name;
    string estimate;
};

struct AircraftData {
    string callsign;
    string type;
    string dep;
    string dest;
    string fl;
    string mach;
    string selcal;
    string trackId;
    bool isWestbound;
    bool isASEL;
    int status; // 0=UNKNOWN, 1=PENDING, 2=CLEARED
    int natStatus; // 0=UNKNOWN, 1=PENDING, 2=CLEARED
    vector<Waypoint> route;
};

struct ProcessedFlightData {
    vector<string> trackIds;
    map<string, vector<AircraftData>> tracks;
};

class CDataBridge {
public:
    static void Start();
    static void Stop();
    static shared_ptr<ProcessedFlightData> GetProcessedData();
    static bool IsConnected() { return lastProcessedData != nullptr; }
    static void SendCommand(string command, string callsign, string value = "");
    static void SendUpdate(string callsign, string field, string value);

private:
    static void UpdateLoop();
    static shared_ptr<ProcessedFlightData> lastProcessedData;
    static mutex dataMutex;
    static int activePort;
    static bool isRunning;
    static void* hInternet;
    static string FetchUrl(string url);
    static string PostUrl(string url, string data);
};
