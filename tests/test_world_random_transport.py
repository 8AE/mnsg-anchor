import copy
import ctypes
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'py'))
import anchor_world as w
import anchor_world_dynamic as d
import anchor_world_random as r


def actor(role=0, ordinal=0, instance=1, receipt=0, serial=1):
    a = [0] * d.WORDS
    a[3] = serial
    a[6:12] = [r.KIND, r.PARENT, r.ENTITY, r.CHILD_MODEL if role else r.ENTITY, 0, role]
    a[24:27] = [1000] * 3
    a[32], a[42], a[64], a[67] = 163, r.CHILD_PHASE if role else r.ROOT_PHASE, ordinal, 17
    a[r.ROLE], a[r.INSTANCE], a[r.RECEIPT], a[r.PRESENT] = role, instance, receipt, 1
    if role:
        a[27:29] = [0x6e7, 0x20]
        a[19:22] = [64, 1, 7500]
        a[31] = 90
        a[r.TARGET_X], a[r.TARGET_Z] = -14000, 29000
    return a


class RandomTransportTests(unittest.TestCase):
    def setUp(self):
        self.wa, self.wb = w.WorldTransport(), w.WorldTransport()
        self.a, self.b = d.DynamicTransport(self.wa), d.DynamicTransport(self.wb)
        self.ca = dict(cid=1, session=10, team='default', connected=True,
                       loaded=True, room=r.ROOM, players={})
        self.cb = dict(self.ca, cid=2, session=20, players={})
        for t, c in ((self.wa, self.ca), (self.wb, self.cb)):
            t.update(c, r.ROOM, 42, 1, [], '00' * 32, 0)
        for c, other, t in ((self.ca, self.cb, self.wb), (self.cb, self.ca, self.wa)):
            c['players'][other['cid']] = dict(online=True, isSaveLoaded=True,
                teamId='default', roomId=r.ROOM, interactionSession=other['session'],
                worldSync=t.advertisement(other))
        self.a.update(self.ca, [], 0);self.b.update(self.cb, [], 0)
        self.send(self.a, self.ca, self.b, self.cb, [], .1)
        self.send(self.b, self.cb, self.a, self.ca, [], .1)

    def send(self, source, ctx, target, tc, rows, now):
        result, packets = source.update(ctx, rows, now)
        for p in packets:
            self.assertTrue(target.receive(tc, p, now + .001))
        source.sent(packets, True, now)
        return result, packets

    def test_overlapping_births_and_shared_target_checkpoint(self):
        rows = [actor(ordinal=2), actor(1, 1, serial=2), actor(1, 2, serial=3)]
        result, packets = self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        self.assertEqual(len(result['a']), 3)
        self.assertEqual(len({tuple(a[:4]) for a in result['a']}), 3)
        self.assertTrue(all(a[r.INSTANCE] == a[r.RECEIPT] == 0 for a in packets[0]['a']))
        late, _ = self.b.update(self.cb, [], 1.02)
        children = [a for a in late['a'] if a[r.ROLE]]
        self.assertEqual(len(children), 2)
        self.assertEqual({a[64] for a in children}, {1, 2})
        self.assertTrue(all(a[r.TARGET_X:r.TARGET_Z+1] == [-14000, 29000] for a in children))
        self.assertTrue(all(a[r.INSTANCE] == 0 and a[r.RECEIPT] for a in children))

    def test_root_is_invulnerable_and_scope_is_typed(self):
        a = actor();a[d.LIFE] = d.CLAIM
        self.assertFalse(d.valid(a))
        for field, value in ((7, 35), (8, 0x19d), (9, 0x12f), (42, 21), (75, 1), (33, 1)):
            bad = actor();bad[field] = value
            self.assertFalse(d.valid(bad), (field, value))
        _, packets = self.a.update(self.ca, [actor()], 1)
        bad = copy.deepcopy(packets[0]);bad['a'][0][0] = 0x7ffffff9
        self.assertFalse(self.b.receive(self.cb, bad, 1.01))
        a = actor(1, 1);a[75] = -3200001
        self.assertFalse(d.valid(a))

    def test_child_expiry_and_kill_use_different_removals(self):
        child = actor(1, 1)
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [child], 1)
        child[r.RECEIPT] = state['a'][0][r.RECEIPT]
        self.send(self.a, self.ca, self.b, self.cb, [child], 1.1)
        expired = list(child);expired[d.LIFE] = d.REMOVED;expired[d.COMMITTER] = 1
        result, _ = self.send(self.a, self.ca, self.b, self.cb, [expired], 1.2)
        self.assertEqual(result['a'][0][d.LANDED], 0)
        # A second independent child is hit by a player: shared kill, not expiry.
        second = actor(1, 2, serial=2)
        result, _ = self.send(self.a, self.ca, self.b, self.cb, [second], 1.3)
        second[r.RECEIPT] = next(a[r.RECEIPT] for a in result['a'] if a[64] == 2)
        second[d.LIFE] = d.CLAIM
        result, _ = self.send(self.a, self.ca, self.b, self.cb, [second], 1.4)
        killed = next(a for a in result['a'] if a[64] == 2)
        self.assertEqual(killed[d.LIFE], d.REMOVED)
        self.assertEqual(killed[d.LANDED], 1)

    def test_c_python_recipe_parity(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'codec.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-std=c99', '-I'+str(ROOT/'include'),
                str(ROOT/'src/world/anchor_world_dynamic_codec.c'),
                str(ROOT/'src/utils/string_utils.c'), '-o', str(path)], check=True)
            lib = ctypes.CDLL(str(path)); Row = ctypes.c_int * d.WORDS
            lib.anchor_world_dynamic_row_valid.argtypes = [ctypes.POINTER(ctypes.c_int)]
            for base in (actor(), actor(1, 1)):
                for col in range(d.WORDS):
                    for value in (-3200001, -3200000, -32769, -1, 0, 1, 2, 3, 17,
                                  22, 23, 36, 64, 90, 91, 256, 32768, 3200000,
                                  3200001, 0x7fffffff):
                        probe = list(base);probe[col] = value
                        self.assertEqual(bool(lib.anchor_world_dynamic_row_valid(Row(*probe))),
                                         d.valid(probe), (base[r.ROLE], col, value))


if __name__ == '__main__':
    unittest.main()
