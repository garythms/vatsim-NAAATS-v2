#pragma once
#include "pch.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

using namespace std;

class CWebServer {
public:
	static void Start(int port);
	static void Stop();
	static void SetData(string jsonData);
	static bool HasPendingUpdates();
	static string GetPendingUpdates();

private:
	static void ServerLoop(int port);
	static void HandleClient(SOCKET clientSocket);
	static string GenerateResponse(string content, string contentType = "application/json");

	static atomic<bool> isRunning;
	static thread serverThread;
	static string currentData;
	static mutex dataMutex;
	static vector<string> pendingUpdates; // Stores updates from client to be processed by DLL
	static SOCKET listenSocket;
};
