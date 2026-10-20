"""
CPDLC ground station for the vNAAATS server, talking to the Hoppie ACARS network (https://www.hoppie.nl/acars).

Each controller signs in to their own station callsign (CZQX, EGGX, NATX ...) with THEIR OWN Hoppie logon code, typed on the
FDD. The code is kept in memory only while that controller is signed in (never written to disk). Pilots send "REQUEST LOGON" to the station callsign; we accept it (automatically unless VNAAATS_CPDLC_AUTOACCEPT=0), remember which aircraft are connected, keep
a message log, and let signed-in controllers send uplinks from the flight data display.

Hoppie rules this follows (https://www.hoppie.nl/acars/system/tech.html):
  * poll every 45-75 s at random, every ~20 s for a short while after something was sent or received
  * 15 s connection timeout, never retry straight away
  * one logon per station callsign at a time (a second client using the same callsign gets "callsign already in use")
  * polling takes messages OUT of Hoppie's queue, so this only works if nobody else polls the same station callsign
The CpdlcHub at the bottom of this file manages the stations: one running CpdlcStation per station callsign.

CPDLC packet:  /data2/<MIN>/<MRN>/<RA>/<message>      (MIN = sender's message number, MRN = number being replied to,
RA = reply wanted: WU wilco/unable, AN affirm/negative, R roger, NE none, Y/N for pilot downlinks)
Poll answer:   ok {FROM type {packet}} {FROM type {packet}} ...      (or just "ok", or "error {reason}")
"""
import logging
import random
import re
import sqlite3
import threading
import time
import urllib.parse
import urllib.request

log = logging.getLogger("vnaaats.cpdlc")

DEFAULT_URL = "https://www.hoppie.nl/acars/system/connect.html"
UPLINK_RA = ("WU", "AN", "R", "NE")
PING_SECONDS = 300
PING_MISSES_TO_DROP = 2
FAST_POLL_SECONDS = 120          # keep polling every ~20 s for this long after traffic
MAX_TEXT = 240
REPLY_STATES = (("WILCO", "WILCO"), ("UNABLE", "UNABLE"), ("ROGER", "ROGER"), ("AFFIRM", "AFFIRM"),
                ("NEGATIVE", "NEGATIVE"), ("STANDBY", "STANDBY"))


# ----------------------------------------------------------------------------- protocol helpers
def parse_blocks(text):
    """'ok {A cpdlc {/data2/..}} {B telex {hi}}' -> ('ok', [(from, type, packet), ...]). Also handles 'error {reason}'."""
    text = (text or "").strip()
    head = text.split(" ", 1)[0].lower() if text else ""
    blocks, depth, start = [], 0, 0
    for i, ch in enumerate(text):
        if ch == "{":
            if depth == 0:
                start = i + 1
            depth += 1
        elif ch == "}" and depth > 0:
            depth -= 1
            if depth == 0:
                blocks.append(text[start:i])
    out = []
    for b in blocks:
        m = re.match(r"\s*(\S+)\s+(\S+)\s+\{(.*)\}\s*$", b, re.S)
        if m:
            out.append((m.group(1).upper(), m.group(2).lower(), m.group(3)))
    return head, out, blocks


def parse_cpdlc_packet(packet):
    """'/data2/15//WU/CONTACT @X@' -> dict(min=15, mrn=None, ra='WU', text='CONTACT @X@') or None."""
    parts = (packet or "").split("/", 5)
    if len(parts) < 6 or parts[1].lower() != "data2":
        return None
    try:
        mn = int(parts[2])
    except ValueError:
        return None
    try:
        mrn = int(parts[3]) if parts[3] else None
    except ValueError:
        mrn = None
    return {"min": mn, "mrn": mrn, "ra": parts[4].upper(), "text": parts[5].strip()}


def readable(text):
    """Hoppie marks variable parts with @; show them plainly."""
    return re.sub(r"\s+", " ", (text or "").replace("@", " ")).strip()


