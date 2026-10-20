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
  GET /GetNetworkTraffic                   -> oceanic-bound flights found in the public VATSIM data feed (not from the plugin)
  GET /GetNatTrakData                      -> natTrak clearances/requests (from https://nattrak.vatsim.net/api/plugins)
  GET /PostNamedFix?code=KEY&name=..&lat=..&lon=..  -> add a fix coordinates are missing for (admin page)
  If a "web" folder exists next to this file, the built flight data display is served from it.
  Controller login for the flight data display (editing): POST /auth/password, GET /auth/login + /auth/callback (VATSIM Connect),
  GET /auth/status, POST /auth/position, POST /auth/logout, POST /EditFlight (needs a login with a position).
  GET /health      GET /unresolved (fixes we could not find coordinates for)   GET /  (simple info page)

Settings (environment variables): VNAAATS_API_KEY (required), VNAAATS_PORT (8080), VNAAATS_DB, VNAAATS_NATTRAK_URL,
VNAAATS_REFRESH_SECONDS (600), VNAAATS_BIND (127.0.0.1), VNAAATS_VATSIM_FEED (feed url; set to 'off' to disable),
VNAAATS_FEED_SECONDS (60), VNAAATS_NATTRAK_PLUGIN_URL (natTrak clearance API; 'off' to disable).
Login (editing from the web): VNAAATS_FDD_PASSWORD (shared password), VNAAATS_POSITIONS (comma list shown on the position screen),
VNAAATS_CONNECT_CLIENT_ID / VNAAATS_CONNECT_CLIENT_SECRET / VNAAATS_PUBLIC_URL (e.g. https://fdd.example.com) for VATSIM Connect,
VNAAATS_CONNECT_BASE (https://auth.vatsim.net, sandbox: https://auth-dev.vatsim.net), VNAAATS_ALLOWED_CIDS (comma list; if empty
any VATSIM member rated VNAAATS_MIN_RATING or higher, default 2 = S1, may log in).
"""
import hmac
import http.cookies
import json
import logging
import os
import re
import secrets
import sqlite3
import threading
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

HERE = os.path.dirname(os.path.abspath(__file__))
log = logging.getLogger("vnaaats")

NATTRAK_URL = os.environ.get("VNAAATS_NATTRAK_URL", "https://nattrak.vatsim.net/api/tracks")
REFRESH = int(os.environ.get("VNAAATS_REFRESH_SECONDS", "600"))
STALE_FLIGHT_SECONDS = 24 * 3600
SERVER_VERSION = "0.4.0"      # shown at /health so you can tell which version is running
VATSIM_FEED = os.environ.get("VNAAATS_VATSIM_FEED", "https://data.vatsim.net/v3/vatsim-data.json")
FEED_SECONDS = int(os.environ.get("VNAAATS_FEED_SECONDS", "60"))
NATTRAK_PLUGIN_URL = os.environ.get("VNAAATS_NATTRAK_PLUGIN_URL", "https://nattrak.vatsim.net/api/plugins")
SESSION_COOKIE = "vnaaats_session"
SESSION_SECONDS = 12 * 3600
DEFAULT_POSITIONS = "CZQO_CTR,CZQO_DEL,EGGX_CTR,EGGX_DEL,NATX_CTR,OBSERVER"
EDIT_FIELDS = ("assignedLevel", "assignedMach", "track", "selcal")       # what the web and the plugin both edit
SELCAL_OK = re.compile(r"[A-HJ-MP-S]{4}")


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
        now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        rec = {
            "callsign": cs, "type": g("type"),
            "assignedLevel": to_int(g("level")), "assignedMach": to_int(g("mach")),
            "track": g("track"), "route": g("route"), "routeEtas": g("routeEtas"),
            "departure": g("departure"), "arrival": g("arrival"),
            "direction": to_bool(g("direction")), "etd": g("etd"), "selcal": g("selcal"),
            "datalinkConnected": to_bool(g("datalinkConnected")), "isEquipped": to_bool(g("isEquipped")),
            "state": g("state"), "relevant": to_bool(g("relevant")), "trackedBy": g("trackedBy"),
            "trackedById": g("trackedById"), "targetMode": to_int(g("targetMode")),
            "lastUpdated": now,
        }
        by = (g("by") or "Plugin")[:40]
        with self.lock:
            row = self.db.execute("SELECT data FROM flights WHERE callsign=?", (cs,)).fetchone()
            old = json.loads(row[0]) if row else None
            if old and old.get("webEditPending"):
                # a controller edited this on the web and the plugin has not fetched it yet: keep the web values
                for k in EDIT_FIELDS:
                    rec[k] = old.get(k, rec[k])
                rec["webEditPending"] = True
            self._track_changes(rec, old, by, now, ("route", "state") + EDIT_FIELDS)
            self.db.execute("INSERT OR REPLACE INTO flights VALUES (?,?,?)", (cs, json.dumps(rec), time.time()))
            self.db.commit()
        return rec

    @staticmethod
    def _track_changes(rec, old, by, now, fields):
        """Keep updatedBy / updatedAt / history only moving when something that matters actually changed."""
        changes = {}
        if old:
            for k in fields:
                if old.get(k) != rec.get(k):
                    changes[k] = [old.get(k), rec.get(k)]
            rec["history"] = old.get("history", [])
            if not changes:
                rec["updatedBy"], rec["updatedAt"] = old.get("updatedBy", by), old.get("updatedAt", now)
                return
        else:
            rec["history"] = []
        rec["updatedBy"], rec["updatedAt"] = by, now
        if changes:
            rec["history"] = (rec["history"] + [{"at": now, "by": by, "changes": changes}])[-10:]

    def edit(self, callsign, changes, by, base=None):
        """Apply a web edit. base is a record to start from when the flight is only known from the VATSIM feed."""
        cs = callsign.strip().upper()
        now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        with self.lock:
            row = self.db.execute("SELECT data FROM flights WHERE callsign=?", (cs,)).fetchone()
            old = json.loads(row[0]) if row else None
            if old is None and base is None:
                return None
            rec = dict(old if old is not None else base)
            rec["callsign"] = cs
            for k, v in changes.items():
                rec[k] = v
            rec["lastUpdated"] = now
            rec["webEditPending"] = True
            self._track_changes(rec, old if old is not None else dict(base, history=[]), by, now, EDIT_FIELDS)
            self.db.execute("INSERT OR REPLACE INTO flights VALUES (?,?,?)", (cs, json.dumps(rec), time.time()))
            self.db.commit()
        return rec

    def ack_web_edit(self, callsign):
        cs = callsign.strip().upper()
        with self.lock:
            row = self.db.execute("SELECT data FROM flights WHERE callsign=?", (cs,)).fetchone()
            if not row:
                return
            rec = json.loads(row[0])
            if rec.get("webEditPending"):
                rec["webEditPending"] = False
                self.db.execute("UPDATE flights SET data=? WHERE callsign=?", (json.dumps(rec), cs))
                self.db.commit()

    def get(self, callsign):
        with self.lock:
            row = self.db.execute("SELECT data FROM flights WHERE callsign=?", (callsign.strip().upper(),)).fetchone()
        return json.loads(row[0]) if row else None

    def purge(self):
        with self.lock:
            self.db.execute("DELETE FROM flights WHERE updated < ?", (time.time() - STALE_FLIGHT_SECONDS,))
            self.db.commit()


# ----------------------------------------------------------------------------- network traffic
_SPEEDLEVEL = re.compile(r"/[NMK]\d{3,4}[FASM]\d{3,4}$")


def oceanic_point(token):
    """Coordinate waypoints as used on the North Atlantic: '52/50', '5730/30', '52N050W', '5230N05000W'.
    Returns (name, lat, lon) or None."""
    t = _SPEEDLEVEL.sub("", token.upper())
    m = re.fullmatch(r"(\d{2})(\d{2})?/(\d{2,3})", t)
    if m:
        lat = int(m.group(1)) + (int(m.group(2)) / 60.0 if m.group(2) else 0.0)
        return t, lat, -float(m.group(3))
    m = re.fullmatch(r"(\d{2})(\d{2})?([NS])(\d{3})(\d{2})?([EW])", t)
    if m:
        lat = int(m.group(1)) + (int(m.group(2)) / 60.0 if m.group(2) else 0.0)
        lon = int(m.group(4)) + (int(m.group(5)) / 60.0 if m.group(5) else 0.0)
        if m.group(3) == "S":
            lat = -lat
        if m.group(6) == "W":
            lon = -lon
        return t, lat, lon
    return None


def parse_level(alt):
    """Flight plan altitude ('39000', 'FL390', 'F390', '390', 'A050') -> flight level (390)."""
    digits = re.sub(r"\D", "", alt or "")
    if not digits:
        return 0
    n = int(digits)
    if alt.upper().startswith("A"):
        return n
    return n // 100 if n >= 1000 else n


def _plain_fix(token):
    """A named 5-letter fix (ALLRY, MALOT) - not DCT, an airway, a procedure or a coordinate."""
    t = _SPEEDLEVEL.sub("", token.upper())
    return t if re.fullmatch(r"[A-Z]{5}", t) and t != "DCT" else None


def convert_network_pilot(pilot, tracks):
    """One pilot from the VATSIM data feed -> a flight record, or None if the route does not cross the ocean.
    The route is the oceanic coordinates plus the named entry fix before them and the exit fix after them."""
    fp = pilot.get("flight_plan") or {}
    route_str = fp.get("route") or ""
    tokens = route_str.split()
    idx = []                                                  # (index in route, (name, lat, lon)) of oceanic coordinates
    for i, tok in enumerate(tokens):
        p = oceanic_point(tok)
        if p and 40 <= p[1] <= 72 and -70 <= p[2] <= -4:
            idx.append((i, p))
    if len(idx) < 2:
        return None
    pts = [p for _, p in idx]
    entry = exit_ = None
    j = idx[0][0] - 1                                         # entry fix: the token before the first coordinate (skipping DCT)
    while j >= 0 and tokens[j].upper() == "DCT":
        j -= 1
    if j >= 0:
        entry = _plain_fix(tokens[j])
    j = idx[-1][0] + 1                                        # exit fix: the token after the last coordinate (skipping DCT)
    while j < len(tokens) and tokens[j].upper() == "DCT":
        j += 1
    if j < len(tokens):
        exit_ = _plain_fix(tokens[j])
    names = ([entry] if entry else []) + [p[0] for p in pts] + ([exit_] if exit_ else [])
    coord_names = [p[0] for p in pts]
    direction = pts[-1][2] > pts[0][2]                        # moving towards Europe = eastbound
    track = "RR"
    flown = {(round(p[1], 2), round(p[2], 2)) for p in pts}   # compare positions, not spelling (61/50 == 61N050W)
    for t in tracks:                                          # on a track if it passes the whole track's coordinate fixes
        tpts = [oceanic_point(r["name"]) for r in t.get("route", [])]
        tpos = [(round(q[1], 2), round(q[2], 2)) for q in tpts if q]
        if len(tpos) >= 3 and all(pos in flown for pos in tpos):
            track = t["id"]
            break
    return {
        "callsign": pilot.get("callsign", "").upper(), "type": fp.get("aircraft_short") or "",
        "assignedLevel": parse_level(fp.get("altitude")), "assignedMach": 0,
        "track": track, "route": " ".join(names), "routeEtas": " ".join(["--"] * len(names)),
        "departure": fp.get("departure") or "", "arrival": fp.get("arrival") or "",
        "direction": direction, "etd": fp.get("deptime") or "", "selcal": "",
        "datalinkConnected": False, "isEquipped": True, "state": "", "relevant": True,
        "trackedBy": "", "trackedById": "", "targetMode": 3,
        "lastUpdated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "source": "vatsim",
    }



class NatTrakFeed:
    """natTrak clearances and pending requests (public endpoint, no login). Keeps the last good copy."""
    def __init__(self, url=NATTRAK_PLUGIN_URL):
        self.url = url
        self.lock = threading.Lock()
        self.items, self.updated, self.error = [], None, None

    def refresh(self, fetch=None):
        try:
            if fetch:
                data = fetch()
            else:
                req = urllib.request.Request(self.url, headers={"User-Agent": "vnaaats-server"})
                with urllib.request.urlopen(req, timeout=30) as r:
                    data = json.loads(r.read().decode("utf-8"))
            if not isinstance(data, list):
                raise ValueError("unexpected natTrak response")
            with self.lock:
                self.items, self.updated, self.error = data, time.time(), None
            log.info("natTrak refreshed: %d messages", len(data))
        except Exception as e:
            with self.lock:
                self.error = str(e)
            log.error("natTrak refresh failed: %s", e)

    def loop(self):
        while True:
            self.refresh()
            time.sleep(FEED_SECONDS)

    def all(self):
        with self.lock:
            return list(self.items)

    def by_callsign(self):
        out = {}
        for it in self.all():                              # later entries (newest) win
            if it.get("callsign"):
                out[it["callsign"].upper()] = it
        return out


class NetworkTraffic:
    """Keeps the oceanic-bound flights from the public VATSIM data feed (flight plan data only)."""
    def __init__(self, cache, url=VATSIM_FEED, natrak=None):
        self.cache, self.url, self.natrak = cache, url, natrak
        self.lock = threading.Lock()
        self.flights, self.updated, self.error = [], None, None

    def refresh(self, fetch=None):
        try:
            if fetch:
                data = fetch()
            else:
                req = urllib.request.Request(self.url, headers={"User-Agent": "vnaaats-server"})
                with urllib.request.urlopen(req, timeout=30) as r:
                    data = json.loads(r.read().decode("utf-8"))
            with self.cache.lock:
                tracks = list(self.cache.tracks)
            out = []
            for pilot in data.get("pilots", []):
                rec = convert_network_pilot(pilot, tracks)
                if rec and rec["callsign"]:
                    out.append(rec)
            with self.lock:
                self.flights, self.updated, self.error = out, time.time(), None
            log.info("Network traffic refreshed: %d oceanic flights", len(out))
        except Exception as e:
            with self.lock:
                self.error = str(e)
            log.error("Network traffic refresh failed: %s", e)

    def loop(self):
        while True:
            self.refresh()
            time.sleep(FEED_SECONDS)

    def all(self):
        with self.lock:
            flights = [dict(f) for f in self.flights]
        if not self.natrak:
            return flights
        clearances = self.natrak.by_callsign()
        with self.cache.lock:
            track_ids = {t["id"] for t in self.cache.tracks}
        for f in flights:                                  # fill in what the controller cleared on natTrak
            c = clearances.get(f["callsign"])
            if not c or c.get("status") != "CLEARED":
                continue
            if str(c.get("level", "")).isdigit():
                f["assignedLevel"] = int(c["level"])
            try:
                f["assignedMach"] = int(round(float(c["mach"]) * 100))
            except (TypeError, ValueError):
                pass
            if c.get("nat") in track_ids:
                f["track"] = c["nat"]
            f["trackedBy"] = "NATTRAK"
        return flights


# ----------------------------------------------------------------------------- login
class Auth:
    """Who may edit flights from the web. VATSIM Connect and/or a shared password. Sessions live in memory (a restart logs everyone out)."""

    def __init__(self, env=os.environ):
        self.password = env.get("VNAAATS_FDD_PASSWORD", "")
        self.client_id = env.get("VNAAATS_CONNECT_CLIENT_ID", "")
        self.client_secret = env.get("VNAAATS_CONNECT_CLIENT_SECRET", "")
        self.public_url = env.get("VNAAATS_PUBLIC_URL", "").rstrip("/")
        self.connect_base = env.get("VNAAATS_CONNECT_BASE", "https://auth.vatsim.net").rstrip("/")
        self.allowed_cids = {c.strip() for c in env.get("VNAAATS_ALLOWED_CIDS", "").split(",") if c.strip()}
        self.min_rating = int(env.get("VNAAATS_MIN_RATING", "2"))
        self.positions = [p.strip().upper() for p in env.get("VNAAATS_POSITIONS", DEFAULT_POSITIONS).split(",") if p.strip()]
        self.lock = threading.Lock()
        self.sessions = {}        # token -> {cid, name, via, position, expires}
        self.states = {}          # oauth state -> expires
        self.failures = {}        # ip -> [timestamps of failed password attempts]

    @property
    def connect_enabled(self):
        return bool(self.client_id and self.client_secret and self.public_url)

    @property
    def enabled(self):
        return bool(self.password) or self.connect_enabled

    def _clean(self):
        now = time.time()
        for t in [t for t, s in self.sessions.items() if s["expires"] < now]:
            del self.sessions[t]
        for t in [t for t, e in self.states.items() if e < now]:
            del self.states[t]

    def create_session(self, cid, name, via):
        token = secrets.token_urlsafe(32)
        with self.lock:
            self._clean()
            self.sessions[token] = {"cid": cid, "name": name, "via": via, "position": "", "expires": time.time() + SESSION_SECONDS}
        return token

    def get(self, token):
        with self.lock:
            s = self.sessions.get(token or "")
            return dict(s) if s and s["expires"] > time.time() else None

    def set_position(self, token, position):
        position = (position or "").strip().upper()
        if position == "":                                   # "change position": go back to the position screen
            with self.lock:
                s = self.sessions.get(token or "")
                if s:
                    s["position"] = ""
                return bool(s)
        if not re.fullmatch(r"[A-Z0-9_]{3,16}", position):
            return False
        with self.lock:
            s = self.sessions.get(token or "")
            if not s:
                return False
            s["position"] = position
            return True

    def logout(self, token):
        with self.lock:
            self.sessions.pop(token or "", None)

    def throttled(self, ip):
        now = time.time()
        with self.lock:
            fails = [t for t in self.failures.get(ip, []) if now - t < 300]
            self.failures[ip] = fails
            return len(fails) >= 5

    def record_failure(self, ip):
        with self.lock:
            self.failures.setdefault(ip, []).append(time.time())

    def check_password(self, given):
        return bool(self.password) and hmac.compare_digest(given.encode(), self.password.encode())

    def new_state(self):
        state = secrets.token_urlsafe(16)
        with self.lock:
            self._clean()
            self.states[state] = time.time() + 600
        return state

    def use_state(self, state):
        with self.lock:
            return self.states.pop(state, 0) > time.time()

    def authorize_url(self):
        return self.connect_base + "/oauth/authorize?" + urllib.parse.urlencode({
            "client_id": self.client_id, "redirect_uri": self.public_url + "/auth/callback",
            "response_type": "code", "scope": "full_name vatsim_details", "state": self.new_state()})

    def exchange_code(self, code, http=None):
        """Trade the code VATSIM sent back for the member's details. Returns (cid, name) or raises ValueError."""
        http = http or _http_json
        tok = http(self.connect_base + "/oauth/token", {
            "grant_type": "authorization_code", "client_id": self.client_id, "client_secret": self.client_secret,
            "redirect_uri": self.public_url + "/auth/callback", "code": code})
        user = http(self.connect_base + "/api/user", None, tok.get("access_token", "")).get("data", {})
        cid = str(user.get("cid", ""))
        name = (user.get("personal") or {}).get("name_full", "") or cid
        try:
            rating = int(((user.get("vatsim") or {}).get("rating") or {}).get("id", -1))
        except (TypeError, ValueError):
            rating = -1
        if not cid:
            raise ValueError("VATSIM did not return a CID")
        if cid not in self.allowed_cids and (self.allowed_cids or rating < self.min_rating):
            raise ValueError("This VATSIM account is not allowed to edit (CID %s)" % cid)
        return cid, name


def _http_json(url, form=None, bearer=None):
    req = urllib.request.Request(url, data=urllib.parse.urlencode(form).encode() if form else None)
    req.add_header("Accept", "application/json")
    if bearer:
        req.add_header("Authorization", "Bearer " + bearer)
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode("utf-8"))


def validate_edit(body):
    """Check an edit from the web. Returns (changes, error)."""
    changes = {}
    if "level" in body:
        lv = to_int(body["level"], -1)
        if not 1 <= lv <= 999:
            return None, "level must be 1-999 (e.g. 350)"
        changes["assignedLevel"] = lv
    if "mach" in body:
        m = to_int(str(body["mach"]).lstrip("Mm.").lstrip("0") or "0", -1)
        if not 1 <= m <= 199:
            return None, "mach must be e.g. 84 for M0.84"
        changes["assignedMach"] = m
    if "track" in body:
        t = str(body["track"]).strip().upper()
        if not re.fullmatch(r"[A-Z]{1,3}", t):
            return None, "track must be 1-3 letters (or RR)"
        changes["track"] = t
    if "selcal" in body:
        sc = re.sub(r"[-\s]", "", str(body["selcal"]).upper())
        if sc and not SELCAL_OK.fullmatch(sc):
            return None, "SELCAL must be 4 letters from A-H, J-M, P-S"
        changes["selcal"] = sc
    if not changes:
        return None, "nothing to change"
    return changes, None


# ----------------------------------------------------------------------------- http
MIME = {".html": "text/html", ".js": "application/javascript", ".css": "text/css", ".json": "application/json",
        ".png": "image/png", ".jpg": "image/jpeg", ".svg": "image/svg+xml", ".ico": "image/x-icon",
        ".woff": "font/woff", ".woff2": "font/woff2", ".txt": "text/plain", ".map": "application/json"}


def make_handler(cache, store, api_key, web_dir=None, network=None, natrak=None, auth=None):
    auth = auth or Auth({})
    secure_cookie = auth.public_url.startswith("https://")

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
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "*")
            self.end_headers()

        def _client_ip(self):
            fwd = self.headers.get("X-Forwarded-For", "")
            return fwd.split(",")[-1].strip() if fwd else self.client_address[0]

        def _token(self):
            c = http.cookies.SimpleCookie(self.headers.get("Cookie", ""))
            return c[SESSION_COOKIE].value if SESSION_COOKIE in c else ""

        def _session(self):
            return auth.get(self._token())

        def _set_cookie(self, token, max_age=SESSION_SECONDS):
            return "%s=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=%d%s" % (SESSION_COOKIE, token, max_age, "; Secure" if secure_cookie else "")

        def _send_cookie(self, code, body, token, max_age=SESSION_SECONDS):
            data = json.dumps(body).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Set-Cookie", self._set_cookie(token, max_age))
            self.end_headers()
            self.wfile.write(data)

        def _redirect(self, where, token=None):
            self.send_response(302)
            self.send_header("Location", where)
            self.send_header("Cache-Control", "no-store")
            if token:
                self.send_header("Set-Cookie", self._set_cookie(token))
            self.end_headers()

        def _status(self, session):
            return {"editing": auth.enabled, "password": bool(auth.password), "vatsim": auth.connect_enabled,
                    "loggedIn": bool(session), "name": session["name"] if session else "", "cid": session["cid"] if session else "",
                    "position": session["position"] if session else "", "positions": auth.positions}

        def do_POST(self):
            p = urlparse(self.path).path.rstrip("/") or "/"
            try:
                if "application/json" not in self.headers.get("Content-Type", ""):
                    return self._send(415, {"error": "send JSON"})          # also blocks cross-site form posts
                n = int(self.headers.get("Content-Length", "0") or 0)
                if n > 20000:
                    return self._send(413, {"error": "too large"})
                try:
                    body = json.loads(self.rfile.read(n).decode("utf-8") or "{}")
                    if not isinstance(body, dict):
                        raise ValueError
                except ValueError:
                    return self._send(400, {"error": "bad JSON"})
                if p == "/auth/password":
                    ip = self._client_ip()
                    if not auth.password:
                        return self._send(404, {"error": "password login is not set up"})
                    if auth.throttled(ip):
                        return self._send(429, {"error": "too many wrong passwords - wait 5 minutes"})
                    name = str(body.get("name", "")).strip()[:40]
                    if not name:
                        return self._send(400, {"error": "enter your name or CID"})
                    if not auth.check_password(str(body.get("password", ""))):
                        auth.record_failure(ip)
                        return self._send(401, {"error": "wrong password"})
                    token = auth.create_session(re.sub(r"\D", "", name)[:8], name, "password")
                    return self._send_cookie(200, self._status(auth.get(token)), token)
                if p == "/auth/logout":
                    auth.logout(self._token())
                    return self._send_cookie(200, self._status(None), "", 0)
                if p == "/auth/position":
                    if not self._session() or not auth.set_position(self._token(), str(body.get("position", ""))):
                        return self._send(400, {"error": "log in first and give a position like CZQO_CTR"})
                    return self._send(200, self._status(self._session()))
                if p == "/EditFlight":
                    sess = self._session()
                    if not sess:
                        return self._send(401, {"error": "log in first"})
                    if not sess["position"] or sess["position"] == "OBSERVER":
                        return self._send(403, {"error": "choose a controlling position to edit (observers are read-only)"})
                    changes, err = validate_edit(body)
                    if err:
                        return self._send(400, {"error": err})
                    cs = str(body.get("callsign", "")).strip().upper()
                    base = None
                    if network and not store.get(cs):
                        base = next((f for f in network.all() if f.get("callsign") == cs), None)
                    by = "%s (%s)" % (sess["position"], sess["name"])
                    rec = store.edit(cs, changes, by[:60], base)
                    return self._send(200, rec) if rec else self._send(404, {"error": "unknown flight"})
                return self._send(404, {"error": "unknown endpoint"})
            except Exception as e:
                log.exception("request failed")
                return self._send(500, {"error": str(e)})

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
                if p == "/auth/status":
                    return self._send(200, self._status(self._session()))
                if p == "/auth/login":
                    if not auth.connect_enabled:
                        return self._send(404, {"error": "VATSIM login is not set up"})
                    return self._redirect(auth.authorize_url())
                if p == "/auth/callback":
                    if not auth.connect_enabled or not auth.use_state(q.get("state", "")) or not q.get("code"):
                        return self._send(400, "Login failed or expired - go back and try again.", "text/plain")
                    try:
                        cid, name = auth.exchange_code(q["code"])
                    except Exception as e:
                        log.warning("VATSIM login refused: %s", e)
                        return self._send(403, "Login refused: %s" % e, "text/plain")
                    return self._redirect("/fdd", auth.create_session(cid, name, "vatsim"))
                if p == "/GetSingleFlightData":
                    rec = store.get(q.get("callsign", ""))
                    if rec:
                        store.ack_web_edit(q.get("callsign", ""))        # the plugin has now seen any web edit
                    return self._send(200, [rec]) if rec else self._send(404, {"error": "not found"})
                if p in ("/PostFlightData", "/UpdateFlightData"):
                    if not api_key or not hmac.compare_digest(q.get("code", ""), api_key):
                        return self._send(401, {"error": "bad key"})
                    rec = store.upsert(q)
                    return self._send(200, rec) if rec else self._send(400, {"error": "callsign required"})
                if p == "/GetAllFlightData":
                    return self._send(200, store.all_flights(newest_first=q.get("sort", "1") != "0"))
                if p == "/GetNetworkTraffic":
                    return self._send(200, network.all() if network else [])
                if p == "/GetNatTrakData":
                    return self._send(200, natrak.all() if natrak else [])
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
                        return self._send(200, {"ok": True, "version": SERVER_VERSION, "tracks": len(cache.tracks), "tracksUpdated": cache.updated, "trackError": cache.error})
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
    natrak = None
    if NATTRAK_PLUGIN_URL.lower() != "off":
        natrak = NatTrakFeed()
        threading.Thread(target=natrak.loop, daemon=True).start()
    network = None
    if VATSIM_FEED.lower() != "off":
        network = NetworkTraffic(cache, natrak=natrak)
        threading.Thread(target=network.loop, daemon=True).start()
    threading.Thread(target=lambda: [store.purge() or time.sleep(3600) for _ in iter(int, 1)], daemon=True).start()
    host, port = os.environ.get("VNAAATS_BIND", "127.0.0.1"), int(os.environ.get("VNAAATS_PORT", "8080"))
    log.info("Listening on %s:%d", host, port)
    auth = Auth()
    log.info("Web editing: %s", "VATSIM Connect + password" if auth.connect_enabled and auth.password else "VATSIM Connect" if auth.connect_enabled else "password" if auth.password else "OFF (set VNAAATS_FDD_PASSWORD)")
    ThreadingHTTPServer((host, port), make_handler(cache, store, api_key, web_dir if os.path.isdir(web_dir) else None, network, natrak, auth)).serve_forever()


if __name__ == "__main__":
    main()
