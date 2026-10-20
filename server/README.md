# vNAAATS server
Backend for the vNAAATS EuroScope plugin and the flight data display (`../web`). Python 3.8+, no packages needed.
Endpoints and settings: see the header of `vnaaats_server.py`.
Tests: `python3 -m unittest discover -s tests -v`
Install on Ubuntu: `bash deploy/setup.sh [domain]` (put the built `web` folder next to `deploy/` to serve the display).
Missing fix coordinates: see `/unresolved`, add them on the admin page `/admin/addfix` (uses the API key) or in `extra_fixes.json`.
The API key is generated on the server (`/etc/vnaaats.env`) and is never stored in this repository.