class HoppieClient:
    def __init__(self, url, logon, station, http=None):
        self.url, self.logon, self.station = url, logon, station
        self._http = http or self._real_http

    @staticmethod
    def _real_http(url, data):
        req = urllib.request.Request(url, data=data)             # POST, form encoded (keeps the logon code out of the URL)
        with urllib.request.urlopen(req, timeout=15) as r:
            return r.read().decode("utf-8", "replace")

    def request(self, to, type_, packet=""):
        form = urllib.parse.urlencode({"logon": self.logon, "from": self.station, "to": to, "type": type_, "packet": packet}).encode()
        return self._http(self.url, form)


# ----------------------------------------------------------------------------- the station
class CpdlcStation:
    def __init__(self, db_path, client, auto_accept=True, now=time.time):
        self.client = client
        self.auto_accept = auto_accept
        self.now = now
        self.lock = threading.RLock()
        self.db = sqlite3.connect(db_path, check_same_thread=False)
        self.db.execute("""CREATE TABLE IF NOT EXISTS cpdlc_messages (id INTEGER PRIMARY KEY AUTOINCREMENT, ts REAL NOT NULL,
            dir TEXT NOT NULL, callsign TEXT NOT NULL, min INTEGER, mrn INTEGER, ra TEXT, text TEXT NOT NULL,
            state TEXT NOT NULL DEFAULT '', seen INTEGER NOT NULL DEFAULT 1, by TEXT NOT NULL DEFAULT '')""")
        try:
            self.db.execute("ALTER TABLE cpdlc_messages ADD COLUMN station TEXT NOT NULL DEFAULT ''")
        except sqlite3.OperationalError:
            pass                                              # column already there
        self.db.execute("CREATE TABLE IF NOT EXISTS cpdlc_conn2 (station TEXT NOT NULL, callsign TEXT NOT NULL, since REAL NOT NULL, PRIMARY KEY (station, callsign))")
        self.db.commit()
        self.stop_flag = False
        # Connections are NOT restored: a station only polls while a controller is signed in to it, so after a stop pilots must log on again.
        self.db.execute("DELETE FROM cpdlc_conn2 WHERE station=?", (client.station,))
        self.db.commit()
        self.connected = {}
        self.next_min = 1
        self.fast_until = 0.0
        self.last_poll = None
        self.error = ""
        self.last_ping = 0.0

    # --- storage
    def _store(self, direction, callsign, mn, mrn, ra, text, state="", seen=1, by=""):
        with self.lock:
            cur = self.db.execute("INSERT INTO cpdlc_messages (ts, dir, callsign, min, mrn, ra, text, state, seen, by, station) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                                  (self.now(), direction, callsign, mn, mrn, ra, text, state, seen, by, self.client.station))
            self.db.commit()
            return cur.lastrowid

    def _row(self, r):
        return {"id": r[0], "ts": r[1], "dir": r[2], "callsign": r[3], "min": r[4], "mrn": r[5], "ra": r[6],
                "text": r[7], "readable": readable(r[7]), "state": r[8], "seen": bool(r[9]), "by": r[10]}

    COLS = "id, ts, dir, callsign, min, mrn, ra, text, state, seen, by"

    def messages(self, callsign=None, after=0, limit=200):
        with self.lock:
            if callsign:
                rows = self.db.execute("SELECT %s FROM cpdlc_messages WHERE station=? AND callsign=? AND id>? ORDER BY id DESC LIMIT ?" % self.COLS,
                                       (self.client.station, callsign.upper(), after, limit)).fetchall()
            else:
                rows = self.db.execute("SELECT %s FROM cpdlc_messages WHERE station=? AND id>? ORDER BY id DESC LIMIT ?" % self.COLS, (self.client.station, after, limit)).fetchall()
        return [self._row(r) for r in reversed(rows)]

    def mark_seen(self, callsign):
        with self.lock:
            self.db.execute("UPDATE cpdlc_messages SET seen=1 WHERE station=? AND callsign=? AND seen=0", (self.client.station, callsign.upper()))
            self.db.commit()

    def unread(self):
        with self.lock:
            return {c: n for c, n in self.db.execute("SELECT callsign, COUNT(*) FROM cpdlc_messages WHERE station=? AND seen=0 GROUP BY callsign", (self.client.station,))}

    def _set_connected(self, callsign, yes):
        with self.lock:
            if yes and callsign not in self.connected:
                self.connected[callsign] = {"since": self.now(), "misses": 0}
                self.db.execute("INSERT OR REPLACE INTO cpdlc_conn2 VALUES (?,?,?)", (self.client.station, callsign, self.connected[callsign]["since"]))
            elif yes:
                self.connected[callsign]["misses"] = 0
            elif callsign in self.connected:
                del self.connected[callsign]
                self.db.execute("DELETE FROM cpdlc_conn2 WHERE station=? AND callsign=?", (self.client.station, callsign))
            self.db.commit()

    def is_connected(self, callsign):
        with self.lock:
            return callsign.upper() in self.connected

    def connected_callsigns(self):
        with self.lock:
            return set(self.connected)

    def status(self):
        with self.lock:
            unread = self.unread()
            conn = [{"callsign": c, "since": v["since"], "unread": unread.get(c, 0)} for c, v in sorted(self.connected.items())]
            return {"enabled": True, "station": self.client.station, "autoAccept": self.auto_accept, "connected": conn,
                    "unread": unread, "lastPoll": self.last_poll, "error": self.error}

    # --- sending
    def _alloc_min(self):
        with self.lock:
            m = self.next_min
            self.next_min = m % 63 + 1
            return m

    def _send_packet(self, callsign, mn, mrn, ra, text):
        packet = "/data2/%d/%s/%s/%s" % (mn, "" if mrn is None else mrn, ra, text)
        answer = self.client.request(callsign, "cpdlc", packet)
        head = answer.strip().split(" ", 1)[0].lower()
        if head != "ok":
            raise RuntimeError("Hoppie refused the message: " + answer.strip()[:120])
        self.fast_until = self.now() + FAST_POLL_SECONDS

    def send(self, callsign, text, ra="WU", mrn=None, by=""):
        """Send an uplink. Raises ValueError for bad input, RuntimeError if Hoppie refuses."""
        cs = (callsign or "").strip().upper()
        text = re.sub(r"[^A-Z0-9 @./,:+\-]", "", (text or "").upper()).strip()      # Hoppie wants upper case letters and numbers
        ra = (ra or "NE").upper()
        if not re.fullmatch(r"[A-Z0-9]{2,8}", cs):
            raise ValueError("bad callsign")
        if not text or len(text) > MAX_TEXT:
            raise ValueError("message must be 1-%d characters" % MAX_TEXT)
        if ra not in UPLINK_RA:
            raise ValueError("reply type must be one of " + ", ".join(UPLINK_RA))
        if not self.is_connected(cs):
            raise ValueError("%s is not connected to CPDLC" % cs)
        mn = self._alloc_min()
        self._send_packet(cs, mn, mrn, ra, text)
        mid = self._store("UP", cs, mn, mrn, ra, text, state="SENT" if ra != "NE" else "", by=by)
        if mrn is not None:                                   # answering a pilot request closes it
            with self.lock:
                self.db.execute("UPDATE cpdlc_messages SET state='ANSWERED' WHERE callsign=? AND dir='DOWN' AND min=? AND state='OPEN'", (cs, mrn))
                self.db.commit()
        return self.messages(cs, after=mid - 1, limit=1)[0]

    # --- receiving
    def handle_cpdlc(self, sender, packet):
        pkt = parse_cpdlc_packet(packet)
        if not pkt:
            log.info("ignored non-CPDLC packet from %s", sender)
            return
        text = pkt["text"].upper()
        if "REQUEST LOGON" in text or text.strip() == "LOGON":
            self._store("DOWN", sender, pkt["min"], pkt["mrn"], pkt["ra"], pkt["text"], state="LOGON", seen=1)
            if self.auto_accept:
                self.accept_logon(sender, pkt["min"])
            else:
                self._store("DOWN", sender, None, None, "", "LOGON WAITING FOR ACCEPT", state="", seen=0)
            return
        if re.search(r"\bLOGOFF\b|\bLOG OFF\b", text):
            self._store("DOWN", sender, pkt["min"], pkt["mrn"], pkt["ra"], pkt["text"], state="", seen=1)
            self._set_connected(sender, False)
            return
        state = "OPEN" if pkt["ra"] == "Y" else ""
        self._store("DOWN", sender, pkt["min"], pkt["mrn"], pkt["ra"], pkt["text"], state=state, seen=0)
        if pkt["mrn"] is not None:                            # a reply to one of our uplinks
            for word, name in REPLY_STATES:
                if text.startswith(word):
                    with self.lock:
                        self.db.execute("UPDATE cpdlc_messages SET state=? WHERE callsign=? AND dir='UP' AND min=?", (name, sender, pkt["mrn"]))
                        self.db.commit()
                    break

    def accept_logon(self, callsign, their_min=None):
        cs = callsign.upper()
        mn = self._alloc_min()
        self._send_packet(cs, mn, their_min, "NE", "LOGON ACCEPTED")
        self._store("UP", cs, mn, their_min, "NE", "LOGON ACCEPTED", state="", by="auto" if self.auto_accept else "")
        self._set_connected(cs, True)

    def poll_once(self):
        try:
            answer = self.client.request("SERVER", "poll", "")
        except Exception as e:                                # network trouble: skip this round, do not retry straight away
            self.error = "poll failed: %s" % e
            return 0
        head, blocks, raw = parse_blocks(answer)
        if head != "ok":
            self.error = "Hoppie: " + answer.strip()[:160]
            return 0
        self.error = ""
        self.last_poll = self.now()
        n = 0
        for sender, type_, packet in blocks:
            if type_ == "cpdlc":
                try:
                    self.handle_cpdlc(sender, packet)
                except Exception:
                    log.exception("could not handle a CPDLC message from %s", sender)
                n += 1
        if n:
            self.fast_until = self.now() + FAST_POLL_SECONDS
        return n

    def ping_connected(self):
        """Drop aircraft that stopped being online on Hoppie (two misses in a row)."""
        with self.lock:
            calls = sorted(self.connected)
        if not calls:
            return
        try:
            answer = self.client.request("SERVER", "ping", " ".join(calls))
        except Exception as e:
            self.error = "ping failed: %s" % e
            return
        head, _, raw = parse_blocks(answer)
        if head != "ok":
            return
        online = set(re.findall(r"[A-Z0-9]{2,8}", " ".join(raw).upper()))
        for c in calls:
            with self.lock:
                v = self.connected.get(c)
                if not v:
                    continue
                if c in online:
                    v["misses"] = 0
                else:
                    v["misses"] += 1
                    if v["misses"] >= PING_MISSES_TO_DROP:
                        log.info("%s is no longer online on Hoppie - connection dropped", c)
                        self._set_connected(c, False)

    def stop(self):
        """Stop polling and forget the logon code. Messages stay in the log."""
        self.stop_flag = True
        self.client.logon = ""
        with self.lock:
            self.connected.clear()
            self.db.execute("DELETE FROM cpdlc_conn2 WHERE station=?", (self.client.station,))
            self.db.commit()

    def loop(self):
        while not self.stop_flag:
            try:
                self.poll_once()
                if self.stop_flag:
                    break
                if self.now() - self.last_ping > PING_SECONDS:
                    self.last_ping = self.now()
                    self.ping_connected()
            except Exception:
                log.exception("CPDLC loop error")
            wait = random.uniform(18, 22) if self.now() < self.fast_until else random.uniform(45, 75)
            while wait > 0 and not self.stop_flag:
                time.sleep(min(1.0, wait))
                wait -= 1.0


# ----------------------------------------------------------------------------- the hub: one station per signed-in controller
DEFAULT_STATIONS = {"CZQO": "CZQX", "EGGX": "EGGX", "NAT": "NATX"}       # position prefix -> CPDLC station callsign


def parse_stations(text):
    """'CZQO_CTR=CZQX,EGGX_CTR=EGGX' -> {'CZQO_CTR': 'CZQX', ...}. Empty/None -> {}."""
    out = {}
    for part in (text or "").split(","):
        if "=" in part:
            k, v = part.split("=", 1)
            k, v = k.strip().upper(), v.strip().upper()
            if k and re.fullmatch(r"[A-Z0-9]{2,8}", v):
                out[k] = v
    return out


class CpdlcHub:
    def __init__(self, db_path, url=DEFAULT_URL, auto_accept=True, stations=None, make_client=None, is_session_alive=None, now=time.time, start_threads=True):
        self.db_path, self.url, self.auto_accept, self.now = db_path, url, auto_accept, now
        self.stations_map = stations or {}
        self.make_client = make_client or (lambda code, station: HoppieClient(self.url, code, station))
        self.is_session_alive = is_session_alive or (lambda token: True)
        self.start_threads = start_threads
        self.lock = threading.RLock()
        self.running = {}        # station callsign -> {"token", "obj"}
        self.enabled = True

    def station_for(self, position):
        pos = (position or "").upper()
        if not pos or pos == "OBSERVER" or pos.endswith("_DEL"):      # observers and delivery positions do not use CPDLC
            return None
        if pos in self.stations_map:
            return self.stations_map[pos]
        if self.stations_map:                      # an explicit list means only those positions
            return None
        return DEFAULT_STATIONS.get(pos.split("_")[0])

    def reap(self):
        """Stop stations whose controller's web session has ended."""
        with self.lock:
            for st, v in list(self.running.items()):
                if not self.is_session_alive(v["token"]):
                    log.info("CPDLC station %s stopped (controller signed out)", st)
                    v["obj"].stop()
                    del self.running[st]

    def start(self, token, position, code):
        """Returns (station, None) or (None, error text)."""
        station = self.station_for(position)
        if not station:
            return None, "your position (%s) has no CPDLC station" % (position or "none")
        code = (code or "").strip()
        if not re.fullmatch(r"[A-Za-z0-9]{6,40}", code):
            return None, "that does not look like a Hoppie logon code"
        self.reap()
        with self.lock:
            cur = self.running.get(station)
            if cur and cur["token"] != token:
                return None, "%s is already in use by another controller" % station
        client = self.make_client(code, station)
        try:                                       # check the code with a harmless ping before going live
            answer = client.request("SERVER", "ping", station)
        except Exception as e:
            return None, "could not reach Hoppie: %s" % e
        if answer.strip().split(" ", 1)[0].lower() != "ok":
            return None, "Hoppie said: " + answer.strip()[:120]
        with self.lock:
            cur = self.running.get(station)
            if cur:                                # same controller re-entering a code
                cur["obj"].client.logon = code
                return station, None
            obj = CpdlcStation(self.db_path, client, auto_accept=self.auto_accept, now=self.now)
            self.running[station] = {"token": token, "obj": obj}
        if self.start_threads:
            threading.Thread(target=obj.loop, daemon=True).start()
        log.info("CPDLC station %s started", station)
        return station, None

    def stop(self, token):
        with self.lock:
            for st, v in list(self.running.items()):
                if v["token"] == token:
                    v["obj"].stop()
                    del self.running[st]
                    log.info("CPDLC station %s stopped", st)

    def get(self, token):
        """The running station owned by this web session, or None."""
        with self.lock:
            for v in self.running.values():
                if v["token"] == token:
                    return v["obj"]
        return None

    def is_connected(self, callsign):
        with self.lock:
            return any(v["obj"].is_connected(callsign) for v in self.running.values())

    def status(self, token=None, position=""):
        mine = self.get(token) if token else None
        station = self.station_for(position)
        with self.lock:
            others = sorted({c for v in self.running.values() if v["obj"] is not mine for c in v["obj"].connected_callsigns()})
            taken = bool(station and station in self.running and not mine)
        if mine:
            st = mine.status()
            st.update({"active": True, "position": position, "stationForPosition": station, "inUse": False})
            return st
        return {"enabled": True, "active": False, "station": station or "", "stationForPosition": station or "", "inUse": taken,
                "connected": [{"callsign": c, "since": 0, "unread": 0} for c in others], "unread": {}, "lastPoll": None, "error": ""}
