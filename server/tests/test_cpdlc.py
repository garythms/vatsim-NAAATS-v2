import json, os, sys, tempfile, threading, time, unittest, urllib.error, urllib.parse, urllib.request
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import cpdlc as c
import vnaaats_server as s


class FakeHoppie:
    """Stands in for hoppie.nl: records what we send, plays back what 'pilots' sent."""
    def __init__(self):
        self.sent, self.queue, self.online, self.refuse = [], [], set(), False

    def __call__(self, url, data):
        f = {k: v[0] for k, v in urllib.parse.parse_qs(data.decode(), keep_blank_values=True).items()}
        self.sent.append(f)
        if self.refuse:
            return "error {callsign already in use}"
        if f["type"] == "poll":
            out, self.queue = self.queue, []
            return "ok " + " ".join("{%s cpdlc {%s}}" % (frm, pkt) for frm, pkt in out)
        if f["type"] == "ping":
            return "ok {%s}" % " ".join(x for x in f["packet"].split() if x in self.online)
        return "ok"


def make(auto=True):
    hop = FakeHoppie()
    st = c.CpdlcStation(os.path.join(tempfile.mkdtemp(), "c.db"), c.HoppieClient("http://x", "code", "CZQO", http=hop), auto_accept=auto)
    return hop, st


class ProtocolTests(unittest.TestCase):
    def test_parse_poll(self):
        head, blocks, _ = c.parse_blocks("ok {LOVV cpdlc {/data2/15//WU/CONTACT @WIEN SOUTH CTR@ @133.80}} {BAW1 telex {hello}}")
        self.assertEqual(head, "ok")
        self.assertEqual(blocks[0], ("LOVV", "cpdlc", "/data2/15//WU/CONTACT @WIEN SOUTH CTR@ @133.80"))
        self.assertEqual(blocks[1][1], "telex")
        self.assertEqual(c.parse_blocks("ok ")[1], [])
        self.assertEqual(c.parse_blocks("error {bad logon}")[0], "error")

    def test_parse_packet(self):
        self.assertEqual(c.parse_cpdlc_packet("/data2/15//WU/CONTACT @X@ 133/80"), {"min": 15, "mrn": None, "ra": "WU", "text": "CONTACT @X@ 133/80"})
        self.assertEqual(c.parse_cpdlc_packet("/data2/3/2/N/WILCO")["mrn"], 2)
        self.assertIsNone(c.parse_cpdlc_packet("hello"))
        self.assertEqual(c.readable("DESCEND TO @FL330@"), "DESCEND TO FL330")


