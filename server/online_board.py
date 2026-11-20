"""Discord "who is online" board: one embed, edited in place, listing the oceanic controllers on VATSIM.

Needs VNAAATS_ONLINE_WEBHOOK=<discord webhook url> in /etc/vnaaats.env (a webhook made for the channel that should
show the board). Without it nothing runs. The webhook URL is a secret: keep it in the env file only, never in the repo.

Settings (environment variables, all optional):
  VNAAATS_ONLINE_PREFIXES  callsign prefixes to list (default CZQO,CZQX,EGGX,NAT,BIRD)
  VNAAATS_ONLINE_SECONDS   how often to check (default 60; a Discord edit only happens when the list changed, or every 10 minutes)
  VNAAATS_ONLINE_LOGO      logo url shown on the board (default https://vnaaats.com/icons/app-512.png)
  VNAAATS_ONLINE_STATE     file holding the message id (default next to the database)
"""
import json, logging, os, time, urllib.request

log = logging.getLogger("vnaaats.online")
HOOK_PREFIXES = ("https://discord.com/api/webhooks/", "https://discordapp.com/api/webhooks/")
FACILITY = {1: "FSS", 2: "DEL", 3: "GND", 4: "TWR", 5: "APP", 6: "CTR"}
COLOUR_ON, COLOUR_OFF = 0x2E9E5B, 0x6B7B8C


def parse_prefixes(text):
    return tuple(x.strip().upper() for x in (text or "").split(",") if x.strip())


def pick(feed, prefixes):
    """Controllers in a VATSIM data feed whose callsign starts with one of the prefixes (ATIS excluded), sorted."""
    out = []
    for c in (feed or {}).get("controllers", []):
        try:
            cs = str(c["callsign"]).upper()
            if cs.endswith("_ATIS") or not cs.startswith(prefixes):
                continue
            out.append({"callsign": cs, "name": str(c.get("name", "")).strip(), "frequency": "%.3f" % float(c.get("frequency", 0)),
                        "logon": str(c.get("logon_time", ""))})
        except (KeyError, TypeError, ValueError):
            pass
    return sorted(out, key=lambda x: x["callsign"])


def since(logon, now=None):
    """'2h 05m' since an ISO logon time, '' when it can't be read."""
    try:
        from datetime import datetime, timezone
        t = datetime.fromisoformat(logon.replace("Z", "+00:00"))
        mins = max(0, int(((now or datetime.now(timezone.utc)) - t).total_seconds() // 60))
        return "%dh %02dm" % (mins // 60, mins % 60) if mins >= 60 else "%dm" % mins
    except Exception:
        return ""


def build(ctrls, logo, stamp=None):
    lines = []
    for c in ctrls:
        extra = " - ".join(x for x in (c["name"], since(c["logon"])) if x)
        lines.append("**%s**  `%s`%s" % (c["callsign"], c["frequency"], ("  " + extra) if extra else ""))
    desc = "\n".join(lines) if lines else "No oceanic controllers are online right now."
    emb = {"title": "Oceanic controllers online", "description": desc[:3900], "color": COLOUR_ON if lines else COLOUR_OFF,
           "thumbnail": {"url": logo}, "footer": {"text": "vNAAATS - vnaaats.com", "icon_url": logo},
           "timestamp": stamp or time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    return {"username": "vNAAATS", "avatar_url": logo, "embeds": [emb], "allowed_mentions": {"parse": []}}


def _call(method, url, body=None):
    req = urllib.request.Request(url, data=json.dumps(body).encode() if body is not None else None, method=method,
                                 headers={"Content-Type": "application/json", "User-Agent": "vnaaats-online-board"})
    with urllib.request.urlopen(req, timeout=15) as r:
        raw = r.read()
    return json.loads(raw) if raw else {}


class Board:
    def __init__(self, hook, prefixes, logo, state_path, feed_url, call=_call, fetch=None):
        self.hook, self.prefixes, self.logo, self.state_path, self.feed_url = hook, prefixes, logo, state_path, feed_url
        self.call = call
        self.fetch = fetch or self._fetch
        self.msg_id, self.sig, self.last_post = None, None, 0
        try:
            self.msg_id = json.load(open(state_path)).get("id")
        except Exception:
            pass

    def _fetch(self):
        req = urllib.request.Request(self.feed_url, headers={"User-Agent": "vnaaats-online-board"})
        return json.load(urllib.request.urlopen(req, timeout=20))

    def _save(self):
        try:
            json.dump({"id": self.msg_id}, open(self.state_path, "w"))
        except OSError:
            log.warning("could not save %s", self.state_path)

    def tick(self, now=None):
        """One check. Returns 'posted', 'edited' or 'same'."""
        now = now or time.time()
        ctrls = pick(self.fetch(), self.prefixes)
        sig = [(c["callsign"], c["frequency"]) for c in ctrls]
        if sig == self.sig and now - self.last_post < 600:
            return "same"
        payload = build(ctrls, self.logo)
        base = self.hook.split("?")[0]
        if self.msg_id:
            try:
                self.call("PATCH", "%s/messages/%s" % (base, self.msg_id), payload)
                self.sig, self.last_post = sig, now
                return "edited"
            except Exception as e:                       # message deleted by hand: post a fresh one
                log.info("board edit failed (%s) - posting a new message", e)
                self.msg_id = None
        res = self.call("POST", base + "?wait=true", payload)
        self.msg_id = str(res.get("id", "")) or None
        self.sig, self.last_post = sig, now
        self._save()
        return "posted"

    def loop(self, seconds):
        while True:
            try:
                self.tick()
            except Exception as e:
                log.warning("online board: %s", e)
            time.sleep(seconds)


def start(db_path, feed_url):
    """Start the board thread when VNAAATS_ONLINE_WEBHOOK is set. Returns the Board or None."""
    import threading
    hook = os.environ.get("VNAAATS_ONLINE_WEBHOOK", "").strip()
    if not hook.startswith(HOOK_PREFIXES) or not feed_url or feed_url.lower() == "off":
        return None
    b = Board(hook, parse_prefixes(os.environ.get("VNAAATS_ONLINE_PREFIXES", "CZQO,CZQX,EGGX,NAT,BIRD")),
              os.environ.get("VNAAATS_ONLINE_LOGO", "https://vnaaats.com/icons/app-512.png"),
              os.environ.get("VNAAATS_ONLINE_STATE", os.path.join(os.path.dirname(os.path.abspath(db_path)), "online-board.json")), feed_url)
    threading.Thread(target=b.loop, args=(max(30, int(os.environ.get("VNAAATS_ONLINE_SECONDS", "60"))),), daemon=True).start()
    log.info("Discord online board is on")
    return b
