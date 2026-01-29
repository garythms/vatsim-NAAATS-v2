#include "pch.h"
#include "RoutesHelper.h"
#include "DataHandler.h"

map<string, CTrack> CRoutesHelper::CurrentTracks;
mutex CRoutesHelper::TracksMutex;

map<string, CPosition> CRoutesHelper::FixCache;
mutex CRoutesHelper::FixCacheMutex;

string CRoutesHelper::CurrentTMI = "";

vector<string> CRoutesHelper::ActiveRoutes;

void CRoutesHelper::InitialiseFixCache(CRadarScreen* screen) {
	lock_guard<mutex> lock(FixCacheMutex);
	if (!FixCache.empty()) return;

	// Load fixes
	CSectorElement fix;
	for (fix = screen->GetPlugIn()->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_FIX);
		fix.IsValid();
		fix = screen->GetPlugIn()->SectorFileElementSelectNext(fix, EuroScopePlugIn::SECTOR_ELEMENT_FIX)) {
		
		CPosition pos;
		if (fix.GetPosition(&pos, 0)) {
			FixCache[fix.GetName()] = pos;
		}
	}
	// Log count
	// CLogger::Log(CLogType::INFO, "FixCache initialized with " + to_string(FixCache.size()) + " fixes.", "CRoutesHelper::InitialiseFixCache");
}

bool CRoutesHelper::GetRoute(CRadarScreen* screen, vector<CRoutePosition>* routeVector, string callsign, CAircraftFlightPlan* copy) {\
	try {
		// Get the flight plan
		CAircraftFlightPlan* fp = copy != nullptr ? copy : CDataHandler::GetFlightData(callsign);

	
		// Check validity
	if (!fp->IsValid || fp->Route.size() == 0) {
		if (fp->IsValid && fp->Route.empty()) {
			CLogger::Log(CLogType::WARN, "Route vector empty for " + callsign + ". RouteRaw size: " + to_string(fp->RouteRaw.size()), "CRoutesHelper::GetRoute");
		}
		return false;
	}

		// Get target
		CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());

		// Get aircraft direction (default to true/Eastbound if target invalid)
		bool direction = true;
		if (target.IsValid()) {
			direction = CUtils::GetAircraftDirection(target.GetPosition().GetReportedHeadingTrueNorth());
		}
	
		// Loop through each route item
		int totalDistance = 0;

		// Iterate through the already filtered route
		for (int idx = 0; idx < fp->Route.size(); idx++) {
				// Create position	
				CRoutePosition position;

				// Fix
				position.Fix = fp->Route[idx].Name;
				position.PositionRaw = fp->Route[idx].Position;

				// Get estimate
				if (target.IsValid()) {
					if (!direction) { // Westbound
						if (target.GetPosition().GetPosition().m_Longitude > fp->Route[idx].Position.m_Longitude) {
							if (totalDistance == 0) {
								// Calculate distance from aircraft
								totalDistance += target.GetPosition().GetPosition().DistanceTo(fp->Route[idx].Position);
								position.DistanceFromLastPoint = target.GetPosition().GetPosition().DistanceTo(fp->Route[idx].Position);
							}
							else {
								// Calculate distance point to point
								totalDistance += fp->Route.at(idx - 1).Position.DistanceTo(fp->Route.at(idx).Position);
								position.DistanceFromLastPoint = fp->Route.at(idx - 1).Position.DistanceTo(fp->Route.at(idx).Position);
							}
							position.Estimate = CUtils::ParseZuluTime(false, CUtils::GetTimeDistanceSpeed((int)round(totalDistance), target.GetPosition().GetReportedGS()));
						}
						else {
							position.Estimate = "--";
							position.DistanceFromLastPoint = 0;
						}
					}
					else { // Eastbound
						if (target.GetPosition().GetPosition().m_Longitude < fp->Route[idx].Position.m_Longitude) {
							if (totalDistance == 0) {
								// Calculate distance from aircraft
								totalDistance += target.GetPosition().GetPosition().DistanceTo(fp->Route[idx].Position);
								position.DistanceFromLastPoint = target.GetPosition().GetPosition().DistanceTo(fp->Route[idx].Position);
							}
							else {
								// Calculate distance point to point
								totalDistance += fp->Route.at(idx - 1).Position.DistanceTo(fp->Route.at(idx).Position);
								position.DistanceFromLastPoint = fp->Route.at(idx - 1).Position.DistanceTo(fp->Route.at(idx).Position);
							}
							position.Estimate = CUtils::ParseZuluTime(false, CUtils::GetTimeDistanceSpeed((int)round(totalDistance), target.GetPosition().GetReportedGS()));
						}
						else {
							position.Estimate = "--";
							position.DistanceFromLastPoint = 0;
						}
					}

					// Altitude
					position.FlightLevel = target.GetPosition().GetFlightLevel() / 100;
				} else {
					// Target invalid - cannot calculate estimate or distance
					position.Estimate = "--";
					position.DistanceFromLastPoint = 0;
					position.FlightLevel = 0;
				}

				// Add the position
				routeVector->push_back(position);
		}
		}
	catch (std::exception & ex) {
		CLogger::DebugLog(screen, "An exception occurred. " + *ex.what());
		CLogger::Log(CLogType::ERR, "An error occurred. Callsign: " + callsign + "\nVerbose details: " + *ex.what(), "CRoutesHelper::GetRoute");
		return false;
	}

	return true;
}