class StationTests(unittest.TestCase):
    def test_logon_auto_accept_and_connected(self):
        hop, st = make()
        hop.queue = [("BAW123", "/data2/5//Y/REQUEST LOGON")]
        st.poll_once()
        self.assertTrue(st.is_connected("BAW123"))
        acc = [x for x in hop.sent if x["type"] == "cpdlc"][0]
        self.assertEqual((acc["to"], acc["from"], acc["packet"]), ("BAW123", "CZQO", "/data2/1/5/NE/LOGON ACCEPTED"))
        self.assertEqual(st.status()["connected"][0]["callsign"], "BAW123")

    def test_manual_accept_waits(self):
        hop, st = make(auto=False)
        hop.queue = [("DLH1", "/data2/2//Y/REQUEST LOGON")]
        st.poll_once()
        self.assertFalse(st.is_connected("DLH1"))
        st.accept_logon("DLH1", 2)
        self.assertTrue(st.is_connected("DLH1"))

    def test_uplink_and_wilco(self):
        hop, st = make()
        hop.queue = [("BAW123", "/data2/5//Y/REQUEST LOGON")]
        st.poll_once()
        with self.assertRaises(ValueError):
            st.send("NOPE1", "CLIMB TO FL350")
        with self.assertRaises(ValueError):
            st.send("BAW123", "CLIMB", ra="XX")
        m = st.send("BAW123", "climb to and maintain @fl350@", "WU", by="CZQO_CTR (Gary)")
        self.assertEqual((m["dir"], m["text"], m["state"], m["by"]), ("UP", "CLIMB TO AND MAINTAIN @FL350@", "SENT", "CZQO_CTR (Gary)"))
        self.assertEqual(hop.sent[-1]["packet"], "/data2/2//WU/CLIMB TO AND MAINTAIN @FL350@")
        hop.queue = [("BAW123", "/data2/6/%d/N/WILCO" % m["min"])]
        st.poll_once()
        self.assertEqual([x for x in st.messages("BAW123") if x["dir"] == "UP" and x["min"] == m["min"]][0]["state"], "WILCO")
        self.assertEqual(st.unread()["BAW123"], 1)
        st.mark_seen("BAW123")
        self.assertEqual(st.unread(), {})

    def test_pilot_request_opens_and_is_answered(self):
        hop, st = make()
        hop.queue = [("BAW123", "/data2/5//Y/REQUEST LOGON")]
        st.poll_once()
        hop.queue = [("BAW123", "/data2/7//Y/REQUEST CLIMB TO FL370")]
        st.poll_once()
        req = [x for x in st.messages("BAW123") if x["text"].startswith("REQUEST CLIMB")][0]
        self.assertEqual((req["state"], req["seen"]), ("OPEN", False))
        st.send("BAW123", "UNABLE", "NE", mrn=req["min"])
        self.assertEqual([x for x in st.messages("BAW123") if x["id"] == req["id"]][0]["state"], "ANSWERED")
        self.assertEqual(hop.sent[-1]["packet"], "/data2/2/7/NE/UNABLE")

    def test_logoff_and_ping_drop(self):
        hop, st = make()
        for cs in ("A1", "B2"):
            hop.queue = [(cs, "/data2/1//Y/REQUEST LOGON")]
            st.poll_once()
        hop.queue = [("A1", "/data2/2//N/LOGOFF")]
        st.poll_once()
        self.assertEqual(st.connected_callsigns(), {"B2"})
        st.ping_connected()                       # B2 not online
        self.assertTrue(st.is_connected("B2"))
        st.ping_connected()
        self.assertFalse(st.is_connected("B2"))
        hop.online = {"B2"}
        hop.queue = [("B2", "/data2/3//Y/REQUEST LOGON")]
        st.poll_once()
        st.ping_connected(); st.ping_connected()
        self.assertTrue(st.is_connected("B2"))

    def test_errors_do_not_crash(self):
        hop, st = make()
        hop.refuse = True
        self.assertEqual(st.poll_once(), 0)
        self.assertIn("callsign already in use", st.error)
        hop.refuse = False
        hop.queue = [("X1", "/data2/1//Y/REQUEST LOGON")]
        st.poll_once()
        hop.refuse = True
        with self.assertRaises(RuntimeError):
            st.send("X1", "HELLO")

    def test_connections_cleared_on_restart(self):
        hop, st = make()
        hop.queue = [("BAW9", "/data2/1//Y/REQUEST LOGON")]
        st.poll_once()
        path = st.db.execute("PRAGMA database_list").fetchone()[2]
        st2 = c.CpdlcStation(path, c.HoppieClient("http://x", "code", "CZQO", http=hop))
        self.assertFalse(st2.is_connected("BAW9"))               # pilots must log on again after a stop

    def test_stop_ends_loop_and_forgets_code(self):
        hop, st = make()
        t = threading.Thread(target=st.loop, daemon=True)
        t.start()
        time.sleep(0.3)
        st.stop()
        t.join(3)
        self.assertFalse(t.is_alive())
        self.assertEqual(st.client.logon, "")


