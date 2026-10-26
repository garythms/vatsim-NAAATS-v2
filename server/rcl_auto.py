"""
Automatic RCL REQ reminder.

A flight that is about to enter oceanic airspace and has not asked for clearance on natTrak is sent the standard
"please request oceanic clearance" message.  The message is typed into EuroScope by a controller's plugin (the same
"natmsg" command the FDD's RCL REQ > Send button uses), so one oceanic controller who is online with the plugin
(autotype=1) is picked as the sender.

A flight is due when ALL of these hold:
  * it is on VATSIM, moving (not parked), and not already handled by natTrak or tracked by an oceanic controller
  * no reminder has been sent to it yet (by hand or automatically)
  * and either its oceanic entry estimate is between MIN_LEAD_MINUTES and DUE_MINUTES away,
    or (no usable times) it is within DUE_NM of a fix on its route
  and it is NOT already inside, or through, oceanic airspace, and NOT closer than MIN_LEAD_MINUTES to its entry point: natTrak
  refuses a new request that late, and a flight past its entry point does not need one.
"""
import logging
import re
import threading
import time
from datetime import datetime, timezone

import commands as commands_mod

log = logging.getLogger("vnaaats")

DUE_MINUTES = 45
MIN_LEAD_MINUTES = 20          # natTrak will not accept a new request closer than this to the oceanic entry point
DUE_NM = 200
START_DELAY_SECONDS = 300      # nothing is sent until an oceanic controller has been online this long (session start settling time)
SENDER_GAP_RESET = 600         # no oceanic controller online for this long = a new session: the wait and the baseline start again
OCA_WEST_LON = -53.0           # Gander OCA western edge / Shanwick OCA eastern edge, rounded outwards for a safe "inside" test
OCA_EAST_LON = -15.0
OCA_MIN_LAT = 43.0
OCA_MAX_LAT = 72.0
MIN_GROUNDSPEED = 100          # knots: a parked or taxiing aircraft is not due
TICK_SECONDS = 10
POLLER_FRESH_SECONDS = 15      # a plugin that asked for commands this recently is "online"
RETRY_SECONDS = 300
MAX_TRIES = 3
INFLIGHT_SECONDS = 90          # the command is given this long to come back before we may queue it again
BY = "AUTO"


def entry_minutes_from_now(route_etas, now_utc_minute):
    """Minutes until the earliest estimate on the route (negative if it has passed). None if there are no times."""
    times = [t for t in str(route_etas or "").split() if re.fullmatch(r"\d{4}", t)]
    if not times:
        return None
    mins, prev, day = [], None, 0
    for t in times:                                        # unwrap midnight so the times form one run
        m = int(t[:2]) * 60 + int(t[2:])
        if prev is not None and m + day * 1440 < prev - 720:
            day += 1
        elif prev is not None and m + day * 1440 > prev + 720:
            day -= 1
        prev = m + day * 1440
        mins.append(prev)
    entry = min(mins) % 1440
    d = entry - now_utc_minute
    if d > 720:
        d -= 1440
    elif d < -720:
        d += 1440
    return d


def oca_state(pos, eastbound):
    """'inside' (in the oceanic area between 53W and 15W), 'past' (through it, direction known) or 'before'. Unknown position = 'before'."""
    try:
        lat, lon = float(pos[0]), float(pos[1])
    except (TypeError, ValueError, IndexError):
        return "before"
    if lat < OCA_MIN_LAT or lat > OCA_MAX_LAT:
        return "before"
    if OCA_WEST_LON <= lon <= OCA_EAST_LON:
        return "inside"
    if eastbound is True and lon > OCA_EAST_LON:
        return "past"
    if eastbound is False and lon < OCA_WEST_LON:
        return "past"
    return "before"


