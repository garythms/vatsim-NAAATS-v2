#pragma once
#include <string>
#include <map>

using namespace std;

struct UserConfig {
    string vatsimCID;
    string vatsimPassword;
    string realName;
    string hoppiesCode;
    string euroScopePath;
    string vNaaatsAsrPath;
    string topSkyAsrPath;
    bool isSetupComplete;
};

class CConfigManager {
public:
    static bool LoadConfig();
    static bool SaveConfig();
    static UserConfig& GetConfig();

private:
    static UserConfig config;
    static string GetConfigPath();
};
