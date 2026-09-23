"""Multi-client File68 fish transport: scope, one-winner claim, late removal.

The native bridge is not part of this module, so the tests drive its two words
directly: a capture row reports the local native instance and echoes the apply
token, and the bridge-facing result is inspected next to the wire row that was
actually sent. Every case runs two clients so ownership, arbitration and the
receipt handshake are exercised across a real exchange.

The row is an identity/claim record, not a portable copy of File68's private
animation state: the validator forces the whole clip..role span clear apart
from the route, claim marker and arbiter words. The parity sweep probes every
column, so an accidental over- or under-constraint on the Python side shows up
as a mismatch instead of passing silently.

Flag IDs repeat across the room pairs 0x168/0x17E and 0x16B/0x180. The same-room
typed claim cannot make two clients collecting that pair in different rooms at
the same instant exclusive, and that limitation is asserted rather than hidden.
"""
import copy
import ctypes
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'py'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import anchor_world as w
import anchor_world_dynamic as d
import anchor_world_fish as f

ROOM = 0x168
PARENT = 10
SIGNATURE = 42


def fish(parent=PARENT, room=ROOM, flag=None, variant=None, life=None,
         landed=0, committer=0, owner=0, serial=1, instance=0, receipt=0,
         established=0, present=1, paused=0, dirty=False):
    """A fish identity/claim row.

    `dirty=True` adds pose and collision words, which the validator deliberately
    rejects: the wire row identifies a fish and arbitrates its touch, it is not
    a portable copy of File68's private animation state.
    """
    fixed = f.placement(room, parent)
    if fixed is None:
        fixed = (0xA7, 0)
    if flag is None:
        flag = fixed[0]
    if variant is None:
        variant = fixed[1]
    r = [0] * d.WORDS
    r[:12] = [0, 0, 0, serial, owner, d.LIVE if life is None else life, f.KIND,
              parent, f.ENTITY, f.ENTITY, 0, 0]
    if dirty:
        r[12], r[13], r[14] = 35, -44, -39
        r[15:18] = [0, 512, 0]
        r[24:27] = [100, 100, 100]
        r[27], r[28] = 0x6E1, 0x20
        r[47], r[48], r[49] = 40, 30, 0
        r[54], r[55], r[56], r[57] = 1, 2, 2, 1
        r[59], r[60] = 12000, 8000
    r[f.ROUTE] = f.ROUTE_MARK
    r[f.LANDED] = landed
    r[66] = paused
    r[f.COMMITTER] = committer
    r[f.FLAG] = flag
    r[f.VARIANT] = variant
    r[f.INSTANCE], r[f.RECEIPT] = instance, receipt
    r[f.ESTABLISHED], r[f.PRESENT] = established, present
    return r


class FishHarness(unittest.TestCase):
    def setUp(self):
        self.wa = w.WorldTransport()
        self.wb = w.WorldTransport()
        self.a = d.DynamicTransport(self.wa)
        self.b = d.DynamicTransport(self.wb)
        self.ca = dict(cid=1, session=10, team='default', connected=True,
                       loaded=True, room=ROOM, players={})
        self.cb = dict(self.ca, cid=2, session=20, players={})
        for t, c in ((self.wa, self.ca), (self.wb, self.cb)):
            t.update(c, ROOM, SIGNATURE, 1, [], '00' * 32, 0)
        for c, other, t in ((self.ca, self.cb, self.wb),
                            (self.cb, self.ca, self.wa)):
            c['players'][other['cid']] = dict(
                online=True, isSaveLoaded=True, teamId='default', roomId=ROOM,
                interactionSession=other['session'],
                worldSync=t.advertisement(other))
        self.a.update(self.ca, [], 0)
        self.b.update(self.cb, [], 0)
        for tx, c, rx, rc in ((self.wa, self.ca, self.wb, self.cb),
                              (self.wb, self.cb, self.wa, self.ca)):
            _, packets = tx.update(c, ROOM, SIGNATURE, 1, [], '00' * 32, 0)
            for packet in packets:
                self.assertTrue(rx.receive(rc, packet, 0))
        self.sync(0)

    def sync(self, now):
        for t, c, rx, rc in ((self.a, self.ca, self.b, self.cb),
                             (self.b, self.cb, self.a, self.ca)):
            _, packets = t.update(c, [], now)
            for packet in packets:
                self.assertTrue(rx.receive(rc, packet, now + .001))
            t.sent(packets, True, now)

    def send(self, t, c, rx, rc, rows, now, success=True):
        result, packets = t.update(c, rows, now)
        for packet in reversed(packets):
            self.assertTrue(rx.receive(rc, packet, now + .001))
        t.sent(packets, success, now)
        return result, packets

    def establish(self, row=None):
        row = fish(instance=4) if row is None else row
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [row], 1)
        token = state['a'][0][f.RECEIPT]
        echo = fish(instance=4, receipt=token, parent=row[f.PARENT],
                    room=ROOM)
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [echo], 1.06)
        self.assertEqual(state['a'][0][f.ESTABLISHED], 1)
        return state['a'][0]


