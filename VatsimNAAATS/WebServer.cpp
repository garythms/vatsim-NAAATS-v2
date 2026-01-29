
#include "pch.h"
#include "WebServer.h"
#include "Logger.h"
#include <iostream>
#include <sstream>

atomic<bool> CWebServer::isRunning = false;
thread CWebServer::serverThread;
string CWebServer::currentData = "[]";
mutex CWebServer::dataMutex;
vector<string> CWebServer::pendingUpdates;
SOCKET CWebServer::listenSocket = INVALID_SOCKET;

// Simple HTML for the FDD interface
const string HTML_CONTENT = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>vNAAATS FDD</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; background-color: #1e1e1e; color: #c5c5c5; margin: 0; padding: 20px; }
        
        /* Tailwind-ish Colors */
        .bg-blue-10 { background-color: #b4e5ff; }
        .bg-blue-9 { background-color: #93B5DE; }
        .bg-yellow-100 { background-color: #fef3c7; }
        .bg-yellow-200 { background-color: #fde68a; }
        .bg-grey-500 { background-color: #6b7280; }
        
        .text-black { color: #000; }
        .text-white { color: #fff; }

        /* Layouts */
        .track-group { margin-bottom: 20px; }
        .track-header { 
            background-color: rgba(107, 114, 128, 0.7); /* grey-500 opacity 70 */
            color: white;
            padding: 5px 10px;
            font-weight: bold;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .header-btn {
            background-color: #3b82f6;
            color: white;
            border: none;
            padding: 4px 8px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
            margin-left: 10px;
        }
        .header-btn:hover { background-color: #2563eb; }

        .flight-strip {
            display: flex;
            flex-direction: column;
            margin-bottom: 2px;
            box-shadow: 0 1px 2px rgba(0,0,0,0.2);
            border-left: 5px solid transparent;
        }

        /* Top Row (Route/Coords) - Mainly for RR */
        .strip-top {
            height: 4rem; /* h-16 */
            display: flex;
            align-items: center;
            font-size: 0.9em;
        }

        /* Main Row */
        .strip-main {
            height: 2rem; /* h-8 */
            display: flex;
            align-items: center;
            font-weight: bold;
            font-size: 1.1em;
        }

        .cell { padding: 0 5px; overflow: hidden; white-space: nowrap; }
        
        /* Width utilities (approximate percentages from Svelte) */
        .w-callsign { width: 14%; } /* w-1/7 */
        .w-route-block { width: 71%; display: flex; } /* w-5/7 */
        .w-end-block { width: 15%; display: flex; justify-content: flex-end; } /* w-1/5 */

        .w-dep-dest { width: 14%; }
        .w-level { width: 7%; } /* Increased width */
        .w-mach { width: 9%; }  /* Increased width */
        .w-selcal { width: 6%; }
        /* Remaining space in w-route-block for waypoints: 100% - (14+7+9+6)% = 64% */
        .w-waypoints-container { width: 64%; display: flex; }

        .w-eta { flex: 1; text-align: center; }
        .coord-stack { flex: 1; display: flex; flex-direction: column; text-align: center; font-size: 0.9em; font-weight: bold; }
        .coord-label { font-size: 1.1em; color: #fff; }

        .editable { 
            cursor: pointer; 
            border: 1px solid #555; 
            background-color: #2a2a2a; 
            color: #ffffff; /* White text */
            font-weight: bold;
            margin: 0 2px;
            border-radius: 3px;
            text-align: center;
        }
        .editable:hover { background-color: #3a3a3a; border-color: #777; }
        
        .editing-input {
            width: 100%;
            height: 100%;
            box-sizing: border-box;
            color: #000000; /* Black text for input to be readable against white bg default or set explicit bg */
            background-color: #ffffff;
            font-weight: bold;
            border: none;
            text-align: center;
        }

    </style>
</head>
<body>
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom: 20px;">
        <div style="display:flex; align-items:center;">
            <h2 style="margin:0; color:white;">Flight Data Display</h2>
            <button class="header-btn" onclick="openCpdlc()">CPDLC</button>
        </div>
        <div id="status" style="color: #888; font-size: 0.9em;">Connecting...</div>
    </div>
    <div id="container"></div>

    <script>
        let currentData = [];
        let isEditing = false; // Flag to pause updates

        async function openCpdlc() {
            await save('SYSTEM', 'COMMAND', 'OPEN_CPDLC');
        }

        async function fetchData() {
            if (isEditing) return; // Don't refresh while editing
            try {
                const response = await fetch('/data');
                const data = await response.json();
                render(data);
                document.getElementById('status').innerText = 'Connected';
                document.getElementById('status').style.color = '#4caf50';
            } catch (e) {
                document.getElementById('status').innerText = 'Disconnected';
                document.getElementById('status').style.color = '#f44336';
            }
        }

        function formatCoordinate(name, lon) {
            // Check if name is like 50N050W
            const regex = /^(\d{2})[NS](\d{3})[EW]$/;
            const match = name.match(regex);
            if (match) {
                // e.g. 50, 050 -> 50/50
                let l = match[1];
                let ln = match[2];
                if (ln.length === 3 && ln.startsWith('0')) ln = ln.substring(1); // 050 -> 50
                if (ln.length === 3 && ln.startsWith('0')) ln = ln.substring(1); // 005 -> 5? No, usually 50/50 is degrees.
                return `${l}/${ln}`;
            }
            return name;
        }

        function getControllerDisplay(callsign) {
            if (!callsign) return '';
            const c = callsign.toUpperCase();
            if (c.startsWith('CZQX')) return 'GAN';
            if (c.startsWith('EGGX')) return 'SWK';
            if (c.startsWith('NAT')) return 'NAT';
            return c; 
        }

        function render(data) {
            // if (JSON.stringify(data) === JSON.stringify(currentData)) return; // Force update for now to be safe
            currentData = data;
            
            // Group by Track
            const tracks = {};
            data.forEach(ac => {
                const trk = ac.Track || 'RR'; // Default to RR if empty
                if (!tracks[trk]) tracks[trk] = [];
                tracks[trk].push(ac);
            });

            // Sort Tracks (Alphabetical, but RR last?)
            const trackKeys = Object.keys(tracks).sort((a, b) => {
                if (a === 'RR') return 1;
                if (b === 'RR') return -1;
                return a.localeCompare(b);
            });

            const container = document.getElementById('container');
            container.innerHTML = '';

            trackKeys.forEach(trk => {
                const trackGroup = document.createElement('div');
                trackGroup.className = 'track-group';
                
                // Header
                trackGroup.innerHTML = `
                    <div class="track-header">
                        ${trk === 'RR' ? 'RANDOM ROUTINGS' : 'TRACK ' + trk}
                    </div>
                `;

                // Strips
                tracks[trk].forEach(ac => {
                    const isWest = ac.Direction === true || ac.Direction === 'true';
                    const mainBg = isWest ? 'bg-blue-10' : 'bg-yellow-100';
                    const topBg = isWest ? 'bg-blue-9' : 'bg-yellow-200';
                    
                    // Track Status / Controller logic
                    // If not tracked by me, maybe dim? 
                    // User asked to identify NAT track status unless tracked by controller.
                    // If tracked by someone else, show their ID.
                    
                    let trackedByDisplay = '';
                    let ctrlName = getControllerDisplay(ac.TrackedBy);

                    if (!ac.IsTrackedByMe) {
                        // NAT Status Logic
                        let natStatusHtml = '';
                        if (ac.NatStatus === 1) natStatusHtml = '<span style="color:orange; font-weight:bold; margin-right:5px;">PENDING</span>';
                        else if (ac.NatStatus === 2) natStatusHtml = '<span style="color:#4caf50; font-weight:bold; margin-right:5px;">CLEARED</span>';
                        
                        if (ac.TrackedBy) {
                            trackedByDisplay = `${natStatusHtml}<span style="color:darkblue">${ctrlName}</span>`;
                        } else {
                            trackedByDisplay = natStatusHtml;
                        }
                    } else {
                        trackedByDisplay = `<b>${ctrlName}</b>`;
                    }

                    const strip = document.createElement('div');
                    strip.className = 'flight-strip';
                    // Add border if cleared?
                    if (ac.IsCleared) {
                        strip.style.borderLeftColor = '#4caf50'; // Green indicator for cleared
                    }

                    // Route Points (Top Row)
                    let topRowHtml = '';
                    if (trk === 'RR' || true) { 
                        let pointsHtml = '';
                        if (ac.RouteDetails) {
                            ac.RouteDetails.forEach(pt => {
                                let lat = Math.round(pt.lat); 
                                let lon = Math.round(pt.lon);
                                let label = formatCoordinate(pt.name, lon);
                                let subLabel = `${Math.abs(lon)}${lon>=0?'E':'W'}`;
                                
                                pointsHtml += `
                                    <div class="coord-stack">
                                        <div class="coord-label">${label}</div>
                                        <div>${subLabel}</div>
                                    </div>
                                `;
                            });
                        }
                        topRowHtml = `
                            <div class="strip-top ${topBg} ${textColor}">
                                <div style="width: 14%;"></div> <!-- Callsign Spacer -->
                                <div class="w-route-block">
                                    <div style="width: 36%;"></div> <!-- Fixed Fields Spacer (14+7+9+6) -->
                                    <div class="w-waypoints-container">
                                        ${pointsHtml}
                                    </div>
                                </div>
                            </div>
                        `;
                    }

                    // Main Row
                    let etasHtml = '';
                    if (ac.RouteDetails) {
                        ac.RouteDetails.forEach(pt => {
                            etasHtml += `<div class="w-eta">${pt.est.substring(0,4)}</div>`;
                        });
                    }

                    strip.innerHTML = `
                        ${topRowHtml}
                        <div class="strip-main ${mainBg} ${textColor}">
                            <div class="cell w-callsign">${ac.Callsign}${ac.IsEquipped ? '' : '*'}</div>
                            <div class="w-route-block">
                                <div class="cell w-dep-dest">${ac.Depart}/${ac.Dest}</div>
                                <div class="cell w-level editable" onclick="edit(this, '${ac.Callsign}', 'FlightLevel', '${ac.FlightLevel}', 'select-fl')">${ac.FlightLevel}</div>
                                <div class="cell w-mach editable" onclick="edit(this, '${ac.Callsign}', 'Mach', '${ac.Mach}', 'select-mach')">M${ac.Mach}</div>
                                <div class="cell w-selcal editable" onclick="edit(this, '${ac.Callsign}', 'SELCAL', '${ac.SELCAL}', 'text')">${ac.SELCAL}</div>
                                <div class="w-waypoints-container">
                                    ${etasHtml}
                                </div>
                            </div>
                            <div class="w-end-block">
                                <div class="cell" style="width: 40%;">${ac.Type}</div>
                                <div class="cell" style="width: 60%; text-align:right; font-size:0.8em;">${trackedByDisplay}</div>
                            </div>
                        </div>
                    `;
                    trackGroup.appendChild(strip);
                });

                container.appendChild(trackGroup);
            });
        }

        function edit(cell, callsign, field, value, type) {
            if (cell.querySelector('input') || cell.querySelector('select')) return;
            
            isEditing = true; // Pause updates
            
            let input;
            if (type === 'select-fl') {
                input = document.createElement('select');
                input.className = 'editing-input';
                // Generate FL options 200-600
                for (let i = 200; i <= 600; i += 10) {
                    const opt = document.createElement('option');
                    opt.value = i.toString();
                    opt.text = "FL" + i;
                    if (i.toString() === value || (value === "" && i === 380)) opt.selected = true;
                    input.appendChild(opt);
                }
                if (!input.value) input.value = "380"; // Default if no match
            } else if (type === 'select-mach') {
                input = document.createElement('select');
                input.className = 'editing-input';
                // Generate Mach options .70 to .99, then 1.0+
                for (let i = 70; i <= 99; i++) {
                    const val = i.toString(); // "70"
                    const opt = document.createElement('option');
                    opt.value = val;
                    opt.text = "M0." + i;
                    if (val === value) opt.selected = true;
                    input.appendChild(opt);
                }
                // Supersonic
                const superSonic = [100, 200];
                superSonic.forEach(s => {
                     const val = s.toString();
                     const opt = document.createElement('option');
                     opt.value = val;
                     opt.text = "M" + (s/100).toFixed(1) + "+";
                     if (val === value) opt.selected = true;
                     input.appendChild(opt);
                });
            } else {
                input = document.createElement('input');
                input.className = 'editing-input';
                input.value = value;
            }

            // Save on blur or change (for select)
            input.onblur = () => save(callsign, field, input.value);
            input.onkeydown = (e) => { if(e.key === 'Enter') input.blur(); };
            if (type.startsWith('select')) {
                input.onchange = () => input.blur(); // Save immediately on selection
            }

            cell.innerText = ''; 
            cell.appendChild(input);
            input.focus();
        }

        async function save(callsign, field, value) {
            const update = { callsign, field, value };
            await fetch('/update', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(update)
            });
            isEditing = false; // Resume updates
            fetchData();
        }

        setInterval(fetchData, 1000);
        fetchData();
    </script>
</body>
</html>
)HTML";

void CWebServer::Start(int port) {
	if (isRunning) return;
	isRunning = true;
	serverThread = thread(ServerLoop, port);
	serverThread.detach();
}

void CWebServer::Stop() {
	isRunning = false;
	if (listenSocket != INVALID_SOCKET) {
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
	}
}

void CWebServer::SetData(string jsonData) {
	lock_guard<mutex> lock(dataMutex);
	currentData = jsonData;
}

bool CWebServer::HasPendingUpdates() {
	lock_guard<mutex> lock(dataMutex);
	return !pendingUpdates.empty();
}

string CWebServer::GetPendingUpdates() {
	lock_guard<mutex> lock(dataMutex);
	if (pendingUpdates.empty()) return "";
	string update = pendingUpdates.front();
	pendingUpdates.erase(pendingUpdates.begin());
	return update;
}

void CWebServer::ServerLoop(int port) {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);

	if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		CLogger::Log(CLogType::ERR, "WebServer bind failed. Port: " + to_string(port), "CWebServer::ServerLoop");
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		CLogger::Log(CLogType::ERR, "WebServer listen failed.", "CWebServer::ServerLoop");
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	CLogger::Log(CLogType::NORM, "WebServer started on port " + to_string(port), "CWebServer::ServerLoop");

	while (isRunning) {
		SOCKET clientSocket = accept(listenSocket, NULL, NULL);
		if (clientSocket != INVALID_SOCKET) {
			thread t(HandleClient, clientSocket);
			t.detach();
		}
		else {
			if (!isRunning) break;
			int err = WSAGetLastError();
			if (err != WSAEINTR) {
				CLogger::Log(CLogType::ERR, "WebServer accept failed. Error: " + to_string(err), "CWebServer::ServerLoop");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	if (listenSocket != INVALID_SOCKET) {
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
	}
	WSACleanup();
}

void CWebServer::HandleClient(SOCKET clientSocket) {
	try {
		// Set receive timeout to 3 seconds to prevent hanging
		DWORD timeout = 3000;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

		char buffer[4096];
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived > 0) {
			buffer[bytesReceived] = 0;
			string request(buffer);
			stringstream ss(request);
			string method, path;
			ss >> method >> path;

			// Debug log for data requests (only log once per 10s to avoid spam if needed, or just log)
			// CLogger::Log(CLogType::INFO, "WebServer request: " + method + " " + path, "CWebServer::HandleClient");

			if (method == "GET") {
				if (path == "/data") {
					string data;
					{
						lock_guard<mutex> lock(dataMutex);
						data = currentData;
					}
					// Log if data is empty, might explain "no data" issue
					if (data.empty() || data == "[]") {
						// CLogger::Log(CLogType::WARN, "WebServer sending empty data.", "CWebServer::HandleClient");
					}
					string response = GenerateResponse(data);
					send(clientSocket, response.c_str(), response.length(), 0);
				}
				else {
					string response = GenerateResponse(HTML_CONTENT, "text/html");
					send(clientSocket, response.c_str(), response.length(), 0);
				}
			}
			else if (method == "POST" && path == "/update") {
				// Find body (after double CRLF)
				size_t bodyPos = request.find("\r\n\r\n");
				if (bodyPos != string::npos) {
					string body = request.substr(bodyPos + 4);
					
					// Basic parsing of JSON body to pendingUpdates
					// Assuming body is valid JSON like {"callsign": "...", "field": "...", "value": "..."}
					{
						lock_guard<mutex> lock(dataMutex);
						pendingUpdates.push_back(body);
					}
					
					string response = GenerateResponse("{\"status\":\"ok\"}", "application/json");
					send(clientSocket, response.c_str(), response.length(), 0);
				}
			}
		}

		closesocket(clientSocket);
	}
	catch (exception& ex) {
		CLogger::Log(CLogType::ERR, "WebServer client handling failed: " + string(ex.what()), "CWebServer::HandleClient");
		closesocket(clientSocket);
	}
	catch (...) {
		CLogger::Log(CLogType::ERR, "WebServer client handling failed with unknown error.", "CWebServer::HandleClient");
		closesocket(clientSocket);
	}
}

string CWebServer::GenerateResponse(string content, string contentType) {
	stringstream ss;
	ss << "HTTP/1.1 200 OK\r\n";
	ss << "Content-Type: " << contentType << "\r\n";
	ss << "Content-Length: " << content.length() << "\r\n";
	ss << "Connection: close\r\n";
	ss << "Access-Control-Allow-Origin: *\r\n";
	ss << "\r\n";
	ss << content;
	return ss.str();
}
