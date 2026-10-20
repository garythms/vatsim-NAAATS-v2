# vNAAATS server
Backend for the vNAAATS EuroScope plugin and the flight data display (`../web`). Python 3.8+, no packages needed.
Endpoints and settings: see the header of `vnaaats_server.py`.
Tests: `python3 -m unittest discover -s tests -v`
Install on Ubuntu: `bash deploy/setup.sh [domain]` (put the built `web` folder next to `deploy/` to serve the display).
Missing fix coordinates: see `/unresolved`, add them on the admin page `/admin/addfix` (uses the API key) or in `extra_fixes.json`.
The API key is generated on the server (`/etc/vnaaats.env`) and is never stored in this repository.


## Network traffic (other controllers' / all oceanic aircraft)

The server also reads the public VATSIM data feed (every `VNAAATS_FEED_SECONDS`, default 60) and keeps every pilot whose
filed route has North Atlantic coordinate waypoints (e.g. `52/50`, `5730/30`, `52N050W`). They are served at
`/GetNetworkTraffic` and the flight data display merges them in (flights posted through the plugin win over the same callsign).
This is filed flight plan data only - it contains no controller clearances, levels assigned, or estimates (those only exist when a
controller posts through the plugin). Set `VNAAATS_VATSIM_FEED=off` to turn it off.

## natTrak clearance data

`/GetNatTrakData` serves the public natTrak API (`https://nattrak.vatsim.net/api/plugins`, refreshed every `VNAAATS_FEED_SECONDS`):
for each callsign the status (CLEARED / PENDING), track, entry fix, level, Mach, estimate, clearance time and extra info.
The flight data display shows it in the "natTrak Information" panel. For flights found in the VATSIM feed, a CLEARED natTrak
clearance also fills in the level, Mach and track on the strip. Set `VNAAATS_NATTRAK_PLUGIN_URL=off` to disable.
(The natTrak controller pages `/controllers/clx/...` need a VATSIM login and are not used.)
