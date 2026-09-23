"""Multi-client slicer transport: presence, receipts, ownership and kills.

The native bridge is not part of this module, so the tests drive its two words
directly: a capture row reports the local native instance and echoes the apply
token, and the bridge-facing result is inspected next to the wire row that was
actually sent. Every case runs two clients so ownership, arbitration and the
receipt handshake are exercised across a real exchange.
"""
import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'py'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import anchor_world as w
import anchor_world_dynamic as d
import anchor_world_slicer as s
import test_world_dynamic as helper

ROOM = 0xab
PARENT = 6
SIGNATURE = 42


def root(room=ROOM, parent=PARENT, ordinal=0, phase=s.INITIAL_WAIT, timer=0,
         life=None, present=1, paused=0, serial=1, instance=0, receipt=0,
         established=0, role=s.ROOT):
    # An out-of-scope parent still needs a shaped row so the recipe, not the
    # builder, is what rejects it.
    fixed = s.PARAMS.get(room, {}).get(parent, (0, 0, 120, 120))
    r = [0] * d.WORDS
    r[:12] = [0, 0, 0, serial, 0, d.LIVE if life is None else life, d.SLICER,
              parent, s.ENTITY, s.ENTITY, 0, role]
    r[24:27] = [100] * 3
    r[27], r[28] = 0x6e1, 0x20
    r[31] = timer
    r[32] = 163
    r[42] = phase
    r[64] = ordinal
    r[66] = paused
    r[67] = 17 if role == s.ROOT else 1
    r[s.ROLE] = role
    r[s.SUBTYPE:s.INITIAL + 1] = list(fixed)
    r[s.INSTANCE], r[s.RECEIPT] = instance, receipt
    r[s.ESTABLISHED], r[s.PRESENT] = established, present
    return r


def blade(ordinal=1, **changes):
    changes.setdefault('phase', s.FLIGHT)
    changes.setdefault('role', s.BLADE)
    changes.setdefault('serial', 2)
    r = root(ordinal=ordinal, **changes)
    r[18] = 100
    r[19] = 256
    return r


def drop(kind=2, role=s.ROOT, ordinal=0, parent=PARENT, serial=7):
    """A slicer drop built on the shared item helper, which keeps flags at 0."""
    r = helper.actor(serial=serial, kind=kind)
    r[7] = parent
    r[8] = s.ENTITY
    r[45] = role + 2
    r[64] = ordinal
    r[74:] = [0] * 9
    return r


class SlicerHarness(unittest.TestCase):
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
        """Exchange one complete (possibly empty) dynamic snapshot each way."""
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

    def key(self, row):
        return tuple(s.root_identity(row, self.wa))


