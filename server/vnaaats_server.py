#!/usr/bin/env python3
"""
vNAAATS server - replaces the old api.vnaaats.net backend. No third-party packages needed (Python 3.8+).

Endpoints used by the plugin (same names as the old backend):
  GET /GetTrackSource                      -> "0"
  GET /GetAllNatTracks[?event=true]        -> NAT tracks (converted from https://nattrak.vatsim.net/api/tracks)
  GET /GetSingleFlightData?callsign=XXX    -> [ {flight data} ]   (404 if unknown)
  GET /PostFlightData?code=KEY&callsign=.. -> store a flight           (needs the API key)
  GET /UpdateFlightData?code=KEY&callsign=..-> update a flight         (needs the API key)
Extra:
  GET /GetAllFlightData[?sort=0|1]         -> every stored flight (flight data display)
  GET /GetNatTrakData                      -> [] for now (natTrak clearance data not connected yet)
  GET /PostNamedFix?code=KEY&name=..&lat=..&lon=..  -> add a fix coordinates are missing for (admin page)
  If a "web" folder exists next to this file, the built flight data display is served from it.
  GET /health      GET /unresolved (fixes we could not find coordinates for)   GET /  (simple info page)

Settings (environment variables): VNAAATS_API_KEY (required), VNAAATS_PORT (8080), VNAAATS_DB, VNAAATS_NATTRAK_URL,
VNAAATS_REFRESH_SECONDS (600), VNAAATS_BIND (127.0.0.1).
"""
import hmac
import json
import logging
import os
import re
import sqlite3
import threading
import time
import urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

HERE = os.path.dirname(os.path.abspath(__file__))
log = logging.getLogger("vnaaats")

NATTRAK_URL = os.environ.get("VNAAATS_NATTRAK_URL", "https://nattrak.vatsim.net/api/tracks")
REFRESH = int(os.environ.get("VNAAATS_REFRESH_SECONDS", "600"))
STALE_FLIGHT_SECONDS = 24 * 3600


# ----------------------------------------------------------------------------- fixes
def load_fixes():
    fixes = {}
    for name in ("fixes.json", "extra_fixes.json"):          # extra_fixes.json wins
        path = os.path.join(HERE, name)
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                fixes.update({k.upper(): v for k, v in json.load(f).items() if not k.startswith("_")})
    return fixes


def token_to_latlon(token, fixes):
    """'56/20' -> (56.0, -20.0); '5730/30' -> (57.5, -30.0); named fix -> lookup; else None."""
    m = re.fullmatch(r"(\d{2})(\d{2})?/(\d{2,3})", token)
    if m:
        lat = int(m.group(1)) + (int(m.group(2)) / 60.0 if m.group(2) else 0.0)
        lon = int(m.group(3))
        return round(lat, 6), -float(lon)                    # NAT tracks are west of Greenwich
    v = fixes.get(token.upper())
    return (v[0], v[1]) if v else None


# ----------------------------------------------------------------------------- tracks
def to_epoch_string(iso):
    """'2026-10-20T01:00:00.000000Z' -> '1792458000' (unix seconds as a string, which is what the plugin and web app expect)."""
    try:
        dt = datetime.fromisoformat(re.sub(r"\.\d+", "", iso).replace("Z", "+00:00"))
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return str(int(dt.timestamp()))
    except (ValueError, AttributeError):
        return ""


def convert_tracks(raw, fixes, unresolved):
    """nattrak JSON -> the format the plugin parses."""
    out, valid_from_dates = [], []
    for t in raw:
        if not t.get("active"):
            continue
        tokens = (t.get("last_routeing") or "").split()
        route = []
        for tok in tokens:
            ll = token_to_latlon(tok, fixes)
            if ll is None:
                unresolved[tok] = unresolved.get(tok, 0) + 1
                log.warning("No coordinates for fix %s (track %s)", tok, t.get("identifier"))
                continue
            route.append({"name": tok, "latitude": ll[0], "longitude": ll[1]})
        if len(route) < 2:
            continue
        direction = {"west": 1, "east": 2}.get(str(t.get("direction", "")).lower(), 0)
        vf = t.get("valid_from") or ""
        valid_from_dates.append(vf)
        out.append({
            "id": t["identifier"],
            "tmi": "",
            "direction": direction,
            "route": route,
            "flightLevels": [int(x) for x in (t.get("flight_levels") or [])],
            "validFrom": to_epoch_string(vf),
            "validTo": to_epoch_string(t.get("valid_to") or ""),
        })
    # TMI = day-of-year of the (earliest) validity start. UNVERIFIED against the official definition.
    tmi = ""
    dates = sorted(d for d in valid_from_dates if d)
    if dates:
        try:
            tmi = str(datetime.fromisoformat(dates[0].replace("Z", "+00:00")).timetuple().tm_yday)
        except ValueError:
            pass
    for t in out:
        t["tmi"] = tmi
    return out