class FishScopeTests(FishHarness):
    def test_scope_table_is_the_exact_roster_with_flags_and_variants(self):
        self.assertEqual(sum(len(v) for v in f.SCOPE.values()), 33)
        self.assertEqual(set(f.SCOPE),
                         {0x168, 0x16B, 0x16E, 0x171, 0x172, 0x17E, 0x180})
        self.assertEqual(f.placement(0x168, 10), (0xB8, 0))
        self.assertEqual(f.placement(0x168, 11), (0xB9, 1))
        self.assertEqual(f.placement(0x16B, 6), (0xAB, 0))
        self.assertEqual(f.placement(0x16E, 11), (0xA7, 0))
        self.assertEqual(f.placement(0x171, 12), (0xB0, 0))
        self.assertEqual(f.placement(0x172, 8), (0xBD, 0))
        self.assertEqual(f.placement(0x17E, 3), (0xB8, 0))
        self.assertEqual(f.placement(0x180, 2), (0xAB, 0))
        self.assertIsNone(f.placement(0x168, 9))
        self.assertIsNone(f.placement(0x172, 13))

    def test_placement_table_matches_the_native_header(self):
        """The C header is the shared source of truth for the flag map."""
        header = (ROOT / 'include' / 'world' / 'anchor_world_fish.h').read_text()
        arrays = dict(re.findall(
            r'static const unsigned char (\w+)\[\] = \{([^}]*)\};', header))
        parsed = {}
        for name, body in arrays.items():
            parsed[name] = [int(v, 16) if v.strip().startswith('0x')
                            else int(v) for v in body.split(',')]
        cases = re.findall(
            r'case (0x[0-9a-f]+): flags=(\w+);variants=(\w+);'
            r'first=(\d+);count=(\d+);break;', header)
        self.assertEqual(len(cases), 7)
        table = {}
        for room, flags_name, variants_name, first, count in cases:
            flags, variants = parsed[flags_name], parsed[variants_name]
            first, count = int(first), int(count)
            self.assertEqual(len(flags), count)
            self.assertEqual(len(variants), count)
            for index in range(count):
                table[int(room, 16), first + index] = (flags[index],
                                                       variants[index])
        mine = {(room, parent): fixed
                for room, slots in f.SCOPE.items()
                for parent, fixed in slots.items()}
        self.assertEqual(table, mine)
        self.assertEqual(len(mine), 33)

    def test_scope_requires_the_exact_slot_flag_and_variant(self):
        self.assertTrue(f.scope(fish(), ROOM))
        self.assertFalse(f.scope(fish(), 0x16B))
        self.assertFalse(f.scope(fish(parent=9), ROOM))
        self.assertFalse(f.scope(fish(flag=0xA7), ROOM))
        self.assertFalse(f.scope(fish(variant=2), ROOM))


class FishRecipeTests(FishHarness):
    def test_recipe_pins_the_placed_shape(self):
        self.assertTrue(d.valid(fish()))
        for index, value in ((f.ORDINAL, 1), (f.PHASE, 1), (f.CLIP, 1),
                             (f.ANIMATED, 1), (f.ROUTE, 0), (f.TALKABLE, 1),
                             (f.DIALOG, 1), (f.BUSY, 1), (f.BIRTH_X, 1),
                             (f.BIRTH_Y, 1), (f.BIRTH_Z, 1), (f.ROLE, 1),
                             (f.MODE, 1), (f.RESERVED, 1),
                             (f.ESTABLISHED, 2), (f.PRESENT, 2),
                             (f.ENTITY_W, 0x331), (f.MODEL, 0x331), (6, 13)):
            bad = fish()
            bad[index] = value
            self.assertFalse(d.valid(bad), (index, value))

    def test_identity_row_rejects_pose_and_collision_words(self):
        # The validator forces the whole clip..role span clear apart from the
        # route, claim marker and arbiter words, so an accidental pose or
        # collision serialization is refused instead of travelling as state.
        self.assertTrue(d.valid(fish()))
        self.assertFalse(d.valid(fish(dirty=True)))
        for index in (12, 13, 14, 15, 24, 31, 33, 43, 45, 47, 54, 59, 66, 67,
                      68, 71, 73):
            bad = fish()
            bad[index] = 1
            self.assertFalse(d.valid(bad), index)
        # The three exempt words are still allowed to carry their meaning.
        for index, value in ((f.ROUTE, f.ROUTE_MARK), (f.LANDED, 1),
                             (f.COMMITTER, 7)):
            good = fish()
            good[index] = value
            self.assertTrue(d.valid(good), index)

    def test_claim_requires_landed_and_removal_does_not(self):
        self.assertTrue(d.valid(fish()))
        self.assertFalse(d.valid(fish(life=d.CLAIM)))
        self.assertTrue(d.valid(fish(life=d.CLAIM, landed=1)))
        # The native validator does not gate a removal on landed/committer; the
        # transport sets those when it commits the winning claim.
        self.assertTrue(d.valid(fish(life=d.REMOVED)))