class SlicerRecipeTests(SlicerHarness):
    def test_shared_table_stays_signed_and_only_the_slicer_overrides_it(self):
        # The optional union is still the NPC continuation's signed 16-bit
        # state; widening it for everyone would corrupt that recipe.
        self.assertEqual(d.BOUNDS[79], (-32768, 32767))
        self.assertEqual(d.BOUNDS[80], (-32768, 32767))
        self.assertEqual(d.BOUNDS[81], (-32768, 32767))
        # The shared window widens for the slicer kind and phase, exactly like
        # the native codec's lo/hi arrays.
        self.assertEqual(d.BOUNDS[6], (1, d.WAVE))
        self.assertEqual(d.BOUNDS[42], (0, d.wave.PURSUE2))
        # The slicer reads the same columns through its own window.
        self.assertEqual(d.SLICER_BOUNDS[79], (0, 0x7fffffff))
        self.assertEqual(d.SLICER_BOUNDS[80], (0, 0x7fffffff))
        self.assertEqual(d.SLICER_BOUNDS[81], (0, 1))
        self.assertEqual(d.SLICER_BOUNDS[6], (1, d.SLICER))
        self.assertEqual(d.SLICER_BOUNDS[42], (0, s.FLIGHT))
        npc = helper.actor(kind=1)
        npc[8], npc[9], npc[74] = 0x2c4, 0x2c4, 8
        npc[78:83] = [2, 2, 1, 9999, 2]
        self.assertTrue(d.valid(npc))
        self.assertTrue(w.integer(npc[81], *d.BOUNDS[81]))
        self.assertFalse(w.integer(npc[81], *d.SLICER_BOUNDS[81]))

    def test_recipe_pins_role_phase_timer_and_scope(self):
        self.assertTrue(d.valid(root()))
        self.assertTrue(d.valid(blade()))
        for index, value in ((s.ROLE, 2), (42, s.FLIGHT), (31, 1),
                             (s.INITIAL, 121), (s.REPEAT, 0), (8, 0x1a0),
                             (11, 1), (19, 255), (67, 1), (43, 256), (21, 1),
                             (65, 1), (33, 1)):
            row = root()
            row[index] = value
            self.assertFalse(d.valid(row), (index, value))
        for index, value in ((64, 0), (42, s.INITIAL_WAIT), (31, 121),
                             (19, 0), (67, 17)):
            row = blade()
            row[index] = value
            self.assertFalse(d.valid(row), (index, value))
        # The room and parent binding is checked separately from the recipe.
        self.assertFalse(s.scope(root(parent=8), ROOM))
        self.assertFalse(s.scope(root(room=0xac), ROOM))
        self.assertTrue(s.scope(root(room=0xac, parent=8), 0xac))
        self.assertFalse(s.scope(blade(parent=9), ROOM))

    def test_identity_is_canonical_and_injective(self):
        wa = w.WorldTransport()
        wa.signature, wa.room = SIGNATURE, ROOM
        self.assertEqual(s.root_identity(root(parent=7), wa),
                         [s.ROOT_ORIGIN, SIGNATURE, ROOM + 1, 7])
        self.assertEqual(s.blade_identity(blade(ordinal=3, parent=7), wa),
                         [s.BLADE_ORIGIN, SIGNATURE, (7 << 10) | (ROOM + 1), 3])
        root_drop = s.loot_identity(drop(role=s.ROOT, ordinal=0), wa)
        blade_drop = s.loot_identity(drop(role=s.BLADE, ordinal=3), wa)
        self.assertNotEqual(root_drop, blade_drop)
        # A root drop serializes as 1 because the role field separates it from a
        # blade drop; zero would be an invalid key.
        self.assertEqual(root_drop[3], 1)
        self.assertEqual(blade_drop[3], 3)
        self.assertEqual(root_drop[2] >> 13, PARENT)
        self.assertEqual(root_drop[2] & 0x3ff, ROOM + 1)
        self.assertEqual((root_drop[2] >> 10) & 1, 0)
        self.assertEqual((blade_drop[2] >> 10) & 1, 1)
        # The item kind is part of the packed word.
        self.assertNotEqual(root_drop,
                            s.loot_identity(drop(kind=3, role=s.ROOT), wa))
        family = tuple(s.root_identity(root(), wa))
        self.assertEqual(s.family(tuple(root_drop)), family)
        self.assertEqual(s.family(tuple(blade_drop)), family)
        self.assertEqual(s.family(tuple(s.blade_identity(blade(), wa))), family)