class TrackCache:
    def __init__(self, fixes):
        self.fixes = fixes
        self.last_raw = []
        self.lock = threading.Lock()
        self.tracks = []
        self.unresolved = {}
        self.updated = None
        self.error = None

    def refresh(self, fetch=None):
        try:
            raw = fetch() if fetch else self._download()
            self.last_raw = raw
            unresolved = {}
            tracks = convert_tracks(raw, self.fixes, unresolved)
            with self.lock:
                self.tracks, self.unresolved = tracks, unresolved
                self.updated, self.error = time.time(), None
            log.info("Tracks refreshed: %d active", len(tracks))
        except Exception as e:                                # keep serving the last good copy
            with self.lock:
                self.error = str(e)
            log.error("Track refresh failed: %s", e)

    def add_fix(self, name, lat, lon):
        """Learn a fix and rebuild the track list from the last download straight away."""
        self.fixes[name.upper()] = [lat, lon]
        if self.last_raw:
            unresolved = {}
            tracks = convert_tracks(self.last_raw, self.fixes, unresolved)
            with self.lock:
                self.tracks, self.unresolved = tracks, unresolved

    @staticmethod
    def _download():
        req = urllib.request.Request(NATTRAK_URL, headers={"User-Agent": "vnaaats-server"})
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read().decode("utf-8"))

    def loop(self):
        while True:
            self.refresh()
            time.sleep(REFRESH)


# ----------------------------------------------------------------------------- flights
def to_bool(v):
    return str(v).strip().lower() in ("1", "true", "yes", "y")


def to_int(v, default=0):
    try:
        return int(float(v))
    except (TypeError, ValueError):
        return default


