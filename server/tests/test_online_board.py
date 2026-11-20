import os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import online_board as ob

FEED = {"controllers": [
    {"callsign": "CZQX_CTR", "name": "A B", "frequency": "132.200", "logon_time": "2026-11-20T10:00:00Z"},
    {"callsign": "CZQX_ATIS", "frequency": "128.000"},
    {"callsign": "EGGX_CTR", "name": "C D", "frequency": "127.900", "logon_time": "2026-11-20T10:00:00Z"},
    {"callsign": "KJFK_TWR", "frequency": "119.100"}]}


class BoardTests(unittest.TestCase):
    def mk(self, feed):
        self.calls = []
        def call(m, u, b=None):
            self.calls.append((m, u)); return {"id": "42"}
        d = tempfile.mkdtemp()
        return ob.Board("https://discord.com/api/webhooks/1/x", ob.parse_prefixes("czqx,eggx"), "https://x/logo.png",
                        os.path.join(d, "s.json"), "u", call=call, fetch=lambda: feed)

    def test_pick_filters_and_sorts(self):
        self.assertEqual([c["callsign"] for c in ob.pick(FEED, ob.parse_prefixes("CZQX,EGGX"))], ["CZQX_CTR", "EGGX_CTR"])

    def test_embed_has_logo(self):
        p = ob.build(ob.pick(FEED, ("CZQX",)), "https://x/logo.png")
        self.assertEqual(p["embeds"][0]["thumbnail"]["url"], "https://x/logo.png")
        self.assertIn("CZQX_CTR", p["embeds"][0]["description"])

    def test_empty_board(self):
        self.assertIn("No oceanic", ob.build([], "l")["embeds"][0]["description"])

    def test_post_then_same_then_edit(self):
        b = self.mk(FEED)
        self.assertEqual(b.tick(1000), "posted")
        self.assertEqual(b.tick(1060), "same")
        b.fetch = lambda: {"controllers": []}
        self.assertEqual(b.tick(1120), "edited")
        self.assertEqual([m for m, _ in self.calls], ["POST", "PATCH"])
        self.assertTrue(self.calls[1][1].endswith("/messages/42"))

    def test_refresh_every_ten_minutes(self):
        b = self.mk(FEED)
        b.tick(1000)
        self.assertEqual(b.tick(1700), "edited")


if __name__ == "__main__":
    unittest.main()