class SlicerTransportTests(SlicerHarness):
    def test_wire_rows_drop_the_instance_and_receipt(self):
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=7)], 1)
        wire = packets[0]['a'][0]
        self.assertEqual((wire[s.INSTANCE], wire[s.RECEIPT]), (0, 0))
        self.assertEqual((wire[s.ESTABLISHED], wire[s.PRESENT]), (0, 1))
        bridge = result['a'][0]
        self.assertEqual(bridge[s.INSTANCE], 7)
        self.assertNotEqual(bridge[s.RECEIPT], 0)

    def test_bootstrap_owner_receives_a_token_and_establishes_on_echo(self):
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=7)], 1)
        token = result['a'][0][s.RECEIPT]
        self.assertNotEqual(token, 0)
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=7, receipt=token)], 1.06)
        self.assertEqual(result['a'][0][s.ESTABLISHED], 1)
        self.assertEqual(packets[0]['a'][0][s.ESTABLISHED], 1)
        # The token is retained rather than cleared, so the next update still
        # hands the bridge a live receipt word.
        self.assertEqual(result['a'][0][s.RECEIPT], token)
        self.assertEqual(self.a.receipts.entry(self.key(root()))['token'], token)

    def test_late_fresh_lower_id_yields_an_established_remote(self):
        result, packets = self.send(self.b, self.cb, self.a, self.ca,
                                    [root(serial=9, instance=4)], 1)
        token = result['a'][0][s.RECEIPT]
        self.send(self.b, self.cb, self.a, self.ca,
                  [root(serial=9, instance=4, receipt=token)], 1.06)
        # Client 1 is fresh and locally spawned; the established remote keeps it.
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(serial=1, instance=8)], 1.2)
        self.assertEqual(result['a'][0][d.OWNER], 2)
        self.assertEqual(packets[0]['a'][0][d.OWNER], 2)

    def test_bootstrap_waits_for_a_complete_peer_snapshot(self):
        self.a.peers.clear()
        result, packets = self.a.update(self.ca, [root(serial=1)], 1)
        # The row is retained, but nobody owns it yet: the peer's complete
        # dynamic snapshot has not arrived, so the room is not ready to settle.
        self.assertEqual(result['a'][0][d.OWNER], 0)
        self.assertTrue(packets[0]['a'])
        self.assertEqual(packets[0]['a'][0][s.ESTABLISHED], 0)
        self.send(self.b, self.cb, self.a, self.ca, [], 1.05)
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(serial=1)], 1.1)
        self.assertEqual(result['a'][0][d.OWNER], 1)

    def test_paused_or_culled_established_remote_is_held(self):
        result, packets = self.send(self.b, self.cb, self.a, self.ca,
                                    [root(serial=9, instance=4)], 1)
        token = result['a'][0][s.RECEIPT]
        self.send(self.b, self.cb, self.a, self.ca,
                  [root(serial=9, instance=4, receipt=token)], 1.06)
        for state in (root(serial=9, instance=4, receipt=token, paused=1),
                      root(serial=9, instance=4, receipt=token, present=0)):
            self.send(self.b, self.cb, self.a, self.ca, [state], 1.2)
            result, _ = self.send(self.a, self.ca, self.b, self.cb,
                                  [root(serial=1, instance=8)], 1.3)
            # Nobody may own the row, so the last checkpoint is held instead of
            # handing it to a fresh local copy.
            self.assertTrue(result['a'])
            self.assertNotEqual(result['a'][0][d.OWNER], 1)

    def test_reload_needs_a_fresh_receipt_and_stale_echo_is_ignored(self):
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=4)], 1)
        token = result['a'][0][s.RECEIPT]
        self.send(self.a, self.ca, self.b, self.cb,
                  [root(instance=4, receipt=token)], 1.06)
        entry = self.a.receipts.entry(self.key(root()))
        self.assertTrue(entry['established'])
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(instance=9)], 1.2)
        self.assertFalse(entry['established'])
        fresh = result['a'][0][s.RECEIPT]
        self.assertTrue(fresh and fresh != token)
        self.send(self.a, self.ca, self.b, self.cb,
                  [root(instance=9, receipt=token)], 1.3)
        self.assertFalse(entry['established'])
        self.send(self.a, self.ca, self.b, self.cb,
                  [root(instance=9, receipt=fresh)], 1.4)
        self.assertTrue(entry['established'])

    def test_reload_receives_checkpoint_while_all_old_holders_are_culled(self):
        state, _ = self.send(self.b, self.cb, self.a, self.ca,
                             [root(instance=4, ordinal=9)], 1)
        token = state['a'][0][s.RECEIPT]
        self.send(self.b, self.cb, self.a, self.ca,
                  [root(instance=4, receipt=token, ordinal=9, present=0, paused=1)], 1.1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [root(instance=8)], 1.2)
        held = result['a'][0]
        self.assertEqual(held[d.OWNER], 0)
        self.assertEqual(held[64], 9)
        self.assertEqual(held[s.INSTANCE], 8)
        self.assertTrue(held[s.RECEIPT])
        self.assertEqual(held[66], 1)
        # Native confirms the held pose but reports its real local pause and
        # presence, allowing this incarnation to resume the room simulation.
        applied = list(held);applied[66] = 0;applied[s.PRESENT] = 1
        result, _ = self.send(self.a, self.ca, self.b, self.cb, [applied], 1.3)
        self.assertEqual(result['a'][0][d.OWNER], 1)
        self.assertEqual(result['a'][0][64], 9)

    def test_replica_stale_snapshot_cannot_rewind_the_emission_counter(self):
        self.send(self.a, self.ca, self.b, self.cb,
                  [root(ordinal=5, instance=4)], 1)
        self.send(self.b, self.cb, self.a, self.ca,
                  [root(ordinal=3, instance=1)], 1.1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [root(ordinal=5, instance=4)], 1.2)
        self.assertEqual(result['a'][0][64], 5)