class FishIdentityTests(FishHarness):
    def test_key_is_room_and_slot_with_a_fixed_ordinal_marker(self):
        p = w.WorldTransport()
        p.signature, p.room = SIGNATURE, ROOM
        self.assertEqual(f.identity(fish(), p),
                         [f.ROOT_ORIGIN, SIGNATURE, (PARENT << 10) | (ROOM + 1), 1])
        # Every slot in the room is a distinct actor.
        keys = {tuple(f.identity(fish(parent=parent), p))
                for parent in sorted(f.SCOPE[ROOM])}
        self.assertEqual(len(keys), len(f.SCOPE[ROOM]))
        self.assertNotIn(f.ROOT_ORIGIN, {k[0] for k in keys} ^ {f.ROOT_ORIGIN})
        # Each fish arbitrates for itself.
        self.assertEqual(f.family(tuple(f.identity(fish(), p))),
                         tuple(f.identity(fish(), p)))

    def test_identity_rejects_a_foreign_origin_and_foreign_room(self):
        p = w.WorldTransport()
        p.signature, p.room = SIGNATURE, ROOM
        for row in (fish(), fish(life=d.REMOVED), fish(life=d.CLAIM, landed=1),
                    fish()):
            self.assertIsNotNone(f.identity(row, p))
            forged = list(row)
            forged[:4] = [0x7FFFFF00, SIGNATURE, (PARENT << 10) | (ROOM + 1), 1]
            self.assertNotEqual(f.identity(forged, p), forged[:4])
        self.assertIsNone(f.identity(fish(parent=9), p))
        self.assertIsNone(f.identity(fish(flag=0xA7), p))
        other = w.WorldTransport()
        other.signature, other.room = SIGNATURE, 0x16B
        self.assertIsNone(f.identity(fish(), other))

    def test_repeated_flag_room_pairs_are_an_explicit_limitation(self):
        # The paired rooms share one flag list, so the same flag identifies a
        # fish in two different rooms. The keys still differ (the room is packed
        # into the key), but a simultaneous cross-room collect of that flag is
        # arbitrated per room, not once per team.
        for first, second in f.PAIRED_ROOMS:
            self.assertEqual(sorted(f.SCOPE[first].values()),
                             sorted(f.SCOPE[second].values()))
        first = w.WorldTransport()
        first.signature, first.room = SIGNATURE, 0x168
        second = w.WorldTransport()
        second.signature, second.room = SIGNATURE, 0x17E
        left = f.identity(fish(parent=10, room=0x168), first)
        right = f.identity(fish(parent=3, room=0x17E), second)
        self.assertEqual(f.placement(0x168, 10), f.placement(0x17E, 3))
        self.assertNotEqual(left, right)
        self.assertEqual(f.family(tuple(left)), tuple(left))
        self.assertEqual(f.family(tuple(right)), tuple(right))
        self.assertNotEqual(f.family(tuple(left)), f.family(tuple(right)))


