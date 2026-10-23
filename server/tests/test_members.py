import json, os, sys, tempfile, threading, unittest, urllib.error, urllib.request
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import members as mb
import vnaaats_server as s

SECRET = "i" * 30
SHARED = "k" * 20


class MemberTests(unittest.TestCase):
    def setUp(self):
        self.store = s.FlightStore(os.path.join(tempfile.mkdtemp(), "m.db"))
        self.t = [1000.0]
        self.m = mb.MemberKeys(self.store, now=lambda: self.t[0])

    def test_issue_and_check(self):
        out = self.m.issue("1234567", "Jane Pilot")
        self.assertTrue(out["key"].startswith("1234567."))
        self.assertEqual(self.m.check_key(out["key"]), ("1234567", ""))
        self.assertEqual(self.m.check_key("1234567.wrongwrongwrongwrongwrong")[1], "unknown")
        self.assertEqual(self.m.check_key("nonsense")[1], "unknown")
        self.assertEqual(self.m.check_key("")[1], "unknown")
        with self.store.lock:                                   # only hashes are stored
            blob = json.dumps(self.store.db.execute("SELECT * FROM members").fetchall())
        self.assertNotIn(out["key"].split(".")[1], blob)
        self.assertNotIn(out["password"], blob)

    def test_expiry_revoke_reissue(self):
        out = self.m.issue("1234567", "Jane Pilot")
        self.t[0] += 91 * 86400
        self.assertEqual(self.m.check_key(out["key"])[1], "expired")
        new = self.m.issue("1234567", "Jane Pilot")              # asking again gives a fresh key and kills the old one
        self.assertEqual(self.m.check_key(new["key"])[0], "1234567")
        self.assertEqual(self.m.check_key(out["key"])[1], "unknown")
        self.assertTrue(self.m.revoke("1234567"))
        self.assertEqual(self.m.check_key(new["key"])[1], "revoked")
        self.assertFalse(self.m.revoke("1234567"))

    def test_login(self):
        out = self.m.issue("1234567", "Jane Pilot")
        self.assertEqual(self.m.check_login("jane pilot", out["password"])["cid"], "1234567")
        self.assertEqual(self.m.check_login("1234567", out["password"])["name"], "Jane Pilot")
        self.assertIsNone(self.m.check_login("1234567", "wrong"))
        self.assertIsNone(self.m.check_login("", out["password"]))
        self.m.revoke("1234567")
        self.assertIsNone(self.m.check_login("1234567", out["password"]))

    def test_sync(self):
        a, b = self.m.issue("111", "A"), self.m.issue("222", "B")
        self.assertEqual(self.m.sync(["111"]), ["222"])
        self.assertEqual(self.m.check_key(a["key"])[0], "111")
        self.assertEqual(self.m.check_key(b["key"])[1], "revoked")


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        store = s.FlightStore(os.path.join(tempfile.mkdtemp(), "h.db"))
        cls.members = mb.MemberKeys(store)
        auth = s.Auth({"VNAAATS_FDD_PASSWORD": "sharedpw"})
        cls.srv = s.ThreadingHTTPServer(("127.0.0.1", 0), s.make_handler(s.TrackCache(s.load_fixes()), store, SHARED, None, None, None, auth, None, None, None, cls.members, SECRET))
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()
        cls.url = "http://127.0.0.1:%d" % cls.srv.server_address[1]

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()

    def call(self, path, body=None, secret=None):
        h = {"Content-Type": "application/json"}
        if secret:
            h["X-Issuer-Secret"] = secret
        req = urllib.request.Request(self.url + path, data=json.dumps(body).encode() if body is not None else None, headers=h)
        try:
            with urllib.request.urlopen(req) as r:
                return r.status, json.loads(r.read() or b"{}")
        except urllib.error.HTTPError as e:
            return e.code, json.loads(e.read() or b"{}")

    def test_issuer_secret_required(self):
        self.assertEqual(self.call("/admin/member/issue", {"cid": "1"})[0], 403)
        self.assertEqual(self.call("/admin/member/issue", {"cid": "1"}, secret="x" * 30)[0], 403)

    def test_key_flow_through_http(self):
        st, out = self.call("/admin/member/issue", {"cid": "9990001", "name": "Test Member"}, secret=SECRET)
        self.assertEqual(st, 200)
        ok = self.call("/PluginCommands?me=A&code=" + out["key"])
        self.assertEqual(ok[0], 200)
        self.assertEqual(self.call("/PluginCommands?me=A&code=" + SHARED)[0], 200)            # shared key still works
        bad = self.call("/PluginCommands?me=A&code=9990001.aaaaaaaaaaaaaaaaaaaaaaaa")
        self.assertEqual((bad[0], bad[1].get("reason")), (401, "unknown"))
        self.assertEqual(self.call("/PluginCommands?me=A&code=caf%C3%A9")[0], 401)              # non-ASCII is a clean refusal
        login = self.call("/auth/password", {"name": "9990001", "password": out["password"]})
        self.assertEqual((login[0], login[1]["name"]), (200, "Test Member"))
        self.assertEqual(self.call("/auth/password", {"name": "9990001", "password": "nope"})[0], 401)
        self.call("/admin/member/revoke", {"cid": "9990001"}, secret=SECRET)
        self.assertEqual(self.call("/PluginCommands?me=A&code=" + out["key"])[1].get("reason"), "revoked")
        self.assertEqual(self.call("/auth/password", {"name": "9990001", "password": out["password"]})[0], 401)

    def test_sync_refuses_empty_list(self):
        self.assertEqual(self.call("/admin/member/sync", {"cids": []}, secret=SECRET)[0], 400)


if __name__ == "__main__":
    unittest.main()
