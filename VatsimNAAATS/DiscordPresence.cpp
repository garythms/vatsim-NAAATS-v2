#include "pch.h"
#include "DiscordPresence.h"
#include "Constants.h"
#include <process.h>
#include <json.hpp>

using namespace std;

namespace {
	CRITICAL_SECTION cs;
	bool csReady = false;
	string wantDetails, wantState;
	long long wantStart = 0;
	bool want = false;          // true: show the card; false: no card
	bool dirty = false;         // what is wanted changed since the last send
	volatile long stopFlag = 0;
	volatile long running = 0;

	void Ensure() {
		if (!csReady) { InitializeCriticalSection(&cs); csReady = true; }
	}

	// One frame of Discord's local protocol: 4 bytes opcode, 4 bytes length, then JSON
	bool WriteFrame(HANDLE pipe, unsigned int op, const string& json) {
		string buf(8, '\0');
		unsigned int len = (unsigned int)json.size();
		memcpy(&buf[0], &op, 4);
		memcpy(&buf[4], &len, 4);
		buf += json;
		DWORD done = 0;
		return WriteFile(pipe, buf.data(), (DWORD)buf.size(), &done, NULL) && done == buf.size();
	}

	bool WaitBytes(HANDLE pipe, DWORD need, DWORD timeoutMs) {
		DWORD waited = 0;
		for (;;) {
			DWORD avail = 0;
			if (!PeekNamedPipe(pipe, NULL, 0, NULL, &avail, NULL)) return false;     // the pipe is gone
			if (avail >= need) return true;
			if (waited >= timeoutMs) return false;
			Sleep(20); waited += 20;
		}
	}

	// Read one frame if there is one (waiting up to timeoutMs for it). Returns false when there is none or the pipe broke.
	bool ReadFrame(HANDLE pipe, unsigned int& op, string& body, DWORD timeoutMs) {
		if (!WaitBytes(pipe, 8, timeoutMs)) return false;
		unsigned char head[8]; DWORD got = 0;
		if (!ReadFile(pipe, head, 8, &got, NULL) || got != 8) return false;
		unsigned int len = 0;
		memcpy(&op, head, 4); memcpy(&len, head + 4, 4);
		if (len > 65536) return false;
		body.assign(len, '\0');
		if (len == 0) return true;
		if (!WaitBytes(pipe, len, 1000)) return false;
		return ReadFile(pipe, &body[0], len, &got, NULL) && got == len;
	}

	// Throw away anything Discord has sent back (answers to our updates); answer its pings. False when the pipe broke or Discord closed it.
	bool Drain(HANDLE pipe) {
		for (;;) {
			DWORD avail = 0;
			if (!PeekNamedPipe(pipe, NULL, 0, NULL, &avail, NULL)) return false;
			if (avail < 8) return true;
			unsigned int op = 0; string body;
			if (!ReadFrame(pipe, op, body, 500)) return false;
			if (op == 2) return false;                          // close
			if (op == 3) WriteFrame(pipe, 4, body);             // ping -> pong
		}
	}

	HANDLE Connect() {
		for (int i = 0; i < 10; i++) {
			string name = "\\\\.\\pipe\\discord-ipc-" + to_string(i);
			HANDLE p = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (p == INVALID_HANDLE_VALUE) continue;
			nlohmann::json hs = { {"v", 1}, {"client_id", string(DISCORD_APP_ID)} };
			unsigned int op = 0; string body;
			if (WriteFrame(p, 0, hs.dump()) && ReadFrame(p, op, body, 3000) && op == 1) return p;     // the READY answer
			CloseHandle(p);
		}
		return INVALID_HANDLE_VALUE;
	}

	bool SendActivity(HANDLE pipe, bool show, const string& details, const string& state, long long start) {
		static long nonce = 0;
		nlohmann::json args = { {"pid", (int)GetCurrentProcessId()} };
		if (show) {
			nlohmann::json act = { {"details", details}, {"state", state} };
			if (start > 0) act["timestamps"] = { {"start", start} };
			act["buttons"] = nlohmann::json::array({
				{ {"label", "Open the Flight Data Display"}, {"url", "https://vnaaats.com/fdd"} },
				{ {"label", "vNAAATS"}, {"url", "https://vnaaats.com"} } });
			args["activity"] = act;
		}
		nlohmann::json msg = { {"cmd", "SET_ACTIVITY"}, {"args", args}, {"nonce", to_string(++nonce)} };
		return WriteFrame(pipe, 1, msg.dump());
	}

	void __cdecl Worker(void*) {
		HANDLE pipe = INVALID_HANDLE_VALUE;
		DWORD lastTry = 0;
		bool shown = false;
		while (!InterlockedCompareExchange(&stopFlag, 0, 0)) {
			bool w, d; string det, st; long long start;
			EnterCriticalSection(&cs);
			w = want; d = dirty; det = wantDetails; st = wantState; start = wantStart;
			LeaveCriticalSection(&cs);
			DWORD now = GetTickCount();
			bool due = d || (now - lastTry >= 15000);
			if (pipe != INVALID_HANDLE_VALUE && !Drain(pipe)) { CloseHandle(pipe); pipe = INVALID_HANDLE_VALUE; shown = false; }
			if (due && (w || shown)) {
				lastTry = now;
				if (pipe == INVALID_HANDLE_VALUE) pipe = Connect();       // Discord not running: tried again in 15 seconds
				if (pipe != INVALID_HANDLE_VALUE) {
					if (SendActivity(pipe, w, det, st, start)) {
						shown = w;
						EnterCriticalSection(&cs); if (wantDetails == det && wantState == st) dirty = false; LeaveCriticalSection(&cs);
					}
					else { CloseHandle(pipe); pipe = INVALID_HANDLE_VALUE; shown = false; }
				}
			}
			Sleep(500);
		}
		if (pipe != INVALID_HANDLE_VALUE) {
			if (shown) SendActivity(pipe, false, "", "", 0);
			CloseHandle(pipe);
		}
		InterlockedExchange(&running, 0);
	}
}

void CDiscordPresence::Set(const string& details, const string& state, long long startUnix) {
	if (string(DISCORD_APP_ID).empty()) return;
	Ensure();
	EnterCriticalSection(&cs);
	if (!want || details != wantDetails || state != wantState) dirty = true;
	want = true; wantDetails = details; wantState = state; wantStart = startUnix;
	LeaveCriticalSection(&cs);
	if (!InterlockedCompareExchange(&running, 1, 0)) {
		InterlockedExchange(&stopFlag, 0);
		_beginthread(Worker, 0, NULL);
	}
}

void CDiscordPresence::Clear() {
	if (!csReady) return;
	EnterCriticalSection(&cs);
	if (want) dirty = true;
	want = false;
	LeaveCriticalSection(&cs);
}

void CDiscordPresence::Stop() {
	InterlockedExchange(&stopFlag, 1);
}
