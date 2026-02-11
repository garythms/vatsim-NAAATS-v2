#include "Launcher.h"
#include "ConfigManager.h"
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <vector>

bool CLauncher::LaunchEuroScope(EControllerVersion version) {
    UserConfig& config = CConfigManager::GetConfig();
    
    // 1. Inject credentials into .asr
    if (version == EControllerVersion::VNAAATS) {
        InjectCredentials(config.vNaaatsAsrPath);
    } else {
        InjectCredentials(config.topSkyAsrPath);
        // 2. Update TopSky Hoppies file
        UpdateTopSkyHoppies(config.euroScopePath, config.hoppiesCode);
    }

    // 3. Launch EuroScope
    string args = " -p \"" + GetProfilePath(version) + "\"";
    HINSTANCE result = ShellExecute(NULL, "open", config.euroScopePath.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
    
    return ((INT_PTR)result > 32);
}

void CLauncher::InjectCredentials(string asrPath) {
    UserConfig& config = CConfigManager::GetConfig();
    ifstream fileIn(asrPath);
    if (!fileIn.is_open()) return;

    vector<string> lines;
    string line;
    bool inLastSession = false;

    while (getline(fileIn, line)) {
        if (line.find("LastSession") != string::npos) {
            inLastSession = true;
        } else if (inLastSession) {
            if (line.find("realname") != string::npos) {
                line = "LastSession\trealname\t" + config.realName;
            } else if (line.find("certificate") != string::npos) {
                line = "LastSession\tcertificate\t" + config.vatsimCID;
            } else if (line.find("password") != string::npos) {
                line = "LastSession\tpassword\t" + config.vatsimPassword;
            } else if (line.empty() || line[0] != 'L') { // End of LastSession block
                inLastSession = false;
            }
        }
        lines.push_back(line);
    }
    fileIn.close();

    ofstream fileOut(asrPath);
    for (const auto& l : lines) {
        fileOut << l << endl;
    }
}

void CLauncher::UpdateTopSkyHoppies(string esPath, string hoppiesCode) {
    // Path: EuroScope\Oceanic Controller\Plugins\TopSky\TopSkyCPDLChoppieCode.txt
    // We assume esPath points to EuroScope.exe, so we get the directory
    size_t lastSlash = esPath.find_last_of("\\/");
    if (lastSlash == string::npos) return;

    string esDir = esPath.substr(0, lastSlash);
    string hoppiesPath = esDir + "\\Oceanic Controller\\Plugins\\TopSky\\TopSkyCPDLChoppieCode.txt";

    ofstream file(hoppiesPath);
    if (file.is_open()) {
        file << hoppiesCode;
    }
}

string CLauncher::GetProfilePath(EControllerVersion version) {
    if (version == EControllerVersion::TOPSKY) {
        return "Profiles\\TopSky_Oceanic.prf"; 
    } else {
        return "Profiles\\vNAAATS_Oceanic.prf";
    }
}