class SlicerClaimTests(SlicerHarness):
    def test_announced_entrant_without_world_metadata_holds_fresh_emitter(self):
        self.ca['players'][2].pop('worldSync')
        result,packets=self.a.update(self.ca,[root(instance=4)],1)
        self.assertEqual(result['a'][0][d.OWNER],0)
        self.assertEqual(result['a'][0][d.COMMITTER],0)
        self.assertTrue(result['a'][0][s.RECEIPT])
        self.assertTrue(packets)
        self.ca['players'][2]['worldSync']=self.wb.advertisement(self.cb)
        self.send(self.b,self.cb,self.a,self.ca,[],1.1)
        result,_=self.a.update(self.ca,[root(instance=4)],1.2)
        self.assertEqual(result['a'][0][d.OWNER],1)

    def test_absent_uncommitted_birth_releases_checkpoint_bookkeeping(self):
        result,_=self.a.update(self.ca,[blade(instance=4)],1)
        key=tuple(result['a'][0][:4])
        self.assertIn(key,self.a.receipts.entries)
        self.assertIn(key,self.a.checkpoints)
        self.a.update(self.ca,[],1.1)
        self.assertNotIn(key,self.a.receipts.entries)
        self.assertNotIn(key,self.a.checkpoints)

    def establish(self):
        row = root(serial=1, instance=4)
        result, packets = self.send(self.a, self.ca, self.b, self.cb, [row], 1)
        token = result['a'][0][s.RECEIPT]
        result, packets = self.send(self.a, self.ca, self.b, self.cb,
                                    [root(serial=1, instance=4, receipt=token)],
                                    1.06)
        self.assertEqual(result['a'][0][s.ESTABLISHED], 1)
        return result['a'][0]

    def test_two_claimants_commit_one_kill_gated_on_the_commit_send(self):
        state = self.establish()
        key = tuple(state[:4])
        claim = root(serial=1, instance=4, life=d.CLAIM)
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
        self.assertEqual(self.a.native_result(committed)['a'][0][d.LIFE], d.REMOVED)
        self.assertIn(key, self.a.dead)
        self.assertEqual(len([k for k in self.a.dead if k == key]), 1)

    def test_local_arbiter_is_not_reset_by_its_own_absence(self):
        state = self.establish()
        family = s.family(tuple(state[:4]))
        self.assertEqual(self.a.arbiters[family], 1)
        # The world helper reports the local client as absent, which must not be
        # read as the arbiter leaving the roster.
        self.assertIsNone(self.wa._peer(self.ca, 1))
        claim = root(serial=1, instance=4, life=d.CLAIM)
        self.send(self.b, self.cb, self.a, self.ca, [copy.deepcopy(claim)], 1.1)
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [copy.deepcopy(claim)], 1.2)
        self.assertEqual(self.a.arbiters[family], 1)
        self.assertEqual(result['a'][0][d.COMMITTER], 1)

    def test_departure_releases_the_family_arbiter(self):
        existing=root(serial=9, instance=4)
        existing[d.COMMITTER]=2
        result, packets = self.send(self.b, self.cb, self.a, self.ca,
                                    [existing], 1)
        token = result['a'][0][s.RECEIPT]
        existing[s.RECEIPT]=token
        self.send(self.b, self.cb, self.a, self.ca,
                  [existing], 1.06)
        family = s.family(tuple(s.root_identity(root(), self.wa)))
        self.assertEqual(self.a.arbiters[family], 2)
        self.ca['players'][2]['online'] = False
        self.send(self.a, self.ca, self.b, self.cb, [root(serial=1)], 2)
        self.assertEqual(self.a.arbiters[family], 1)

    def test_culled_claim_commits_without_a_present_simulator(self):
        state=self.establish()
        claim=list(state)
        claim[d.LIFE]=d.CLAIM
        claim[s.PRESENT]=0
        claim[66]=1
        result,packets=self.a.update(self.ca,[claim],2)
        self.assertEqual(result['a'][0][d.LIFE],d.REMOVED)
        self.assertEqual(result['a'][0][d.COMMITTER],1)
        self.assertFalse(self.a.native_result(result)['a'])
        self.a.sent(packets,True,2)
        self.assertEqual(self.a.native_result(result)['a'][0][d.LANDED],1)

    def test_family_arbiter_does_not_follow_blade_simulation_owner(self):
        state=self.establish()
        child=blade(instance=7)
        child[d.OWNER]=2
        child[d.COMMITTER]=1
        self.send(self.b,self.cb,self.a,self.ca,[child],2)
        family=s.family(tuple(state[:4]))
        self.assertEqual(self.a.arbiters[family],1)
        child[d.LIFE]=d.CLAIM
        self.send(self.b,self.cb,self.a,self.ca,[child],2.1)
        result,packets=self.a.update(self.ca,[],2.2)
        killed=[r for r in result['a'] if r[s.ROLE]==s.BLADE][0]
        self.assertEqual(killed[d.COMMITTER],1)
        self.assertEqual(killed[d.LIFE],d.REMOVED)
        self.assertTrue(any(r[:4]==killed[:4] for p in packets for r in p['a']))