class HubTests(unittest.TestCase):
    def make_hub(self, alive=None):
        hop = FakeHoppie()
        hub = c.CpdlcHub(os.path.join(tempfile.mkdtemp(), "h.db"), make_client=lambda code, station: c.HoppieClient("http://x", code, station, http=hop),
                         is_session_alive=(lambda t: t in alive) if alive is not None else None, start_threads=False)
        return hop, hub

    def test_station_for_position(self):
        _, hub = self.make_hub()
        self.assertEqual([hub.station_for(p) for p in ("CZQO_CTR", "CZQO_DEL", "EGGX_CTR", "NAT_FSS", "OBSERVER", "FOO_CTR", "")],
                         ["CZQX", "CZQX", "EGGX", "NATX", None, None, None])
        self.assertEqual(c.parse_stations("czqo_ctr=czqx, x=")["CZQO_CTR"], "CZQX")

    def test_start_validates_and_blocks_second_controller(self):
        hop, hub = self.make_hub()
        self.assertIn("no CPDLC station", hub.start("t1", "FOO_CTR", "abcdef12")[1])
        self.assertIn("logon code", hub.start("t1", "CZQO_CTR", "x")[1])
        self.assertEqual(hub.start("t1", "CZQO_CTR", "abcdef12"), ("CZQX", None))
        self.assertEqual(hop.sent[-1]["logon"], "abcdef12")
        self.assertIn("already in use", hub.start("t2", "CZQO_DEL", "zzzzzz99")[1])
        self.assertEqual(hub.start("t1", "CZQO_CTR", "newcode99"), ("CZQX", None))      # same controller may re-enter the code
        self.assertEqual(hub.get("t1").client.logon, "newcode99")
        self.assertEqual(hub.start("t3", "EGGX_CTR", "otherone1")[0], "EGGX")
        hub.stop("t1")
        self.assertIsNone(hub.get("t1"))
        self.assertEqual(hub.start("t2", "CZQO_DEL", "zzzzzz99")[0], "CZQX")

    def test_bad_code_rejected(self):
        hop, hub = self.make_hub()
        hop.refuse = True
        self.assertIn("Hoppie said", hub.start("t1", "CZQO_CTR", "abcdef12")[1])
        self.assertIsNone(hub.get("t1"))

    def test_reap_when_session_ends(self):
        alive = {"t1"}
        _, hub = self.make_hub(alive)
        hub.start("t1", "CZQO_CTR", "abcdef12")
        hub.reap()
        self.assertIsNotNone(hub.get("t1"))
        alive.clear()
        hub.reap()
        self.assertIsNone(hub.get("t1"))


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tmp = tempfile.mkdtemp()
        cls.hop = FakeHoppie()
        store = s.FlightStore(os.path.join(tmp, "f.db"))
        store.upsert({"callsign": "BAW123", "level": "350", "mach": "84", "track": "A", "relevant": "1"})
        store.upsert({"callsign": "DLH9", "level": "350", "mach": "84", "track": "A", "relevant": "1"})
        cache = s.TrackCache(s.load_fixes())
        auth = s.Auth({"VNAAATS_FDD_PASSWORD": "pw"})
        cls.hub = c.CpdlcHub(os.path.join(tmp, "h.db"), make_client=lambda code, st: c.HoppieClient("http://x", code, st, http=cls.hop),
                             is_session_alive=lambda t: auth.get(t) is not None, start_threads=False)
        cls.srv = s.ThreadingHTTPServer(("127.0.0.1", 0), s.make_handler(cache, store, "k" * 20, None, None, None, auth, cls.hub))
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()
        cls.base = "http://127.0.0.1:%d" % cls.srv.server_port
        cls.ck = cls.login_static("CZQO_CTR")
        assert cls.call("/cpdlc/logon", {"code": "abcdef12"}, cls.ck)[0] == 200
        cls.hop.queue = [("BAW123", "/data2/5//Y/REQUEST LOGON")]
        cls.hub.running["CZQX"]["obj"].poll_once()

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()

    @classmethod
    def call(cls, path, body=None, cookie=""):
        req = urllib.request.Request(cls.base + path, data=json.dumps(body).encode() if body is not None else None,
                                     headers={"Content-Type": "application/json", "Cookie": cookie})
        try:
            with urllib.request.urlopen(req) as r:
                return r.status, json.loads(r.read().decode()), r.headers.get("Set-Cookie", "")
        except urllib.error.HTTPError as e:
            return e.code, json.loads(e.read().decode()), ""

    @classmethod
    def login_static(cls, position):
        ck = cls.call("/auth/password", {"name": "Gary", "password": "pw"})[2].split(";")[0]
        if position:
            cls.call("/auth/position", {"position": position}, ck)
        return ck

    def login(self, position="CZQO_CTR"):
        return self.login_static(position)

    def test_flags_and_status(self):
        flights = {f["callsign"]: f for f in self.call("/GetAllFlightData")[1]}
        self.assertTrue(flights["BAW123"]["cpdlc"])
        self.assertFalse(flights["DLH9"]["cpdlc"])
        self.assertTrue(self.call("/GetSingleFlightData?callsign=BAW123")[1][0]["cpdlc"])
        anon = self.call("/cpdlc/status")[1]
        self.assertEqual((anon["enabled"], anon["active"], anon["connected"][0]["callsign"]), (True, False, "BAW123"))
        mine = self.call("/cpdlc/status", None, self.ck)[1]
        self.assertEqual((mine["active"], mine["station"], mine["connected"][0]["callsign"]), (True, "CZQX", "BAW123"))

    def test_login_needed_for_messages_and_send(self):
        self.assertEqual(self.call("/cpdlc/messages?callsign=BAW123")[0], 401)
        self.assertEqual(self.call("/cpdlc/send", {"callsign": "BAW123", "text": "HELLO"})[0], 401)
        self.assertEqual(self.call("/cpdlc/send", {"callsign": "BAW123", "text": "HELLO"}, self.login("OBSERVER"))[0], 403)
        self.assertEqual(self.call("/cpdlc/logon", {"code": "abcdef12"}, self.login("OBSERVER"))[0], 403)

    def test_needs_code_first_and_station_in_use(self):
        ck = self.login("EGGX_DEL")
        self.assertEqual(self.call("/cpdlc/send", {"callsign": "BAW123", "text": "HELLO"}, ck)[0], 409)
        self.assertEqual(self.call("/cpdlc/messages?callsign=BAW123", None, ck)[1], [])
        busy = self.login("CZQO_DEL")                                   # CZQX is held by cls.ck
        code, body, _ = self.call("/cpdlc/logon", {"code": "zzzzzz99"}, busy)
        self.assertEqual(code, 400)
        self.assertIn("already in use", body["error"])
        self.assertTrue(self.call("/cpdlc/status", None, busy)[1]["inUse"])
        self.assertEqual(self.call("/cpdlc/logon", {"code": "bad"}, ck)[0], 400)

    def test_send_and_read(self):
        ck = self.ck
        code, msg, _ = self.call("/cpdlc/send", {"callsign": "BAW123", "text": "expect climb fl370 at 30w", "ra": "R"}, ck)
        self.assertEqual((code, msg["dir"], msg["ra"]), (200, "UP", "R"))
        self.assertEqual(self.call("/cpdlc/send", {"callsign": "DLH9", "text": "HELLO"}, ck)[0], 400)     # not connected
        self.assertEqual(self.call("/cpdlc/send", {"callsign": "BAW123", "text": "HELLO", "ra": "ZZ"}, ck)[0], 400)
        texts = [m["text"] for m in self.call("/cpdlc/messages?callsign=BAW123", None, ck)[1]]
        self.assertIn("EXPECT CLIMB FL370 AT 30W", texts)

    def test_position_change_stops_station(self):
        ck = self.login("NAT_FSS")
        self.assertEqual(self.call("/cpdlc/logon", {"code": "natcode12"}, ck)[1]["station"], "NATX")
        self.call("/auth/position", {"position": "EGGX_CTR"}, ck)
        self.assertNotIn("NATX", self.hub.running)

    def test_off_by_default(self):
        tmp = tempfile.mkdtemp()
        srv = s.ThreadingHTTPServer(("127.0.0.1", 0), s.make_handler(s.TrackCache(s.load_fixes()), s.FlightStore(os.path.join(tmp, "o.db")), "k" * 20))
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        with urllib.request.urlopen("http://127.0.0.1:%d/cpdlc/status" % srv.server_port) as r:
            self.assertFalse(json.loads(r.read().decode())["enabled"])
        srv.shutdown()


if __name__ == "__main__":
    unittest.main()
