import json, os, sys, tempfile, threading, unittest, urllib.error, urllib.request
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import commands as cm
import vnaaats_server as s


class QueueTests(unittest.TestCase):
    def test_clean_track(self):
        t, cs, a, e = cm.clean_command({"type": "track", "callsign": "baw123", "level": "350", "mach": "M084", "selcal": "ad-bf"})
        self.assertEqual((t, cs, a, e), ("track", "BAW123", {"level": 350, "mach": 84, "selcal": "ADBF"}, None))
        self.assertEqual(cm.clean_command({"type": "track", "callsign": "BAW123", "level": "350", "mach": "200"})[2]["mach"], 200)
        for bad in ({"type": "track", "callsign": "BAW123", "level": "0", "mach": "84"},
                    {"type": "track", "callsign": "BAW123", "level": "350", "mach": "0"},
                    {"type": "track", "callsign": "BAW123", "level": "350", "mach": "84", "selcal": "AINO"},
                    {"type": "nuke", "callsign": "BAW123"}, {"type": "untrack", "callsign": "bad callsign"},
                    {"type": "transfer", "callsign": "BAW123", "target": ""}):
            self.assertIsNotNone(cm.clean_command(bad)[3], bad)
        self.assertEqual(cm.clean_command({"type": "transfer", "callsign": "BAW123", "target": "eisn_ctr", "freq": "127.500"})[2], {"target": "EISN_CTR", "freq": "127.500"})
        self.assertEqual(cm.clean_command({"type": "transfer", "callsign": "BAW123", "target": "EISN_CTR", "freq": "x"})[2]["freq"], "")

    def test_lifecycle(self):
        now = [1000.0]
        q = cm.CommandQueue(now=lambda: now[0])
        cid = q.add("CZQO_CTR", "untrack", "BAW123", {}, by="x")
        self.assertEqual(q.pending_for("EGGX_CTR"), [])               # other controllers never see it
        self.assertEqual(q.status(cid)["state"], "pending")
        now[0] += 25
        self.assertEqual(q.status(cid)["state"], "waiting")           # nobody collected it
        got = q.pending_for("czqo_ctr")
        self.assertEqual([c["id"] for c in got], [cid])
        self.assertEqual(q.pending_for("CZQO_CTR"), [])               # handed out once
        self.assertEqual(q.status(cid)["state"], "delivered")
        self.assertEqual(q.done(cid, True, "ok")["state"], "ok")
        self.assertEqual(q.status(cid)["state"], "ok")
        self.assertIsNone(q.done(cid, False))                         # already finished
        old = q.add("CZQO_CTR", "untrack", "BAW1", {})
        now[0] += cm.TTL_SECONDS + 1
        self.assertEqual(q.pending_for("CZQO_CTR"), [])               # expired
        self.assertIsNone(q.status(old))


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tmp = tempfile.mkdtemp()
        store = s.FlightStore(os.path.join(tmp, "f.db"))
        cls.net = s.NetworkTraffic(s.TrackCache(s.load_fixes()))
        cls.net.refresh(fetch=lambda: {"pilots": [], "controllers": [
            {"callsign": "EISN_CTR", "frequency": "127.500", "name": "A", "facility": 6},
            {"callsign": "EGGX_CTR", "frequency": "127.900", "name": "B", "facility": 6},
            {"callsign": "EGLL_TWR", "frequency": "118.500", "name": "C", "facility": 4},
            {"callsign": "EGLL_ATIS", "frequency": "128.000", "name": "D", "facility": 4},
            {"callsign": "NAT_FSS", "frequency": "128.100", "name": "E", "facility": 1}]})
        auth = s.Auth({"VNAAATS_FDD_PASSWORD": "pw"})
        cls.srv = s.ThreadingHTTPServer(("127.0.0.1", 0), s.make_handler(s.TrackCache(s.load_fixes()), store, "k" * 20, None, cls.net, None, auth))
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()
        cls.base = "http://127.0.0.1:%d" % cls.srv.server_port

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

    def login(self, position="CZQO_CTR"):
        ck = self.call("/auth/password", {"name": "Gary", "password": "pw"})[2].split(";")[0]
        self.call("/auth/position", {"position": position}, ck)
        return ck

    def test_controllers_list(self):
        names = [c["callsign"] for c in self.call("/GetControllers")[1]]
        self.assertEqual(names, ["EGGX_CTR", "EISN_CTR", "NAT_FSS"])      # no tower, no ATIS

    def test_full_flow(self):
        K = "k" * 20
        self.assertEqual(self.call("/cmd/queue", {"type": "untrack", "callsign": "BAW1"})[0], 401)
        self.assertEqual(self.call("/cmd/queue", {"type": "untrack", "callsign": "BAW1"}, self.login("OBSERVER"))[0], 403)
        ck = self.login("CZQO_CTR")
        self.assertEqual(self.call("/cmd/queue", {"type": "track", "callsign": "BAW1", "level": "0", "mach": "84"}, ck)[0], 400)
        code, body, _ = self.call("/cmd/queue", {"type": "transfer", "callsign": "BAW1", "target": "EISN_CTR", "freq": "127.500"}, ck)
        self.assertEqual(code, 200)
        cid = body["id"]
        self.assertEqual(self.call("/PluginCommands?code=wrong&me=CZQO_CTR")[0], 401)
        self.assertEqual(self.call("/PluginCommands?code=%s&me=EGGX_CTR" % K)[1], [])
        cmds = self.call("/PluginCommands?code=%s&me=CZQO_CTR" % K)[1]
        self.assertEqual((cmds[0]["id"], cmds[0]["type"], cmds[0]["args"]["target"]), (cid, "transfer", "EISN_CTR"))
        self.assertEqual(self.call("/PluginCommands?code=%s&me=CZQO_CTR" % K)[1], [])
        self.assertEqual(self.call("/cmd/status?id=%d" % cid, None, ck)[1]["state"], "delivered")
        self.assertEqual(self.call("/PluginCommandDone?code=%s&id=%d&ok=1&msg=started" % (K, cid))[1], {"ok": True})
        st = self.call("/cmd/status?id=%d" % cid, None, ck)[1]
        self.assertEqual((st["state"], st["msg"]), ("ok", "started"))
        self.assertEqual(self.call("/cmd/status?id=%d" % cid)[0], 401)


