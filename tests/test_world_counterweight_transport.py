import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'py'))
import anchor_world as w
import anchor_world_counterweight as cw


def row(index=13, mask=0, paused=0):
    r = [0] * 50
    r[:4] = [index, cw.ENTITY, cw.KIND, int(bool(mask))]
    r[4:10] = cw.POSES[index]
    r[10:16] = [r[5]] * 6
    r[16] = 63
    r[38], r[46], r[48] = paused, mask, index + 1
    return r


class CounterweightTransportTests(unittest.TestCase):
    def setUp(self):
        self.a, self.b = w.WorldTransport(), w.WorldTransport()
        self.ca = dict(cid=1, session=10, team='default', connected=True,
                       loaded=True, room=cw.ROOM, players={})
        self.cb = dict(self.ca, cid=2, session=20, players={})
        for t, c in ((self.a, self.ca), (self.b, self.cb)):
            self.tick(t, c, 0, [])
        for c, other, t in ((self.ca, self.cb, self.b),
                            (self.cb, self.ca, self.a)):
            c['players'][other['cid']] = dict(
                online=True, isSaveLoaded=True, teamId='default', roomId=cw.ROOM,
                interactionSession=other['session'], worldSync=t.advertisement(other))
        self.send(self.a, self.ca, self.b, self.cb, .1, [])
        self.send(self.b, self.cb, self.a, self.ca, .1, [])

    def tick(self, t, c, now, rows):
        return t.update(c, c['room'], 42, 1, rows, '00' * 32, now)

    def send(self, t, c, target, tc, now, rows):
        result, packets = self.tick(t, c, now, rows)
        for p in packets:
            self.assertTrue(target.receive(tc, p, now + .001))
        t.sent(packets, True, now)
        return result, packets

    def test_all_three_atomic_rows_and_recipe_bounds(self):
        for index in cw.POSES:
            r = row(index)
            self.assertTrue(w.row_valid(r))
            for slot, (lo, hi) in enumerate(cw.RANGES):
                for delta, expected in ((lo, True), (hi, True), (lo-1, False), (hi+1, False)):
                    probe = list(r);probe[10+slot] = probe[5]+delta
                    self.assertEqual(w.row_valid(probe), expected)
            for field, value in ((0, 12), (4, r[4]+1), (8, 17), (16, 31),
                                 (17, 3), (18, 1), (19, 1), (46, 64), (47, 64)):
                probe = list(r);probe[field] = value
                self.assertFalse(w.row_valid(probe), (field, value))
        self.assertIsNone(w.pad_inputs([[13, 1], [13, 32]], cw.ROOM))
        self.assertIsNone(w.pad_inputs([[12, 1]], cw.ROOM))
        self.assertIsNone(w.pad_inputs([[13, 4]], w.CRANE_ROOM))

    def test_opposing_riders_and_multiple_roots_stay_separate(self):
        # B only advertises presence and inputs after A owns these roots.
        a = [row(13, 1), row(14, 2), row(15, 4)]
        self.send(self.a, self.ca, self.b, self.cb, 1, a)
        fresh = [row(13, 32), row(14, 16), row(15, 8)]
        offers, _ = self.send(self.b, self.cb, self.a, self.ca, 1.1, fresh)
        self.assertEqual(len(offers['a']), 3)
        for i, received in enumerate(offers['a']):
            fresh[i][w.RECEIPT] = received[2+w.RECEIPT]
        self.send(self.b, self.cb, self.a, self.ca, 1.2, fresh)
        result, packets = self.tick(self.a, self.ca, 1.3, a)
        self.assertEqual([r[2+47] for r in result['a']], [33, 18, 12])
        self.assertTrue(all(r[0] == 1 for r in result['a']))
        for p in packets:
            self.assertEqual(p['u'], [[13, 1], [14, 2], [15, 4]])
            self.assertTrue(all(r[46:48] == [0, 0] for r in p['a']))

    def test_pause_cull_and_departure_release_only_their_inputs(self):
        self.send(self.a, self.ca, self.b, self.cb, 1, [row()])
        self.send(self.b, self.cb, self.a, self.ca, 1.1, [row(mask=32)])
        self.assertEqual(self.tick(self.a, self.ca, 1.12, [row(mask=1)])[0]['a'][0][49], 33)
        self.send(self.b, self.cb, self.a, self.ca, 1.2, [row(mask=32, paused=1)])
        self.assertEqual(self.tick(self.a, self.ca, 1.22, [row(mask=1)])[0]['a'][0][49], 1)
        self.send(self.b, self.cb, self.a, self.ca, 1.3, [])
        self.assertEqual(self.tick(self.a, self.ca, 1.32, [row(mask=1)])[0]['a'][0][49], 1)
        self.ca['players'][2]['online'] = False
        self.assertEqual(self.tick(self.a, self.ca, 1.4, [row(mask=1)])[0]['a'][0][49], 1)

    def test_room_and_wire_local_field_rejections(self):
        _, packets = self.tick(self.a, self.ca, 1, [row()])
        p = packets[0]
        for field in (46, 47, 48, 49):
            bad = copy.deepcopy(p);bad['a'][0][field] = 1
            self.assertFalse(self.b.receive(self.cb, bad, 1.01))
        bad = copy.deepcopy(p);bad['u'] = [[13, 1], [13, 2]]
        self.assertFalse(self.b.receive(self.cb, bad, 1.02))
        self.ca['room'] = 0x6a
        self.assertFalse(self.tick(self.a, self.ca, 2, [row()])[1])


if __name__ == '__main__':
    unittest.main()
