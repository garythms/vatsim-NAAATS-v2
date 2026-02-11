#include "DataBridge.h"
#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <thread>
#include <chrono>

#pragma comment(lib, "wininet.lib")

shared_ptr<ProcessedFlightData> CDataBridge::lastProcessedData = nullptr;
mutex CDataBridge::dataMutex;
bool CDataBridge::isRunning = false;
int CDataBridge::activePort = 8080;
void* CDataBridge::hInternet = NULL;

void CDataBridge::Start() {
    if (isRunning) return;
    isRunning = true;

    hInternet = InternetOpenA("OceanicController", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        DWORD timeout = 2000; // 2 seconds
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    }

    thread(&CDataBridge::UpdateLoop).detach();
}

void CDataBridge::Stop() {
    isRunning = false;
    if (hInternet) {
        InternetCloseHandle(hInternet);
        hInternet = NULL;
    }
}

void CDataBridge::UpdateLoop() {
    while (isRunning) {
        // Try current active port
        string url = "http://localhost:" + to_string(activePort) + "/data";
        string jsonStr = FetchUrl(url);
        
        // If failed, try to find the port (8080-8085)
        if (jsonStr.empty()) {
            for (int p = 8080; p <= 8085; p++) {
                if (!isRunning) break;
                string testUrl = "http://localhost:" + to_string(p) + "/data";
                string testJson = FetchUrl(testUrl);
                if (!testJson.empty()) {
                    activePort = p;
                    jsonStr = testJson;
                    break;
                }
            }
        }

        if (!jsonStr.empty()) {
            try {
                json j = json::parse(jsonStr);
                if (j.is_array()) {
                    auto newData = make_shared<ProcessedFlightData>();
                    for (auto& item : j) {
                        if (!item.is_object()) continue;
                        
                        AircraftData ac;
                        ac.callsign = item.value("Callsign", "");
                        ac.type = item.value("Type", "");
                        ac.dep = item.value("Depart", "");
                        ac.dest = item.value("Dest", "");
                        // Force empty scratchpad if the plugin is sending data
                        ac.fl = ""; 
                        ac.mach = "";
                        ac.selcal = item.value("SELCAL", "");
                        ac.trackId = item.value("Track", "");
                        if (ac.trackId.empty()) ac.trackId = "Random Routes";
                        ac.isWestbound = item.value("Direction", false);
                        ac.isASEL = item.value("IsASEL", false);
                        ac.status = item.value("NatStatus", 0);
                        ac.natStatus = item.value("NatStatus", 0); // Both for now
                        
                        if (item.contains("RouteDetails") && item["RouteDetails"].is_array()) {
                            for (auto& pt : item["RouteDetails"]) {
                                Waypoint wp;
                                wp.name = pt.value("name", "");
                                wp.estimate = pt.value("est", "");
                                ac.route.push_back(wp);
                            }
                        }
                        newData->tracks[ac.trackId].push_back(ac);
                    }

                    // Pre-sort track IDs
                    for (auto const& [id, list] : newData->tracks) {
                        newData->trackIds.push_back(id);
                    }
                    sort(newData->trackIds.begin(), newData->trackIds.end(), [](string a, string b) {
                        if (a == "Random Routes") return false;
                        if (b == "Random Routes") return true;
                        return a < b;
                    });

                    lock_guard<mutex> lock(dataMutex);
                    lastProcessedData = newData;
                }
            }
            catch (...) {}
        }

        // Sleep for a bit to prevent 100% CPU and give UI thread room
        this_thread::sleep_for(chrono::milliseconds(1000));
    }
}

shared_ptr<ProcessedFlightData> CDataBridge::GetProcessedData() {
    lock_guard<mutex> lock(dataMutex);
    return lastProcessedData;
}

void CDataBridge::SendCommand(string command, string callsign, string value) {
    thread([command, callsign, value]() {
        json j;
        j["callsign"] = callsign;
        j["field"] = "COMMAND";
        j["value"] = command;
        if (!value.empty()) {
            j["extra_value"] = value; // Add extra_value for commands that need it
        }

        string url = "http://localhost:" + to_string(activePort) + "/update";
        PostUrl(url, j.dump());
    }).detach();
}

void CDataBridge::SendUpdate(string callsign, string field, string value) {
    thread([callsign, field, value]() {
        json j;
        j["callsign"] = callsign;
        j["field"] = field;
        j["value"] = value;

        string url = "http://localhost:" + to_string(activePort) + "/update";
        PostUrl(url, j.dump());
    }).detach();
}

string CDataBridge::FetchUrl(string url) {
    if (!hInternet) return "";

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) return "";

    string result;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    InternetCloseHandle(hConnect);
    return result;
}

string CDataBridge::PostUrl(string url, string data) {
    if (!hInternet) return "";

    // Parse URL to get host and path
    char host[256], path[256];
    URL_COMPONENTSA urlComp = { sizeof(urlComp) };
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);

    if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp)) return "";

    HINTERNET hConnect = InternetConnectA(hInternet, host, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) return "";

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        return "";
    }

    string headers = "Content-Type: application/json\r\n";
    HttpSendRequestA(hRequest, headers.c_str(), headers.length(), (LPVOID)data.c_str(), data.length());

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    return "";
}
