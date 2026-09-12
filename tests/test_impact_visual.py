import copy
import json
from types import SimpleNamespace
import unittest
import anchor_impact_visual as visual
from test_impact_players import context


def battle(role):
    return SimpleNamespace(role=role, owner=1, owner_session=100, owner_visit=1,
                           term=1, e=(1,100,1), paused=False)


def rows(n=64):
    return [[i+1,121,38,2,0xFFFFFFFF,9,0xFF01,
             0xC9742400,0xC9742400,0xC9742400,65535,65535,65535,
             0x49742400,0x49742400,0x49742400,0x49742400,
             *([0x876F,0x7FFFFF]*6)] for i in range(n)]


class ImpactVisualTests(unittest.TestCase):
    def setUp(self):
        self.a,self.b = context(1),context(2)
        self.host,self.guest = battle(1),battle(2)
        self.tx,self.rx = visual.ImpactVisualTransport(),visual.ImpactVisualTransport()
        self.rx.update(self.b,self.guest,1,0x260,1,1,None,10)

    def send(self, now=10, sample=None):
        return self.tx.update(self.a,self.host,1,0x260,1,1,rows() if sample is None else sample,now)[1]

    def view(self, now=10):
        return self.rx.update(self.b,self.guest,1,0x260,1,1,None,now)[0]

    def test_atomic_reordered_paging_and_empty_frame(self):
        packets = self.send()
        for p in [packets[3],packets[0],packets[2]]:
            self.assertTrue(self.rx.receive(self.b,self.guest,p,10))
            self.assertIsNone(self.view())
        self.assertTrue(self.rx.receive(self.b,self.guest,packets[1],10))
        self.assertEqual(self.view(),rows())
        self.assertFalse(self.rx.receive(self.b,self.guest,packets[0],10))
        empty = self.send(10.2,[])
        self.assertTrue(self.rx.receive(self.b,self.guest,empty[0],10.2))
        self.assertEqual(self.view(10.2),[])
        self.assertIsNone(self.view(11))

    def test_owner_term_session_visit_stage_and_route_fences(self):
        packet = self.send()[0]
        for key,value in [("clientId",3),("session",101),("visit",2),("term",2),
                          ("s",0x220),("k",2),("v",True),("e",[1,100,2]),
                          ("targetTeamId","elsewhere"),("targetClientId",2),("addToQueue",False)]:
            with self.subTest(key=key):
                self.assertFalse(self.rx.receive(self.b,self.guest,{**packet,key:value},10))
        self.guest.term = 2
        self.assertFalse(self.rx.receive(self.b,self.guest,packet,10))
        self.assertIsNone(self.view())
        self.guest.term = 1
        self.view()
        self.b["players"][1]["online"] = False
        self.assertFalse(self.rx.receive(self.b,self.guest,packet,10))

    def test_new_frame_discards_incomplete_old_and_duplicate_ids(self):
        old,new = self.send(),self.send(10.2)
        self.assertTrue(self.rx.receive(self.b,self.guest,old[0],10))
        self.assertTrue(self.rx.receive(self.b,self.guest,new[1],10.2))
        self.assertFalse(self.rx.receive(self.b,self.guest,old[1],10.2))
        for p in [new[0],new[2],new[3]]:
            self.rx.receive(self.b,self.guest,p,10.2)
        self.assertEqual(self.view(10.2),rows())
        new = self.send(10.4)
        new[3]["r"][0][0] = 1
        for p in new[:3]:
            self.assertTrue(self.rx.receive(self.b,self.guest,p,10.4))
        self.assertFalse(self.rx.receive(self.b,self.guest,new[3],10.4))
        self.assertEqual(self.view(10.4),rows())

    def test_wire_budget_and_hard_cadence(self):
        count = size = 0
        for tick in range(240):
            for p in self.send(10+tick/240):
                count += 1
                n = len(json.dumps(p,separators=(",", ":")).encode())+1
                self.assertLessEqual(n,visual.PACKET_BYTES)
                self.assertEqual(p["targetTeamId"],"default")
                self.assertNotIn("addToQueue",p)
                size += n
        self.assertLessEqual(count,32)
        self.assertLessEqual(size,192*1024)
        self.assertEqual(self.rx.update(self.b,self.guest,1,0x260,1,1,rows(),11)[1],[])

    def test_malformed_words_and_page_bounds(self):
        original = self.send()[0]
        for index,value in [(0,0),(1,122),(2,39),(3,3),(5,16),(6,2),
                            (7,0x7F800000),(10,65536),(17,0x8770),(18,0x800000)]:
            p = copy.deepcopy(original); p["r"][0][index] = value
            self.assertFalse(self.rx.receive(self.b,self.guest,p,10))
        for key,value in [("n",5),("p",4),("q",True),("extension","x"*6144)]:
            self.assertFalse(self.rx.receive(self.b,self.guest,{**original,key:value},10))

    def test_pause_disconnect_and_local_reentry_clear_cache(self):
        for p in self.send(): self.rx.receive(self.b,self.guest,p,10)
        self.assertIsNotNone(self.view())
        self.guest.paused = True
        self.assertIsNone(self.view())
        self.guest.paused = False
        self.assertIsNone(self.view())
        for p in self.send(10.2): self.rx.receive(self.b,self.guest,p,10.2)
        self.assertIsNotNone(self.view(10.2))
        self.assertIsNone(self.rx.update(self.b,self.guest,1,0x260,1,2,None,10.3)[0])
        self.b["connected"] = False
        self.assertIsNone(self.view(10.4))

    def test_no_render_traffic_without_an_eligible_peer(self):
        self.a["players"] = {1:self.a["players"][1]}
        self.assertEqual(self.send(),[])


if __name__ == "__main__":
    unittest.main()
