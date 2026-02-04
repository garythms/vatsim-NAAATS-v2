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

	// Load VORs
	for (fix = screen->GetPlugIn()->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_VOR);
		fix.IsValid();
		fix = screen->GetPlugIn()->SectorFileElementSelectNext(fix, EuroScopePlugIn::SECTOR_ELEMENT_VOR)) {

		CPosition pos;
		if (fix.GetPosition(&pos, 0)) {
			FixCache[fix.GetName()] = pos;
		}
	}

	// Load NDBs
	for (fix = screen->GetPlugIn()->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_NDB);
		fix.IsValid();
		fix = screen->GetPlugIn()->SectorFileElementSelectNext(fix, EuroScopePlugIn::SECTOR_ELEMENT_NDB)) {

		CPosition pos;
		if (fix.GetPosition(&pos, 0)) {
			FixCache[fix.GetName()] = pos;
		}
	}
}

bool CRoutesHelper::GetRoute(CRadarScreen* screen, vector<CRoutePosition>* routeVector, string callsign, CAircraftFlightPlan* copy) {
	try {
		// Get the flight plan
		CAircraftFlightPlan* fp = copy != nullptr ? copy : CDataHandler::GetFlightData(callsign);

	
		// Check validity
	if (fp == nullptr || !fp->IsValid) {
		return false;
	}
	
	// If Route is empty but RouteRaw has data, build route from RouteRaw
	if (fp->Route.empty() && !fp->RouteRaw.empty()) {
		// Build temporary route from RouteRaw for display
		// This allows displaying coordinates while async parsing completes
		bool direction = true;
		CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (target.IsValid()) {
			direction = CUtils::GetAircraftDirection(target.GetPosition().GetReportedHeadingTrueNorth());
		}
		
		// Find entry and exit indices in RouteRaw
			int startIdx = -1;
			int endIdx = -1;
			
			for (int i = 0; i < fp->RouteRaw.size(); i++) {
				string name = fp->RouteRaw[i];
				size_t slash = name.find('/');
				if (slash != string::npos) {
					// Only strip if it's NOT a coordinate format (e.g. 54/30)
					bool isCoord = false;
					if (slash > 0 && slash < name.length() - 1) {
						if (isdigit((unsigned char)name[slash - 1]) && isdigit((unsigned char)name[slash + 1])) {
							isCoord = true;
						}
					}
					if (!isCoord) name = name.substr(0, slash);
				}
				if (IsOceanicEntryPoint(name, direction) || CUtils::IsCoord(name)) {
					if (startIdx == -1) startIdx = i;
					endIdx = i;
				}
				if (IsOceanicExitPoint(name, direction)) {
					endIdx = i;
					// Once we find an exit point, we can stop updating endIdx 
					// unless another exit point or coordinate is found later.
					// Actually, the last exit point in the route is usually the one we want.
				}
			}
			
			// If no start found, default to beginning
			if (startIdx == -1) startIdx = 0;
			// If no end found, default to end
			if (endIdx == -1) endIdx = fp->RouteRaw.size() - 1;
			
			int totalDistance = 0;
			CPosition lastPos;
			bool lastPosValid = false;
			
			for (int i = startIdx; i <= endIdx && i < fp->RouteRaw.size(); i++) {
				string waypointName = fp->RouteRaw[i];
				size_t slash = waypointName.find('/');
				if (slash != string::npos) {
					// Only strip if it's NOT a coordinate format (e.g. 54/30)
					bool isCoord = false;
					if (slash > 0 && slash < waypointName.length() - 1) {
						if (isdigit((unsigned char)waypointName[slash - 1]) && isdigit((unsigned char)waypointName[slash + 1])) {
							isCoord = true;
						}
					}
					if (!isCoord) waypointName = waypointName.substr(0, slash);
				}
				
				if (waypointName == "DCT") continue;
				
				// Filter: Only include waypoints that are Oceanic points, Coordinates, or NAT Tracks
				bool isOceanic = IsOceanicEntryPoint(waypointName, direction) || IsOceanicExitPoint(waypointName, direction) || CUtils::IsCoord(waypointName);
				bool isNatPattern = (waypointName.size() > 3 && waypointName.substr(0, 3) == "NAT");
				
				if (!isOceanic && !isNatPattern && waypointName != "NAT") {
					// Skip non-oceanic points (airways, SIDs, etc.)
					continue;
				}
			
			// Check for NAT track
			string tid = "";
			if (waypointName.size() > 3 && waypointName.substr(0, 3) == "NAT") {
				tid = waypointName.substr(3);
			}
			else if (waypointName == "NAT" && i + 1 < fp->RouteRaw.size()) {
				tid = fp->RouteRaw[i + 1];
				i++;
			}

			if (!tid.empty()) {
				vector<CPosition> trackPoints;
				bool found = false;
				if (tid == "SM") { for (const auto& wp : NatSM) trackPoints.push_back(wp.Position); found = true; }
				else if (tid == "SN") { for (const auto& wp : NatSN) trackPoints.push_back(wp.Position); found = true; }
				else if (tid == "SP") { for (const auto& wp : NatSP) trackPoints.push_back(wp.Position); found = true; }
				else if (tid == "SL") { for (const auto& wp : NatSL) trackPoints.push_back(wp.Position); found = true; }
				else if (tid == "SO") { for (const auto& wp : NatSO) trackPoints.push_back(wp.Position); found = true; }
				else {
					lock_guard<mutex> lock(TracksMutex);
					if (CurrentTracks.find(tid) != CurrentTracks.end()) {
						trackPoints = CurrentTracks.at(tid).RouteRaw;
						found = true;
					}
				}

		if (found) {
			for (int k = 0; k < trackPoints.size(); k++) {
				CRoutePosition position;
				// Use the actual waypoint name from the track definition if available
				lock_guard<mutex> lock(TracksMutex);
				if (CurrentTracks.find(tid) != CurrentTracks.end() && k < CurrentTracks.at(tid).Route.size()) {
					position.Fix = CurrentTracks.at(tid).Route[k];
				} else {
					position.Fix = tid; // Fallback
				}
				position.PositionRaw = trackPoints[k];
				position.Estimate = "--";
				position.DistanceFromLastPoint = 0;
				position.FlightLevel = target.IsValid() ? target.GetPosition().GetFlightLevel() / 100 : 0;
				routeVector->push_back(position);
			}
			continue;
		}
			}

			CRoutePosition position;
			position.Fix = waypointName;
			
			// Try to parse position from coordinate format
			CPosition pos;
			pos.m_Latitude = 0.0;
			pos.m_Longitude = 0.0;
			
			// Check if it's a coordinate (has numbers)
			bool isCoordinate = !CUtils::IsAllAlpha(waypointName);
			
			if (isCoordinate) {
				// Parse coordinate
				string s = waypointName;
				try {
					if (s.length() == 5 && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && 
						isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]) && isalpha((unsigned char)s[4])) {
						// 5430N format
						pos.m_Latitude = stod(s.substr(0, 2));
						pos.m_Longitude = -(stod(s.substr(2, 2)));
					}
					else if (s.length() == 5 && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && 
						s[2] == '/' && isdigit((unsigned char)s[3]) && isdigit((unsigned char)s[4])) {
						// 58/20 format
						pos.m_Latitude = stod(s.substr(0, 2));
						pos.m_Longitude = -(stod(s.substr(3, 2)));
					}
					else if (s.length() == 7) {
						// 54N030W format
						pos.m_Latitude = stod(s.substr(0, 2));
						double lon = stod(s.substr(3, 3));
						if (s[6] == 'W') lon = -lon;
						pos.m_Longitude = lon;
					}
					else if (s.length() == 9) {
						// 6230N040W format
						double latDeg = stod(s.substr(0, 2));
						double latMin = stod(s.substr(2, 2));
						pos.m_Latitude = latDeg + (latMin / 60.0);
						if (s[4] == 'S') pos.m_Latitude = -pos.m_Latitude;

						double lonDeg = stod(s.substr(5, 3));
						pos.m_Longitude = -lonDeg;
						if (s[8] == 'E') pos.m_Longitude = -pos.m_Longitude;
					}
					else if (s.length() == 11) {
						// 5430N03000W format
						double latDeg = stod(s.substr(0, 2));
						double latMin = stod(s.substr(2, 2));
						pos.m_Latitude = latDeg + (latMin / 60.0);
						if (s[4] == 'S') pos.m_Latitude = -pos.m_Latitude;
						
						double lonDeg = stod(s.substr(5, 3));
						double lonMin = stod(s.substr(8, 2));
						double lon = lonDeg + (lonMin / 60.0);
						if (s[10] == 'W') lon = -lon;
						pos.m_Longitude = lon;
					}
				} catch (...) {}
			}
			else {
				// Try fix cache lookup
				lock_guard<mutex> lock(FixCacheMutex);
				if (FixCache.find(waypointName) != FixCache.end()) {
					pos = FixCache[waypointName];
				}
				else {
					// Last resort: live EuroScope lookup
					CSectorElement liveFix = screen->GetPlugIn()->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_FIX);
					bool liveFound = false;
					// This is expensive so we should avoid it if possible, but it's a good fallback
					// Actually, let's try VOR and NDB too if FIX fails
					int types[] = { EuroScopePlugIn::SECTOR_ELEMENT_FIX, EuroScopePlugIn::SECTOR_ELEMENT_VOR, EuroScopePlugIn::SECTOR_ELEMENT_NDB };
					for (int type : types) {
						for (liveFix = screen->GetPlugIn()->SectorFileElementSelectFirst(type);
							liveFix.IsValid();
							liveFix = screen->GetPlugIn()->SectorFileElementSelectNext(liveFix, type)) {
							if (waypointName == liveFix.GetName()) {
								liveFix.GetPosition(&pos, 0);
								FixCache[waypointName] = pos; // Cache it for next time
								liveFound = true;
								break;
							}
						}
						if (liveFound) break;
					}
				}
			}
			
			position.PositionRaw = pos;
			
			// Calculate estimate if we have valid position and target
			if (target.IsValid() && (pos.m_Latitude != 0.0 || pos.m_Longitude != 0.0)) {
				bool isPassed = false;
				if (!direction) { // Westbound
					isPassed = target.GetPosition().GetPosition().m_Longitude <= pos.m_Longitude;
				} else { // Eastbound
					isPassed = target.GetPosition().GetPosition().m_Longitude >= pos.m_Longitude;
				}
				
				if (!isPassed) {
					if (!lastPosValid) {
						totalDistance = target.GetPosition().GetPosition().DistanceTo(pos);
					} else {
						totalDistance += lastPos.DistanceTo(pos);
					}
					position.Estimate = CUtils::ParseZuluTime(false, CUtils::GetTimeDistanceSpeed((int)round(totalDistance), target.GetPosition().GetReportedGS()));
					position.DistanceFromLastPoint = lastPosValid ? lastPos.DistanceTo(pos) : target.GetPosition().GetPosition().DistanceTo(pos);
				} else {
					position.Estimate = "--";
					position.DistanceFromLastPoint = 0;
				}
				position.FlightLevel = target.GetPosition().GetFlightLevel() / 100;
				
				lastPos = pos;
				lastPosValid = true;
			} else {
				position.Estimate = "--";
				position.DistanceFromLastPoint = 0;
				position.FlightLevel = 0;
			}
			
			routeVector->push_back(position);
		}
		
		return !routeVector->empty();
	}
	
	// Original logic for when Route is populated
	if (fp->Route.size() == 0) {
		return false;
	}

		// Get target
		CRadarTarget target = screen->GetPlugIn()->RadarTargetSelect(callsign.c_str());

		// Get aircraft direction (default to true/Eastbound if target invalid)
		bool direction = true;
		if (target.IsValid()) {
			direction = CUtils::GetAircraftDirection(target.GetPosition().GetReportedHeadingTrueNorth());
		} else {
			// Use flight plan direction if target invalid
			// fp->Direction: True = Westbound, False = Eastbound
			// direction: True = Eastbound, False = Westbound
			direction = !fp->Direction;
		}

		// Filter route to Oceanic Entry/Exit points
		int startIndex = 0;
		int endIndex = fp->Route.size() - 1;

		// Find Entry Point
		for (int i = 0; i < fp->Route.size(); i++) {
			if (fp->Route[i].Name == "AIRCRAFT" || IsOceanicEntryPoint(fp->Route[i].Name, direction) || CUtils::IsCoord(fp->Route[i].Name)) {
				startIndex = i;
				break;
			}
		}

		// Find Exit Point (search after startIndex)
		for (int i = startIndex; i < fp->Route.size(); i++) {
			if (IsOceanicExitPoint(fp->Route[i].Name, direction) || CUtils::IsCoord(fp->Route[i].Name)) {
				endIndex = i;
			}
		}
	
		// Loop through each route item
		int totalDistance = 0;

		// Iterate through the filtered route
		for (int idx = startIndex; idx <= endIndex; idx++) {
				// Filter out non-oceanic waypoints
				bool isOceanic = IsOceanicEntryPoint(fp->Route[idx].Name, direction) || IsOceanicExitPoint(fp->Route[idx].Name, direction) || CUtils::IsCoord(fp->Route[idx].Name);
				if (!isOceanic && fp->Route[idx].Name != "AIRCRAFT") continue;

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
		// Flight plan data copies
		vector<string> routeRaw;
		string trackId;
		
		// If data has copies, use them (preferred)
		if (!data->RouteRaw.empty()) routeRaw = data->RouteRaw;
		if (!data->Track.empty()) trackId = data->Track;

		// Check if we need to fetch from shared memory (fallback)
		// NOTE: Fetching from shared memory is dangerous if map changes.
		// We use a local copy if we must.
		if (routeRaw.empty() || trackId.empty()) {
			CAircraftFlightPlan fpCopy;
			CDataHandler::GetFlightData(data->Callsign, fpCopy);
			if (fpCopy.IsValid) {
				if (routeRaw.empty()) routeRaw = fpCopy.RouteRaw;
				if (trackId.empty()) trackId = fpCopy.Track;
			}
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
		// PRIORITY: Use manual parsing if RouteRaw is provided (manual update), otherwise use ExtractedRoute
		if (!routeRaw.empty() && routeRaw.size() > 0) { // Get their route as per the route string
			// Manual parsing with Entry/Exit/Coord filtering
			vector<CWaypoint> tempRoute;
			bool direction = data->Direction;

			for (int i = 0; i < routeRaw.size(); i++) {
				CWaypoint point;
				string waypointName = routeRaw[i];

				// Strip speed/level data
				size_t slashPos = waypointName.find('/');
				if (slashPos != string::npos) {
					// Only strip if it's NOT a coordinate format (e.g. 54/30)
					bool isCoord = false;
					if (slashPos > 0 && slashPos < waypointName.length() - 1) {
						if (isdigit((unsigned char)waypointName[slashPos - 1]) && isdigit((unsigned char)waypointName[slashPos + 1])) {
							isCoord = true;
						}
					}
					if (!isCoord) waypointName = waypointName.substr(0, slashPos);
				}

				// Check alpha
				bool isAllAlpha = true;
				for (char c : waypointName) {
					if (isdigit((unsigned char)c)) isAllAlpha = false;
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
								for (int k = 0; k < trackObj.Route.size(); k++) {
									CWaypoint tp;
									tp.Name = trackObj.Route[k];
									if (k < trackObj.RouteRaw.size()) {
										tp.Position = trackObj.RouteRaw[k];
									}
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
					else if (waypointName == "AIRCRAFT") {
						point.Name = waypointName;
						point.Position = data->Position;
					}
					else {
						// Fix not found in cache - keep the waypoint name but mark position as invalid
						// Entry/exit points are essential even without exact position
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
						if (s.length() == 5 && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]) && isalpha((unsigned char)s[4])) {
							pos.m_Latitude = stod(s.substr(0, 2));
							pos.m_Longitude = -(stod(s.substr(2, 2)));
						}
						else if (s.length() == 5 && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && s[2] == '/' && isdigit((unsigned char)s[3]) && isdigit((unsigned char)s[4])) {
							// 58/20 format
							pos.m_Latitude = stod(s.substr(0, 2));
							pos.m_Longitude = -(stod(s.substr(3, 2)));
						}
						else if (s.length() == 7 && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && isalpha((unsigned char)s[2]) && isdigit((unsigned char)s[3]) && isdigit((unsigned char)s[4]) && isdigit((unsigned char)s[5]) && isalpha((unsigned char)s[6])) {
							pos.m_Latitude = stod(s.substr(0, 2));
							double lon = stod(s.substr(3, 3));
							if (s[6] == 'W') lon = -lon;
							pos.m_Longitude = lon;
						}
						else if (s.length() == 11 && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]) && isalpha((unsigned char)s[4]) &&
							isdigit((unsigned char)s[5]) && isdigit((unsigned char)s[6]) && isdigit((unsigned char)s[7]) && isdigit((unsigned char)s[8]) && isdigit((unsigned char)s[9]) && isalpha((unsigned char)s[10])) {
							// 5430N03000W
							double latDeg = stod(s.substr(0, 2));
							double latMin = stod(s.substr(2, 2));
							pos.m_Latitude = latDeg + (latMin / 60.0);
							if (s[4] == 'S') pos.m_Latitude = -pos.m_Latitude;

							double lonDeg = stod(s.substr(5, 3));
							double lonMin = stod(s.substr(8, 2));
							double lon = lonDeg + (lonMin / 60.0);
							if (s[10] == 'W') lon = -lon;
							pos.m_Longitude = lon;
						}
						else {
							if (s.length() >= 5) {
								pos.m_Latitude = stod(s.substr(0, 2));
								if (!isdigit((unsigned char)s[2])) {
									pos.m_Longitude = -(stod(s.substr(3, 2)));
								}
								else {
									pos.m_Longitude = -(stod(s.substr(2, 2)));
								}
							}
							else {
								// Can't parse - skip this waypoint
								continue;
							}
						}
					}
					catch (...) {
						// Parse error - skip this waypoint
						continue;
					}
					point.Name = waypointName;
					point.Position = pos;
				}
				tempRoute.push_back(point);
			}

			// Filter tempRoute: Show only points between entry and exit fixes, plus any coordinates.
			// If no entry/exit fix is found, use geographic bounds (-5W to -65W).
			int entryIdx = -1;
			int exitIdx = -1;
			for (int i = 0; i < tempRoute.size(); i++) {
				if (IsOceanicEntryPoint(tempRoute[i].Name, direction)) { entryIdx = i; break; }
			}
			for (int i = tempRoute.size() - 1; i >= 0; i--) {
				if (IsOceanicExitPoint(tempRoute[i].Name, direction)) { exitIdx = i; break; }
			}

			// Geographic fallback if fixes not found
			if (entryIdx == -1) {
				for (int i = 0; i < tempRoute.size(); i++) {
					double lon = tempRoute[i].Position.m_Longitude;
					if (direction) { // Eastbound
						if (lon > -70.0 && lon < -40.0) { entryIdx = i; break; }
					} else { // Westbound
						if (lon < 5.0 && lon > -20.0) { entryIdx = i; break; }
					}
				}
			}
			if (exitIdx == -1) {
				for (int i = tempRoute.size() - 1; i >= 0; i--) {
					double lon = tempRoute[i].Position.m_Longitude;
					if (direction) { // Eastbound
						if (lon < 5.0 && lon > -20.0) { exitIdx = i; break; }
					} else { // Westbound
						if (lon > -70.0 && lon < -40.0) { exitIdx = i; break; }
					}
				}
			}

			int start = (entryIdx != -1) ? entryIdx : 0;
			int end = (exitIdx != -1) ? exitIdx : tempRoute.size() - 1;

			for (int i = start; i <= end; i++) {
				bool isEnt = (entryIdx != -1 && i == entryIdx);
				bool isExt = (exitIdx != -1 && i == exitIdx);
				bool isCoord = !CUtils::IsAllAlpha(tempRoute[i].Name);
				bool isAircraft = tempRoute[i].Name == "AIRCRAFT";
				// If on a track, include all points between entry and exit
				bool onTrack = (trackReturned != "" && trackReturned != "RR");
				
				if (isEnt || isExt || isCoord || isAircraft || onTrack) {
					parsedRoute.push_back(tempRoute[i]);
				}
			}
		}
		else { // We get the route as per their VATSIM flight plan if no route string
			// Target, flight plan and route
			
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
			if (trackReturned != "") { // If on a track
				trackId = trackReturned;
			}

			// Find our entry and exit points regardless of track status
			int entryPoint = -1;
			int exitPoint = -1;
			bool direction = data->Direction; // Use pre-fetched direction
			
			for (int i = 0; i < extractedRoutePoints.size(); i++) {
				string wpName = extractedRoutePoints[i].Name;
				size_t slashPos = wpName.find('/');
				if (slashPos != string::npos) {
					// Only strip if it's NOT a coordinate format (e.g. 54/30)
					bool isCoord = false;
					if (slashPos > 0 && slashPos < wpName.length() - 1) {
						if (isdigit((unsigned char)wpName[slashPos - 1]) && isdigit((unsigned char)wpName[slashPos + 1])) {
							isCoord = true;
						}
					}
					if (!isCoord) wpName = wpName.substr(0, slashPos);
				}
				if (IsOceanicEntryPoint(wpName, direction) || CUtils::IsCoord(wpName)) {
					if (entryPoint == -1) entryPoint = i;
					exitPoint = i;
				}
				if (IsOceanicExitPoint(wpName, direction)) {
					exitPoint = i;
				}
			}

			// Geographic fallback if fixes not found
			if (entryPoint == -1) {
				for (int i = 0; i < extractedRoutePoints.size(); i++) {
					double lon = extractedRoutePoints[i].Position.m_Longitude;
					if (direction) { // Eastbound
						if (lon > -70.0 && lon < -40.0) { entryPoint = i; break; }
					} else { // Westbound
						if (lon < 5.0 && lon > -20.0) { entryPoint = i; break; }
					}
				}
			}
			if (exitPoint == -1) {
				for (int i = (entryPoint == -1 ? 0 : entryPoint); i < extractedRoutePoints.size(); i++) {
					double lon = extractedRoutePoints[i].Position.m_Longitude;
					if (direction) { // Eastbound
						if (lon < 5.0 && lon > -20.0) { exitPoint = i; }
					} else { // Westbound
						if (lon > -70.0 && lon < -40.0) { exitPoint = i; }
					}
				}
			}

			// If on track
			bool routeFetched = false;
			if (trackReturned != "") {
				// 1. Add Entry Point if found and it's not the same as the first track point
				if (entryPoint != -1) {
					CWaypoint point;
					point.Name = CUtils::ConvertCoordinateFormat(extractedRoutePoints[entryPoint].Name, 0);
					point.Position = extractedRoutePoints[entryPoint].Position;
					parsedRoute.push_back(point);
				}

				// 2. Add Track Coordinates
				// Re-fetch track from global map to ensure we have the latest route names
				lock_guard<mutex> lock(TracksMutex);
				if (CurrentTracks.find(trackReturned) != CurrentTracks.end()) {
					CTrack& activeTrack = CurrentTracks[trackReturned];
					for (int i = 0; i < activeTrack.Route.size(); i++) {
						// Skip if this is the same as the entry point we just added
						if (!parsedRoute.empty() && parsedRoute.back().Name == activeTrack.Route[i]) continue;

						CWaypoint point;
						point.Name = activeTrack.Route[i];
						if (i < activeTrack.RouteRaw.size()) {
							point.Position = activeTrack.RouteRaw[i];
						}
						parsedRoute.push_back(point);
					}
					routeFetched = true;
				}

				// 3. Add Exit Point if found and it's not the same as the last point added
				if (exitPoint != -1) {
					CWaypoint point;
					point.Name = CUtils::ConvertCoordinateFormat(extractedRoutePoints[exitPoint].Name, 0);
					point.Position = extractedRoutePoints[exitPoint].Position;
					
					if (parsedRoute.empty() || parsedRoute.back().Name != point.Name) {
						parsedRoute.push_back(point);
					}
				}
			}
			else {
				// Random Route
				trackId = "RR";

				if (entryPoint != -1 && exitPoint != -1) {
					for (int i = entryPoint; i <= exitPoint; i++) {
						// Filter logic: Only Entry, Exit, or Coordinates
						bool isEntry = (i == entryPoint);
						bool isExit = (i == exitPoint);
						bool isCoordinate = !CUtils::IsAllAlpha(extractedRoutePoints[i].Name);

						if (isEntry || isExit || isCoordinate) {
							CWaypoint point;
							point.Name = CUtils::ConvertCoordinateFormat(extractedRoutePoints[i].Name, 0);
							point.Position = extractedRoutePoints[i].Position;

							// Avoid duplicates
							if (parsedRoute.empty() || parsedRoute.back().Name != point.Name) {
								parsedRoute.push_back(point);
							}
						}
					}
				}
			}
		}

		// Return the vector
		// Use nullptr for last argument as we don't have a safe pointer to the map entry
		CDataHandler::SetRoute(data->Callsign, &parsedRoute, trackId, nullptr);

		// Cleanup
		delete data;
	}
	catch (std::exception & ex) {
		// CLogger::DebugLog(data->Screen, "An exception occurred. " + *ex.what());
		CLogger::Log(CLogType::ERR, "An error occurred. Callsign: " + data->Callsign + "\nVerbose details: " + *ex.what(), "CRoutesHelper::InitialiseRoute");
		delete data;
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
			if (rawInput.size() <= 2) {
				// Convert to uppercase
				string trackInput = rawInput;
				for (auto& c : trackInput) c = toupper((unsigned char)c);

				if (CRoutesHelper::CurrentTracks.find(trackInput) != CRoutesHelper::CurrentTracks.end()) {
					route = CRoutesHelper::CurrentTracks.at(trackInput).Route;
					track = CRoutesHelper::CurrentTracks.at(trackInput).Identifier;
				}
				else {
					// Even if not found in current tracks, allow setting the track ID
					// This handles cases where tracks are manually assigned or not yet in the map
					track = trackInput;
					route.clear(); // We don't have a defined route for an unknown track ID
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

			// Check if the route string contains a NAT track keyword or sequence
			string foundTrack = OnNatTrack(screen, callsign, rawInput, true);
			if (foundTrack != "") {
				track = foundTrack;
			}

			// Loop the tokens
			for (int i = 0; i < tokens.size(); i++) {
				// Check if digits
				bool isAllAlpha = true;
				for (int j = 0; j < tokens.at(i).size(); j++) {
					if (isdigit((unsigned char)tokens.at(i).at(j))) {
						isAllAlpha = false;
					}
				}

				// If waypoint check the size
				if (isAllAlpha) {
					// Reject if greater than 5 or less than 2 (VORs are 3, NDBs 2-3, Intersections 5)
					// Also explicitly allow DCT and AIRCRAFT (which gets stripped later)
					if ((tokens.at(i).size() < 2 || tokens.at(i).size() > 5) && tokens.at(i) != "DCT" && tokens.at(i) != "AIRCRAFT") {
						return 1;
					}
					else {
						// Otherwise make uppercase and push back
						string waypoint;
						for (int j = 0; j < tokens.at(i).size(); j++) {
							waypoint += toupper((unsigned char)tokens.at(i)[j]);
						}
						route.push_back(waypoint);
					}
				}
				else { // It's a coordinate
					// Allow 3 chars (30W), 4 chars (030W), 5 chars (54/30, 5430N), 7 chars (54N030W), 11 chars (5430N03000W)
					if (tokens.at(i).size() != 3 && tokens.at(i).size() != 4 && tokens.at(i).size() != 5 && tokens.at(i).size() != 7 && tokens.at(i).size() != 11) {
						return 1;
					}
					else {
						// Check manually
						string s = tokens.at(i);
						if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]))
							return 1;

						if (s.size() == 3) {
							// 30W
							if (!isalpha((unsigned char)s[2])) return 1;
						}
						else if (s.size() == 4) {
							// 030W
							if (!isdigit((unsigned char)s[2]) || !isalpha((unsigned char)s[3])) return 1;
						}
						else if (s.size() == 5) {
							// 54/30 or 5430N
							if (s[2] == '/') {
								if (!isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) return 1;
							}
							else if (isdigit((unsigned char)s[2])) {
								if (!isdigit((unsigned char)s[3]) || !isalpha((unsigned char)s[4])) return 1;
							}
							else {
								return 1;
							}
						}
						else if (s.size() == 7) {
							// 54N030W
							if (!isalpha((unsigned char)s[2])) return 1;
							if (!isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4]) || !isdigit((unsigned char)s[5])) return 1;
							if (!isalpha((unsigned char)s[6])) return 1;
						}
						else if (s.size() == 11) {
							// 5430N03000W
							if (!isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3])) return 1;
							if (!isalpha((unsigned char)s[4])) return 1;
							if (!isdigit((unsigned char)s[5]) || !isdigit((unsigned char)s[6]) || !isdigit((unsigned char)s[7]) || !isdigit((unsigned char)s[8]) || !isdigit((unsigned char)s[9])) return 1;
							if (!isalpha((unsigned char)s[10])) return 1;
						}

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
			CAircraftFlightPlan* fp = CDataHandler::GetFlightData(callsign);
			if (fp) {
				fp->Track = track;
				fp->RouteRaw.clear();
				for (int i = 0; i < route.size(); i++) {
					if (route[i] == "AIRCRAFT") {
						route.at(i).erase();
						continue;
					}

					fp->RouteRaw.push_back(route[i]);
				}
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

bool CRoutesHelper::IsOceanicEntryPoint(string pointName, bool eastbound) {
	// 1. Check static lists
	if (CUtils::IsEntryPoint(pointName, eastbound)) return true;

	// 2. Check active tracks from natTrak
	lock_guard<mutex> lock(TracksMutex);
	for (auto const& kv : CurrentTracks) {
		const CTrack& t = kv.second;
		if (t.Route.empty()) continue;
		
		// If aircraft is eastbound, we look for points on the West side (Gander)
		// If aircraft is westbound, we look for points on the East side (Shanwick)
		if (eastbound) {
			// Eastbound entry points are at the beginning of Eastbound tracks 
			// or end of Westbound tracks
			if (t.Direction == CTrackDirection::EAST && t.Route.front() == pointName) return true;
			if (t.Direction == CTrackDirection::WEST && t.Route.back() == pointName) return true;
		} else {
			// Westbound entry points are at the beginning of Westbound tracks
			// or end of Eastbound tracks
			if (t.Direction == CTrackDirection::WEST && t.Route.front() == pointName) return true;
			if (t.Direction == CTrackDirection::EAST && t.Route.back() == pointName) return true;
		}
	}
	return false;
}

bool CRoutesHelper::IsOceanicExitPoint(string pointName, bool eastbound) {
	// 1. Check static lists
	if (CUtils::IsExitPoint(pointName, eastbound)) return true;

	// 2. Check active tracks from natTrak
	lock_guard<mutex> lock(TracksMutex);
	for (auto const& kv : CurrentTracks) {
		const CTrack& t = kv.second;
		if (t.Route.empty()) continue;

		if (eastbound) {
			// Eastbound exit points are at the end of Eastbound tracks
			// or start of Westbound tracks
			if (t.Direction == CTrackDirection::EAST && t.Route.back() == pointName) return true;
			if (t.Direction == CTrackDirection::WEST && t.Route.front() == pointName) return true;
		} else {
			// Westbound exit points are at the end of Westbound tracks
			// or start of Eastbound tracks
			if (t.Direction == CTrackDirection::WEST && t.Route.back() == pointName) return true;
			if (t.Direction == CTrackDirection::EAST && t.Route.front() == pointName) return true;
		}
	}
	return false;
}

string CRoutesHelper::OnNatTrack(CRadarScreen* screen, string callsign, string routeString, bool disableEuroScopeFetch) {
	try {
		// Flight plan
		string route = routeString;
		if (route.empty() && !disableEuroScopeFetch) {
			CFlightPlan fp = screen->GetPlugIn()->FlightPlanSelect(callsign.c_str());
			if (fp.IsValid()) {
				route = fp.GetFlightPlanData().GetRoute();
			}
		}

		// Normalize route string for searching
		string normRoute = " " + route + " ";
		for (auto& c : normRoute) c = toupper((unsigned char)c);

		// 1. Search for " NAT X " or " NATX " pattern
		size_t found = normRoute.find(" NAT");
		while (found != string::npos) {
			// Check if it's " NAT " or " NATX"
			if (found + 4 < normRoute.size()) {
				char nextChar = normRoute[found + 4];
				string trackId = "";
				
				if (nextChar == ' ') { // " NAT B"
					if (found + 5 < normRoute.size() && isalpha((unsigned char)normRoute[found + 5])) {
						trackId = normRoute.substr(found + 5, 1);
						// Check for 2nd char (Concorde)
						if (found + 6 < normRoute.size() && isalpha((unsigned char)normRoute[found + 6]) && normRoute[found + 7] == ' ') {
							trackId += normRoute[found + 6];
						}
					}
				} else if (isalpha((unsigned char)nextChar)) { // " NATB"
					trackId = normRoute.substr(found + 4, 1);
					// Check for 2nd char (Concorde)
					if (found + 5 < normRoute.size() && isalpha((unsigned char)normRoute[found + 5]) && normRoute[found + 6] == ' ') {
						trackId += normRoute[found + 5];
					}
				}

				if (!trackId.empty()) {
					lock_guard<mutex> lock(TracksMutex);
					if (CurrentTracks.find(trackId) != CurrentTracks.end()) {
						return trackId;
					}
				}
			}
			found = normRoute.find(" NAT", found + 1);
		}

		// 2. Fuzzy match sequence of coordinates
		vector<string> tokens;
		CUtils::StringSplit(route, ' ', &tokens);

		vector<string> normalizedTokens;
		for (string t : tokens) {
			if (t.empty()) continue;
			size_t slashPos = t.find('/');
			if (slashPos != string::npos) {
				if (slashPos > 0 && slashPos < t.length() - 1) {
					if (!isdigit((unsigned char)t[slashPos - 1]) || !isdigit((unsigned char)t[slashPos + 1])) {
						t = t.substr(0, slashPos);
					}
				}
			}
			string normalized = CUtils::ConvertCoordinateFormat(t, 0);
			for (auto& c : normalized) c = toupper((unsigned char)c);
			normalizedTokens.push_back(normalized);
		}

		lock_guard<mutex> lock(TracksMutex);
		for (auto const& kv : CurrentTracks) {
			const string& id = kv.first;
			const CTrack& t = kv.second;
			if (t.Route.size() == 0) continue;

			int matchCount = 0;
			for (const string& trPoint : t.Route) {
				string normTr = CUtils::ConvertCoordinateFormat(trPoint, 0);
				if (normTr.empty()) continue;
				for (auto& c : normTr) c = toupper((unsigned char)c);

				for (const string& flPoint : normalizedTokens) {
					if (flPoint.empty()) continue;
					if (normTr == flPoint) {
						matchCount++;
						break;
					}
					// Also check for partial coordinate match (e.g. 54N040W matches 54/40)
					if (normTr.length() >= 4 && flPoint.length() >= 4) {
						if (normTr.substr(0, 2) == flPoint.substr(0, 2) && normTr.substr(normTr.length() - 2) == flPoint.substr(flPoint.length() - 2)) {
							matchCount++;
							break;
						}
					}
				}
			}

			// If at least 2 points match, or 1 point matches if the track is short
			if (matchCount >= 2 || (matchCount >= 1 && t.Route.size() <= 2)) {
				return id;
			}
		}

		return "";
	}
	catch (std::exception & ex) {
		CLogger::Log(CLogType::ERR, "An error occurred. Callsign: " + callsign + "\nVerbose details: " + *ex.what(), "CRoutesHelper::OnNatTrack");
		return "";
	}
}