void CRoutesHelper::InitialiseRoute(void* args) {
	// Convert args
	CUtils::CAsyncData* data = (CUtils::CAsyncData*) args;
	try {
		// Flight plan
		CAircraftFlightPlan* fp = data->FP != nullptr ? data->FP : CDataHandler::GetFlightData(data->Callsign);

		// Use thread-safe data copies if available
		vector<string> routeRaw = data->RouteRaw;
		string trackId = data->Track;

		// Fallback to fp if data copies are empty (legacy behavior protection)
		if (routeRaw.empty() && fp != nullptr) {
			routeRaw = fp->RouteRaw;
		}
		if (trackId.empty() && fp != nullptr) {
			trackId = fp->Track;
		}

		// Try to parse raw route string if routeRaw is still empty
		if (routeRaw.empty() && !data->RawRouteString.empty()) {
			CUtils::StringSplit(data->RawRouteString, ' ', &routeRaw);
		}

		// Final route vector
		vector<CWaypoint> parsedRoute;

		// Check if we have data
		bool hasRouteData = false;
		if (!routeRaw.empty()) hasRouteData = true;
		else if (!data->ExtractedRoute.empty()) hasRouteData = true;

		if (!hasRouteData) {
			CLogger::Log(CLogType::WARN, "Route vector empty for " + data->Callsign + ". RouteRaw size: " + to_string(routeRaw.size()), "CRoutesHelper::InitialiseRoute");
		}

		// Track
		string trackReturned = "";

		// First we check if they have a route string
		// PRIORITY: Use ExtractedRoute if available (from EuroScope), otherwise fallback to manual parsing
		if (data->ExtractedRoute.empty() && !routeRaw.empty() && routeRaw.size() > 0) { // Get their route as per the route string
			// Manual parsing with Entry/Exit/Coord filtering
			vector<CWaypoint> tempRoute;
			bool direction = data->Direction;

			for (int i = 0; i < routeRaw.size(); i++) {
				CWaypoint point;
				string waypointName = routeRaw[i];

				// Strip speed/level data
				size_t slashPos = waypointName.find('/');
				if (slashPos != string::npos) {
					waypointName = waypointName.substr(0, slashPos);
				}

				// Check alpha
				bool isAllAlpha = true;
				for (char c : waypointName) {
					if (isdigit(c)) isAllAlpha = false;
				}

				if (isAllAlpha) {
					// Check for Track
					string tid = "";
					bool isTrack = false;
					if (waypointName.size() > 3 && waypointName.substr(0, 3) == "NAT") {
						tid = waypointName.substr(3);
					}
					else if (waypointName == "NAT" && i + 1 < routeRaw.size()) {
						// Check next token
						string nextToken = routeRaw[i + 1];
						// Simple check: if it's short, assume it's ID
						if (nextToken.size() <= 2) {
							tid = nextToken;
							i++; // Consume next token
						}
					}

					if (!tid.empty()) {
						if (tid == "SM") { tempRoute.insert(tempRoute.end(), NatSM.begin(), NatSM.end()); isTrack = true; }
						else if (tid == "SN") { tempRoute.insert(tempRoute.end(), NatSN.begin(), NatSN.end()); isTrack = true; }
						else if (tid == "SP") { tempRoute.insert(tempRoute.end(), NatSP.begin(), NatSP.end()); isTrack = true; }
						else if (tid == "SL") { tempRoute.insert(tempRoute.end(), NatSL.begin(), NatSL.end()); isTrack = true; }
						else if (tid == "SO") { tempRoute.insert(tempRoute.end(), NatSO.begin(), NatSO.end()); isTrack = true; }
						else {
							lock_guard<mutex> lock(TracksMutex);
							if (CurrentTracks.find(tid) != CurrentTracks.end()) {
								auto& trackObj = CurrentTracks.at(tid);
								for (int k = 0; k < trackObj.RouteRaw.size(); k++) {
									CWaypoint tp;
									if (k < trackObj.Route.size()) tp.Name = trackObj.Route[k];
									else tp.Name = trackObj.Identifier;
									tp.Position = trackObj.RouteRaw[k];
									tempRoute.push_back(tp);
								}
								isTrack = true;
							}
						}
					}

					if (isTrack) continue;

					// Fix lookup
					lock_guard<mutex> lock(FixCacheMutex);
					if (FixCache.find(waypointName) != FixCache.end()) {
						point.Name = waypointName;
						point.Position = FixCache[waypointName];
					}
					else {
						point.Name = waypointName;
						point.Position.m_Latitude = 0.0;
						point.Position.m_Longitude = 0.0;
					}
				}
				else {
					// Coordinate parsing
					CPosition pos;
					try {
						string s = waypointName;
						if (s.length() == 5 && isdigit(s[0]) && isdigit(s[1]) && isdigit(s[2]) && isdigit(s[3]) && isalpha(s[4])) {
							pos.m_Latitude = stod(s.substr(0, 2));
							pos.m_Longitude = -(stod(s.substr(2, 2)));
						}
						else if (s.length() == 7 && isdigit(s[0]) && isdigit(s[1]) && isalpha(s[2]) && isdigit(s[3]) && isdigit(s[4]) && isdigit(s[5]) && isalpha(s[6])) {
							pos.m_Latitude = stod(s.substr(0, 2));
							double lon = stod(s.substr(3, 3));
							if (s[6] == 'W') lon = -lon;
							pos.m_Longitude = lon;
						}
						else {
							if (s.length() >= 5) {
								pos.m_Latitude = stod(s.substr(0, 2));
								if (!isdigit(s[2])) {
									pos.m_Longitude = -(stod(s.substr(3, 2)));
								}
								else {
									pos.m_Longitude = -(stod(s.substr(2, 2)));
								}
							}
							else {
								pos.m_Latitude = 0.0;
								pos.m_Longitude = 0.0;
							}
						}
					}
					catch (...) {
						pos.m_Latitude = 0.0;
						pos.m_Longitude = 0.0;
					}
					point.Name = waypointName;
					point.Position = pos;
				}
				tempRoute.push_back(point);
			}

			// Filter tempRoute
			int entryIdx = -1;
			int exitIdx = -1;
			for (int i = 0; i < tempRoute.size(); i++) {
				if (CUtils::IsEntryPoint(tempRoute[i].Name, direction)) { entryIdx = i; break; }
			}
			for (int i = tempRoute.size() - 1; i >= 0; i--) {
				if (CUtils::IsExitPoint(tempRoute[i].Name, direction)) { exitIdx = i; break; }
			}

			int start = (entryIdx != -1) ? entryIdx : 0;
			int end = (exitIdx != -1) ? exitIdx : tempRoute.size() - 1;

			for (int i = start; i <= end; i++) {
				bool isEnt = (entryIdx != -1 && i == entryIdx);
				bool isExt = (exitIdx != -1 && i == exitIdx);
				bool isCoord = !CUtils::IsAllAlpha(tempRoute[i].Name);
				if (isEnt || isExt || isCoord) {
					parsedRoute.push_back(tempRoute[i]);
				}
			}
		}
		else { // We get the route as per their VATSIM flight plan if no route string
			// Target, flight plan and route
			// CRadarTarget target = data->Screen->GetPlugIn()->RadarTargetSelect(data->Callsign.c_str());
			
			// Use pre-fetched route if available (thread-safe)
			vector<CWaypoint> extractedRoutePoints;
			int calculatedIndex = 0;

			if (!data->ExtractedRoute.empty()) {
				extractedRoutePoints = data->ExtractedRoute;
				calculatedIndex = data->ExtractedRouteCalculatedIndex;
			}
			else {
				// Fallback 
				CLogger::Log(CLogType::WARN, "Extracted route data missing for " + data->Callsign, "CRoutesHelper::InitialiseRoute");
			}

			if (extractedRoutePoints.empty()) {
				CLogger::Log(CLogType::WARN, "EuroScope Extracted Route is empty for " + data->Callsign, "CRoutesHelper::InitialiseRoute");
			}

			// Get NAT track (if they are on it)
			trackReturned = OnNatTrack(data->Screen, data->Callsign.c_str(), data->RawRouteString, true);
			CTrack track;
			if (trackReturned != "") { // If on a track
				// Check if it is concorde first
				if (trackReturned.size() == 1) {
					bool loopBreak = false;
				{
					lock_guard<mutex> lock(TracksMutex);
					for (auto kv : CRoutesHelper::CurrentTracks) {
						if (kv.first == trackReturned) { // Assign track to the returned box
							track = kv.second;
							loopBreak = true;
							break;
						}
					}
				}
					if (!loopBreak) { // If for some reason the pilot's track doesn't exist, ignore it
						trackReturned = "";
					}
					// Track id
					fp->Track = trackReturned;
				}
			}

			// Find our entry and exit points regardless of track status
			int entryPoint = -1;
			int exitPoint = -1;
			bool direction = data->Direction; // Use pre-fetched direction
			
			for (int i = 0; i < extractedRoutePoints.size(); i++) {
				string wpName = extractedRoutePoints[i].Name;
				size_t slashPos = wpName.find('/');
				if (slashPos != string::npos) {
					wpName = wpName.substr(0, slashPos);
				}

				if (entryPoint == -1) { // Check entry point
					if (CUtils::IsEntryPoint(wpName, direction ? true : false)) {
						entryPoint = i;
						continue;
					}
				}
				if (exitPoint == -1) { // Exit point
					if (CUtils::IsExitPoint(wpName, direction ? true : false)) {
						exitPoint = i;
						break;
					}
				}
			}

			// If on track
			bool routeFetched = false;
			if (trackReturned != "") {
				// 1. Add Entry Point if found
				if (entryPoint != -1) {
					CWaypoint point;
					point.Name = CUtils::ConvertCoordinateFormat(extractedRoutePoints[entryPoint].Name, 0);
					point.Position = extractedRoutePoints[entryPoint].Position;
					parsedRoute.push_back(point);
				}

				// 2. Add Track Coordinates
				// Check concorde
				if (trackReturned.size() == 2) {
					if (trackReturned == "SM") {
						parsedRoute.insert(parsedRoute.end(), NatSM.begin(), NatSM.end());
						routeFetched = true;
					}
					else if (trackReturned == "SN") {
						parsedRoute.insert(parsedRoute.end(), NatSN.begin(), NatSN.end());
						routeFetched = true;
					}
					else if (trackReturned == "SP") {
						parsedRoute.insert(parsedRoute.end(), NatSP.begin(), NatSP.end());
						routeFetched = true;
					}
					else if (trackReturned == "SL") {
						parsedRoute.insert(parsedRoute.end(), NatSL.begin(), NatSL.end());
						routeFetched = true;
					}
					else {
						parsedRoute.insert(parsedRoute.end(), NatSO.begin(), NatSO.end());
						routeFetched = true;
					}
				}

				if (!routeFetched) {
					for (int i = 0; i < track.RouteRaw.size(); i++) {
						// Make waypoint
						CWaypoint point;
						point.Name = track.Route[i];
						point.Position = track.RouteRaw[i];

						// Add to route vector
						parsedRoute.push_back(point);
					}
				}

				// 3. Add Exit Point if found
				if (exitPoint != -1) {
					CWaypoint point;
					point.Name = CUtils::ConvertCoordinateFormat(extractedRoutePoints[exitPoint].Name, 0);
					point.Position = extractedRoutePoints[exitPoint].Position;
					parsedRoute.push_back(point);
				}
			}
			else {
				// Track id
				fp->Track = "RR";

				// Entry and exit points
				int start = entryPoint == -1 ? 0 : entryPoint;
				int stop = exitPoint == -1 ? extractedRoutePoints.size() : exitPoint + 1;

				// Get entry and exit points
				for (int i = start; i < stop; i++) {
					// Filter logic: Only Entry, Exit, or Coordinates (including NAT track points which have numbers)
					// If entryPoint is -1 (not found), start is calculatedIndex. We just treat start as the beginning of the segment.
					bool isEntry = (entryPoint != -1 && i == entryPoint);
					bool isExit = (exitPoint != -1 && i == exitPoint);
					// Check if coordinate (has numbers)
					bool isCoordinate = !CUtils::IsAllAlpha(extractedRoutePoints[i].Name);

					if (isEntry || isExit || isCoordinate) {
						// First check if position is within reasonable longitudinal and lateral bounds
						if (extractedRoutePoints[i].Position.m_Longitude >= -180 && extractedRoutePoints[i].Position.m_Longitude <= 180
							&& extractedRoutePoints[i].Position.m_Latitude >= -90 && extractedRoutePoints[i].Position.m_Latitude <= 90) {
							// Now make the waypoint
							CWaypoint point;
							point.Name = CUtils::ConvertCoordinateFormat(extractedRoutePoints[i].Name, 0);
							point.Position = extractedRoutePoints[i].Position;

							// Add waypoint to parsed vector
							parsedRoute.push_back(point);
						}
					}
				}
			}
		}

		// Return the vector
		CDataHandler::SetRoute(data->Callsign, &parsedRoute, fp->Track, data->FP != nullptr ? data->FP : nullptr);

		// Cleanup
		delete args;
	}
	catch (std::exception & ex) {
		CLogger::DebugLog(data->Screen, "An exception occurred. " + *ex.what());
		CLogger::Log(CLogType::ERR, "An error occurred. Callsign: " + data->Callsign + "\nVerbose details: " + *ex.what(), "CRoutesHelper::InitialiseRoute");
	}
}

