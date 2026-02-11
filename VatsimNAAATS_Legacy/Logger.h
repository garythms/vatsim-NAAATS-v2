#pragma once
#include "EuroScopePlugIn.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <mutex>
#include "Utils.h"

using namespace std;
using namespace EuroScopePlugIn;

class CLogger
{
	public:
		// Log a message
		static void Log(CLogType type, string text, string invokedBy = "");

		// Log to chat
		static void DebugLog(CRadarScreen* screen, string text);

		static void LogAircraftDebugInfo(string text);

		// Generate the log file
		static void InstantiateLogFile();
		
		// Crash handler
		static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionInfo);

	private:
		// The file to log to
		static string logFilePath;
		
		// Mutex for thread safety
		static recursive_mutex logMutex;

		// Get log prefix (ID + date and time)
		static string GeneratePrefix(CLogType type);

		// To append lines after initialisation instead of overwriting
		static bool initialised;
		static bool initialisedAc;
};

