"""
Commands from the flight data display to a controller's EuroScope plugin.

A web page cannot talk to EuroScope, so the FDD puts a command in this queue (addressed to the controller's
position callsign, e.g. CZQO_CTR) and that controller's plugin collects it with /PluginCommands a few seconds later,
carries it out, and reports the result with /PluginCommandDone.  The FDD watches the result with /cmd/status.

Commands: track (start tracking + clearance, with level / Mach / SELCAL), untrack, transfer (handoff to a controller).
(EuroScope's own dot commands .selcal / .contactme / .wallop cannot be run by a plugin; the FDD copies them instead.)
"""
import re
import threading
import time

TYPES = ("track", "untrack", "transfer", "selcal", "contactme", "natmsg")   # last three are typed into EuroScope by the plugin (autotype=1)
TTL_SECONDS = 300          # an unclaimed command is dropped after this
KEEP_SECONDS = 600         # finished commands stay readable this long
SELCAL_OK = re.compile(r"[A-HJ-MP-S]{4}")
CALLSIGN_OK = re.compile(r"[A-Z0-9_\-]{2,12}")


NATMSG = ("Hello, you will soon be entering oceanic airspace. Please request oceanic clearance via nattrak.vatsim.net "
          "no later than 20 minutes before your oceanic entry point. Clearance via CPDLC is unavailable.")


def clean_command(body):
    """Check a command from the FDD. Returns (type, callsign, args, error)."""
    typ = str(body.get("type", "")).lower()
    if typ not in TYPES:
        return None, None, None, "unknown command"
    cs = str(body.get("callsign", "")).strip().upper()
    if not re.fullmatch(r"[A-Z0-9]{2,8}", cs):
        return None, None, None, "bad callsign"
    args = {}
    if typ == "track":
        try:
            level = int(str(body.get("level", "")).strip() or 0)
            mach = int(str(body.get("mach", "")).lstrip("Mm.").lstrip("0") or 0)
        except ValueError:
            return None, None, None, "level and Mach must be numbers"
        if not 1 <= level <= 999:
            return None, None, None, "level must be 1-999 (e.g. 350)"
        if not 1 <= mach <= 299:
            return None, None, None, "Mach must be e.g. 84 for M0.84"
        sel = re.sub(r"[-\s]", "", str(body.get("selcal", "")).upper())
        if sel and not SELCAL_OK.fullmatch(sel):
            return None, None, None, "SELCAL must be 4 letters from A-H, J-M, P-S"
        args = {"level": level, "mach": mach, "selcal": sel}
    elif typ == "transfer":
        target = str(body.get("target", "")).strip().upper()
        if not CALLSIGN_OK.fullmatch(target):
            return None, None, None, "choose a controller to transfer to"
        freq = str(body.get("freq", "")).strip()
        args = {"target": target, "freq": freq if re.fullmatch(r"1[0-3]\d\.\d{1,3}", freq) else ""}
    elif typ == "selcal":
        sel = re.sub(r"[-\s]", "", str(body.get("selcal", "")).upper())
        if not SELCAL_OK.fullmatch(sel):
            return None, None, None, "SELCAL must be 4 letters from A-H, J-M, P-S"
        args = {"text": ".selcal " + sel}
    elif typ == "contactme":
        args = {"text": ".contactme " + cs}
    elif typ == "natmsg":
        args = {"text": ".msg " + cs + " " + NATMSG}
    return typ, cs, args, None


class CommandQueue:
    def __init__(self, now=time.time):
        self.now = now
        self.lock = threading.Lock()
        self.items = {}          # id -> dict
        self.next_id = 1

    def _clean(self):
        t = self.now()
        for i in [i for i, c in self.items.items()
                  if (c["state"] == "pending" and t - c["ts"] > TTL_SECONDS) or t - c["ts"] > KEEP_SECONDS + TTL_SECONDS]:
            del self.items[i]

    def add(self, target, typ, callsign, args, by=""):
        with self.lock:
            self._clean()
            cid = self.next_id
            self.next_id += 1
            self.items[cid] = {"id": cid, "target": target.upper(), "type": typ, "callsign": callsign, "args": args,
                               "by": by, "state": "pending", "msg": "", "ts": self.now()}
            return cid

    def pending_for(self, me):
        """Commands addressed to this controller callsign; they are marked delivered so they are handed out once."""
        out = []
        with self.lock:
            self._clean()
            for c in self.items.values():
                if c["state"] == "pending" and c["target"] == (me or "").upper():
                    c["state"] = "delivered"
                    c["delivered_ts"] = self.now()
                    out.append({"id": c["id"], "type": c["type"], "callsign": c["callsign"], "args": c["args"], "by": c["by"]})
        return out

    def done(self, cid, ok, msg=""):
        """Plugin result. Returns the command (for follow-up actions) or None."""
        with self.lock:
            c = self.items.get(cid)
            if not c or c["state"] not in ("delivered", "pending"):
                return None
            c["state"] = "ok" if ok else "failed"
            c["msg"] = (msg or "")[:200]
            return dict(c)

    def status(self, cid):
        with self.lock:
            c = self.items.get(cid)
            if not c:
                return None
            state = c["state"]
            if state == "pending" and self.now() - c["ts"] > 20:
                state = "waiting"          # nobody has collected it: the plugin is not running / not that callsign
            return {"id": cid, "state": state, "msg": c["msg"], "type": c["type"], "callsign": c["callsign"]}
