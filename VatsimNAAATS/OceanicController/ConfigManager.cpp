#include "ConfigManager.h"
#include <fstream>
#include <windows.h>
#include <shlobj.h>

UserConfig CConfigManager::config = { "", "", "", "", "", "", "", false };

bool CConfigManager::LoadConfig() {
    ifstream file(GetConfigPath());
    if (!file.is_open()) return false;

    string line;
    while (getline(file, line)) {
        size_t pos = line.find('=');
        if (pos == string::npos) continue;

        string key = line.substr(0, pos);
        string val = line.substr(pos + 1);

        if (key == "CID") config.vatsimCID = val;
        else if (key == "PWD") config.vatsimPassword = val;
        else if (key == "Name") config.realName = val;
        else if (key == "Hoppies") config.hoppiesCode = val;
        else if (key == "ESPath") config.euroScopePath = val;
        else if (key == "vNAAATSAsr") config.vNaaatsAsrPath = val;
        else if (key == "TopSkyAsr") config.topSkyAsrPath = val;
        else if (key == "SetupDone") config.isSetupComplete = (val == "1");
    }
    return true;
}

bool CConfigManager::SaveConfig() {
    ofstream file(GetConfigPath());
    if (!file.is_open()) return false;

    file << "CID=" << config.vatsimCID << endl;
    file << "PWD=" << config.vatsimPassword << endl;
    file << "Name=" << config.realName << endl;
    file << "Hoppies=" << config.hoppiesCode << endl;
    file << "ESPath=" << config.euroScopePath << endl;
    file << "vNAAATSAsr=" << config.vNaaatsAsrPath << endl;
    file << "TopSkyAsr=" << config.topSkyAsrPath << endl;
    file << "SetupDone=" << (config.isSetupComplete ? "1" : "0") << endl;

    return true;
}

UserConfig& CConfigManager::GetConfig() {
    return config;
}

string CConfigManager::GetConfigPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        string dir = string(path) + "\\OceanicController";
        CreateDirectoryA(dir.c_str(), NULL);
        return dir + "\\config.ini";
    }
    return "config.ini";
}