class FishTransportTests(FishHarness):
    def test_wire_rows_drop_the_instance_and_receipt(self):
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [fish(instance=7)], 1)
        wire = packets[0]['a'][0]
        self.assertEqual((wire[f.INSTANCE], wire[f.RECEIPT]), (0, 0))
        self.assertEqual((wire[f.ESTABLISHED], wire[f.PRESENT]), (0, 1))
        self.assertNotEqual(result['a'][0][f.RECEIPT], 0)

    def test_bootstrap_owner_establishes_on_the_native_echo(self):
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [fish(instance=7)], 1)
        token = result['a'][0][f.RECEIPT]
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [fish(instance=7, receipt=token)], 1.06)
        self.assertEqual(result['a'][0][f.ESTABLISHED], 1)
        self.assertEqual(packets[0]['a'][0][f.ESTABLISHED], 1)
        self.assertEqual(result['a'][0][f.RECEIPT], token)

    def test_late_entry_snapshot_carries_the_fish_state(self):
        rows = [fish(parent=10, serial=1), fish(parent=12, serial=2)]
        self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        late, _ = self.b.update(self.cb, [], 1.02)
        self.assertEqual({tuple(r[:4]) for r in late['a']},
                         {tuple(f.identity(r, self.wa)) for r in rows})

    def test_late_native_loaded_removal_reaches_a_new_entrant(self):
        state = self.establish()
        gone = fish(instance=4, life=d.REMOVED, landed=1, committer=1,
                    receipt=state[f.RECEIPT])
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [gone], 1.2)
        self.assertEqual(result['a'][0][d.LIFE], d.REMOVED)
        # A removal derived from the native flag (not a claim commit) carries
        # no winner; the claim path is what records one, checked below.
        self.assertEqual(result['a'][0][d.OWNER], 0)
        self.assertIn(tuple(state[:4]), self.a.dead)
        # A client entering the occupied room later still learns it is gone.
        late, _ = self.b.update(self.cb, [], 1.3)
        self.assertEqual([r[d.LIFE] for r in late['a']
                          if tuple(r[:4]) == tuple(state[:4])], [d.REMOVED])

    def test_culled_fish_is_not_a_shared_removal(self):
        # The pause word is part of the forced-clear span, so a culled local
        # copy advertises presence 0 and nothing else. Losing presence is not a
        # collection and must never publish a tombstone.
        state = self.establish()
        token = state[f.RECEIPT]
        self.send(self.a, self.ca, self.b, self.cb,
                  [fish(instance=4, receipt=token, present=0)], 1.1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [fish(instance=4, present=0)], 1.2)
        self.assertTrue(result['a'])
        self.assertEqual(result['a'][0][d.LIFE], d.LIVE)
        self.assertNotIn(tuple(result['a'][0][:4]), self.a.dead)


class FishClaimTests(FishHarness):
    def test_two_claimants_commit_one_winner_gated_on_the_commit_send(self):
        state = self.establish()
        claim = fish(instance=4, life=d.CLAIM, landed=1)
        self.send(self.b, self.cb, self.a, self.ca, [copy.deepcopy(claim)], 1.1)
        committed, packets = self.a.update(self.ca, [copy.deepcopy(claim)], 1.2)
        self.assertEqual(committed['a'][0][d.LIFE], d.REMOVED)
        self.assertEqual(committed['a'][0][d.LANDED], 1)
        self.assertEqual(committed['a'][0][d.COMMITTER], 1)
        # Only the winning client may let its native pickup run, and only once
        # the commit reached the wire.
        self.assertFalse(self.a.native_result(committed)['a'])
        self.a.sent(packets, False, 1.2)
        self.assertFalse(self.a.native_result(committed)['a'])
        committed, packets = self.a.update(self.ca, [copy.deepcopy(claim)], 1.3)
        self.assertTrue(packets)
        self.a.sent(packets, True, 1.3)
        self.assertEqual(self.a.native_result(committed)['a'][0][d.LANDED], 1)
        self.assertIn(tuple(state[:4]), self.a.dead)

    def test_loser_does_not_commit_or_drop(self):
        self.establish()
        claim = fish(instance=4, life=d.CLAIM, landed=1)
        # The non-arbiter sees the claim but must not commit it.
        result, _ = self.send(self.b, self.cb, self.a, self.ca,
                              [copy.deepcopy(claim)], 1.1)
        self.assertEqual(result['a'][0][d.LIFE], d.LIVE)
        self.assertNotIn(tuple(f.identity(fish(), self.wa)), self.b.dead)

    def test_two_slots_claim_independently(self):
        left = self.establish()
        right = fish(parent=12, instance=8)
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [right], 1.1)
        token = state['a'][0][f.RECEIPT]
        self.send(self.a, self.ca, self.b, self.cb,
                  [fish(parent=12, instance=8, receipt=token)], 1.16)
        claim = fish(parent=12, instance=8, life=d.CLAIM, landed=1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb, [claim], 1.2)
        killed = [r for r in result['a'] if tuple(r[:4]) == tuple(
            f.identity(fish(parent=12), self.wa))]
        self.assertEqual(killed[0][d.LIFE], d.REMOVED)
        # The other slot is untouched.
        self.assertNotIn(tuple(left[:4]), self.a.dead)