int CRoutesHelper::ParseRoute(CRadarScreen* screen, string callsign, string rawInput, bool isTrack, CAircraftFlightPlan* copy) {
	try {
		// Return vector
		vector<string> route;

		// Track
		string track;

		// Deal with track
		if (isTrack) {
			lock_guard<mutex> lock(TracksMutex);
			if (rawInput.size() < 3) {
				if (CRoutesHelper::CurrentTracks.find(rawInput) != CRoutesHelper::CurrentTracks.end()) {
					route = CRoutesHelper::CurrentTracks.at(rawInput).Route;
					track = CRoutesHelper::CurrentTracks.at(rawInput).Identifier;
				}
				else {
					return 1;
				}
			}
			else {
				// Check concorde
				if (rawInput == "SM") {
					track = rawInput;
					for (int i = 0; i < NatSM.size(); i++) {

					}
				}
				else if (rawInput == "SN") {
					track = rawInput;
					for (int i = 0; i < NatSN.size(); i++) {
					}
				}
				else if (rawInput == "SP") {
					track = rawInput;
					for (int i = 0; i < NatSP.size(); i++) {
					}
				}
				else if (rawInput == "SL") {
					track = rawInput;
					for (int i = 0; i < NatSL.size(); i++) {
					}
				}
				else if (rawInput == "SO") {
					track = rawInput;
					for (int i = 0; i < NatSO.size(); i++) {
					}
				}
				else {
					return 1;
				}
			}
		}
		else {
			/// Validation
			// Tokens for split string
			vector<string> tokens;

			// String stream
			stringstream stream(rawInput);

			// Intermediate value
			string intermediate;

			track = "RR";

			// Tokenise the string
			while (getline(stream, intermediate, ' '))
			{
				tokens.push_back(intermediate);
			}

			// Loop the tokens
			for (int i = 0; i < tokens.size(); i++) {
				// Check if digits
				bool isAllAlpha = true;
				for (int j = 0; j < tokens.at(i).size(); j++) {
					if (isdigit(tokens.at(i).at(j))) {
						isAllAlpha = false;
					}
				}

				// If waypoint check the size
				if (isAllAlpha) {
					// Reject if greater or less than 5
					if (tokens.at(i).size() < 5 || tokens.at(i).size() > 5) {
						return 1;
					}
					else {
						// Otherwise make uppercase and push back
						string waypoint;
						for (int j = 0; j < tokens.at(i).size(); j++) {
							waypoint += toupper(tokens.at(i)[j]);
						}
						route.push_back(waypoint);
					}
				}
				else { // It's a coordinate
					if (tokens.at(i).size() < 5 || tokens.at(i).size() > 5) {
						return 1;
					}
					else {
						// Check manually
						if (!isdigit(tokens.at(i)[0]))
							return 1;
						if (!isdigit(tokens.at(i)[1]))
							return 1;
						if (tokens.at(i)[2] != '/')
							return 1;
						if (!isdigit(tokens.at(i)[3]))
							return 1;
						if (!isdigit(tokens.at(i)[4]))
							return 1;

						// We got here so push it
						route.push_back(tokens.at(i));
					}
				}
			}
		}

		// We got here, so set the route and return success code
		if (copy != nullptr) {
			copy->Track = track;
			copy->RouteRaw.clear();
			for (int i = 0; i < route.size(); i++) {
				if (route[i] == "AIRCRAFT") {
					route.at(i).erase();
					continue;
				}
				copy->RouteRaw.push_back(route[i]);
			}
		}
		else {
			CDataHandler::GetFlightData(callsign)->Track = track;
			CDataHandler::GetFlightData(callsign)->RouteRaw.clear();
			for (int i = 0; i < route.size(); i++) {
				if (route[i] == "AIRCRAFT") {
					route.at(i).erase();
					continue;
				}
					
				CDataHandler::GetFlightData(callsign)->RouteRaw.push_back(route[i]);
			}
		}

	}
	catch (std::exception & ex) {
		CLogger::DebugLog(screen, "An exception occurred. " + *ex.what());
		CLogger::Log(CLogType::ERR, "An error occurred. Callsign: " + callsign + "\nVerbose details: " + *ex.what(), "CRoutesHelper::ParseRoute");
		return 1;
	}

	return 0;
}

