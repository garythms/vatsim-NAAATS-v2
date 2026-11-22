#pragma once
#include "pch.h"
#include <string>

// Discord Rich Presence: the "Playing vNAAATS" card on the controller's own Discord profile.
// It talks to the Discord app on this computer through its local named pipe (discord-ipc-0..9); nothing is sent to any other server.
// A small worker thread does the talking so EuroScope never waits for Discord. If Discord is not running nothing happens.
class CDiscordPresence
{
	public:
		// Set what the card shows (call as often as you like; it is only sent when it changed or every 15 seconds).
		static void Set(const std::string& details, const std::string& state, long long startUnix);
		// Take the card down (not connected as a controller, or switched off).
		static void Clear();
		// Stop the worker thread (plugin unloaded).
		static void Stop();
};
