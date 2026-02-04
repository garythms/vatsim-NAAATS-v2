
#include "pch.h"
#include "WebServer.h"
#include "Logger.h"
#include <iostream>
#include <sstream>

atomic<bool> CWebServer::isRunning = false;
atomic<int> CWebServer::runningPort = 0;
thread CWebServer::serverThread;
string CWebServer::currentData = "[]";
mutex CWebServer::dataMutex;
vector<string> CWebServer::pendingUpdates;
SOCKET CWebServer::listenSocket = INVALID_SOCKET;

// HTML for the FDD interface
const string HTML_PART1 = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>vNAAATS FDD</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { 
            font-family: 'Consolas', 'Courier New', monospace; 
            background-color: #1a1a2e; 
            color: #eee; 
            font-size: 12px;
            height: 100vh;
            display: flex;
            flex-direction: column;
        }
        
        /* Fixed Header */
        .header-container {
            position: sticky;
            top: 0;
            z-index: 100;
            background: #1a1a2e;
            padding: 8px;
            border-bottom: 2px solid #4a5568;
        }
        
        .header-bar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            background: linear-gradient(180deg, #2d3748 0%, #1a202c 100%);
            padding: 8px 12px;
            border-radius: 4px;
            border: 1px solid #4a5568;
            margin-bottom: 8px;
        }
        .header-left { display: flex; align-items: center; gap: 12px; }
        .header-title { font-size: 14px; font-weight: bold; color: #fff; }
        .tmi-display { 
            background: #1a4731; 
            padding: 4px 10px; 
            border-radius: 3px; 
            color: #68d391; 
            font-weight: bold;
            border: 1px solid #68d391;
            font-size: 13px;
        }
        .time-display { color: #a0aec0; font-size: 13px; font-weight: bold; }
        
        /* Buttons */
        .btn {
            padding: 5px 12px;
            border: 1px solid #555;
            border-radius: 3px;
            cursor: pointer;
            font-weight: bold;
            font-size: 11px;
            transition: all 0.15s;
        }
        .btn-track { background: #2b6cb0; color: white; border-color: #3182ce; }
        .btn-track:hover { background: #3182ce; }
        .btn-selcal { background: #6b46c1; color: white; border-color: #805ad5; }
        .btn-selcal:hover { background: #805ad5; }
        .btn-cpdlc { background: #dd6b20; color: white; border-color: #ed8936; }
        .btn-cpdlc:hover { background: #ed8936; }
        .btn-find { background: #2d3748; color: white; border-color: #4a5568; }
        .btn-find:hover { background: #4a5568; }
        .btn:disabled { background: #4a5568; cursor: not-allowed; opacity: 0.5; border-color: #333; }
        
        .header-right { display: flex; align-items: center; gap: 8px; }
        .status { font-size: 11px; padding: 3px 8px; border-radius: 3px; }
        .status.connected { color: #68d391; background: #1a4731; }
        .status.disconnected { color: #fc8181; background: #4a1a1a; }
        
        /* Search Box */
        .search-container { display: none; margin-bottom: 8px; }
        .search-container.active { display: flex; gap: 8px; align-items: center; }
        .search-input {
            padding: 5px 10px;
            border: 1px solid #4a5568;
            border-radius: 3px;
            background: #2d3748;
            color: #fff;
            font-size: 12px;
            width: 150px;
        }
        .search-input:focus { outline: none; border-color: #68d391; }
        
        /* Scrollable Content */
        .content-container {
            flex: 1;
            overflow-y: auto;
            padding: 0 8px 8px 8px;
        }
        
        /* Track Groups */
        .track-group { margin-bottom: 12px; }
        
        .track-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 5px 10px;
            font-weight: bold;
            font-size: 12px;
            border-radius: 3px 3px 0 0;
            position: sticky;
            top: 0;
            z-index: 10;
        }
        .track-header.westbound { background: linear-gradient(90deg, #1e4e8c 0%, #2b6cb0 100%); color: white; }
        .track-header.eastbound { background: linear-gradient(90deg, #92600a 0%, #b7791f 100%); color: white; }
        
        /* Flight Strips */
        .strip-container { 
            border: 1px solid #4a5568; 
            border-top: none;
            border-radius: 0 0 3px 3px;
        }
        
        .flight-strip {
            display: grid;
            grid-template-columns: 80px 55px 80px 45px 45px 55px 1fr;
            gap: 4px;
            padding: 4px 8px;
            border-bottom: 1px solid #3a3a4a;
            cursor: pointer;
            transition: filter 0.1s;
            align-items: center;
            min-height: 28px;
        }
        .flight-strip:last-child { border-bottom: none; }
        .flight-strip:hover { filter: brightness(1.1); }
        .flight-strip.selected { outline: 2px solid #68d391; outline-offset: -2px; }
        
        .flight-strip.westbound { background: #b4d5f5; color: #1a365d; }
        .flight-strip.westbound:nth-child(even) { background: #9ec5e8; }
        .flight-strip.eastbound { background: #fef3c7; color: #744210; }
        .flight-strip.eastbound:nth-child(even) { background: #fde68a; }
        
        .cell { 
            overflow: hidden; 
            white-space: nowrap;
            text-overflow: ellipsis;
        }
        .cell-callsign { font-weight: bold; }
        
        /* Editable cells */
        .cell-edit {
            background: #2a2a3a;
            color: #fff;
            border: 1px solid #555;
            border-radius: 2px;
            padding: 2px 4px;
            text-align: center;
            cursor: pointer;
            font-weight: bold;
            font-size: 11px;
        }
        .cell-edit:hover { background: #3a3a4a; border-color: #777; }
        .cell-edit.selcal { background: #4a3a5a; border-color: #6b46c1; }
        .cell-edit.selcal:hover { background: #5a4a6a; }
        
        /* Route display */
        .route-cell {
            display: flex;
            gap: 8px;
            font-size: 10px;
            overflow-x: auto;
        }
        .waypoint {
            display: flex;
            flex-direction: column;
            align-items: center;
            min-width: 45px;
        }
        .wp-name { font-weight: bold; font-size: 11px; }
        .wp-time { opacity: 0.8; }
        
        /* Empty state */
        .empty-state {
            text-align: center;
            padding: 40px;
            color: #718096;
        }
        
        /* Dropdown */
        .dropdown {
            position: fixed;
            background: #2d3748;
            border: 1px solid #4a5568;
            border-radius: 4px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.4);
            max-height: 200px;
            overflow-y: auto;
            z-index: 1000;
            min-width: 60px;
        }
        .dropdown-item {
            padding: 5px 10px;
            cursor: pointer;
            color: #eee;
            font-size: 11px;
            text-align: center;
        }
        .dropdown-item:hover { background: #4a5568; }
    </style>
</head>
<body>
    <div class="header-container">
        <div class="header-bar">
            <div class="header-left">
                <span class="header-title">Flight Data Display</span>
                <span class="tmi-display" id="tmiDisplay">TMI: ---</span>
                <span class="time-display" id="timeDisplay">--:--Z</span>
            </div>
            <div class="header-right">
                <button class="btn btn-find" onclick="toggleSearch()">Find AC</button>
                <button class="btn btn-track" id="btnTrack" onclick="doTrack()" disabled>TRACK</button>
                <button class="btn btn-selcal" id="btnSelcal" onclick="doSelcal()" disabled>SELCAL</button>
                <button class="btn btn-cpdlc" onclick="doCpdlc()">CPDLC</button>
                <span class="status" id="status">...</span>
            </div>
        </div>
        <div class="search-container" id="searchContainer">
            <input type="text" class="search-input" id="searchInput" placeholder="Callsign..." onkeyup="doSearch(event)">
            <button class="btn btn-find" onclick="clearSearch()">Clear</button>
        </div>
    </div>
    <div class="content-container" id="container"></div>
    <div class="dropdown" id="dropdown" style="display:none;"></div>
)HTML";

const string HTML_PART2 = R"HTML(
    <script>
        let currentData = [];
        let selectedCallsign = null;
        let searchFilter = '';

        // Calculate TMI (day of year in Zulu)
        function getTMI() {
            const now = new Date();
            const start = new Date(Date.UTC(now.getUTCFullYear(), 0, 0));
            const diff = now - start;
            const oneDay = 1000 * 60 * 60 * 24;
            const dayOfYear = Math.floor(diff / oneDay);
            return String(dayOfYear).padStart(3, '0');
        }

        function updateTime() {
            const now = new Date();
            const h = String(now.getUTCHours()).padStart(2, '0');
            const m = String(now.getUTCMinutes()).padStart(2, '0');
            document.getElementById('timeDisplay').textContent = h + ':' + m + 'Z';
            document.getElementById('tmiDisplay').textContent = 'TMI: ' + getTMI();
        }
        setInterval(updateTime, 1000);
        updateTime();

        function doCpdlc() {
            sendCommand('SYSTEM', 'OPEN_CPDLC');
        }

        function doTrack() {
            if (selectedCallsign) {
                sendCommand(selectedCallsign, 'TRACK');
            }
        }

        function doSelcal() {
            if (selectedCallsign) {
                sendCommand(selectedCallsign, 'SELCAL');
            }
        }

        function toggleSearch() {
            const container = document.getElementById('searchContainer');
            container.classList.toggle('active');
            if (container.classList.contains('active')) {
                document.getElementById('searchInput').focus();
            }
        }

        function doSearch(event) {
            searchFilter = document.getElementById('searchInput').value.toUpperCase();
            render(currentData);
            if (event.key === 'Enter' && searchFilter) {
                // Select first matching aircraft
                const match = currentData.find(ac => ac.Callsign && ac.Callsign.toUpperCase().includes(searchFilter));
                if (match) selectAircraft(match.Callsign);
            }
        }

        function clearSearch() {
            searchFilter = '';
            document.getElementById('searchInput').value = '';
            render(currentData);
        }

        function selectAircraft(callsign) {
            selectedCallsign = callsign;
            document.querySelectorAll('.flight-strip').forEach(el => el.classList.remove('selected'));
            const strip = document.querySelector('[data-callsign="' + callsign + '"]');
            if (strip) {
                strip.classList.add('selected');
                strip.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
            }
            document.getElementById('btnTrack').disabled = false;
            document.getElementById('btnSelcal').disabled = false;
            sendCommand(callsign, 'SELECT');
        }

        function sendCommand(callsign, command) {
            fetch('/update', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ callsign: callsign, field: 'COMMAND', value: command })
            });
        }

        function sendUpdate(callsign, field, value) {
            fetch('/update', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ callsign: callsign, field: field, value: value })
            }).then(() => fetchData());
        }

        function formatCoord(name) {
            if (!name) return '';
            const m1 = name.match(/^(\d{2})(\d{2})([NS])(\d{3})(\d{2})([EW])$/);
            if (m1) return m1[1] + m1[3] + '/' + parseInt(m1[4]) + m1[6];
            const m2 = name.match(/^(\d{2})([NS])(\d{3})([EW])$/);
            if (m2) return m2[1] + m2[2] + '/' + parseInt(m2[3]) + m2[4];
            return name;
        }

        async function fetchData() {
            try {
                const response = await fetch('/data');
                const data = await response.json();
                currentData = Array.isArray(data) ? data : [];
                render(currentData);
                document.getElementById('status').textContent = 'Connected';
                document.getElementById('status').className = 'status connected';
            } catch (e) {
                document.getElementById('status').textContent = 'Disconnected';
                document.getElementById('status').className = 'status disconnected';
            }
        }

        function showDropdown(element, callsign, field, options) {
            const dropdown = document.getElementById('dropdown');
            const rect = element.getBoundingClientRect();
            dropdown.style.left = rect.left + 'px';
            dropdown.style.top = rect.bottom + 'px';
            dropdown.style.display = 'block';
            
            dropdown.innerHTML = options.map(opt => 
                '<div class="dropdown-item" onclick="selectOption(\'' + callsign + '\',\'' + field + '\',\'' + opt + '\')">' + opt + '</div>'
            ).join('');
            
            setTimeout(() => document.addEventListener('click', hideDropdown, { once: true }), 10);
        }

        function hideDropdown() {
            document.getElementById('dropdown').style.display = 'none';
        }

        function selectOption(callsign, field, value) {
            hideDropdown();
            sendUpdate(callsign, field, value);
        }

        function showFLDropdown(el, callsign) {
            const opts = [];
            for (let fl = 450; fl >= 280; fl -= 10) opts.push(String(fl));
            showDropdown(el, callsign, 'FlightLevel', opts);
        }

        function showMachDropdown(el, callsign) {
            const opts = [];
            for (let m = 90; m >= 74; m--) opts.push('0' + m);
            showDropdown(el, callsign, 'Mach', opts);
        }

        function editSelcal(el, callsign, current) {
            const newVal = prompt('Enter SELCAL code:', current || '');
            if (newVal !== null) {
                sendUpdate(callsign, 'SELCAL', newVal.toUpperCase());
            }
        }

        function render(data) {
            if (!Array.isArray(data) || data.length === 0) {
                document.getElementById('container').innerHTML = '<div class="empty-state">No aircraft data available</div>';
                return;
            }

            // Filter by search
            let filtered = data;
            if (searchFilter) {
                filtered = data.filter(ac => ac.Callsign && ac.Callsign.toUpperCase().includes(searchFilter));
            }

            if (filtered.length === 0) {
                document.getElementById('container').innerHTML = '<div class="empty-state">No matching aircraft</div>';
                return;
            }

            // Group by Track then Direction
            const groups = {};
            filtered.forEach(ac => {
                const track = ac.Track || 'RR';
                const dir = ac.Direction ? 'west' : 'east';
                const key = track + '_' + dir;
                if (!groups[key]) groups[key] = { track, dir, list: [] };
                groups[key].list.push(ac);
            });

            // Sort keys
            const keys = Object.keys(groups).sort((a, b) => {
                const [ta, da] = a.split('_');
                const [tb, db] = b.split('_');
                if (ta === 'RR' && tb !== 'RR') return 1;
                if (tb === 'RR' && ta !== 'RR') return -1;
                if (ta !== tb) return ta.localeCompare(tb);
                return da === 'west' ? -1 : 1;
            });

            let html = '';
            keys.forEach(key => {
                const g = groups[key];
                const isWest = g.dir === 'west';
                const dirClass = isWest ? 'westbound' : 'eastbound';
                const trackLabel = g.track === 'RR' ? 'Random Routes' : 'Track ' + g.track;
                const dirLabel = isWest ? 'WESTBOUND' : 'EASTBOUND';

                html += '<div class="track-group">';
                html += '<div class="track-header ' + dirClass + '">';
                html += '<span>' + trackLabel + ' [' + dirLabel + ']</span>';
                html += '<span>' + g.list.length + ' aircraft</span>';
                html += '</div>';
                html += '<div class="strip-container">';

                g.list.forEach(ac => {
                    const cs = ac.Callsign || '';
                    const sel = cs === selectedCallsign ? ' selected' : '';
                    let fl = ac.FlightLevel || '';
                    if (parseInt(fl) > 1000) fl = String(Math.floor(parseInt(fl) / 100));
                    let mach = ac.Mach || '';
                    const selcal = ac.SELCAL || 'N/A';
                    const type = ac.Type || '';
                    const dep = ac.Depart || '';
                    const dest = ac.Dest || '';

                    // Route
                    let routeHtml = '';
                    if (ac.RouteDetails && ac.RouteDetails.length > 0) {
                        ac.RouteDetails.forEach(pt => {
                            if (pt && pt.name) {
                                routeHtml += '<div class="waypoint"><span class="wp-name">' + formatCoord(pt.name) + '</span>';
                                routeHtml += '<span class="wp-time">' + (pt.est || '--') + '</span></div>';
                            }
                        });
                    }

                    html += '<div class="flight-strip ' + dirClass + sel + '" data-callsign="' + cs + '" onclick="selectAircraft(\'' + cs + '\')">';
                    html += '<div class="cell cell-callsign">' + cs + '</div>';
                    html += '<div class="cell">' + type + '</div>';
                    html += '<div class="cell">' + dep + '/' + dest + '</div>';
                    html += '<div class="cell-edit" onclick="event.stopPropagation();showFLDropdown(this,\'' + cs + '\')">' + fl + '</div>';
                    html += '<div class="cell-edit" onclick="event.stopPropagation();showMachDropdown(this,\'' + cs + '\')">' + mach + '</div>';
                    html += '<div class="cell-edit selcal" onclick="event.stopPropagation();editSelcal(this,\'' + cs + '\',\'' + selcal + '\')">' + selcal + '</div>';
                    html += '<div class="route-cell">' + routeHtml + '</div>';
                    html += '</div>';
                });

                html += '</div></div>';
            });

            document.getElementById('container').innerHTML = html;

            // Re-select if still exists
            if (selectedCallsign) {
                const strip = document.querySelector('[data-callsign="' + selectedCallsign + '"]');
                if (strip) strip.classList.add('selected');
            }
        }

        setInterval(fetchData, 2000);
        fetchData();
    </script>
</body>
</html>
)HTML";

const string HTML_CONTENT = HTML_PART1 + HTML_PART2;

void CWebServer::Start(int port) {
	if (isRunning) return;
	isRunning = true;
	serverThread = thread(ServerLoop, port);
}

void CWebServer::Stop() {
	isRunning = false;
	if (listenSocket != INVALID_SOCKET) {
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
	}
	if (serverThread.joinable()) {
		serverThread.join();
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

int CWebServer::GetRunningPort() {
	return runningPort;
}

void CWebServer::ServerLoop(int port) {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	int currentPort = port;
	bool bound = false;
	for (int i = 0; i < 10; i++) {
		sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(currentPort);

		if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR) {
			bound = true;
			break;
		}
		currentPort++;
	}

	if (!bound) {
		CLogger::Log(CLogType::ERR, "WebServer bind failed", "CWebServer::ServerLoop");
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		CLogger::Log(CLogType::ERR, "WebServer listen failed", "CWebServer::ServerLoop");
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	CLogger::Log(CLogType::NORM, "WebServer started on port " + to_string(currentPort), "CWebServer::ServerLoop");
	runningPort = currentPort;

	while (isRunning) {
		SOCKET clientSocket = accept(listenSocket, NULL, NULL);
		if (clientSocket != INVALID_SOCKET) {
			thread clientThread(HandleClient, clientSocket);
			clientThread.detach();
		}
	}

	closesocket(listenSocket);
	WSACleanup();
}

void CWebServer::HandleClient(SOCKET clientSocket) {
	char buffer[4096];
	int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
	if (bytesReceived > 0) {
		string request(buffer, bytesReceived);
		
		stringstream ss(request);
		string line;
		getline(ss, line);
		
		stringstream lineSs(line);
		string method, path, version;
		lineSs >> method >> path >> version;

		string response;
		if (path == "/") {
			response = GenerateResponse(HTML_CONTENT, "text/html");
		}
		else if (path == "/data") {
			lock_guard<mutex> lock(dataMutex);
			response = GenerateResponse(currentData, "application/json");
		}
		else if (path == "/update" && method == "POST") {
			size_t bodyPos = request.find("\r\n\r\n");
			if (bodyPos != string::npos) {
				string body = request.substr(bodyPos + 4);
				lock_guard<mutex> lock(dataMutex);
				pendingUpdates.push_back(body);
				response = GenerateResponse("{\"status\":\"ok\"}", "application/json");
			}
			else {
				response = GenerateResponse("{\"status\":\"error\"}", "application/json");
			}
		}
		else {
			response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
		}

		send(clientSocket, response.c_str(), response.length(), 0);
	}
	closesocket(clientSocket);
}

string CWebServer::GenerateResponse(string content, string contentType) {
	return "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: " + contentType + "; charset=utf-8\r\nContent-Length: " + to_string(content.length()) + "\r\n\r\n" + content;
}
