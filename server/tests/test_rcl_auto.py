import os, sys, tempfile, unittest
from datetime import datetime, timezone
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import rcl_auto as r
import commands as cm
import vnaaats_server as s


class FakeNet:
    def __init__(self):
        self.pos, self.pilots, self.offline, self.flights = {}, {}, {}, []
    def is_offline(self, cs): return self.offline.get(cs)
    def position(self, cs): return self.pos.get(cs)
    def lookup_pilot(self, cs): return self.pilots.get(cs)
    def all(self): return list(self.flights)


class FakeNat:
    def __init__(self): self.d = {}
    def by_callsign(self): return self.d


class RclTests(unittest.TestCase):
    def setUp(self):
        self.store = s.FlightStore(os.path.join(tempfile.mkdtemp(), "f.db"))
        self.q = cm.CommandQueue()
        self.net, self.nat = FakeNet(), FakeNat()
        self.t = [datetime(2026, 10, 23, 12, 0, tzinfo=timezone.utc).timestamp()]
        self.dist = [None]
        self.rcl = r.RclAuto(self.store, self.q, self.net, self.nat, lambda route, pos: self.dist[0], ("CZQO", "EGGX"), now=lambda: self.t[0], start=False)
        self.rec = {"callsign": "BAW1", "route": "A B", "routeEtas": "1230 1300", "trackedBy": ""}
        self.net.flights = [self.rec]
        self.net.pos["BAW1"] = (50.0, -20.0)
        self.net.pilots["BAW1"] = {"groundspeed": 450}

    def poll(self, me="CZQO_CTR"):
        self.rcl.note_poll(me)

    def test_entry_minutes(self):
        self.assertEqual(r.entry_minutes_from_now("1230 1300", 12 * 60), 30)
        self.assertEqual(r.entry_minutes_from_now("0010 0100", 23 * 60 + 50), 20)    # across midnight
        self.assertEqual(r.entry_minutes_from_now("1150 1300", 12 * 60), -10)
        self.assertIsNone(r.entry_minutes_from_now("-- --", 0))

    def test_due_rules(self):
        self.assertTrue(self.rcl.due(self.rec))                         # 30 min to entry
        self.rec["routeEtas"] = "1330 1400"
        self.assertFalse(self.rcl.due(self.rec))                        # 90 min away, no distance known
        self.dist[0] = 150
        self.assertTrue(self.rcl.due(self.rec))                         # but within 200 nm
        self.dist[0] = 250
        self.assertFalse(self.rcl.due(self.rec))
        self.rec["routeEtas"] = "1140 1300"
        self.dist[0] = 100
        self.assertFalse(self.rcl.due(self.rec))                        # entry passed 20 min ago
        self.rec["routeEtas"] = "1158 1300"
        self.assertFalse(self.rcl.due(self.rec))                        # entry passed 2 min ago: already in oceanic
        self.rec["routeEtas"] = "1215 1300"
        self.assertFalse(self.rcl.due(self.rec))                        # 15 min to entry: natTrak would refuse the request
        self.rec["routeEtas"] = "1220 1300"
        self.assertTrue(self.rcl.due(self.rec))                         # exactly 20 min: still allowed
        self.rec["routeEtas"] = "-- --"
        self.dist[0] = 100
        self.net.pilots["BAW1"] = {"groundspeed": 450}
        self.assertFalse(self.rcl.due(self.rec))                        # no times: 100 nm at 450 kt is 13 min, too late
        self.dist[0] = 190
        self.assertTrue(self.rcl.due(self.rec))                         # 190 nm at 450 kt is 25 min
        self.rec["routeEtas"] = "1230 1300"
        self.rec["routeEtas"] = "1230 1300"
        self.net.pilots["BAW1"] = {"groundspeed": 5}
        self.assertFalse(self.rcl.due(self.rec))                        # parked
        self.net.pilots["BAW1"] = {"groundspeed": 450}
        self.nat.d["BAW1"] = {"status": "PENDING"}
        self.assertFalse(self.rcl.due(self.rec))                        # already on natTrak
        del self.nat.d["BAW1"]
        self.rec["trackedBy"] = "CZQO_CTR"
        self.assertFalse(self.rcl.due(self.rec))                        # an oceanic controller has it
        self.rec["trackedBy"] = ""
        self.rec["ntSent"] = {"by": "x"}
        self.assertFalse(self.rcl.due(self.rec))                        # already reminded by hand
        self.rec.pop("ntSent")
        self.net.offline["BAW1"] = True
        self.assertFalse(self.rcl.due(self.rec))

    def test_needs_an_oceanic_sender(self):
        self.assertIsNone(self.rcl.tick())                              # nobody online
        self.poll("EGLL_TWR")
        self.assertIsNone(self.rcl.tick())                              # not an oceanic position
        self.poll("CZQO_CTR")
        self.assertEqual(self.rcl.tick(), "BAW1")
        cmds = self.q.pending_for("CZQO_CTR")
        self.assertEqual(cmds[0]["type"], "natmsg")
        self.assertEqual(cmds[0]["by"], "AUTO")
        self.assertTrue(cmds[0]["args"]["text"].startswith(".msg BAW1 Hello"))

    def test_sent_once_and_marked(self):
        self.poll()
        self.assertEqual(self.rcl.tick(), "BAW1")
        self.assertIsNone(self.rcl.tick())                              # already in flight
        c = self.q.done(1, True, "ok")
        self.rcl.done(c, True)
        self.assertTrue(self.rcl.already_sent("BAW1"))
        self.assertTrue(self.store.get("BAW1")["ntSent"])
        self.t[0] += 3600
        self.poll()
        self.assertIsNone(self.rcl.tick())                              # never twice

    def test_failure_retries_then_gives_up(self):
        self.poll()
        for n in range(r.MAX_TRIES):
            self.assertEqual(self.rcl.tick(), "BAW1")
            c = self.q.done(n + 1, False, "autotype is off")
            self.rcl.done(c, False, "autotype is off")
            self.assertIsNone(self.rcl.tick())                          # waits before trying again
            self.t[0] += r.RETRY_SECONDS + 1
            self.poll()
        self.assertIsNone(self.rcl.tick())
        self.assertFalse(self.rcl.already_sent("BAW1"))

    def test_switch_off(self):
        self.poll()
        self.rcl.set_enabled(False)
        self.assertIsNone(self.rcl.tick())
        self.rcl.set_enabled(True)
        self.assertEqual(self.rcl.tick(), "BAW1")


if __name__ == "__main__":
    unittest.main()
