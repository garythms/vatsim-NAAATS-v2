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

## Web editing, login and positions (server 0.4.0)

The flight data display can now change level, Mach, track and SELCAL. Only signed-in controllers with a position (not OBSERVER) can.
Reading (viewing the FDD) stays open. The plugin keeps using the API key for its own saves.

- **Password login** (works straight away): `VNAAATS_FDD_PASSWORD` in `/etc/vnaaats.env`. `deploy/setup.sh` creates one the first time and prints it. Five wrong tries from one address lock that address out for 5 minutes.
- **Positions** on the opening screen: `VNAAATS_POSITIONS=CZQO_CTR,CZQO_DEL,EGGX_CTR,...` (OBSERVER = view only). Anyone can also type another position.
- **VATSIM login** (needs HTTPS and a VATSIM Connect client; see below): `VNAAATS_CONNECT_CLIENT_ID`, `VNAAATS_CONNECT_CLIENT_SECRET`, `VNAAATS_PUBLIC_URL=https://your.domain`. Optional `VNAAATS_ALLOWED_CIDS=1234567,7654321` (only these CIDs) or `VNAAATS_MIN_RATING` (default 2 = S1 and above). For the VATSIM sandbox set `VNAAATS_CONNECT_BASE=https://auth-dev.vatsim.net`.
  Register the app at https://auth.vatsim.net (production needs an approved organisation); the redirect URL to register is `https://your.domain/auth/callback`.
- Each flight remembers `updatedBy` / `updatedAt` / the last 10 changes (`history`). A web edit is held until the plugin has fetched it, so a plugin that has not yet polled cannot overwrite it with an old value.

## CPDLC (server 0.5.0)

The server can act as the CPDLC ground station through Hoppie's ACARS. It is off until you set both of these in `/etc/vnaaats.env` and restart:

- `VNAAATS_HOPPIE_CODE` - your Hoppie logon code (private; type it on the server yourself).
- `VNAAATS_HOPPIE_STATION` - the station callsign pilots log on to (e.g. `CZQO`).
- Optional: `VNAAATS_HOPPIE_URL` (default `https://www.hoppie.nl/acars/system/connect.html`), `VNAAATS_CPDLC_AUTOACCEPT` (default 1: logon requests are accepted automatically).

How it works: the server polls Hoppie (about every minute, faster after traffic), accepts `REQUEST LOGON`, stores the messages and pings connected aircraft every 5 minutes (dropping them after two misses). Connected aircraft get `"cpdlc": true` in the flight data, so the plugin shows `[BAW123]` and the FDD shows brackets plus the CPDLC drawer (send/reply, quick templates, unread alerts). Only signed-in controllers with a real position can send.

Cautions:
- Only one program may poll a given station callsign at a time (polling removes the messages). Do not use a callsign that TopSky or another client is logged on with - use a test callsign first.
- LOGOFF handling and exact reply behaviour have not yet been checked against the live Hoppie system.