class SlicerDropTests(SlicerHarness):
    def test_drops_keep_their_own_flags_and_use_the_marker_origin(self):
        for kind in (2, 3):
            row = drop(kind=kind, role=s.ROOT, ordinal=0)
            self.assertEqual(row[27:29], [0, 0])
            self.assertTrue(d.valid(row))
        high = drop(kind=2, role=s.BLADE, ordinal=0x7fffffff)
        self.assertTrue(d.valid(high))
        result, packets = self.send(self.a, self.ca, self.b, self.cb, [high], 1)
        self.assertEqual(result['a'][0][:4], s.loot_identity(high, self.wa))
        self.assertTrue(result['a'][0][3])
        result, _ = self.send(self.a, self.ca, self.b, self.cb,
                              [drop(role=s.ROOT, ordinal=0)], 1.1)
        self.assertEqual(result['a'][0][3], 1)
        # Both peers coalesce on the same dropped item.
        other = copy.deepcopy(high)
        other[3] = 99
        result, _ = self.send(self.b, self.cb, self.a, self.ca, [other], 1.2)
        keys = [r[:4] for r in result['a'] if r[6] == 2]
        self.assertEqual(keys.count(s.loot_identity(high, self.wa)), 1)
        # The marker is required: a normal item keeps the placed window.
        plain = helper.actor(serial=1, kind=2)
        plain[64] = 0x7fffffff
        self.assertFalse(s.loot(plain, ROOM))
        result, _ = self.send(self.a, self.ca, self.b, self.cb, [plain], 1.3)
        placed = [r[:4] for r in result['a'] if r[:4][0] == 0x7ffffffe]
        self.assertEqual(placed, [[0x7ffffffe, SIGNATURE, ROOM + 1, 1]])


class SlicerEnvelopeTests(SlicerHarness):
    def test_receive_rejects_forged_duplicate_and_cross_scope_rows(self):
        _, packets = self.a.update(self.ca, [root(serial=1)], 1)
        packet = packets[0]
        self.assertTrue(self.b.receive(self.cb, packet, 1.001))
        self.assertFalse(self.b.receive(self.cb, packet, 1.002))
        for index, value in ((s.INSTANCE, 3), (s.RECEIPT, 9)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.01), index)
        for index, value in ((3, 99), (7, 8), (42, 22), (s.ROLE, 2)):
            bad = copy.deepcopy(packet)
            bad['a'][0][index] = value
            self.assertFalse(self.b.receive(self.cb, bad, 1.02), index)
        duplicate = copy.deepcopy(packet)
        duplicate['a'] = [packet['a'][0], list(packet['a'][0])]
        self.assertFalse(self.b.receive(self.cb, duplicate, 1.03))
        # A drop row bound to a foreign origin is rejected too.
        drop_packet = copy.deepcopy(packet)
        drop_packet['a'] = [drop(role=s.BLADE, ordinal=2)]
        drop_packet['a'][0][:4] = [s.LOOT_ORIGIN, SIGNATURE, ROOM + 1, 2]
        self.assertFalse(self.b.receive(self.cb, drop_packet, 1.04))

    def test_envelope_cap_cadence_and_packet_type_are_unchanged(self):
        rows = [blade(ordinal=i + 1, serial=i + 1) for i in range(d.MAX_ACTORS)]
        result, packets = self.send(self.a, self.ca, self.b, self.cb, rows, 1)
        self.assertEqual(len(packets), d.MAX_PARTS)
        self.assertEqual(d.PACKET_TYPE, 'MNSG_WORLD_ACTORS')
        for packet in packets:
            self.assertEqual(packet['type'], d.PACKET_TYPE)
            self.assertEqual(packet['targetTeamId'], self.ca['team'])
            self.assertNotIn('targetClientId', packet)
            self.assertFalse(packet.get('addToQueue', False))
            self.assertTrue(packet['quiet'])
            self.assertLessEqual(
                len(json.dumps(packet, separators=(',', ':')).encode()) + 1,
                d.PACKET_BYTES)
        # A slicer does not get a faster steady-state cadence than the rest.
        self.assertFalse(self.a.update(self.ca, rows, 1.01)[1])


if __name__ == '__main__':
    unittest.main()