if __name__ == "__main__":
    unittest.main()


class RouteEditTests(unittest.TestCase):
    def test_route_formats(self):
        import vnaaats_server as s
        self.assertEqual(s.route_problem("52/50 52/40 SOMAX"), "")
        self.assertIn("51/20", s.route_problem("51N20W"))
        self.assertIn("51/20", s.route_problem("5120N"))
        self.assertIn("not a fix", s.route_problem("52/50 AB"))
        self.assertNotEqual(s.route_problem(""), "")
        ch, err = s.validate_edit({"route": "52/50  52/30"})
        self.assertEqual(ch, {"route": "52/50 52/30"})

    def test_edit_keeps_times_and_survives_plugin_update(self):
        import vnaaats_server as s, tempfile, os
        st = s.FlightStore(os.path.join(tempfile.mkdtemp(), "f.db"))
        q = {"callsign": "BAW1", "route": "52/50 52/40 52/30", "routeEtas": "1200 1215 1230", "relevant": "1", "track": "RR"}
        st.upsert(q)
        rec = st.edit("BAW1", {"route": "52/50 53/40 52/30"}, "CZQO")
        self.assertEqual(rec["routeEtas"], "1200 -- 1230")
        st.upsert(q)                                   # plugin posts its old route again
        self.assertEqual(st.get("BAW1")["route"], "52/50 53/40 52/30")


class DistanceTests(unittest.TestCase):
    def test_distance(self):
        import vnaaats_server as s
        fixes = {}
        self.assertEqual(s.route_distance_nm("52/50 52/40", (52.0, -50.0), fixes), 0)
        d = s.route_distance_nm("52/50 52/40", (52.0, -45.0), fixes)
        self.assertTrue(150 < d < 200, d)                      # ~5 degrees of longitude at 52N, nearest fix 52/40 or 52/50
        self.assertIsNone(s.route_distance_nm("52/50", None, fixes))
        self.assertIsNone(s.route_distance_nm("ZZZZZ", (52.0, -45.0), fixes))


