#pragma once
#include <string>

using namespace std;

enum class EControllerVersion {
    VNAAATS,
    TOPSKY
};

class CLauncher {
public:
    static bool LaunchEuroScope(EControllerVersion version);
private:
    static string GetProfilePath(EControllerVersion version);
    static void InjectCredentials(string asrPath);
    static void UpdateTopSkyHoppies(string esPath, string hoppiesCode);
};