string CRoutesHelper::OnNatTrack(CRadarScreen* screen, string callsign, string routeString, bool disableEuroScopeFetch) {
	lock_guard<mutex> lock(TracksMutex);
	try {
		// Flight plan
		string route = routeString;
		if (route.empty() && !disableEuroScopeFetch) {
			CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());
			if (fp.IsValid()) {
				route = fp.GetFlightPlanData().GetRoute();
			}
		}

		// Get route and begin search
		size_t found = route.find(string(" NAT"));
		size_t trackIdIndex = string::npos;
		bool isConcorde = false;

		// Iterate through occurrences
		while (found != string::npos) {
			// " NAT" is 4 chars.
			// Format expected: " NATx " or " NATx" (end of string)
			// Indices: found (space), found+1 (N), found+2 (A), found+3 (T), found+4 (ID)
			
			// Check if we have enough chars for ID
			if (found + 4 < route.size()) {
				// Check if this is a standalone " NAT" (not part of "NATAL")
				// We check char at found+5. It must be space or end of string.
				bool isValidTerminator = false;
				if (found + 5 >= route.size()) {
					isValidTerminator = true; // End of string
				}
				else if (route.at(found + 5) == 0x20) {
					isValidTerminator = true; // Space
				}
				else {
					// Check for Concorde (2-letter ID)
					// Format: " NATSM "
					// found+4=S, found+5=M. found+6 must be space/EOS.
					if (found + 6 >= route.size() || route.at(found + 6) == 0x20) {
						char c1 = route.at(found + 4);
						char c2 = route.at(found + 5);
						// Check if it matches Concorde tracks
						if ((c2 == 'L' || c2 == 'M' || c2 == 'N' || c2 == 'O' || c2 == 'P') && 
							(c1 == 'S')) { // Concorde tracks usually start with S? The original code checked c2 for L/M/N/O/P.
							// Original code checked route.at(found+5) for L,M,N,O,P.
							// It didn't explicitly check found+4 but assumed it was part of ID.
							// Let's assume found+4 and found+5 form the ID.
							trackIdIndex = found + 4;
							isConcorde = true;
							break;
						}
					}
				}

				if (isValidTerminator) {
					trackIdIndex = found + 4;
					isConcorde = false;
					break;
				}
			}

			// Not found or invalid, search next
			found = route.find(string(" NAT"), found + 1);
		}

		// If found
		if (trackIdIndex != string::npos) {
			string trackId;
			if (isConcorde) {
				if (trackIdIndex + 1 < route.size()) {
					trackId.push_back(route.at(trackIdIndex));
					trackId.push_back(route.at(trackIdIndex + 1));
				}
			} else {
				trackId.push_back(route.at(trackIdIndex));
			}

			// Check if it exists
			if (CurrentTracks.find(trackId) != CurrentTracks.end())
				return trackId;
			else
				return "";
		}
		else { // Not on a NAT
			return "";
		}
	}
	catch (std::exception & ex) {
		CLogger::DebugLog(screen, "An exception occurred. " + *ex.what());
		CLogger::Log(CLogType::ERR, "An error occurred. Callsign: " + callsign + "\nVerbose details: " + *ex.what(), "CRoutesHelper::OnNatTrack");
		return "";
	}
}