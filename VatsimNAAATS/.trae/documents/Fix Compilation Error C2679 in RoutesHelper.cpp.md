## Bug Analysis
The compilation error `C2679` in [RoutesHelper.cpp](file:///d:/Development/vatsim-NAAATS-v2/VatsimNAAATS/RoutesHelper.cpp) is a type mismatch.
- `trackPoints` is declared as `std::vector<CPosition>`.
- `NatSM`, `NatSN`, `NatSP`, `NatSL`, and `NatSO` are declared as `std::vector<CWaypoint>`.
- `CWaypoint` is a struct containing a `string Name` and a `CPosition Position`.
- C++ cannot automatically convert or assign a `vector<CWaypoint>` to a `vector<CPosition>`.

## Proposed Fix
I will modify [RoutesHelper.cpp](file:///d:/Development/vatsim-NAAATS-v2/VatsimNAAATS/RoutesHelper.cpp) to manually extract the `Position` member from each `CWaypoint` when assigning to `trackPoints`.

### Implementation Steps:
1.  **Modify `CRoutesHelper::GetRoute`**: Update the logic that handles NAT tracks (`SM`, `SN`, etc.) to use a loop for copying coordinates.
2.  **Verify consistency**: Ensure that fetching from `CurrentTracks` (which uses `RouteRaw` of type `vector<CPosition>`) still works correctly.
3.  **Rebuild**: Run the build command again to confirm the fix.

## Code Changes
```cpp
// In RoutesHelper.cpp
if (tid == "SM") { for (const auto& wp : NatSM) trackPoints.push_back(wp.Position); found = true; }
else if (tid == "SN") { for (const auto& wp : NatSN) trackPoints.push_back(wp.Position); found = true; }
else if (tid == "SP") { for (const auto& wp : NatSP) trackPoints.push_back(wp.Position); found = true; }
else if (tid == "SL") { for (const auto& wp : NatSL) trackPoints.push_back(wp.Position); found = true; }
else if (tid == "SO") { for (const auto& wp : NatSO) trackPoints.push_back(wp.Position); found = true; }
```

Please confirm if you would like me to proceed with this fix.