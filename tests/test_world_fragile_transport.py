"""Multi-client File30 fragile transport: scope, identity, kills and drops.

The native bridge is not part of this module, so the tests drive its two words
directly: a capture row reports the local native instance and echoes the apply
token, and the bridge-facing result is inspected next to the wire row that was
actually sent. Every case runs two clients so ownership, arbitration and the
receipt handshake are exercised across a real exchange.

The scope table is the only place the family is pinned to a room. Its fixture
parses the C worker's anchor_world_fragile_scope bitmask table and compares all
117 slots, so a one-based/zero-based slip cannot ship on either side.
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
import anchor_world_fragile as f
import test_world_dynamic as helper

ROOM = 0x91
PARENT = 28          # 0x331, the family that installs a 0x331 linked child
STATIC_PARENT = 29   # a 0x330 root with no child
SIGNATURE = 42


def root(parent=PARENT, entity=None, role=f.ROOT, variant=0, clip=None,
         life=None, landed=0, committer=0, serial=1, instance=0, receipt=0,
         established=0, present=1, paused=0, room=ROOM, model=None):
    entity = f.entity(room, parent) if entity is None else entity
    if clip is None:
        if entity == 0x330:
            clip = 1 if variant == 2 else 0
        else:
            fixed = f.CLIPS.get((entity, role))
            clip = fixed[0] if fixed else 0
    r = [0] * d.WORDS
    r[:12] = [0, 0, 0, serial, 0, d.LIVE if life is None else life, f.KIND,
              parent, entity,
              (f.SAVE_MODEL if entity == f.SAVE_ENTITY else entity)
              if model is None else model,
              clip,
              1 if (entity, role) == (f.VARIANT_ENTITY, f.CHILD) else 0]
    r[24:27] = [100] * 3
    r[32] = f.ROUTE_MARK
    r[42] = f.PHASE
    r[44] = landed
    r[64] = role
    r[66] = paused
    r[72] = committer
    r[f.ROLE] = role
    r[f.VARIANT] = variant
    r[f.INSTANCE], r[f.RECEIPT] = instance, receipt
    r[f.ESTABLISHED], r[f.PRESENT] = established, present
    return r


def drop(kind=2, role=f.ROOT, parent=PARENT, serial=7, entity=None, room=ROOM):
    """A fragile drop on the shared item helper, which keeps the union empty."""
    r = helper.actor(serial=serial, kind=kind)
    r[7] = parent
    r[8] = f.entity(room, parent) if entity is None else entity
    r[45] = role + f.ROOT_DROP
    r[64] = role
    r[74:] = [0] * 9
    return r


class FragileHarness(unittest.TestCase):
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


class FragileScopeTests(FragileHarness):
    def test_scope_table_is_one_based_and_matches_the_native_bitmask(self):
        header = (ROOT / 'include' / 'world' / 'anchor_world_fragile.h').read_text()
        rows = re.findall(
            r'{(0x[0-9a-fA-F]+),(0x[0-9a-fA-F]+),(0x[0-9a-fA-F]+)ull}', header)
        self.assertEqual(len(rows), 48)
        table = {}
        for room, entity, mask in rows:
            room, entity, mask = int(room, 16), int(entity, 16), int(mask, 16)
            for bit in range(64):
                if mask >> bit & 1:
                    table.setdefault(room, {})[bit + 1] = entity
        self.assertEqual(table, f.SCOPE)
        self.assertEqual(sum(len(v) for v in f.SCOPE.values()), 117)
        counts = {}
        for entity in f.ROOT_ENTITIES:
            counts[entity] = sum(list(v.values()).count(entity)
                                 for v in f.SCOPE.values())
        self.assertEqual(counts, {0x196: 44, 0x330: 58, 0x331: 1,
                                  0x332: 5, 0x339: 2, 0x3EC: 7})

    def test_slot_is_the_placed_index_plus_one(self):
        # The C mask 0x3 for room 0x84 allows placed indices 0 and 1, which are
        # parent slots 1 and 2; slot 0 is never a placement.
        self.assertEqual(f.entity(0x84, 1), 0x330)
        self.assertEqual(f.entity(0x84, 2), 0x330)
        self.assertIsNone(f.entity(0x84, 0))
        self.assertEqual(f.entity(0x5, 10), 0x196)
        self.assertIsNone(f.entity(0x5, 9))
        self.assertEqual(f.entity(0x91, 28), 0x331)
        self.assertEqual(f.entity(0x85, 12), 0x332)
        self.assertEqual(f.entity(0x14E, 20), 0x339)
        self.assertEqual(f.entity(0x15C, 15), 0x339)
        self.assertIsNone(f.entity(0x15C, 14))

    def test_scope_requires_the_exact_room_and_slot_entity(self):
        self.assertTrue(f.scope(root(), ROOM))
        self.assertFalse(f.scope(root(), 0x90))
        # Slot 40 is not a placement in this room, so no entity can bind it.
        self.assertFalse(f.scope(root(parent=40), ROOM))
        # A 0x330 slot never installs a child.
        self.assertFalse(f.scope(root(parent=STATIC_PARENT, role=f.CHILD), ROOM))
        self.assertTrue(f.scope(root(parent=STATIC_PARENT), ROOM))


class FragileRecipeTests(FragileHarness):
    def test_recipe_pins_role_variant_and_appearance(self):
        self.assertTrue(d.valid(root()))
        self.assertTrue(d.valid(root(role=f.CHILD)))
        self.assertTrue(d.valid(root(parent=12, room=0x85, role=f.CHILD)))
        self.assertTrue(d.valid(root(parent=20, room=0x14E, clip=4)))
        self.assertTrue(d.valid(root(parent=12, room=0x85, variant=4)))
        for index, value in ((f.ROLE, 2), (64, 1), (32, 0), (42, 22),
                             (f.RESERVED0, 1), (f.RESERVED1, 1),
                             (f.RESERVED2, 1), (f.ESTABLISHED, 2),
                             (f.PRESENT, 2), (40, 1), (41, 1), (65, 1)):
            bad = root()
            bad[index] = value
            self.assertFalse(d.valid(bad), (index, value))
        # Only 0x332 carries a definition-byte variant, capped at the native
        # D0 range; a 0x339 presentation root is always variant zero.
        self.assertFalse(d.valid(root(parent=12, room=0x85, variant=5)))
        self.assertFalse(d.valid(root(parent=20, room=0x14E, clip=4, variant=1)))
        self.assertFalse(d.valid(root(parent=20, room=0x14E, clip=4, model=0x339)))
        # The linked 0x332 child is the only animated role in the family.
        self.assertFalse(d.valid(root(parent=12, room=0x85, role=f.CHILD, clip=0)))
        self.assertFalse(d.valid(root(role=f.CHILD, clip=1)))

    def test_zero_thirty_thirty_pins_the_definition_byte_and_its_clip(self):
        # 0x330's native tint continuation installs clip 1 only for variant 2,
        # and the definition byte never exceeds 2. The other static families
        # ignore D0 entirely and always install clip 0.
        self.assertTrue(d.valid(root(parent=STATIC_PARENT, variant=0)))
        self.assertTrue(d.valid(root(parent=STATIC_PARENT, variant=1)))
        self.assertTrue(d.valid(root(parent=STATIC_PARENT, variant=2, clip=1)))
        for variant, clip in ((0, 1), (1, 1), (2, 0)):
            self.assertFalse(
                d.valid(root(parent=STATIC_PARENT, variant=variant, clip=clip)),
                (variant, clip))
        self.assertFalse(d.valid(root(parent=STATIC_PARENT, variant=3, clip=0)))
        self.assertTrue(d.valid(root(parent=10, room=0x5, clip=0)))
        self.assertFalse(d.valid(root(parent=10, room=0x5, clip=1)))
        self.assertTrue(d.valid(root(parent=6, room=0x81, clip=0)))
        self.assertFalse(d.valid(root(parent=6, room=0x81, clip=1)))

    def test_variant_word_keeps_the_native_definition_byte_range(self):
        # The native adapter reads D0 as a byte, so the recipe pins 0..255 even
        # for the families that ignore the value.
        for variant in (-1, 256, 32767):
            self.assertFalse(d.valid(root(parent=STATIC_PARENT, variant=variant,
                                          clip=0)), variant)
            self.assertFalse(d.valid(root(variant=variant, clip=1)), variant)

    def test_codec_parity_for_root_child_and_drop_rows(self):
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
            probes = [-32769, -1, 0, 1, 2, 3, 4, 5, 23, 163, 255, 256, 32768,
                      0x7fffffff]
            bases = [
                root(), root(role=f.CHILD), root(parent=12, room=0x85, variant=4),
                root(parent=12, room=0x85, role=f.CHILD, variant=4),
                root(parent=STATIC_PARENT, variant=2), root(parent=20, room=0x14E),
                root(parent=10, room=0x5), root(parent=6, room=0x81),
                root(parent=19, room=0x91), root(parent=34, room=0x87, variant=1),
                drop(), drop(kind=3, role=f.CHILD), drop(kind=4),
            ]
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
            self.assertGreater(checked, 14000)

    def test_claim_needs_landed_and_removal_needs_a_committer(self):
        self.assertFalse(d.valid(root(life=d.CLAIM)))
        self.assertTrue(d.valid(root(life=d.CLAIM, landed=1)))
        self.assertFalse(d.valid(root(life=d.REMOVED, landed=1)))
        self.assertTrue(d.valid(root(life=d.REMOVED, landed=1, committer=1)))
        # A route exit relayed with the flag clear stays a non-lethal removal.
        self.assertTrue(d.valid(root(life=d.REMOVED)))


class FragileIdentityTests(FragileHarness):
    def placed(self):
        p = w.WorldTransport()
        p.signature, p.room = SIGNATURE, ROOM
        return p

    def test_root_child_and_drop_keys_are_distinct_and_share_one_family(self):
        p = self.placed()
        r, c = root(), root(role=f.CHILD)
        rk = f.root_identity(r, p)
        ck = f.child_identity(c, p)
        self.assertEqual(rk, [f.ROOT_ORIGIN, SIGNATURE, ROOM + 1, PARENT])
        self.assertEqual(ck, [f.CHILD_ORIGIN, SIGNATURE, ROOM + 1, PARENT])
        self.assertNotEqual(rk[:3], ck[:3])
        self.assertEqual(f.family(tuple(rk)), tuple(rk))
        self.assertEqual(f.family(tuple(ck)), tuple(rk))
        keys = {tuple(rk), tuple(ck)}
        for kind in (2, 3, 4):
            for role in (f.ROOT, f.CHILD):
                k = tuple(f.loot_identity(drop(kind=kind, role=role), p))
                self.assertNotIn(k, keys)
                keys.add(k)
                self.assertEqual(f.family(k), tuple(rk))
        self.assertEqual(len(keys), 8)

    def test_drop_keeps_the_item_entity_and_is_typed_by_the_emitting_role(self):
        p = self.placed()
        for kind in (2, 3, 4):
            row = drop(kind=kind)
            self.assertTrue(d.valid(row), kind)
            self.assertTrue(f.loot(row, ROOM), kind)
            self.assertEqual(row[45], f.ROOT_DROP)
        # The native capture keeps the spawned item's own entity, so the placed
        # slot and the emitting role are what bind the drop to its emitter.
        row = drop(entity=0x1A4)
        self.assertTrue(f.loot(row, ROOM))
        self.assertEqual(
            f.loot_identity(row, p),
            [f.LOOT_ORIGIN, SIGNATURE,
             (PARENT << 13) | (f.ROOT << 10) | (ROOM + 1), 1])
        # A child drop only exists where the family installs a child.
        self.assertTrue(f.loot(drop(role=f.CHILD), ROOM))
        self.assertFalse(f.loot(drop(role=f.CHILD, parent=STATIC_PARENT), ROOM))
        self.assertFalse(f.loot(drop(parent=STATIC_PARENT), 0x90))
        # The ordinal repeats the emitting role and the union stays empty.
        bad = drop()
        bad[64] = f.CHILD
        self.assertFalse(f.loot_shape(bad))
        bad = drop()
        bad[74] = 1
        self.assertFalse(d.valid(bad))

    def test_malformed_live_drop_is_never_typed_as_fragile_loot(self):
        # The marker alone must not type a row: a live drop still has to satisfy
        # the shared item appearance and phase windows the codec enforces, so a
        # rejected snapshot cannot poison a merged response.
        for index, value in ((9, 7), (10, 0), (11, 1), (42, 5)):
            bad = drop()
            bad[index] = value
            self.assertFalse(f.loot_shape(bad), (index, value))
            self.assertFalse(f.loot(bad, ROOM), (index, value))
        for kind, index, value in ((3, 10, 4), (4, 9, 1), (4, 42, 2)):
            bad = drop(kind=kind)
            bad[index] = value
            self.assertFalse(f.loot_shape(bad), (kind, index, value))
        # A tombstone keeps the marker check and skips the live windows.
        gone = drop()
        gone[d.LIFE] = d.REMOVED
        gone[9] = gone[10] = gone[11] = 0
        self.assertTrue(f.loot_shape(gone))


class FragileTransportTests(FragileHarness):
    def test_wire_rows_drop_the_instance_and_receipt(self):
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=7)], 1)
        wire = packets[0]['a'][0]
        self.assertEqual((wire[f.INSTANCE], wire[f.RECEIPT]), (0, 0))
        self.assertEqual((wire[f.ESTABLISHED], wire[f.PRESENT]), (0, 1))
        self.assertNotEqual(result['a'][0][f.RECEIPT], 0)

    def test_bootstrap_owner_establishes_on_the_native_echo(self):
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [root(instance=7)], 1)
        token = result['a'][0][f.RECEIPT]
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=7, receipt=token)], 1.06)
        self.assertEqual(result['a'][0][f.ESTABLISHED], 1)
        self.assertEqual(packets[0]['a'][0][f.ESTABLISHED], 1)
        self.assertEqual(result['a'][0][f.RECEIPT], token)

    def test_bootstrap_waits_for_a_complete_peer_snapshot(self):
        self.a.peers.clear()
        result, packets = self.a.update(self.ca, [root(serial=1)], 1)
        self.assertEqual(result['a'][0][d.OWNER], 0)
        self.send(self.b, self.cb, self.a, self.ca, [], 1.05)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [root(serial=1)], 1.1)
        self.assertEqual(result['a'][0][d.OWNER], 1)

    def test_late_entry_snapshot_carries_root_child_and_drop_state(self):
        # This proves the transport hands a late entrant the full typed state.
        # Native reconstruction of an absent fragile root/child is deliberately
        # out of scope on the C side, so the test name must not claim it.
        rows = [root(serial=1), root(serial=2, role=f.CHILD), drop(serial=3)]
        self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        late, _ = self.b.update(self.cb, [], 1.02)
        self.assertEqual({tuple(r[:4]) for r in late['a']},
                         {tuple(f.root_identity(root(), self.wa)),
                          tuple(f.child_identity(root(role=f.CHILD), self.wa)),
                          tuple(f.loot_identity(drop(), self.wa))})

    def test_drop_row_coalesces_across_peers(self):
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [drop()], 1)
        self.assertEqual(state['a'][0][:4], f.loot_identity(drop(), self.wa))
        other = copy.deepcopy(drop())
        other[3] = 99
        result, _ = self.send(self.b, self.cb, self.a, self.ca, [other], 1.1)
        keys = [tuple(r[:4]) for r in result['a'] if r[6] == 2]
        self.assertEqual(keys.count(tuple(f.loot_identity(drop(), self.wa))), 1)

    def test_proximity_unload_is_not_a_shared_removal(self):
        state, _ = self.send(self.a, self.ca, self.b, self.cb,
                             [root(instance=4)], 1)
        token = state['a'][0][f.RECEIPT]
        self.send(self.a, self.ca, self.b, self.cb,
                  [root(instance=4, receipt=token, present=0, paused=1)], 1.1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [root(instance=4, present=0, paused=1)], 1.2)
        # Proximity unload is not a kill: the row stays live, never a tombstone.
        self.assertEqual(result['a'][0][d.LIFE], d.LIVE)
        self.assertNotIn(tuple(result['a'][0][:4]), self.a.dead)


class FragileClaimTests(FragileHarness):
    def establish(self, row=None):
        row = root(instance=4) if row is None else row
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [row], 1)
        token = state['a'][0][f.RECEIPT]
        echo = root(instance=4, receipt=token)
        state, _ = self.send(self.a, self.ca, self.b, self.cb, [echo], 1.06)
        self.assertEqual(state['a'][0][f.ESTABLISHED], 1)
        return state['a'][0]

    def test_two_claimants_commit_one_kill_gated_on_the_commit_send(self):
        state = self.establish()
        claim = root(instance=4, life=d.CLAIM, landed=1)
        self.send(self.b, self.cb, self.a, self.ca, [copy.deepcopy(claim)], 1.1)
        committed, packets = self.a.update(self.ca, [copy.deepcopy(claim)], 1.2)
        self.assertEqual(committed['a'][0][d.LIFE], d.REMOVED)
        self.assertEqual(committed['a'][0][d.LANDED], 1)
        self.assertEqual(committed['a'][0][d.COMMITTER], 1)
        # The native retirement waits for the commit to reach the wire.
        self.assertFalse(self.a.native_result(committed)['a'])
        self.a.sent(packets, False, 1.2)
        self.assertFalse(self.a.native_result(committed)['a'])
        committed, packets = self.a.update(self.ca, [copy.deepcopy(claim)], 1.3)
        self.assertTrue(packets)
        self.a.sent(packets, True, 1.3)
        self.assertEqual(self.a.native_result(committed)['a'][0][d.LANDED], 1)
        self.assertIn(tuple(state[:4]), self.a.dead)

    def test_child_and_drop_resolve_to_the_root_family_arbiter(self):
        state = self.establish()
        family = f.family(tuple(state[:4]))
        child = root(serial=2, role=f.CHILD, instance=7)
        child[d.COMMITTER] = 2
        self.send(self.b, self.cb, self.a, self.ca, [child], 2)
        child_key = tuple(f.child_identity(child, self.wa))
        drop_key = tuple(f.loot_identity(drop(role=f.CHILD), self.wa))
        # A child and a drop carry no arbiter of their own: both resolve to the
        # placed root, which is the only key the kill lease is tracked under.
        self.assertEqual(f.family(child_key), family)
        self.assertEqual(f.family(drop_key), family)
        self.assertNotIn(child_key, self.a.arbiters)
        self.assertNotIn(drop_key, self.a.arbiters)
        self.assertEqual(self.a.arbiters[family], 1)

    def test_owner_handoff_does_not_move_the_kill_arbiter(self):
        state = self.establish()
        family = f.family(tuple(state[:4]))
        self.assertEqual(self.a.arbiters.get(family), 1)
        # The lower-id client takes over simulation, but the arbiter is a lease
        # advertised by COMMITTER, not a by-product of the simulation owner.
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [root(serial=9, instance=8)], 1.2)
        self.assertEqual(result['a'][0][d.OWNER], 1)
        self.assertEqual(self.a.arbiters[family], 1)
        claim = root(serial=9, instance=8, life=d.CLAIM, landed=1)
        result, _ = self.send(self.b, self.cb, self.a, self.ca,
                              [copy.deepcopy(claim)], 1.3)
        self.assertEqual(result['a'][0][d.LIFE], d.LIVE)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [copy.deepcopy(claim)], 1.4)
        self.assertEqual(result['a'][0][d.LIFE], d.REMOVED)
        self.assertEqual(result['a'][0][d.COMMITTER], 1)


class FragileIsolationTests(FragileHarness):
    def test_receive_rejects_forged_stale_and_cross_scope_rows(self):
        _, packets = self.a.update(self.ca, [root(serial=1)], 1)
        packet = packets[0]
        self.assertTrue(self.b.receive(self.cb, packet, 1.001))
        self.assertFalse(self.b.receive(self.cb, packet, 1.002))
        for index, value in ((f.INSTANCE, 3), (f.RECEIPT, 9)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.01), index)
        for index, value in ((3, 99), (7, 27), (42, 22), (f.ROLE, 2),
                             (64, 1), (f.RESERVED0, 1)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.02), index)
        # A forged origin that is not the canonical key is refused.
        bad = copy.deepcopy(packet)
        bad['a'][0][:4] = [f.CHILD_ORIGIN, SIGNATURE, ROOM + 1, PARENT]
        self.assertFalse(self.b.receive(self.cb, bad, 1.03))
        # The reserved family origins cannot be spoofed by an untyped row.
        for origin in (f.ROOT_ORIGIN, f.CHILD_ORIGIN, f.LOOT_ORIGIN):
            bad = copy.deepcopy(packet)
            bad['a'][0][0] = origin
            self.assertFalse(self.b.receive(self.cb, bad, 1.04), origin)
        duplicate = copy.deepcopy(packet)
        duplicate['a'] = [packet['a'][0], list(packet['a'][0])]
        self.assertFalse(self.b.receive(self.cb, duplicate, 1.05))

    def test_team_and_room_isolation(self):
        _, packets = self.a.update(self.ca, [root(serial=1)], 1)
        foreign = copy.deepcopy(packets[0])
        foreign['targetTeamId'] = 'other'
        self.assertFalse(self.b.receive(self.cb, foreign, 1.01))
        # A local capture whose slot belongs to another room's placement is
        # refused before it can become a shared row for this room.
        elsewhere = root(parent=1, room=0x84, entity=0x330)
        self.assertTrue(d.valid(elsewhere))
        result, packets = self.a.update(self.ca, [elsewhere], 1.1)
        self.assertFalse(result['a'])
        self.assertFalse(packets)

    def test_envelope_cap_cadence_and_packet_type_are_unchanged(self):
        # Every distinct placed slot of this room, its linked child and the
        # drops each emitter can produce: 40 rows, so the family exercises
        # real multi-part framing rather than a single packet.
        rows, serial = [], 0
        for parent in sorted(f.SCOPE[ROOM]):
            serial += 1
            rows.append(root(serial=serial, parent=parent))
            if f.entity(ROOM, parent) in f.CHILD_ENTITIES:
                serial += 1
                rows.append(root(serial=serial, parent=parent, role=f.CHILD))
            for kind in (2, 3, 4):
                serial += 1
                rows.append(drop(kind=kind, parent=parent, serial=serial))
                if f.entity(ROOM, parent) in f.CHILD_ENTITIES:
                    serial += 1
                    rows.append(drop(kind=kind, role=f.CHILD, parent=parent,
                                     serial=serial))
        self.assertEqual(len(rows), 40)
        # Local rows are not canonical yet; the identity is what must be unique.
        self.assertEqual(len({tuple(f.identity(r, self.wa)) for r in rows}), 40)
        result, packets = self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        self.assertEqual(len(packets), 4)
        self.assertEqual(d.PACKET_TYPE, 'MNSG_WORLD_ACTORS')
        for packet in packets:
            self.assertEqual(packet['type'], d.PACKET_TYPE)
            self.assertEqual(packet['targetTeamId'], self.ca['team'])
            self.assertNotIn('targetClientId', packet)
            self.assertFalse(packet.get('addToQueue', False))
            self.assertTrue(packet['quiet'])
            self.assertLessEqual(len(packet['a']), d.ROWS_PER_PACKET)
            self.assertLessEqual(
                len(json.dumps(packet, separators=(',', ':')).encode()) + 1,
                d.PACKET_BYTES)
        self.assertFalse(self.a.update(self.ca, rows, 1.01)[1])


if __name__ == '__main__':
    unittest.main()
