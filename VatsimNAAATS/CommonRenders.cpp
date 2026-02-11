#include "pch.h"

#include "CommonRenders.h"
#include "FlightPlanWindow.h"
#include "Styles.h"
#include <vector>
#include <map>



using namespace Colours;



CRect CCommonRenders::RenderButton(CDC* dc, CRadarScreen* screen, POINT topLeft, int width, int height, CWinButton* obj, int vtcAlign) {

	// Save context for later

	int sDC = dc->SaveDC();



	// Create rectangle

	CRect button(topLeft.x, topLeft.y, topLeft.x + width, topLeft.y + height);



	// Check if pressed

	if (obj->State == CInputState::ACTIVE) {

		dc->FillSolidRect(button, ButtonPressed.ToCOLORREF());

		// Button bevel

		dc->Draw3dRect(button, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

		InflateRect(button, -1, -1);

		dc->Draw3dRect(button, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

	}

	else {

		dc->FillSolidRect(button, ScreenBlue.ToCOLORREF());

		// Button bevel

		dc->Draw3dRect(button, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

		InflateRect(button, -1, -1);

		dc->Draw3dRect(button, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

	}



	// Draw text

	if (obj->Label == "PSR_SYMBOL") {

		FontSelector::SelectNormalFont(30, dc);

		obj->Label = "*";

	}

	else {

		FontSelector::SelectNormalFont(16, dc);

	}

		

	if (obj->State == CInputState::DISABLED) { // Disabled text colour

		dc->SetTextColor(Disabled.ToCOLORREF());

	}

	else {

		dc->SetTextColor(TextWhite.ToCOLORREF());

	}

	dc->SetTextAlign(TA_CENTER);

	dc->TextOut(button.left + (button.Width() / 2), button.top + (vtcAlign != -1 ? vtcAlign : ((button.bottom - button.top) / 4.5)), obj->Label.c_str());



	// Restore device context

	dc->RestoreDC(sDC);



	// Add object and return the rectangle

	screen->AddScreenObject(obj->Type, to_string(obj->Id).c_str(), button, false, "");



	// Return

	return button;

}



void CCommonRenders::RenderTextInput(CDC* dc, CRadarScreen* screen, POINT topLeft, int width, int height, CTextInput* obj) {

	// Save context for later

	int sDC = dc->SaveDC();



	// Select font

	FontSelector::SelectATCFont(14, dc);

	dc->SetTextColor(TextWhite.ToCOLORREF());

	dc->SetTextAlign(TA_LEFT);



	// Create rectangle

	CRect rect(topLeft.x, topLeft.y, topLeft.x + width, topLeft.y + height);



	// Draw rectangle

	dc->FillSolidRect(rect, ScreenBlue.ToCOLORREF());

	// Button bevel

	dc->Draw3dRect(rect, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());

	InflateRect(rect, -1, -1);

	dc->Draw3dRect(rect, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());



	// Select text colour

	if (obj->State == CInputState::DISABLED) {

		dc->SetTextColor(Disabled.ToCOLORREF());

	}

	else {

		dc->SetTextColor(TextWhite.ToCOLORREF());

	}



	// Draw text
	if (obj->Error) {
		dc->SetTextColor(RGB(255, 0, 0)); // Red for error
		dc->TextOut(rect.left + 3, rect.top + 2, "SYNTAX");
	}
	else {
		dc->TextOut(rect.left + 3, rect.top + 2, obj->Content.c_str());
	}



	// Restore device context

	dc->RestoreDC(sDC);



	// If editable, then add object and return the rectangle

	if (obj->State == CInputState::ACTIVE) {

		screen->AddScreenObject(obj->Type, to_string(obj->Id).c_str(), rect, false, "");

	}

}



CRect CCommonRenders::RenderCheckBox(CDC* dc, Graphics* g, CRadarScreen* screen, POINT topLeft, int height, CCheckBox* obj) {

	// Save context for later

	int sDC = dc->SaveDC();



	// Pens

	Pen white(TextWhite, 2);

	Pen grey(Grey, 2);



	// Create rectangle

	CRect rect(topLeft.x, topLeft.y, topLeft.x + height, topLeft.y + height);

	// Button bevel

	if (obj->IsChecked) {

		dc->Draw3dRect(rect, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());

		InflateRect(rect, -1, -1);

		dc->Draw3dRect(rect, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());

		dc->FillSolidRect(rect, ButtonPressed.ToCOLORREF());

		g->DrawLine(&white, rect.left + 1, rect.top + 1, rect.right - 2, rect.bottom - 2);

		g->DrawLine(&white, rect.left + 1, rect.bottom - 2, rect.right - 2, rect.top + 1);

	}

	else {

		dc->Draw3dRect(rect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

		InflateRect(rect, -1, -1);

		dc->Draw3dRect(rect, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

	}



	// Restore device context
	dc->RestoreDC(sDC);

	// Clean up



	// Add object

	screen->AddScreenObject(obj->Type, to_string(obj->Id).c_str(), rect, false, "");



	return rect;

}



void CCommonRenders::RenderDropDown(CDC* dc, Graphics* g, CRadarScreen* screen, POINT topLeft, int width, int height, CDropDown* obj, bool drawList) {

	// Save context for later

	int sDC = dc->SaveDC();



	// Create dropdown area

	CRect dropDown(topLeft.x, topLeft.y, topLeft.x + width - 15, topLeft.y + height);



	// Pen

	Pen white(TextWhite, 2);



	// Fill

	dc->FillSolidRect(dropDown, ScreenBlue.ToCOLORREF());



	// Select font

	FontSelector::SelectNormalFont(MEN_FONT_SIZE, dc);

	dc->SetTextColor(TextWhite.ToCOLORREF());

	dc->SetTextAlign(TA_LEFT);



	// Dropdown bevel

	dc->Draw3dRect(dropDown, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());

	InflateRect(dropDown, -1, -1);

	dc->Draw3dRect(dropDown, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());



	// Create dropdown button

	CRect button(topLeft.x + width - 15, topLeft.y, topLeft.x + width, topLeft.y + height);



	// Value for main box

	string displayValue;



	// Check whether items are checked

	for (auto kv : obj->Items) {

		if (kv.second.IsCheckItem && kv.second.State == CInputState::ACTIVE) {

			displayValue += (displayValue.empty() && displayValue.size()) <= 3 ? kv.second.Label : (", " + kv.second.Label);

			displayValue += displayValue.size() > 3 ? "..." : "";

		}

	}



	// Check if pressed

	if (obj->State == CInputState::ACTIVE) {

		// Button pressed fill
		dc->FillSolidRect(button, ButtonPressed.ToCOLORREF());

		if (drawList) {
			CRect area(dropDown.left, dropDown.bottom, dropDown.right, dropDown.bottom + (obj->Items.size() * 20) + 2);

			dc->FillSolidRect(area, ScreenBlue.ToCOLORREF());

			dc->Draw3dRect(area, BevelDark.ToCOLORREF(), BevelDark.ToCOLORREF());



			// Draw items

			int offsetY = 2;

			for (auto kv : obj->Items) {

				CRect object(area.left, area.top + offsetY, area.right, area.top + offsetY + 20);

				if (kv.second.IsHovered)

					dc->FillSolidRect(object, ButtonPressed.ToCOLORREF());

				if (kv.second.IsCheckItem && kv.second.State == CInputState::ACTIVE) {

					CRect rect(object.right - 20, object.top, object.right, object.bottom);

					g->DrawLine(&white, rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4);

					g->DrawLine(&white, rect.left + 4, rect.bottom - 4, rect.right - 4, rect.top + 4);

				}

				dc->TextOut(area.left + 2, area.top + offsetY + 2, kv.second.Label.c_str());

				screen->AddScreenObject(kv.second.Type, to_string(kv.second.Id).c_str(), object, false, "");

				offsetY += 20;

			}
		}

	}

	else {

		// Button

		dc->FillSolidRect(button, ScreenBlue.ToCOLORREF());

	}



	// Write selected text

	obj->Value = !displayValue.empty() ? displayValue : obj->Value;

	if (obj->Id == 101 && obj->Type == MENBAR && displayValue.empty())

		obj->Value = "None";



	dc->TextOut(dropDown.left + 2, dropDown.top + 1, obj->Value.c_str());



	// Button triangle

	SolidBrush brush(Grey);

	g->SetSmoothingMode(SmoothingModeAntiAlias);

	// Coz GDI+ doesn't like GDI

	Rect rectangle(topLeft.x + width - 15, topLeft.y, topLeft.x + width, topLeft.y + height);

	Point points[3] = { Point(rectangle.X + 2, rectangle.Y + 4),

		Point(rectangle.X + 12, rectangle.Y + 4),

		Point(rectangle.X + 7, rectangle.Y + 14) };

	g->FillPolygon(&brush, points, 3);

	// Button bevel

	dc->Draw3dRect(button, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

	InflateRect(button, -1, -1);

	dc->Draw3dRect(button, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());



	// '3d' border trick

	Pen darkerPen(BevelDark, 1.5);

	g->DrawLine(&darkerPen, points[1], points[2]);



	// Restore device context

	dc->RestoreDC(sDC);







	// Add object

	screen->AddScreenObject(obj->Type, to_string(obj->Id).c_str(), button, false, "");

}



void CCommonRenders::RenderScrollBar(CDC* dc, Graphics* g, CRadarScreen* screen, POINT topLeft, CWinScrollBar* scrollView) {

	// Save context for later

	int sDC = dc->SaveDC();



	// Draw scroll track

	CRect scrollBarTrack;

	if (scrollView->IsHorizontal) { // Different track depending on whether it's horizontal or not

		scrollBarTrack = CRect(topLeft.x, topLeft.y, topLeft.x + scrollView->FrameSize, topLeft.y + 12);

	}

	else {

		scrollBarTrack = CRect(topLeft.x, topLeft.y, topLeft.x + 12, topLeft.y + scrollView->FrameSize);

	}

	dc->FillSolidRect(scrollBarTrack, ButtonPressed.ToCOLORREF());

	dc->Draw3dRect(scrollBarTrack, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());

	InflateRect(scrollBarTrack, -1, -1);

	dc->Draw3dRect(scrollBarTrack, BevelDark.ToCOLORREF(), BevelLight.ToCOLORREF());



	// Get grip position

	scrollView->PositionDelta = (scrollView->FrameSize - 12 - scrollView->GripSize) * (scrollView->WindowPos / scrollView->TotalScrollableArea);



	// Draw scroll buttons & grip

	CRect buttonRect1;

	CRect buttonRect2;

	CRect grip;

	SolidBrush brush(ScreenBlue);

	Pen lighterPen(BevelLight, 1.5);

	Pen darkerPen(BevelDark, 1.5);

	g->SetSmoothingMode(SmoothingModeAntiAlias);

	if (scrollView->IsHorizontal) {

		Point btnA[3] = { Point(scrollBarTrack.left + 1, scrollBarTrack.top + 4),

				Point(scrollBarTrack.left + 9, scrollBarTrack.top),

				Point(scrollBarTrack.left + 9, scrollBarTrack.bottom - 2) };

		g->FillPolygon(&brush, btnA, 3);

		Point btnB[3] = { Point(scrollBarTrack.right - 2, scrollBarTrack.top + 4),

			Point(scrollBarTrack.right - 10, scrollBarTrack.top),

			Point(scrollBarTrack.right - 10, scrollBarTrack.bottom - 2) };

		g->FillPolygon(&brush, btnB, 3);



		// Set rectangles

		buttonRect1 = CRect(scrollBarTrack.left, scrollBarTrack.top, scrollBarTrack.left + 11, scrollBarTrack.bottom);

		buttonRect2 = CRect(scrollBarTrack.right - 11, scrollBarTrack.top, scrollBarTrack.right, scrollBarTrack.bottom);

		

		// Grip

		grip = CRect(scrollBarTrack.left + 11 + (!isnan(scrollView->PositionDelta) ? scrollView->WindowPos : 0), scrollBarTrack.top, scrollBarTrack.left + (!isnan(scrollView->PositionDelta) ? scrollView->WindowPos : 0) + scrollView->GripSize, scrollBarTrack.bottom - 1);

		dc->FillSolidRect(grip, ScreenBlue.ToCOLORREF());

		dc->Draw3dRect(grip, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

		InflateRect(grip, -1, -1);

		dc->Draw3dRect(grip, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());



		// '3d' border trick

		g->DrawLine(&lighterPen, btnA[0], btnA[1]);

		g->DrawLine(&darkerPen, btnA[1], btnA[2]);

		g->DrawLine(&darkerPen, btnA[0], btnA[2]);

		g->DrawLine(&lighterPen, btnB[0], btnB[1]);

		g->DrawLine(&darkerPen, btnB[1], btnB[2]);

		g->DrawLine(&darkerPen, btnB[0], btnB[2]);

	}

	else {

		Point btnA[3] = { Point(scrollBarTrack.left + 3, scrollBarTrack.top + 1),

			Point(scrollBarTrack.left - 1, scrollBarTrack.top + 9),

			Point(scrollBarTrack.right - 2, scrollBarTrack.top + 9) };

		g->FillPolygon(&brush, btnA, 3);

		Point btnB[3] = { Point(scrollBarTrack.left + 3, scrollBarTrack.bottom - 2),

			Point(scrollBarTrack.left - 1, scrollBarTrack.bottom - 10),

			Point(scrollBarTrack.right - 2, scrollBarTrack.bottom - 10) };

		g->FillPolygon(&brush, btnB, 3);



		// Grip

		grip = CRect(scrollBarTrack.left, scrollBarTrack.top + 11 + (!isnan(scrollView->PositionDelta) ? scrollView->WindowPos : 0), scrollBarTrack.right - 1, scrollBarTrack.top + 11 + (!isnan(scrollView->PositionDelta) ? scrollView->WindowPos : 0) + scrollView->GripSize);

		dc->FillSolidRect(grip, ScreenBlue.ToCOLORREF());

		dc->Draw3dRect(grip, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());

		InflateRect(grip, -1, -1);

		dc->Draw3dRect(grip, BevelLight.ToCOLORREF(), BevelDark.ToCOLORREF());



		// '3d' border trick

		g->DrawLine(&lighterPen, btnA[0], btnA[2]);

		g->DrawLine(&darkerPen, btnA[1], btnA[2]);

		g->DrawLine(&darkerPen, btnA[0], btnA[1]);

		g->DrawLine(&lighterPen, btnB[0], btnB[2]);

		g->DrawLine(&darkerPen, btnB[1], btnB[2]);

		g->DrawLine(&darkerPen, btnB[0], btnB[1]);

	}



	// Screen objects

	screen->AddScreenObject(scrollView->Type, to_string(scrollView->Id).c_str(), grip, true, "");



	// Cleanup



	// Restore device context

	dc->RestoreDC(sDC);

}



void CCommonRenders::RenderTracks(CDC* dc, Graphics* g, CRadarScreen* screen, COverlayType type, CMenuBar* menubar) {

	// Save context

	int iDC = dc->SaveDC();



	// Pen

	Pen pen(TextWhite, 2);



	// Font

	FontSelector::SelectMonoFont(14, dc);

	dc->SetTextColor(TextWhite.ToCOLORREF());

	dc->SetTextAlign(TA_CENTER);



	// Anti-aliasing

	g->SetSmoothingMode(SmoothingModeAntiAlias);



	// Loop tracks
	map<string, CTrack> tracksToRender;
	{
		lock_guard<mutex> lock(CRoutesHelper::TracksMutex);
		tracksToRender = CRoutesHelper::CurrentTracks;
	}

	for (auto const& kv : tracksToRender) {
		// Show eastbound/eastbound only if that type is selected
		if (type == COverlayType::TCKS_EAST && kv.second.Direction != CTrackDirection::EAST) {
			continue;
		}
		else if (type == COverlayType::TCKS_WEST && kv.second.Direction != CTrackDirection::WEST) {
			continue;
		}
		
		// Show selected overlays
		if (type == COverlayType::TCKS_SEL) {
			vector<string> tracks;
			menubar->GetSelectedTracks(tracks);
			bool show = false;
			for (int i = 0; i < tracks.size(); i++) {
				if (kv.first == tracks[i]) {
					show = true;
				}
			}
			if (!show) {
				continue;
			}
		}

		// Build resolved route positions - look up named waypoints from cache
		vector<CPosition> resolvedRoute;
		for (size_t i = 0; i < kv.second.Route.size(); i++) {
			CPosition pos;
			bool found = false;
			
			// Check if we have a valid coordinate in RouteRaw (same index if sizes match)
			if (i < kv.second.RouteRaw.size()) {
				pos = kv.second.RouteRaw[i];
				// Check if position is valid
				if (pos.m_Latitude > 30.0 && pos.m_Latitude < 80.0) {
					found = true;
				}
			}
			
			// If not found, try to look up the waypoint name from cache
			if (!found) {
				string waypointName = kv.second.Route[i];
				lock_guard<mutex> lock(CRoutesHelper::FixCacheMutex);
				if (CRoutesHelper::FixCache.count(waypointName)) {
					pos = CRoutesHelper::FixCache[waypointName];
					found = true;
				}
			}
			
			// Only add valid positions
			if (found) {
				resolvedRoute.push_back(pos);
			}
		}
		
		// Skip if no valid route points
		if (resolvedRoute.empty()) {
			continue;
		}

		// Move to start and draw 
		POINT pointCoord = screen->ConvertCoordFromPositionToPixel(resolvedRoute[0]);
		string id = kv.first;
		if (kv.second.Direction == CTrackDirection::EAST) {
			dc->TextOut(pointCoord.x - 12, pointCoord.y - 5, id.c_str());
		}
		else {
			dc->TextOut(pointCoord.x + 12, pointCoord.y - 5, id.c_str());
		}

		// Draw lines
		for (size_t i = 0; i < resolvedRoute.size(); i++) {
			POINT nextPoint = screen->ConvertCoordFromPositionToPixel(resolvedRoute[i]);
			g->DrawLine(&pen, pointCoord.x, pointCoord.y, nextPoint.x, nextPoint.y);
			pointCoord = nextPoint;
		}
	}



	// Cleanup



	// Restore context
	dc->RestoreDC(iDC);
}

void CCommonRenders::RenderAdjacentSectors(CDC* dc, Graphics* g, CRadarScreen* screen, void* fltPlnWindowPtr) {
	if (!fltPlnWindowPtr) return;
	CFlightPlanWindow* fltPlnWindow = (CFlightPlanWindow*)fltPlnWindowPtr;

	// Mapping of callsign prefixes to sector boundary names
	static map<string, string> AdjacentSectorMap = {
		{"EISN", "EISN"},     // Shannon
		{"EGPX", "EGPX"},     // Scottish
		{"LFRR", "LFRR"},     // Brest
		{"LPPO", "LPPO"},     // Santa Maria
		{"BIRD", "BIRD"},     // Reykjavik
		{"CZQX", "CZQX"},     // Gander
		{"CZQM", "CZQM"},     // Moncton
		{"CZUL", "CZUL"},     // Montreal
		{"ZBW", "ZBW"},       // Boston
		{"ZNY", "ZNY"}        // New York
	};

	// Iterate through online controllers
	for (auto it = fltPlnWindow->onlineControllers.begin(); it != fltPlnWindow->onlineControllers.end(); ++it) {
		string callsign = it->first;
		CController controller = it->second;

		string prefix = callsign.substr(0, 4);
		if (AdjacentSectorMap.find(prefix) != AdjacentSectorMap.end()) {
			string boundaryName = AdjacentSectorMap[prefix];

			// Search for boundary element
			CSectorElement element;
			for (element = screen->GetPlugIn()->SectorFileElementSelectFirst(11); // 11 = SECTOR_ELEMENT_ARTC
				element.IsValid();
				element = screen->GetPlugIn()->SectorFileElementSelectNext(element, 11))
			{
				if (string(element.GetName()) == boundaryName) {
					// Draw boundary
					vector<Point> points;
					CPosition pos;
					for (int i = 0; element.GetPosition(&pos, i); ++i) {
						POINT pt = screen->ConvertCoordFromPositionToPixel(pos);
						points.push_back(Point(pt.x, pt.y));
					}

					if (!points.empty()) {
						// Highlight area
						SolidBrush highlightBrush(Color(40, 255, 255, 0)); // Semi-transparent yellow
						g->FillPolygon(&highlightBrush, points.data(), points.size());

						// Draw frequency label
						int minX = points[0].X, maxX = points[0].X, minY = points[0].Y, maxY = points[0].Y;
						for (size_t pIdx = 0; pIdx < points.size(); ++pIdx) {
							if (points[pIdx].X < minX) minX = points[pIdx].X;
							if (points[pIdx].X > maxX) maxX = points[pIdx].X;
							if (points[pIdx].Y < minY) minY = points[pIdx].Y;
							if (points[pIdx].Y > maxY) maxY = points[pIdx].Y;
						}

						POINT labelPt = { minX + (maxX - minX) / 2, minY + (maxY - minY) / 2 };

						// Ensure it's on screen
						CRect radarArea = screen->GetRadarArea();
						if (labelPt.x < radarArea.left) labelPt.x = radarArea.left + 50;
						if (labelPt.x > radarArea.right) labelPt.x = radarArea.right - 50;
						if (labelPt.y < radarArea.top) labelPt.y = radarArea.top + 50;
						if (labelPt.y > radarArea.bottom) labelPt.y = radarArea.bottom - 50;

						char freqBuf[10];
						sprintf_s(freqBuf, "%.3f", controller.GetPrimaryFrequency());
						string label = string(element.GetName()) + " " + freqBuf;

						int sDC = dc->SaveDC();
						FontSelector::SelectATCFont(14, dc);
						dc->SetTextColor(RGB(255, 255, 0)); // Yellow
						dc->SetTextAlign(TA_CENTER);
						dc->TextOut(labelPt.x, labelPt.y, label.c_str());
						dc->RestoreDC(sDC);
					}
					break; // Found it
				}
			}
		}
	}
}



void CCommonRenders::RenderRoutes(CDC* dc, Graphics* g, CRadarScreen* screen) {

	// Save context

	int iDC = dc->SaveDC();



	// Pen & brush

	Pen pen(TargetOrange, 2);

	SolidBrush brush(TargetOrange);



	// Font

	FontSelector::SelectMonoFont(12, dc);

	dc->SetTextColor(TargetOrange.ToCOLORREF());

	dc->SetTextAlign(TA_LEFT);



	// Anti-aliasing

	g->SetSmoothingMode(SmoothingModeAntiAlias);





	// Loop through each aircraft in the vector

	for (int i = 0; i < CRoutesHelper::ActiveRoutes.size(); i++) {

		// Get route

		vector<CRoutePosition> route;

		CRoutesHelper::GetRoute(screen, &route, CRoutesHelper::ActiveRoutes.at(i));



		// If route not there then we skip
		if (route.empty()) {
			continue;
		}



		// Now we loop through each waypoint and draw the route

		POINT lastPoint = screen->ConvertCoordFromPositionToPixel(route.at(0).PositionRaw);

		for (int j = 0; j < route.size(); j++) {

			// Get point, text rectangle & define y offset

			string text = route.at(j).Fix; // To get TextExtent & check if AIRCRAFT



			// Only draw text if not aircraft position

			if (text != "AIRCRAFT") { 

				POINT point = screen->ConvertCoordFromPositionToPixel(route.at(j).PositionRaw);

				CRect box(point.x - (dc->GetTextExtent(text.c_str()).cx / 2), point.y + 10, point.x + (dc->GetTextExtent(text.c_str()).cx), point.y + 50);

				int offsetY = 0;



				// Draw dot

				Rect pointRect(point.x - 3, point.y - 3, 6, 6);

				g->FillEllipse(&brush, pointRect);



				// Print text for fix

				dc->TextOut(box.left, box.top, text.c_str());

				offsetY += 14;



				// Print text for estimate

				text = route.at(j).Estimate;

				dc->TextOut(box.left, box.top + offsetY, text.c_str());

				offsetY += 14;



				// Print text for flight level

				text = to_string(route.at(j).FlightLevel);

				dc->TextOut(box.left, box.top + offsetY, text.c_str());

			}

			

			// Draw line to

			g->DrawLine(&pen, lastPoint.x, lastPoint.y, screen->ConvertCoordFromPositionToPixel(route.at(j).PositionRaw).x, (screen->ConvertCoordFromPositionToPixel(route.at(j).PositionRaw).y));

			lastPoint = screen->ConvertCoordFromPositionToPixel(route.at(j).PositionRaw);

		}

	}



	// Cleanup



	// Restore context

	dc->RestoreDC(iDC);

}



void CCommonRenders::RenderQDM(CDC* dc, Graphics* g, CRadarScreen* screen, CPosition* position1, CPosition* position2, POINT cursorPosition, CPosition* cursorLatLon) {

	// Save context

	int iDC = dc->SaveDC();



	// Get raw screen coordinates

	POINT rawPoint1(screen->ConvertCoordFromPositionToPixel(*position1));

	POINT rawPoint2(screen->ConvertCoordFromPositionToPixel(*position2));



	// Convert to Point objects

	Point point1(rawPoint1.x, rawPoint1.y);

	Point point2(rawPoint2.x, rawPoint2.y);



	// Drawing tools

	Pen pen(DarkGrey, 2);

	SolidBrush brush(DarkGrey);



	// Font

	FontSelector::SelectMonoFont(12, dc);

	dc->SetTextColor(TextWhite.ToCOLORREF());

	dc->SetTextAlign(TA_CENTER);



	// Anti-aliasing

	g->SetSmoothingMode(SmoothingModeAntiAlias);



	// Define the positions

	Rect positionRect1(point1.X - 2, point1.Y - 2, 4, 4);

	Rect positionRect2(point2.X - 2, point2.Y - 2, 4, 4);



	// Fill number 1

	g->FillEllipse(&brush, positionRect1);



	// If number 2 defined fill it and draw line between, otherwise draw line to cursor position

	if (position2->m_Latitude != 0.0 && position2->m_Longitude != 0.0) {

		// Draw dots and connect

		g->FillEllipse(&brush, positionRect2);

		g->DrawLine(&pen, point1, point2);



		// Draw the lat lon text

		if (point1.Y < point2.Y) {

			// Point 1 has text on top

			dc->TextOut(point1.X, point1.Y - 15, CUtils::GetLatLonString(position1, false, 2, false).c_str());

			dc->TextOut(point2.X, point2.Y + 10, CUtils::GetLatLonString(position2, false, 2, false).c_str());

		}

		else {

			// Point 2 has text on top

			dc->TextOut(point2.X, point2.Y - 15, CUtils::GetLatLonString(position2, false, 2, false).c_str());

			dc->TextOut(point1.X, point1.Y + 10, CUtils::GetLatLonString(position1, false, 2, false).c_str());

		}



		// Draw distance text

		POINT midpoint = CUtils::GetMidPoint(rawPoint1, rawPoint2);

		dc->TextOut(midpoint.x, midpoint.y, (to_string((int)position1->DistanceTo(*position2)) + "nm").c_str());

	}

	else {

		g->DrawLine(&pen, point1, Point(cursorPosition.x, cursorPosition.y));



		// Draw the lat lon text

		if (point1.Y < cursorPosition.y) {

			// Point 1 has text on top

			dc->TextOut(point1.X, point1.Y - 15, CUtils::GetLatLonString(position1, false, 1, false).c_str());

			dc->TextOut(cursorPosition.x, cursorPosition.y + 10, CUtils::GetLatLonString(cursorLatLon, false, 2, false).c_str());

		}

		else {

			// Point 2 has text on top

			dc->TextOut(cursorPosition.x, cursorPosition.y - 15, CUtils::GetLatLonString(cursorLatLon, false, 2, false).c_str());

			dc->TextOut(point1.X, point1.Y + 10, CUtils::GetLatLonString(position1, false, 1, false).c_str());

		}



		// Draw distance text

		POINT midpoint = CUtils::GetMidPoint(cursorPosition, rawPoint1);

		dc->TextOut(midpoint.x, midpoint.y, (to_string((int)position1->DistanceTo(*cursorLatLon)) + "nm").c_str());

	}



	// Cleanup

	// Restore context

	dc->RestoreDC(iDC);

}

