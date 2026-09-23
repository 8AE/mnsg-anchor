"""Multi-client File34 boulder transport: scope, race keys, expiry and version.

The native bridge is not part of this module, so the tests drive its two words
directly: a capture row reports the local native instance and echoes the apply
token, and the bridge-facing result is inspected next to the wire row that was
actually sent. Every case runs two clients so ownership, arbitration and the
receipt handshake are exercised across a real exchange.

The scope table is the only place the family is pinned to a room, and it also
carries the immutable original placement coordinate. Its fixture asserts the
exact 20-slot whitelist and the per-room split, so a zero-based/one-based slip
cannot ship.
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
import anchor_world_boulder as b
import test_world_dynamic as helper

ROOM = 0x13D
PARENT = 10
OTHER_ROOM = 0x13F
OTHER_PARENT = 8
SIGNATURE = 42


def boulder(parent=PARENT, room=ROOM, ordinal=0, phase=0, age=0, life=None,
            x=0, y=0, z=0, vx=0, vy=0, vz=0, serial=1, instance=0, receipt=0,
            established=0, present=1, paused=0, model=None, birth=None,
            role=0, reserved=None, clip=0, animated=None):
    original = b.placement(room, parent)
    if birth is None:
        birth = original if original is not None else (0, 0, 0)
    if model is None:
        model = b.FALLING_MODEL if phase >= b.PHASE_MAX else b.ENTITY
    if animated is None:
        animated = 1 if phase >= b.PHASE_MAX else 0
    r = [0] * d.WORDS
    r[:12] = [0, 0, 0, serial, 0, d.LIVE if life is None else life, b.KIND,
              parent, b.ENTITY, model, clip, animated]
    r[b.X], r[b.Y], r[b.Z] = x, y, z
    r[b.VX], r[b.VY], r[b.VZ] = vx, vy, vz
    r[b.TIMER] = age
    r[b.ROUTE] = b.ROUTE_MARK
    r[b.PHASE] = phase
    r[b.ORDINAL] = ordinal
    r[b.BIRTH_X], r[b.BIRTH_Y], r[b.BIRTH_Z] = birth
    r[66] = paused
    r[b.ROLE] = role
    for index, value in (reserved or {}).items():
        r[index] = value
    r[b.INSTANCE], r[b.RECEIPT] = instance, receipt
    r[b.ESTABLISHED], r[b.PRESENT] = established, present
    return r


class BoulderHarness(unittest.TestCase):
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
        row = boulder(instance=4) if row is None else row
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [row], 1)
        token = state['a'][0][b.RECEIPT]
        echo = boulder(instance=4, receipt=token, phase=row[b.PHASE],
                       ordinal=row[b.ORDINAL], parent=row[b.PARENT])
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [echo], 1.06)
        self.assertEqual(state['a'][0][b.ESTABLISHED], 1)
        return state['a'][0]


class BoulderScopeTests(BoulderHarness):
    def test_scope_whitelist_is_the_exact_twenty_slot_roster(self):
        self.assertEqual(set(b.SCOPE), {0x13D, 0x13F})
        self.assertEqual(sorted(b.SCOPE[0x13D]), list(range(10, 18)))
        self.assertEqual(sorted(b.SCOPE[0x13F]),
                         [8, 9, 10, 11, 12, 18, 19, 20, 21, 22, 23, 24])
        self.assertEqual(sum(len(v) for v in b.SCOPE.values()), 20)
        # The one-based slots are the production roster indices plus one.
        self.assertEqual(b.placement(0x13D, 10), (9, -65, 161))
        self.assertEqual(b.placement(0x13F, 8), (84, -213, 465))
        self.assertIsNone(b.placement(0x13D, 9))
        self.assertIsNone(b.placement(0x13F, 13))

    def test_scope_binds_room_slot_entity_and_the_immutable_birth(self):
        self.assertTrue(b.scope(boulder(), ROOM))
        self.assertTrue(b.scope(boulder(parent=OTHER_PARENT, room=OTHER_ROOM),
                                OTHER_ROOM))
        # A slot that belongs to the other room never binds here.
        self.assertFalse(b.scope(boulder(parent=OTHER_PARENT, room=OTHER_ROOM),
                                 ROOM))
        self.assertFalse(b.scope(boulder(parent=9), ROOM))
        # The birth is the original placement, not the offset pose.
        self.assertFalse(b.scope(boulder(birth=(0, 0, 0)), ROOM))
        self.assertFalse(b.scope(boulder(birth=(9, -65, 160)), ROOM))
        # The race offset moves the pose only; the birth stays the placement.
        self.assertTrue(b.scope(boulder(x=50, z=-50, ordinal=2), ROOM))
        self.assertEqual(b.identity(boulder(x=50, z=-50, ordinal=2), self.wa),
                         b.identity(boulder(x=-30, z=20, ordinal=2), self.wa))
        self.assertFalse(b.scope(boulder(ordinal=b.ORDINAL_MAX + 1), ROOM))

    def test_room_split_matches_the_production_counts(self):
        self.assertEqual(len(b.SCOPE[0x13D]), 8)
        self.assertEqual(len(b.SCOPE[0x13F]), 12)

    def test_placement_table_matches_the_native_header(self):
        """The C header is the shared source of truth for the 20 placements."""
        header = (ROOT / 'include' / 'world' / 'anchor_world_boulder.h').read_text()
        blocks = re.findall(r'static const short ([ab])\[(\d+)\]\[3\]=\{(.*?)\};',
                            header, re.S)
        self.assertEqual({name for name, _, _ in blocks}, {'a', 'b'})
        table = {}
        for name, count, body in blocks:
            coords = [tuple(int(v) for v in triple)
                      for triple in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+)\}',
                                               body)]
            self.assertEqual(len(coords), int(count))
            if name == 'a':
                for offset, coord in enumerate(coords):
                    table[0x13D, 10 + offset] = coord
            else:
                for offset, coord in enumerate(coords):
                    parent = 8 + offset if offset < 5 else 13 + offset
                    table[0x13F, parent] = coord
        mine = {(room, parent): coord
                for room, slots in b.SCOPE.items()
                for parent, coord in slots.items()}
        self.assertEqual(table, mine)
        self.assertEqual(len(mine), 20)


class BoulderRecipeTests(BoulderHarness):
    def test_recipe_pins_phase_age_and_the_typed_tail(self):
        for phase in range(b.PHASE_MAX + 1):
            self.assertTrue(d.valid(boulder(phase=phase)), phase)
        for index, value in ((b.PHASE, b.PHASE_MAX + 1), (b.PHASE, -1),
                             (b.TIMER, b.AGE_MAX + 1), (b.TIMER, -1),
                             (b.ROLE, 1), (b.ORDINAL, -1), (6, 12), (6, 14),
                             (b.PARENT, 0), (b.PARENT, 257),
                             (b.ESTABLISHED, 2), (b.PRESENT, 2),
                             (b.ENTITY_W, 0x330)):
            bad = boulder()
            bad[index] = value
            self.assertFalse(d.valid(bad), (index, value))
        for reserved in (b.RESERVED0, b.RESERVED1, b.RESERVED2, b.RESERVED3):
            self.assertFalse(d.valid(boulder(reserved={reserved: 1})), reserved)
        # The age is a signed halfword natively but the wire window is 0..150.
        self.assertTrue(d.valid(boulder(age=0)))
        self.assertTrue(d.valid(boulder(age=b.AGE_MAX)))

    def test_model_swaps_only_at_the_final_phase(self):
        # Native phase 3 swaps the model as it hands over to phase 4.
        for phase in range(b.PHASE_MAX):
            self.assertTrue(d.valid(boulder(phase=phase, model=b.ENTITY)), phase)
            self.assertFalse(
                d.valid(boulder(phase=phase, model=b.FALLING_MODEL)), phase)
        self.assertTrue(d.valid(boulder(phase=b.PHASE_MAX,
                                        model=b.FALLING_MODEL)))
        self.assertFalse(d.valid(boulder(phase=b.PHASE_MAX, model=b.ENTITY)))

    def test_clip_is_zero_and_only_the_final_phase_must_be_animated(self):
        # The phase-3 handover installs clip 0 with the animation rate and loop
        # flag, so the final phase is animated and every phase keeps clip 0.
        for phase in range(b.PHASE_MAX):
            self.assertTrue(d.valid(boulder(phase=phase, animated=0)), phase)
            self.assertTrue(d.valid(boulder(phase=phase, animated=1)), phase)
        self.assertFalse(d.valid(boulder(phase=b.PHASE_MAX, animated=0)))
        self.assertTrue(d.valid(boulder(phase=b.PHASE_MAX, animated=1)))
        for phase in range(b.PHASE_MAX + 1):
            self.assertFalse(d.valid(boulder(phase=phase, clip=1)), phase)

    def test_route_path_block_and_interaction_words_stay_clear(self):
        self.assertTrue(d.valid(boulder()))
        for index in (b.ROUTE, b.TALKABLE, b.DIALOG, b.BUSY,
                      *range(b.PATH_FIRST, b.PATH_LAST + 1)):
            bad = boulder()
            bad[index] = 1
            self.assertFalse(d.valid(bad), index)

    def test_codec_parity_for_every_phase_and_slot(self):
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
            probes = [-32769, -1, 0, 1, 2, 3, 4, 5, 163, 255, 256, 65535, 65536,
                      0x7fffffff]
            bases = [boulder(phase=phase) for phase in range(b.PHASE_MAX + 1)]
            bases += [boulder(ordinal=ordinal) for ordinal in (0, 1, 65535)]
            bases += [boulder(parent=parent) for parent in sorted(b.SCOPE[ROOM])]
            bases += [boulder(parent=parent, room=OTHER_ROOM)
                      for parent in sorted(b.SCOPE[OTHER_ROOM])]
            bases += [boulder(life=d.REMOVED), boulder(age=b.AGE_MAX)]
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
            self.assertGreater(checked, 30000)

    def test_expiry_is_not_a_kill(self):
        # There is no hit claim and no landed/committer handshake for a boulder,
        # so a claim row is never a valid checkpoint.
        self.assertTrue(d.valid(boulder()))
        self.assertTrue(d.valid(boulder(life=d.REMOVED)))
        self.assertFalse(d.valid(boulder(life=d.CLAIM)))


class BoulderIdentityTests(BoulderHarness):
    def placed(self):
        p = w.WorldTransport()
        p.signature, p.room = SIGNATURE, ROOM
        return p

    def test_key_is_the_placed_slot_plus_the_race_ordinal(self):
        p = self.placed()
        self.assertEqual(b.identity(boulder(), p),
                         [b.ROOT_ORIGIN, SIGNATURE, (PARENT << 10) | (ROOM + 1), 1])
        original = b.identity(boulder(ordinal=0), p)
        duplicates = {tuple(b.identity(boulder(ordinal=i), p))
                      for i in range(1, 6)}
        self.assertEqual(len(duplicates), 5)
        self.assertNotIn(tuple(original), duplicates)
        # The key word is the ordinal biased by one, so the original boulder
        # still satisfies the shared "every key word is non-zero" invariant.
        self.assertEqual(b.identity(boulder(ordinal=3), p)[3], 4)
        # A duplicate carries the same birth but its own pose.
        first = b.identity(boulder(ordinal=1, x=30, z=-20), p)
        self.assertEqual(first, b.identity(boulder(ordinal=1, x=-40, z=45), p))
        # Another slot or another room is a different actor.
        self.assertNotEqual(b.identity(boulder(parent=11), p), original)
        other = w.WorldTransport()
        other.signature, other.room = SIGNATURE, OTHER_ROOM
        self.assertNotEqual(
            b.identity(boulder(parent=OTHER_PARENT, room=OTHER_ROOM), other),
            original)
        # Each boulder arbitrates for itself; there is no shared emitter.
        self.assertEqual(b.family(tuple(original)), tuple(original))

    def test_identity_rejects_a_foreign_origin_and_a_bad_slot(self):
        p = self.placed()
        for row in (boulder(), boulder(ordinal=3), boulder(life=d.REMOVED),
                    boulder(phase=b.PHASE_MAX)):
            self.assertIsNotNone(b.identity(row, p))
            forged = list(row)
            forged[:4] = [0x7FFFFF00, SIGNATURE, (PARENT << 10) | (ROOM + 1), 0]
            self.assertNotEqual(b.identity(forged, p), forged[:4])
        self.assertIsNone(b.identity(boulder(parent=9), p))
        self.assertIsNone(b.identity(boulder(birth=(0, 0, 0)), p))


class BoulderTransportTests(BoulderHarness):
    def test_wire_rows_drop_the_instance_and_receipt(self):
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [boulder(instance=7)], 1)
        wire = packets[0]['a'][0]
        self.assertEqual((wire[b.INSTANCE], wire[b.RECEIPT]), (0, 0))
        self.assertEqual((wire[b.ESTABLISHED], wire[b.PRESENT]), (0, 1))
        self.assertNotEqual(result['a'][0][b.RECEIPT], 0)

    def test_bootstrap_owner_establishes_on_the_native_echo(self):
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [boulder(instance=7)], 1)
        token = result['a'][0][b.RECEIPT]
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [boulder(instance=7, receipt=token)], 1.06)
        self.assertEqual(result['a'][0][b.ESTABLISHED], 1)
        self.assertEqual(packets[0]['a'][0][b.ESTABLISHED], 1)
        self.assertEqual(result['a'][0][b.RECEIPT], token)

    def test_bootstrap_waits_for_a_complete_peer_snapshot(self):
        self.a.peers.clear()
        result, packets = self.a.update(self.ca, [boulder(serial=1)], 1)
        self.assertEqual(result['a'][0][d.OWNER], 0)
        self.send(self.b, self.cb, self.a, self.ca, [], 1.05)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [boulder(serial=1)], 1.1)
        self.assertEqual(result['a'][0][d.OWNER], 1)

    def test_late_entry_snapshot_carries_the_boulder_state(self):
        rows = [boulder(serial=1, phase=2, age=40, x=30, z=-20),
                boulder(serial=2, ordinal=1, phase=1, age=12)]
        self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        late, _ = self.b.update(self.cb, [], 1.02)
        self.assertEqual({tuple(r[:4]) for r in late['a']},
                         {tuple(b.identity(r, self.wa)) for r in rows})
        by_ordinal = {r[b.ORDINAL]: r for r in late['a']}
        self.assertEqual(by_ordinal[0][b.PHASE], 2)
        self.assertEqual(by_ordinal[0][b.TIMER], 40)
        self.assertEqual(by_ordinal[1][b.TIMER], 12)

    def test_race_duplicates_stay_separate_actors(self):
        rows = [boulder(serial=i + 1, ordinal=i, x=i * 10, z=-i * 5)
                for i in range(4)]
        state, _ = self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        self.assertEqual(len(state['a']), 4)
        self.assertEqual(len({tuple(r[:4]) for r in state['a']}), 4)
        # A second client reporting the same race set coalesces onto them.
        other = [boulder(serial=90 + i, ordinal=i, x=i * 10, z=-i * 5)
                 for i in range(4)]
        result, _ = self.send(self.b, self.cb, self.a, self.ca, other, 1.1)
        self.assertEqual(len(result['a']), 4)
        self.assertEqual({tuple(r[:4]) for r in result['a']},
                         {tuple(r[:4]) for r in state['a']})

    def test_expiry_tombstone_propagates_from_the_acting_owner(self):
        state = self.establish()
        gone = boulder(instance=4, life=d.REMOVED, phase=b.PHASE_MAX,
                       receipt=state[b.RECEIPT])
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [gone], 1.2)
        self.assertEqual(result['a'][0][d.LIFE], d.REMOVED)
        self.assertIn(tuple(state[:4]), self.a.dead)
        # A late entrant receives the expiry as an occupied-room checkpoint.
        late, _ = self.b.update(self.cb, [], 1.3)
        self.assertEqual([r[d.LIFE] for r in late['a']
                          if tuple(r[:4]) == tuple(state[:4])], [d.REMOVED])

    def test_proximity_unload_is_not_a_shared_removal(self):
        state = self.establish()
        token = state[b.RECEIPT]
        self.send(self.a, self.ca, self.b, self.cb,
                  [boulder(instance=4, receipt=token, present=0, paused=1)], 1.1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [boulder(instance=4, present=0, paused=1)], 1.2)
        self.assertEqual(result['a'][0][d.LIFE], d.LIVE)
        self.assertNotIn(tuple(result['a'][0][:4]), self.a.dead)


class BoulderIsolationTests(BoulderHarness):
    def test_receive_rejects_forged_stale_and_out_of_scope_rows(self):
        _, packets = self.a.update(self.ca, [boulder(serial=1)], 1)
        packet = packets[0]
        self.assertTrue(self.b.receive(self.cb, packet, 1.001))
        self.assertFalse(self.b.receive(self.cb, packet, 1.002))
        for index, value in ((b.INSTANCE, 3), (b.RECEIPT, 9)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.01), index)
        for index, value in ((b.PARENT, 9), (b.PHASE, 5), (b.TIMER, 151),
                             (b.ROLE, 1), (b.RESERVED0, 1), (b.MODEL, 0x191)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.02), index)
        # A forged origin that is not the canonical key is refused.
        bad = copy.deepcopy(packet)
        bad['a'][0][:4] = [b.ROOT_ORIGIN, SIGNATURE, (PARENT << 10) | (ROOM + 1), 5]
        self.assertFalse(self.b.receive(self.cb, bad, 1.03))
        # A different room's slot cannot be claimed here.
        bad = copy.deepcopy(packet)
        bad['a'][0][b.PARENT] = 9
        self.assertFalse(self.b.receive(self.cb, bad, 1.04))
        duplicate = copy.deepcopy(packet)
        duplicate['a'] = [packet['a'][0], list(packet['a'][0])]
        self.assertFalse(self.b.receive(self.cb, duplicate, 1.05))

    def test_team_isolation_and_local_out_of_scope_refusal(self):
        _, packets = self.a.update(self.ca, [boulder(serial=1)], 1)
        foreign = copy.deepcopy(packets[0])
        foreign['targetTeamId'] = 'other'
        self.assertFalse(self.b.receive(self.cb, foreign, 1.01))
        # A capture bound to the other room's placement never becomes a row.
        elsewhere = boulder(parent=OTHER_PARENT, room=OTHER_ROOM)
        self.assertTrue(d.valid(elsewhere))
        result, packets = self.a.update(self.ca, [elsewhere], 1.1)
        self.assertFalse(result['a'])
        self.assertFalse(packets)

    def test_envelope_type_and_protocol_version(self):
        rows, serial = [], 0
        for parent in sorted(b.SCOPE[ROOM]):
            for ordinal in range(3):
                serial += 1
                rows.append(boulder(parent=parent, ordinal=ordinal,
                                    serial=serial))
        self.assertEqual(len(rows), 24)
        self.assertEqual(len({tuple(b.identity(r, self.wa)) for r in rows}), 24)
        _, packets = self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        self.assertEqual(len(packets), 2)
        for packet in packets:
            self.assertEqual(packet['type'], d.PACKET_TYPE)
            self.assertEqual(packet['targetTeamId'], self.ca['team'])
            self.assertNotIn('targetClientId', packet)
            self.assertFalse(packet.get('addToQueue', False))
            self.assertLessEqual(len(packet['a']), d.ROWS_PER_PACKET)
            self.assertLessEqual(
                len(json.dumps(packet, separators=(',', ':')).encode()) + 1,
                d.PACKET_BYTES)
        # The boulder recipe shipped at protocol 18; the newest recipe owns the
        # exact current-version assertion in its own suite.
        self.assertGreaterEqual(w.VERSION, 18)


if __name__ == '__main__':
    unittest.main()