class OfflineTests(unittest.TestCase):
    def test_offline(self):
        import vnaaats_server as s, time
        n = s.NetworkTraffic(None, "off", None)
        self.assertIsNone(n.is_offline("BAW1"))            # no feed yet: cannot tell
        n.positions, n.updated = {"BAW1": (50.0, -10.0)}, time.time()
        self.assertFalse(n.is_offline("baw1"))
        self.assertTrue(n.is_offline("GONE9"))
        n.updated = time.time() - 4000                     # stale feed: cannot tell
        self.assertIsNone(n.is_offline("GONE9"))


class CommTests(unittest.TestCase):
    def test_comm(self):
        import vnaaats_server as s
        n = s.NetworkTraffic(None, "x", None)
        n.atc_freq = {"133.500": "CZQO_CTR", "127.100": "EGTT_CTR"}
        n.tuned = {"A1": {"133.500"}, "B2": {"127.100"}, "C3": {"122.800"}, "D4": {"118.000"}}
        self.assertEqual(n.comm_for("a1")["kind"], "oceanic")
        self.assertEqual(n.comm_for("B2")["by"], "EGTT_CTR")
        self.assertEqual(n.comm_for("C3")["kind"], "unicom")
        self.assertIsNone(n.comm_for("D4"))
        self.assertIsNone(n.comm_for("NOPE"))
        n.refresh_transceivers(lambda: [{"callsign": "E5", "transceivers": [{"frequency": 133500000}, {"frequency": 133500000}]}])
        self.assertEqual(n.comm_for("E5")["by"], "CZQO_CTR")


class NatTrakReminderTests(unittest.TestCase):
    def test_star(self):
        import vnaaats_server as s, tempfile, os
        st = s.FlightStore(os.path.join(tempfile.mkdtemp(), "f.db"))
        st.upsert({"callsign": "BAW1", "route": "52/50", "relevant": "1", "track": "RR"})
        self.assertIsNone(st.mark_nt("NOPE1", "me"))
        st.mark_nt("BAW1", "CZQO_CTR (Gary)")
        st.upsert({"callsign": "BAW1", "route": "52/50", "relevant": "1", "track": "RR"})     # plugin update keeps the mark
        rec = st.get("BAW1")
        self.assertTrue(rec["ntSent"])
        class Nt:
            def __init__(self, d): self.d = d
            def by_callsign(self): return self.d
        self.assertFalse(s.nt_stops_reminder(rec, Nt({})))
        self.assertTrue(s.nt_stops_reminder(rec, Nt({"BAW1": {}})))
        self.assertTrue(s.nt_stops_reminder(dict(rec, trackedBy="CZQO_CTR"), Nt({})))
        self.assertTrue(s.nt_stops_reminder(dict(rec, trackedBy="NAT_FSS"), Nt({})))
        self.assertFalse(s.nt_stops_reminder(dict(rec, trackedBy="EGLL_TWR"), Nt({})))
        st.mark_nt("BAW1", "x", sent=False)
        self.assertNotIn("ntSent", st.get("BAW1"))


class TypedCommandTests(unittest.TestCase):
    def test_typed(self):
        import commands as c
        self.assertEqual(c.clean_command({"type": "contactme", "callsign": "baw1"})[2], {"text": ".contactme BAW1"})
        self.assertEqual(c.clean_command({"type": "selcal", "callsign": "BAW1", "selcal": "ab-cd"})[2], {"text": ".selcal ABCD"})
        self.assertIsNotNone(c.clean_command({"type": "selcal", "callsign": "BAW1", "selcal": "APPW"})[3])
        t = c.clean_command({"type": "natmsg", "callsign": "BAW1"})[2]["text"]
        self.assertTrue(t.startswith(".msg BAW1 Hello, you will soon be entering oceanic airspace."))
