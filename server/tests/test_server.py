import json, os, sys, tempfile, threading, unittest, urllib.error, urllib.request
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import vnaaats_server as s

SAMPLE = [
 {"identifier": "V", "active": True, "last_routeing": "ALLRY 51/50 52/40 5330/30 53/20 MALOT GISTI",
  "valid_from": "2026-10-20T01:00:00.000000Z", "valid_to": "2026-10-20T08:00:00.000000Z",
  "concorde": 0, "flight_levels": [34000, 35000], "direction": "east"},
 {"identifier": "H", "active": False, "last_routeing": "MALOT GISTI", "valid_from": "x", "valid_to": "y", "flight_levels": [], "direction": "west"},
 {"identifier": "A", "active": True, "last_routeing": "ETARI 56/20 NEEKO", "valid_from": "2026-10-20T01:00:00Z",
  "valid_to": "2026-10-20T08:00:00Z", "flight_levels": [36000], "direction": "west"},
]
KEY = "k" * 20


class T(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        cache = s.TrackCache(s.load_fixes())
        cache.refresh(fetch=lambda: SAMPLE)
        store = s.FlightStore(os.path.join(cls.tmp, "t.db"))
        cls.srv = s.ThreadingHTTPServer(("127.0.0.1", 0), s.make_handler(cache, store, KEY))
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()
        cls.base = "http://127.0.0.1:%d" % cls.srv.server_port
        cls.cache = cache

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()

    def get(self, path):
        try:
            with urllib.request.urlopen(self.base + path) as r:
                return r.status, r.read().decode()
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode()

    def test_track_source(self):
        self.assertEqual(self.get("/GetTrackSource"), (200, "0"))

    def test_tracks_format(self):
        code, body = self.get("/GetAllNatTracks")
        self.assertEqual(code, 200)
        t = json.loads(body)
        self.assertEqual([x["id"] for x in t], ["V", "A"])          # inactive track dropped
        v = t[0]
        self.assertEqual(v["direction"], 2)
        self.assertEqual(t[1]["direction"], 1)
        self.assertEqual(v["flightLevels"], [34000, 35000])
        names = [p["name"] for p in v["route"]]
        names = [n for n in names if n != "ALLRY"]                      # ALLRY unknown until a fix is posted
        self.assertEqual(names, ["51/50", "52/40", "5330/30", "53/20", "MALOT", "GISTI"])
        r = [p for p in v["route"] if p["name"] != "ALLRY"]
        self.assertEqual((r[0]["latitude"], r[0]["longitude"]), (51.0, -50.0))
        self.assertEqual((r[2]["latitude"], r[2]["longitude"]), (53.5, -30.0))
        self.assertEqual((r[4]["latitude"], r[4]["longitude"]), (53.0, -15.0))
        self.assertEqual(v["tmi"], "293")
        self.assertEqual(v["validFrom"], "1792458000")                  # 2026-10-20 01:00Z as unix seconds
        self.assertEqual(v["validTo"], "1792483200")
        for k in ("id", "tmi", "direction", "route", "flightLevels", "validFrom", "validTo"):
            self.assertIn(k, v)

    def test_unresolved_reported(self):
        self.cache.fixes.pop("ZZZZZ", None)
        s.convert_tracks([{"identifier": "Q", "active": True, "last_routeing": "ZZZZZ 51/50 52/40"}], self.cache.fixes, u := {})
        self.assertIn("ZZZZZ", u)

    def test_flight_roundtrip(self):
        q = ("callsign=baw123&type=B772&level=350&mach=84&track=V&route=51/50 52/40&routeEtas=1,2&departure=EGLL&arrival=KJFK"
             "&direction=1&etd=1200&selcal=ABCD&datalinkConnected=true&isEquipped=1&state=CLEARED&relevant=1&targetMode=3"
             "&trackedBy=Shanwick&trackedById=EGGX")
        code, _ = self.get("/PostFlightData?code=%s&%s" % (KEY, q.replace(" ", "%20")))
        self.assertEqual(code, 200)
        code, body = self.get("/GetSingleFlightData?callsign=BAW123")
        self.assertEqual(code, 200)
        f = json.loads(body)[0]
        self.assertEqual(f["callsign"], "BAW123")
        self.assertEqual(f["assignedLevel"], 350)
        self.assertIs(f["datalinkConnected"], True)
        self.assertIs(f["isEquipped"], True)
        self.assertIs(f["direction"], True)
        self.assertEqual(f["targetMode"], 3)
        self.assertEqual(f["route"], "51/50 52/40")
        # update changes it
        self.get("/UpdateFlightData?code=%s&callsign=BAW123&level=370&state=ISSUED" % KEY)
        self.assertEqual(json.loads(self.get("/GetSingleFlightData?callsign=BAW123")[1])[0]["assignedLevel"], 370)

    def test_requires_key(self):
        self.assertEqual(self.get("/PostFlightData?callsign=X")[0], 401)
        self.assertEqual(self.get("/PostFlightData?code=wrong&callsign=X")[0], 401)
        self.assertEqual(self.get("/UpdateFlightData?code=&callsign=X")[0], 401)

    def test_all_flights_and_cors(self):
        self.get("/PostFlightData?code=%s&callsign=AAA1&level=340" % KEY)
        self.get("/PostFlightData?code=%s&callsign=BBB2&level=360" % KEY)
        with urllib.request.urlopen(self.base + "/GetAllFlightData?sort=1") as r:
            self.assertEqual(r.headers["Access-Control-Allow-Origin"], "*")
            names = [f["callsign"] for f in json.loads(r.read())]
        self.assertIn("AAA1", names)
        self.assertLess(names.index("BBB2"), names.index("AAA1"))      # newest first

    def test_named_fix_resolves_track(self):
        self.assertEqual(self.get("/PostNamedFix?name=ALLRY&lat=50.5&lon=-50.0")[0], 401)
        self.assertEqual(self.get("/PostNamedFix?code=%s&name=ALLRY&lat=abc&lon=1" % KEY)[0], 400)
        self.assertEqual(self.get("/PostNamedFix?code=%s&name=bad!&lat=1&lon=1" % KEY)[0], 400)
        code, body = self.get("/PostNamedFix?code=%s&name=allry&lat=50.5&lon=-50.0" % KEY)
        self.assertEqual(code, 200)
        v = [t for t in json.loads(self.get("/GetAllNatTracks")[1]) if t["id"] == "V"][0]
        self.assertEqual(v["route"][0]["name"], "ALLRY")
        self.assertEqual((v["route"][0]["latitude"], v["route"][0]["longitude"]), (50.5, -50.0))
        self.assertNotIn("ALLRY", json.loads(self.get("/unresolved")[1]))

    def test_nattrak_placeholder(self):
        self.assertEqual(self.get("/GetNatTrakData"), (200, "[]"))

    def test_unknown_flight_404(self):
        self.assertEqual(self.get("/GetSingleFlightData?callsign=NOPE1")[0], 404)

    def test_failed_refresh_keeps_last_tracks(self):
        def boom(): raise RuntimeError("down")
        self.cache.refresh(fetch=boom)
        self.assertEqual(len(json.loads(self.get("/GetAllNatTracks")[1])), 2)
        self.assertEqual(json.loads(self.get("/health")[1])["trackError"], "down")


class NetworkTests(unittest.TestCase):
    TRACKS = [{"id": "A", "route": [{"name": "ETARI"}, {"name": "56/20"}, {"name": "56/30"}, {"name": "56/40"}, {"name": "NEEKO"}]}]

    def pilot(self, cs, route, alt="39000", dep="EGLL", arr="KJFK"):
        return {"callsign": cs, "flight_plan": {"aircraft_short": "A333", "departure": dep, "arrival": arr,
                                                 "altitude": alt, "deptime": "1200", "route": route}}

    def test_oceanic_point(self):
        self.assertEqual(s.oceanic_point("52/50")[1:], (52.0, -50.0))
        self.assertEqual(s.oceanic_point("5730/30")[1:], (57.5, -30.0))
        self.assertEqual(s.oceanic_point("52N050W")[1:], (52.0, -50.0))
        self.assertEqual(s.oceanic_point("5230N05000W")[1:], (52.5, -50.0))
        self.assertEqual(s.oceanic_point("52/50/M082F390")[1:], (52.0, -50.0))
        self.assertIsNone(s.oceanic_point("ALLRY"))

    def test_levels(self):
        self.assertEqual(s.parse_level("39000"), 390)
        self.assertEqual(s.parse_level("FL390"), 390)
        self.assertEqual(s.parse_level("F390"), 390)
        self.assertEqual(s.parse_level(""), 0)

    def test_random_route(self):
        r = s.convert_network_pilot(self.pilot("BAW1", "DCT 52/50 52/40 52/30 DCT"), self.TRACKS)
        self.assertEqual(r["track"], "RR")
        self.assertTrue(r["direction"])                     # 50W -> 30W is heading east
        self.assertEqual(r["route"], "52/50 52/40 52/30")
        self.assertEqual(r["assignedLevel"], 390)

    def test_track_match_and_direction(self):
        r = s.convert_network_pilot(self.pilot("DLH1", "ETARI 56/20 56/30 56/40 NEEKO", dep="EDDF", arr="KJFK"), self.TRACKS)
        self.assertEqual(r["track"], "A")
        r2 = s.convert_network_pilot(self.pilot("DLH2", "56/40 56/30 56/20", dep="KJFK", arr="EDDF"), self.TRACKS)
        self.assertTrue(r2["direction"])

    def test_non_oceanic_ignored(self):
        self.assertIsNone(s.convert_network_pilot(self.pilot("X1", "DCT LAM DCT"), self.TRACKS))
        self.assertIsNone(s.convert_network_pilot({"callsign": "X2"}, self.TRACKS))
        self.assertIsNone(s.convert_network_pilot(self.pilot("X3", "26/138 27/139"), self.TRACKS))

    def test_endpoint_and_failure(self):
        cache = s.TrackCache(s.load_fixes())
        net = s.NetworkTraffic(cache)
        net.refresh(fetch=lambda: {"pilots": [self.pilot("BAW1", "52/50 52/40"), self.pilot("X1", "DCT")]})
        self.assertEqual([f["callsign"] for f in net.all()], ["BAW1"])
        def boom(): raise RuntimeError("feed down")
        net.refresh(fetch=boom)
        self.assertEqual(len(net.all()), 1)                  # keeps last good copy
        self.assertEqual(net.error, "feed down")


class NatTrakTests(unittest.TestCase):
    ITEMS = [{"callsign": "BAW1", "status": "CLEARED", "nat": "A", "fix": "ETARI", "level": "370", "mach": "0.82",
              "estimating_time": "09:05", "clearance_issued": "2026-10-20T08:18:59.000000Z", "extra_info": None},
             {"callsign": "BAW2", "status": "PENDING", "nat": "RR", "fix": "ALLRY", "level": "350", "mach": "0.80",
              "estimating_time": "09:10", "clearance_issued": None, "extra_info": None}]

    def test_feed_keeps_last_good(self):
        f = s.NatTrakFeed()
        f.refresh(fetch=lambda: self.ITEMS)
        self.assertEqual(len(f.all()), 2)
        def boom(): raise RuntimeError("down")
        f.refresh(fetch=boom)
        self.assertEqual(len(f.all()), 2)
        f.refresh(fetch=lambda: {"not": "a list"})
        self.assertEqual(len(f.all()), 2)

    def test_overlay_on_network_flights(self):
        cache = s.TrackCache(s.load_fixes())
        cache.refresh(fetch=lambda: SAMPLE)                # has track A
        nt = s.NatTrakFeed()
        nt.refresh(fetch=lambda: self.ITEMS)
        net = s.NetworkTraffic(cache, natrak=nt)
        pil = lambda cs: {"callsign": cs, "flight_plan": {"aircraft_short": "A333", "departure": "EGLL", "arrival": "KJFK",
                          "altitude": "30000", "deptime": "1200", "route": "52/20 52/30 52/40"}}
        net.refresh(fetch=lambda: {"pilots": [pil("BAW1"), pil("BAW2")]})
        got = {f["callsign"]: f for f in net.all()}
        self.assertEqual((got["BAW1"]["assignedLevel"], got["BAW1"]["assignedMach"], got["BAW1"]["track"]), (370, 82, "A"))
        self.assertEqual(got["BAW2"]["assignedLevel"], 300)    # pending is not a clearance: unchanged


if __name__ == "__main__":
    unittest.main()