class RclAuto:
    def __init__(self, store, queue, network, natrak, distance_fn, oceanic_prefixes, now=time.time, start=True):
        self.store, self.queue, self.network, self.natrak = store, queue, network, natrak
        self.distance_fn = distance_fn          # (route, (lat, lon)) -> nm to the nearest fix on the route, or None
        self.prefixes = tuple(oceanic_prefixes)
        self.now = now
        self.lock = threading.Lock()
        self.pollers = {}          # position callsign -> time its plugin last asked for commands
        self.inflight = {}         # flight callsign -> {"cid", "ts", "base", "sender"}
        self.tries = {}            # flight callsign -> {"n": attempts, "next": earliest next try}
        self.enabled = True
        self.last_error = ""
        self.sender_since = None   # when an oceanic controller first came online in this session
        self.sender_last = 0.0     # last time one was seen online
        self.baselined = False
        self.baseline = set()      # flights already in/at the oceanic area when the session settled: never reminded
        with self.store.lock:
            self.store.db.execute("CREATE TABLE IF NOT EXISTS rcl_auto (callsign TEXT PRIMARY KEY, ts REAL NOT NULL)")
            self.store.db.execute("CREATE TABLE IF NOT EXISTS rcl_setting (id INTEGER PRIMARY KEY CHECK (id = 1), on_ INTEGER NOT NULL)")
            row = self.store.db.execute("SELECT on_ FROM rcl_setting WHERE id=1").fetchone()
            self.store.db.commit()
        self.enabled = True if row is None else bool(row[0])
        if start:
            threading.Thread(target=self.loop, daemon=True).start()

    # --- settings
    def set_enabled(self, on):
        self.enabled = bool(on)
        with self.store.lock:
            self.store.db.execute("INSERT OR REPLACE INTO rcl_setting VALUES (1, ?)", (1 if on else 0,))
            self.store.db.commit()

    # --- who can send
    def note_poll(self, me):
        me = (me or "").strip().upper()
        if me:
            with self.lock:
                self.pollers[me] = self.now()

    def sender(self):
        """The oceanic controller whose plugin will type the message, or None."""
        t = self.now()
        with self.lock:
            live = sorted(c for c, ts in self.pollers.items() if t - ts <= POLLER_FRESH_SECONDS and c.startswith(self.prefixes))
        return live[0] if live else None

    def status(self):
        with self.lock:
            n = len(self.inflight)
        wait = 0
        if self.sender_since is not None:
            wait = max(0, int(self.sender_since + START_DELAY_SECONDS - self.now()))
        return {"on": self.enabled, "sender": self.sender() or "", "inflight": n, "error": self.last_error, "startsInSeconds": wait}

    # --- the decision
    def already_sent(self, cs):
        with self.store.lock:
            return self.store.db.execute("SELECT 1 FROM rcl_auto WHERE callsign=?", (cs,)).fetchone() is not None

    def due(self, rec, now_dt=None):
        """True when this flight should get the reminder now."""
        cs = str(rec.get("callsign", "")).upper()
        if not cs or rec.get("ntSent"):
            return False
        if self.natrak and cs in self.natrak.by_callsign():
            return False
        by = str(rec.get("trackedBy", "")).upper()
        if by and by.startswith(self.prefixes + ("NATTRAK",)):
            return False
        if self.network:
            if self.network.is_offline(cs):
                return False
            pos = self.network.position(cs)
            pilot = self.network.lookup_pilot(cs) or {}
        else:
            return False
        if not pos:
            return False                                    # not on the network feed: we cannot tell where it is
        gs = pilot.get("groundspeed")
        if isinstance(gs, (int, float)) and gs < MIN_GROUNDSPEED:
            return False
        if cs in self.baseline:
            return False
        if oca_state(pos, rec.get("direction") if isinstance(rec.get("direction"), bool) else None) != "before":
            return False                                    # already inside, or through, the oceanic area: nothing to request
        now_dt = now_dt or datetime.fromtimestamp(self.now(), timezone.utc)
        m = entry_minutes_from_now(rec.get("routeEtas", ""), now_dt.hour * 60 + now_dt.minute)
        if m is not None:
            if m < MIN_LEAD_MINUTES:
                return False        # natTrak takes no new request inside 20 minutes of entry (and one already past entry, or through, needs none)
            if m <= DUE_MINUTES:
                return True
        d = self.distance_fn(rec.get("route", ""), pos)
        if d is None or d > DUE_NM:
            return False
        if m is None and isinstance(gs, (int, float)) and gs > 0 and d / gs * 60 < MIN_LEAD_MINUTES:
            return False            # no times on the flight: judge the 20 minutes from the distance and speed instead
        return True

    # --- the loop
    def candidates(self):
        seen, out = set(), []
        for rec in self.store.all_flights() + (self.network.all() if self.network else []):
            cs = str(rec.get("callsign", "")).upper()
            if cs and cs not in seen:
                seen.add(cs)
                out.append(rec)
        return out

    def take_baseline(self):
        """Flights already in the oceanic area, or inside the 20-minute no-new-request window, when the wait ends are marked
        as handled for this session so they are never reminded."""
        now_dt = datetime.fromtimestamp(self.now(), timezone.utc)
        for rec in self.candidates():
            cs = str(rec.get("callsign", "")).upper()
            pos = self.network.position(cs) if self.network and cs else None
            if not pos:
                continue
            direction = rec.get("direction") if isinstance(rec.get("direction"), bool) else None
            m = entry_minutes_from_now(rec.get("routeEtas", ""), now_dt.hour * 60 + now_dt.minute)
            if oca_state(pos, direction) != "before" or (m is not None and m < MIN_LEAD_MINUTES):
                self.baseline.add(cs)
        self.baselined = True
        log.info("Auto RCL baseline taken: %d flights already at/in the oceanic area will not be reminded", len(self.baseline))

    def tick(self):
        """Queue at most one reminder. Returns the callsign queued, or None."""
        if not self.enabled:
            return None
        sender = self.sender()
        t = self.now()
        if not sender:
            if self.sender_since is not None and t - self.sender_last > SENDER_GAP_RESET:
                self.sender_since, self.baselined = None, False      # the session ended: start fresh next time
            return None
        self.sender_last = t
        if self.sender_since is None:
            self.sender_since, self.baselined, self.baseline = t, False, set()
        if t - self.sender_since < START_DELAY_SECONDS:
            return None                                              # let the session settle first
        if not self.baselined:
            self.take_baseline()
        with self.lock:
            for cs in [c for c, v in self.inflight.items() if t - v["ts"] > INFLIGHT_SECONDS]:
                del self.inflight[cs]                       # never came back; it may be tried again later
        hidden = self.store.dismissed_set()
        for rec in self.candidates():
            cs = str(rec.get("callsign", "")).upper()
            if cs in hidden or not re.fullmatch(r"[A-Z0-9]{2,8}", cs):
                continue
            with self.lock:
                if cs in self.inflight:
                    continue
                tr = self.tries.get(cs, {"n": 0, "next": 0})
                if tr["n"] >= MAX_TRIES or t < tr["next"]:
                    continue
            if self.already_sent(cs) or not self.due(rec):
                continue
            typ, _, args, err = commands_mod.clean_command({"type": "natmsg", "callsign": cs})
            if err:
                continue
            cid = self.queue.add(sender, typ, cs, args, by=BY)
            with self.lock:
                self.inflight[cs] = {"cid": cid, "ts": t, "base": rec, "sender": sender}
                self.tries[cs] = {"n": tr["n"] + 1, "next": t + RETRY_SECONDS}
            log.info("Auto RCL REQ queued for %s via %s", cs, sender)
            return cs
        return None

    def done(self, cmd, ok, msg=""):
        """Called when the plugin reports back on one of our commands (cmd is the finished queue item)."""
        cs = str(cmd.get("callsign", "")).upper()
        with self.lock:
            info = self.inflight.pop(cs, None)
        if not info:
            return
        if ok:
            with self.store.lock:
                self.store.db.execute("INSERT OR REPLACE INTO rcl_auto VALUES (?, ?)", (cs, self.now()))
                self.store.db.commit()
            self.store.mark_nt(cs, "Auto RCL REQ", True, info.get("base"))
            self.last_error = ""
            log.info("Auto RCL REQ sent to %s", cs)
        else:
            self.last_error = "%s: %s" % (cs, (msg or "failed")[:120])
            log.warning("Auto RCL REQ for %s failed: %s", cs, msg)

    def purge(self):
        with self.store.lock:
            self.store.db.execute("DELETE FROM rcl_auto WHERE ts < ?", (self.now() - 3 * 86400,))
            self.store.db.commit()

    def loop(self):
        n = 0
        while True:
            try:
                self.tick()
                n += 1
                if n % 360 == 0:
                    self.purge()
            except Exception:
                log.exception("auto RCL check failed")
            time.sleep(TICK_SECONDS)
