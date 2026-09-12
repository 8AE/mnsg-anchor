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

    def receive_sample(self, now, x=0, angle=0, handle=65, frame=0):
        row = rows(1)[0]
        row[0] = handle
        row[6] = 0
        row[7:10] = [visual._bits(x), 0, 0]
        row[10:13] = [angle, 0x8000, 0]
        row[13:16] = [visual._bits(1)] * 3
        row[16] = visual._bits(frame)
        for packet in self.send(now, [row]):
            self.assertTrue(self.rx.receive(self.b, self.guest, packet, now))
        return row

    def test_render_interpolates_every_tick_with_shortest_arc(self):
        first = self.receive_sample(10, 0, 1000, frame=2)
        self.assertEqual(self.view(10), [first])  # no startup blank frame
        self.receive_sample(10.125, 12, 24, frame=8)
        positions = []
        for now in (10.15, 10.175, 10.2, 10.225, 10.25, 10.275):
            row = self.view(now)[0]
            positions.append(visual._float(row[7]))
            self.assertEqual(row[11], 0x8000)
            self.assertTrue(visual.rows_valid([row]))
        for actual, expected in zip(positions, (0, 2.4, 4.8, 7.2, 9.6, 12)):
            self.assertAlmostEqual(actual, expected, places=4)
        middle = self.view(10.2125)[0]
        self.assertEqual(middle[10], 0)
        self.assertAlmostEqual(visual._float(middle[16]), 5)
        self.assertEqual(visual._float(self.rx.latest[0][7]), 12)  # never mutate receipt

    def test_spawn_despawn_reuse_teleport_and_animation_reset(self):
        self.receive_sample(10, 0, frame=30)
        self.receive_sample(10.125, 6, frame=1)
        self.assertEqual(visual._float(self.view(10.2)[0][16]), 1)
        born = self.receive_sample(10.25, 60, handle=129)
        self.assertEqual(self.view(10.25), [born])  # same slot, different lifetime
        jumped = self.receive_sample(10.375, 1500, handle=129)
        self.assertEqual(self.view(10.45), [jumped])
        for packet in self.send(10.5, []): self.rx.receive(self.b, self.guest, packet, 10.5)
        self.assertEqual(self.view(10.5), [])  # no ghost attacks in delay buffer
        resumed = self.receive_sample(11, 1, handle=193)
        self.assertEqual(self.view(11), [resumed])
        self.assertEqual(len(self.rx.history), 1)

    def test_jitter_gaps_hold_latest_then_expire_and_history_is_bounded(self):
        for n, now in enumerate((10, 10.14, 10.30, 10.43, 10.58, 10.72, 10.89, 11.02)):
            self.receive_sample(now, n * 10)
        self.assertEqual(len(self.rx.history), 6)
        self.assertEqual(visual._float(self.view(11.5)[0][7]), 70)  # no extrapolation
        self.assertIsNone(self.view(11.8))
        self.guest.term += 1
        self.assertIsNone(self.view(11.8))
        self.assertEqual(len(self.rx.history), 0)

    def test_animated_texture_offsets_do_not_disable_motion_smoothing(self):
        a = self.receive_sample(10, 0)
        b = list(a); b[7] = visual._bits(12); b[20] -= 64
        out = visual.blend_row(a, b, .5)
        self.assertEqual(visual._float(out[7]), 6)
        self.assertEqual(out[20], a[20])  # texture stays on the buffered frame
        b[19] -= 1  # changing the asset file is a discontinuity
        self.assertEqual(visual.blend_row(a, b, .5), b)

    def test_generation_handles_keep_slot_and_wire_bounds(self):
        sample = rows()
        for i, row in enumerate(sample): row[0] = (0x3FFFFFE << 6) + i + 1
        self.assertTrue(visual.rows_valid(sample))
        packets = self.send(sample=sample)
        self.assertEqual(len(packets), 4)
        for packet in packets:
            self.assertLessEqual(len(json.dumps(packet, separators=(",", ":")).encode())+1,
                                 visual.PACKET_BYTES)
            self.assertTrue(self.rx.receive(self.b, self.guest, packet, 10))
        self.assertEqual(self.view(), sample)
        sample[1][0] = sample[0][0] - 64  # distinct ID, same pool slot is invalid
        self.assertFalse(visual.rows_valid(sample))

    def test_no_render_traffic_without_an_eligible_peer(self):
        self.a["players"] = {1:self.a["players"][1]}
        self.assertEqual(self.send(),[])


if __name__ == "__main__":
    unittest.main()