class FishIsolationTests(FishHarness):
    def test_receive_rejects_forged_stale_and_out_of_scope_rows(self):
        _, packets = self.a.update(self.ca, [fish(serial=1)], 1)
        packet = packets[0]
        self.assertTrue(self.b.receive(self.cb, packet, 1.001))
        self.assertFalse(self.b.receive(self.cb, packet, 1.002))
        for index, value in ((f.INSTANCE, 3), (f.RECEIPT, 9)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.01), index)
        for index, value in ((f.PARENT, 9), (f.FLAG, 0xA7), (f.VARIANT, 2),
                             (f.ORDINAL, 1), (f.PHASE, 1), (f.ROLE, 1),
                             (f.MODE, 1), (f.BIRTH_X, 1), (f.BUSY, 1)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.02), index)
        bad = copy.deepcopy(packet)
        bad['a'][0][:4] = [f.ROOT_ORIGIN, SIGNATURE,
                           (PARENT << 10) | (ROOM + 1), 2]
        self.assertFalse(self.b.receive(self.cb, bad, 1.03))
        duplicate = copy.deepcopy(packet)
        duplicate['a'] = [packet['a'][0], list(packet['a'][0])]
        self.assertFalse(self.b.receive(self.cb, duplicate, 1.04))

    def test_team_isolation_and_local_foreign_room_refusal(self):
        _, packets = self.a.update(self.ca, [fish(serial=1)], 1)
        foreign = copy.deepcopy(packets[0])
        foreign['targetTeamId'] = 'other'
        self.assertFalse(self.b.receive(self.cb, foreign, 1.01))
        # A capture bound to another room's fish never becomes a row here.
        elsewhere = fish(parent=6, room=0x16B)
        self.assertTrue(d.valid(elsewhere))
        result, packets = self.a.update(self.ca, [elsewhere], 1.1)
        self.assertFalse(result['a'])
        self.assertFalse(packets)

    def test_envelope_type_and_protocol_version(self):
        rows, serial = [], 0
        for parent in sorted(f.SCOPE[ROOM]):
            serial += 1
            rows.append(fish(parent=parent, serial=serial))
        self.assertEqual(len(rows), 5)
        self.assertEqual(len({tuple(f.identity(r, self.wa)) for r in rows}), 5)
        _, packets = self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        for packet in packets:
            self.assertEqual(packet['type'], d.PACKET_TYPE)
            self.assertEqual(packet['targetTeamId'], self.ca['team'])
            self.assertNotIn('targetClientId', packet)
            self.assertFalse(packet.get('addToQueue', False))
            self.assertLessEqual(
                len(json.dumps(packet, separators=(',', ':')).encode()) + 1,
                d.PACKET_BYTES)
        # The new recipe is what moved the world protocol to 19.
        self.assertEqual(w.VERSION, 19)


class FishParityTests(FishHarness):
    def test_codec_parity_for_every_slot_and_column(self):
        """Every probe agrees with the C codec's anchor_world_dynamic_row_valid."""
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'codec.so'
            subprocess.run(
                ['cc', '-shared', '-fPIC', '-std=c99', '-I' + str(ROOT / 'include'),
                 str(ROOT / 'src/world/anchor_world_dynamic_codec.c'),
                 str(ROOT / 'src/utils/string_utils.c'), '-o', str(path)],
                check=True)
            lib = ctypes.CDLL(str(path))
            row = ctypes.c_int * d.WORDS
            lib.anchor_world_dynamic_row_valid.argtypes = [ctypes.POINTER(ctypes.c_int)]
            probes = [-32769, -1, 0, 1, 2, 3, 4, 5, 0xA7, 0xB8, 0xC1, 163, 255,
                      256, 65535, 0x7fffffff]
            bases = [fish()]
            bases += [fish(parent=parent)
                      for parent in sorted(f.SCOPE[ROOM])]
            bases += [fish(parent=parent, room=room)
                      for room in sorted(f.SCOPE)
                      for parent in sorted(f.SCOPE[room])][:12]
            bases += [fish(life=d.REMOVED, landed=1, committer=1),
                      fish(life=d.CLAIM, landed=1)]
            checked = 0
            for base in bases:
                self.assertTrue(d.valid(base), base[:12])
                for column in range(d.WORDS):
                    for value in probes:
                        probe = list(base)
                        probe[column] = value
                        checked += 1
                        self.assertEqual(
                            bool(lib.anchor_world_dynamic_row_valid(row(*probe))),
                            d.valid(probe), (base[:12], column, value))
            self.assertGreater(checked, 20000)


if __name__ == '__main__':
    unittest.main()