class FlightStore:
    def __init__(self, path):
        self.lock = threading.Lock()
        self.db = sqlite3.connect(path, check_same_thread=False)
        self.db.execute("CREATE TABLE IF NOT EXISTS flights (callsign TEXT PRIMARY KEY, data TEXT NOT NULL, updated REAL NOT NULL)")
        self.db.execute("CREATE TABLE IF NOT EXISTS fixes (name TEXT PRIMARY KEY, lat REAL NOT NULL, lon REAL NOT NULL)")
        self.db.commit()

    def all_flights(self, newest_first=True):
        with self.lock:
            rows = self.db.execute("SELECT data FROM flights ORDER BY updated %s" % ("DESC" if newest_first else "ASC")).fetchall()
        return [json.loads(r[0]) for r in rows]

    def set_fix(self, name, lat, lon):
        with self.lock:
            self.db.execute("INSERT OR REPLACE INTO fixes VALUES (?,?,?)", (name.upper(), lat, lon))
            self.db.commit()

    def get_fixes(self):
        with self.lock:
            return {n: [la, lo] for n, la, lo in self.db.execute("SELECT name, lat, lon FROM fixes")}

    def upsert(self, q):
        cs = q.get("callsign", "").strip().upper()
        if not cs:
            return None
        g = lambda k: q.get(k, "")
        rec = {
            "callsign": cs, "type": g("type"),
            "assignedLevel": to_int(g("level")), "assignedMach": to_int(g("mach")),
            "track": g("track"), "route": g("route"), "routeEtas": g("routeEtas"),
            "departure": g("departure"), "arrival": g("arrival"),
            "direction": to_bool(g("direction")), "etd": g("etd"), "selcal": g("selcal"),
            "datalinkConnected": to_bool(g("datalinkConnected")), "isEquipped": to_bool(g("isEquipped")),
            "state": g("state"), "relevant": to_bool(g("relevant")), "trackedBy": g("trackedBy"),
            "trackedById": g("trackedById"), "targetMode": to_int(g("targetMode")),
            "lastUpdated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        }
        with self.lock:
            self.db.execute("INSERT OR REPLACE INTO flights VALUES (?,?,?)", (cs, json.dumps(rec), time.time()))
            self.db.commit()
        return rec

    def get(self, callsign):
        with self.lock:
            row = self.db.execute("SELECT data FROM flights WHERE callsign=?", (callsign.strip().upper(),)).fetchone()
        return json.loads(row[0]) if row else None

    def purge(self):
        with self.lock:
            self.db.execute("DELETE FROM flights WHERE updated < ?", (time.time() - STALE_FLIGHT_SECONDS,))
            self.db.commit()


# ----------------------------------------------------------------------------- http
MIME = {".html": "text/html", ".js": "application/javascript", ".css": "text/css", ".json": "application/json",
        ".png": "image/png", ".jpg": "image/jpeg", ".svg": "image/svg+xml", ".ico": "image/x-icon",
        ".woff": "font/woff", ".woff2": "font/woff2", ".txt": "text/plain", ".map": "application/json"}


def make_handler(cache, store, api_key, web_dir=None):
    class Handler(BaseHTTPRequestHandler):
        server_version = "vnaaats-server"

        def log_message(self, fmt, *args):
            log.info("%s %s", self.address_string(), fmt % args)

        def _send(self, code, body, ctype="application/json"):
            data = body if isinstance(body, bytes) else (body if isinstance(body, str) else json.dumps(body)).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", ctype + "; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")      # the flight data display runs in a browser
            self.end_headers()
            self.wfile.write(data)

        def do_OPTIONS(self):
            self.send_response(204)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "*")
            self.end_headers()

        def _static(self, path):
            """Serve the built web app (flight data display) if a web folder is configured."""
            if not web_dir:
                return False
            rel = path.lstrip("/") or "index.html"
            full = os.path.realpath(os.path.join(web_dir, rel))
            root = os.path.realpath(web_dir)
            if not (full == root or full.startswith(root + os.sep)):
                return False
            if os.path.isdir(full):
                full = os.path.join(full, "index.html")
            if not os.path.isfile(full):
                alt = full + ".html"                                    # /fdd -> fdd.html
                if os.path.isfile(alt):
                    full = alt
                else:
                    return False
            with open(full, "rb") as f:
                body = f.read()
            self._send(200, body, MIME.get(os.path.splitext(full)[1].lower(), "application/octet-stream"))
            return True

        def do_GET(self):
            u = urlparse(self.path)
            q = {k: v[0] for k, v in parse_qs(u.query, keep_blank_values=True).items()}
            p = u.path.rstrip("/") or "/"
            try:
                if p == "/GetTrackSource":
                    return self._send(200, "0", "text/plain")
                if p == "/GetAllNatTracks":
                    with cache.lock:
                        return self._send(200, cache.tracks)
                if p == "/GetSingleFlightData":
                    rec = store.get(q.get("callsign", ""))
                    return self._send(200, [rec]) if rec else self._send(404, {"error": "not found"})
                if p in ("/PostFlightData", "/UpdateFlightData"):
                    if not api_key or not hmac.compare_digest(q.get("code", ""), api_key):
                        return self._send(401, {"error": "bad key"})
                    rec = store.upsert(q)
                    return self._send(200, rec) if rec else self._send(400, {"error": "callsign required"})
                if p == "/GetAllFlightData":
                    return self._send(200, store.all_flights(newest_first=q.get("sort", "1") != "0"))
                if p == "/GetNatTrakData":
                    return self._send(200, [])          # TODO: natTrak clearance data (source not connected yet)
                if p == "/PostNamedFix":
                    if not api_key or not hmac.compare_digest(q.get("code", ""), api_key):
                        return self._send(401, {"error": "bad key"})
                    name = q.get("name", "").strip().upper()
                    try:
                        lat, lon = float(q.get("lat", "")), float(q.get("lon", ""))
                    except ValueError:
                        return self._send(400, {"error": "lat and lon must be numbers"})
                    if not re.fullmatch(r"[A-Z0-9]{2,8}", name) or not (-90 <= lat <= 90) or not (-180 <= lon <= 180):
                        return self._send(400, {"error": "invalid fix"})
                    store.set_fix(name, lat, lon)
                    cache.add_fix(name, lat, lon)
                    return self._send(200, {"name": name, "lat": lat, "lon": lon})
                if p == "/health":
                    with cache.lock:
                        return self._send(200, {"ok": True, "tracks": len(cache.tracks), "tracksUpdated": cache.updated, "trackError": cache.error})
                if p == "/unresolved":
                    with cache.lock:
                        return self._send(200, cache.unresolved)
                if self._static(p):
                    return
                if p == "/":
                    return self._send(200, "<h1>vNAAATS server</h1><p>Backend for the vNAAATS EuroScope plugin.</p>", "text/html")
                return self._send(404, {"error": "unknown endpoint"})
            except Exception as e:
                log.exception("request failed")
                return self._send(500, {"error": str(e)})

    return Handler


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    api_key = os.environ.get("VNAAATS_API_KEY", "")
    if len(api_key) < 16:
        raise SystemExit("Set VNAAATS_API_KEY to a long random secret (16+ characters).")
    store = FlightStore(os.environ.get("VNAAATS_DB", os.path.join(HERE, "vnaaats.db")))
    fixes = load_fixes()
    fixes.update(store.get_fixes())                           # fixes added through the admin page win
    cache = TrackCache(fixes)
    web_dir = os.environ.get("VNAAATS_WEB_DIR", os.path.join(HERE, "web"))
    threading.Thread(target=cache.loop, daemon=True).start()
    threading.Thread(target=lambda: [store.purge() or time.sleep(3600) for _ in iter(int, 1)], daemon=True).start()
    host, port = os.environ.get("VNAAATS_BIND", "127.0.0.1"), int(os.environ.get("VNAAATS_PORT", "8080"))
    log.info("Listening on %s:%d", host, port)
    ThreadingHTTPServer((host, port), make_handler(cache, store, api_key, web_dir if os.path.isdir(web_dir) else None)).serve_forever()


if __name__ == "__main__":
    main()
