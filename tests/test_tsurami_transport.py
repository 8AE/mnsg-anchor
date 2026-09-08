"""Framed Tsurami boss and projectile-reflection coordination regressions."""

import copy
import json
import unittest
from unittest import mock

from test_dharumanyo_transport import Relay as BossRelay
import anchor_congo
import anchor_dharumanyo
import anchor_tsurami


def state(hp=12, tick=0, projectiles=0, fill=0):
    root = [fill] * anchor_tsurami.ROOT_WORDS
    root[:3] = [1, tick, hp]
    return {
        "r": root,
        "v": [fill] * anchor_tsurami.VISUAL_WORDS,
        "s": projectiles,
        "t": tick,
        "p": [fill] * (projectiles * anchor_tsurami.PROJECTILE_WORDS),
    }


class Relay(BossRelay):
    """Reuse the framed Anchor team/direct relay, supplying Tsurami state."""

    def add(self, cid, room=anchor_tsurami.ROOM, team="blue", session=None):
        client = super().add(cid, room, team, session)
        self.states[cid] = state()
        self.ready[cid] = room == anchor_tsurami.ROOM
        return client

    def roster(self):
        for receiver in self.clients.values():
            members = []
            for cid, client in self.clients.items():
                local = client._player_states.get(cid, {})
                context = client._boss_context()
                member = {
                    "clientId": cid, "self": receiver is client,
                    "name": client._player_name, "online": client._connected,
                    "teamId": client._team_id,
                    "isSaveLoaded": client._local_save_loaded,
                    "currentRoomId": local.get("roomId", client._local_room_id),
                    "interactionSession": client._interaction_session,
                }
                for module, transport in (
                    (anchor_congo, client._congo),
                    (anchor_dharumanyo, client._dharumanyo),
                    (anchor_tsurami, client._tsurami),
                ):
                    member[module.METADATA_KEY] = transport.advertisement(context)
                members.append(member)
            receiver._replace_all_client_states(members)
            for cid in self.clients:
                receiver._player_states[cid]["playerEpoch"] = 7

    def tick(self, advance=0.0, flush=True):
        self.now += advance
        for cid, client in self.clients.items():
            if client._connected:
                self.status[cid] = json.loads(client.update_tsurami(
                    self.ready[cid], self.visit[cid], self.paused[cid],
                    json.dumps(self.states[cid], separators=(",", ":")),
                ))
        if flush:
            self.flush()
        return self.status

    def packets(self, op=None):
        return [(stamp, cid, packet) for stamp, cid, packet in self.history
                if packet["type"] == anchor_tsurami.PACKET_TYPE and
                (op is None or packet["op"] == op)]


class TsuramiTransportTests(unittest.TestCase):
    def setUp(self):
        self.net = Relay(self)
        self.a = self.net.add(2)
        self.b = self.net.add(3)

    def test_exact_integer_checkpoint_schema_and_bounds(self):
        maximal = state(hp=0xffffffff, tick=0xffffffff,
                        projectiles=anchor_tsurami.MAX_PROJECTILES,
                        fill=0xffffffff)
        maximal["r"][0] = 0xffffffff
        self.assertIs(anchor_tsurami.validate_state(maximal), maximal)
        bad = [None, [], {}, {**state(), "extra": 1},
               {**state(), "s": True}, {**state(), "t": 1.0},
               {**state(), "s": -1}, {**state(), "t": 0x100000000},
               {**state(), "p": [0]},
               state(projectiles=anchor_tsurami.MAX_PROJECTILES + 1)]
        for key in ("r", "v"):
            for values in (state()[key][:-1], state()[key] + [0],
                           [True] + state()[key][1:]):
                bad.append({**state(), key: values})
        for value in bad:
            with self.subTest(value=value):
                self.assertIsNone(anchor_tsurami.validate_state(value))
        self.assertLessEqual(len(json.dumps(maximal, separators=(",", ":"))),
                             anchor_tsurami.MAX_STATE_BYTES)

    def test_single_authority_team_route_version_and_coalesced_receive(self):
        other_team = self.net.add(4, team="red")
        self.net.start()
        self.assertEqual([self.net.status[c]["role"] for c in (2, 3)], [1, 2])
        self.assertEqual(self.net.status[3]["state"], state())
        self.assertEqual(self.net.status[3]["encounter"], [2, 202, 1])
        self.assertEqual(other_team._tsurami.owner, 4)
        packets = self.net.packets()
        self.assertEqual({cid for _, cid, _ in packets}, {2})
        for _, cid, packet in packets:
            self.assertEqual(packet["clientId"], cid)
            self.assertEqual(packet["targetTeamId"], "blue")
            self.assertEqual(packet["v"], 1)
            self.assertTrue(packet["quiet"])
            self.assertNotIn("addToQueue", packet)
        while self.b.has_packet():
            self.assertNotEqual(json.loads(self.b.poll_packet())["type"],
                                anchor_tsurami.PACKET_TYPE)

    def test_native_projectile_orientation_reaches_peer_and_despawns(self):
        self.net.start()
        checkpoint = state(tick=20, projectiles=1)
        # func_080049A4 calls func_8021A310, which writes the native
        # 0x8000 orientation marker to object yaw and pitch.
        checkpoint["p"] = [
            1, 20, 0, 1, 300, 20, 0x3f800000, 0, 0,
            0x8000, 0, 0, 0, 0x3f800000, 0x3f800000, 0x3f800000,
            0, 0, 0x8000, 25,
        ]
        self.net.states[2] = checkpoint
        self.net.tick(0.11)
        self.net.tick()
        self.assertEqual(self.net.status[3]["state"], checkpoint)
        self.assertEqual(self.net.packets("s")[-1][2]["d"], checkpoint)

        cleared = copy.deepcopy(checkpoint)
        cleared["r"][1] += 1
        cleared["t"] += 1
        cleared["p"] = []
        self.net.states[2] = cleared
        self.net.tick(0.11)
        self.net.tick()
        self.assertEqual(self.net.status[3]["state"], cleared)

    def test_metadata_coexists_and_stale_revision_does_not_remove_encounter(self):
        self.net.start()
        original = list(self.b._player_states[2][anchor_tsurami.METADATA_KEY])
        self.a.set_character("Goemon")
        self.net.flush()
        self.assertEqual(self.b._player_states[2][anchor_tsurami.METADATA_KEY],
                         original)
        self.assertIsNone(self.a._congo.e)
        self.assertIsNone(self.a._dharumanyo.e)
        self.a.update_client_state("{}")
        metadata = json.loads(self.a._sock.sent[-1][:-1])["state"]
        for module in (anchor_congo, anchor_dharumanyo, anchor_tsurami):
            self.assertIsNotNone(module.metadata(metadata[module.METADATA_KEY]))
        stale = list(original)
        stale[1], stale[4] = 0, 0
        self.b._merge_client_state(2, {anchor_tsurami.METADATA_KEY: stale})
        self.assertEqual(self.b._player_states[2][anchor_tsurami.METADATA_KEY],
                         original)

    def test_snapshot_spoof_version_session_visit_and_room_rejected(self):
        self.net.start()
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        packet["q"] += 1
        revision = self.b._tsurami.revision
        for changes in ({"type": anchor_congo.PACKET_TYPE}, {"v": 2},
                        {"v": True}, {"v": 1.0}, {"clientId": 999},
                        {"targetTeamId": "red"}, {"session": 999},
                        {"clientId": 3}, {"targetClientId": 99}, {"visit": 2}):
            with self.subTest(changes=changes):
                self.net.deliver(self.b, {**packet, **changes})
                self.assertEqual(self.b._tsurami.revision, revision)
        self.b._merge_client_state(2, {"currentRoomId": 10})
        self.net.deliver(self.b, packet)
        self.assertEqual(self.b._tsurami.revision, revision)

    def test_duplicate_reordered_and_malformed_snapshots_cannot_rollback(self):
        self.net.start()
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        checkpoint = state(hp=7, tick=20, projectiles=4)
        self.net.states[2] = checkpoint
        self.net.tick(0.1)
        current = copy.deepcopy(self.net.packets("s")[-1][2])
        revision = self.b._tsurami.revision
        for packet in (old, current, {**current, "q": 100, "d": {}},
                       {**current, "q": 100,
                        "a": [[3, 303, 7, 1]] * (anchor_tsurami.ACK_ROWS + 1)}):
            self.net.deliver(self.b, packet)
        self.assertEqual(self.b._tsurami.state, checkpoint)
        self.assertEqual(self.b._tsurami.revision, revision)
        self.assertEqual(self.b._tsurami.acks, {})

    def test_late_intro_join_adopts_active_hazards_without_local_capture(self):
        self.net.start()
        checkpoint = state(hp=4, tick=150, projectiles=7)
        self.net.states[2] = checkpoint
        self.net.tick(0.1)
        late = self.net.add(1)
        self.net.states[1] = None
        for advance in (0.0, 0.1, 0.1):
            self.net.tick(advance)
        self.assertEqual(self.net.status[1]["role"], 2)
        self.assertEqual(self.net.status[1]["owner"], 2)
        self.assertEqual(self.net.status[1]["state"], checkpoint)
        self.assertFalse(late._tsurami.capture_ready)

    def test_respawn_after_solo_owner_progress_requests_fresh_state_before_election(self):
        self.net.start()
        old_encounter = list(self.net.status[3]["encounter"])
        self.net.ready[3] = False
        self.net.tick(0.1)
        # The remaining owner stops transmitting when nobody else is ready,
        # but continues the native fight while the dead participant reloads.
        for tick in range(45):
            self.net.states[2] = state(hp=6, tick=tick, projectiles=3)
            self.net.tick(0.1)
        self.assertEqual(self.b._tsurami.cache[tuple(old_encounter)]["state"], state())
        self.net.ready[3] = True
        self.net.visit[3] = 2  # Coordinator advances even if native room visit did not.
        self.net.states[3] = state()  # The locally rebuilt boss starts at full HP.
        self.net.history.clear()
        for _ in range(6):
            self.net.tick(0.1)
        self.assertEqual((self.a._tsurami.owner, self.b._tsurami.owner), (2, 2))
        self.assertEqual((self.a._tsurami.term, self.b._tsurami.term), (1, 1))
        self.assertEqual(self.net.status[3]["role"], 2)
        self.assertEqual(self.net.status[3]["encounter"], old_encounter)
        self.assertEqual(self.net.status[3]["state"], state(hp=6, tick=44, projectiles=3))
        self.assertTrue(self.net.packets("r"))
        self.assertEqual({cid for _, cid, _ in self.net.packets("s")}, {2})

    def test_respawn_while_two_peers_keep_fighting_adopts_latest_cached_hazards(self):
        self.net.add(4)
        self.net.start()
        self.net.ready[3] = False
        self.net.tick(0.1)
        for tick in range(45):
            self.net.states[2] = state(hp=5, tick=tick, projectiles=6)
            self.net.tick(0.1)
        self.net.ready[3] = True
        self.net.visit[3] = 2
        self.net.states[3] = state()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["role"], 2)
        self.assertEqual(self.net.status[3]["owner"], 2)
        self.assertEqual(self.net.status[3]["state"], state(hp=5, tick=44, projectiles=6))
        self.assertEqual({client._tsurami.owner for client in self.net.clients.values()}, {2})

    def test_departure_and_lease_handoff_preserve_checkpoint_and_fence_old_owner(self):
        self.net.start()
        checkpoint = state(hp=3, tick=210, projectiles=6)
        self.net.states[2] = checkpoint
        self.net.tick(0.1)
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["role"], 1)
        self.assertEqual(self.net.status[3]["term"], 2)
        self.assertEqual(self.net.status[3]["state"], checkpoint)
        self.net.deliver(self.b, old)
        self.assertEqual(self.b._tsurami.owner, 3)
        other = Relay(self)
        owner, follower = other.add(2), other.add(3)
        other.start()
        other.now += anchor_tsurami.LEASE + 0.1
        successor = json.loads(follower.update_tsurami(1, 1, 0, "null"))
        self.assertEqual((successor["role"], successor["term"]), (1, 2))
        other.flush()
        resumed = json.loads(owner.update_tsurami(1, 1, 0, json.dumps(state())))
        self.assertEqual((resumed["role"], resumed["owner"]), (2, 3))

    def test_root_hits_and_reflections_retry_with_target_and_ack_once(self):
        self.net.start()
        for sequence, target in ((1, 0), (2, 17)):
            self.assertTrue(self.b.send_tsurami_hit(sequence, 1, target))
            self.net.tick(0.1)
            first = copy.deepcopy(self.net.packets("h")[-1][2])
            self.assertEqual(first["targetClientId"], 2)
            self.assertEqual(first["h"], [7, sequence, 1, target])
            # No owner game update: the follower retries the same identity.
            self.net.now += anchor_tsurami.RETRY
            self.b.update_tsurami(1, 1, 0, json.dumps(state()))
            self.net.flush()
            self.assertEqual(self.net.packets("h")[-1][2]["h"], first["h"])
            self.net.deliver(self.a, first)
            self.net.tick(0.01)
            self.assertEqual(self.net.status[2]["hits"],
                             [[3, 303, 7, sequence, 1, target]])
            self.assertLess(self.a._tsurami.acks.get((3, 303, 7), 0), sequence)
            self.net.states[2] = state(hp=12 - sequence, tick=sequence)
            self.net.tick(0.1)
            self.assertEqual(self.a._tsurami.acks[(3, 303, 7)], sequence)
            self.assertFalse(self.b._tsurami.outgoing)
            self.net.deliver(self.a, first)
            self.net.tick(0.1)
            self.assertEqual(self.net.status[2]["hits"], [])

    def test_reflection_target_rejects_wrong_shape_types_and_bounds(self):
        self.net.start()
        for target in (-1, 0x80000000, 0xffffffff, 0x100000000,
                       True, 1.0, "1", None):
            self.assertFalse(self.b.send_tsurami_hit(1, 1, target))
        for amount in (0, 5, 6, 7, 9, True, 1.0):
            self.assertFalse(self.b.send_tsurami_hit(1, amount, 0))
        for hit in ([7, 1, 1], [7, 1, 1, -1], [7, 1, 1, True],
                    [7, 1, 1, 0x80000000], [7, 1, 1, 0xffffffff],
                    [7, 1, 1, 0x100000000], [7, 1, 1, 0, 0], [8, 1, 1, 0]):
            packet = self.b._tsurami._packet(
                self.b._boss_context(), "h", targetClientId=2, h=hit)
            self.net.deliver(self.a, packet)
        self.assertFalse(self.a._tsurami.incoming)
        self.assertTrue(self.b.send_tsurami_hit(1, 1, 0x7fffffff))
        self.net.tick(0.1)
        self.assertEqual(self.net.packets("h")[-1][2]["h"],
                         [7, 1, 1, 0x7fffffff])

    def test_reflection_sequence_deduplicates_changed_target_and_survives_handoff(self):
        self.net.start()
        self.assertTrue(self.b.send_tsurami_hit(1, 3, 17))
        self.assertTrue(self.b.send_tsurami_hit(1, 8, 18))
        self.net.tick(0.1)
        packet = copy.deepcopy(self.net.packets("h")[-1][2])
        self.assertEqual(packet["h"], [7, 1, 3, 17])
        self.net.deliver(self.a, {**packet, "h": [7, 1, 8, 18]})
        self.net.tick(0.01)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 3, 17]])
        self.net.states[2] = state(hp=9, tick=1, projectiles=1)
        self.net.tick(0.1)
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["role"], 1)
        self.assertEqual(self.b._tsurami.acks[(3, 303, 7)], 1)
        self.assertTrue(self.b.send_tsurami_hit(1, 8, 18))
        self.net.tick(0.1)
        self.assertEqual(self.net.status[3]["hits"], [])

    def test_eight_row_ack_slices_rotate_and_merge_on_all_followers(self):
        for cid in range(4, 20):
            self.net.add(cid)
        self.net.start()
        transport = self.a._tsurami
        expected = {(cid, cid * 101, 7): 9 for cid in range(3, 20)}
        transport.acks = dict(expected)
        transport.ack_pending = set(expected)
        transport.ack_revision += 1
        self.net.history.clear()
        for _ in range(3):
            self.net.tick(0.1)
        packets = self.net.packets("s")
        self.assertEqual([len(packet["a"]) for _, _, packet in packets], [8, 8, 1])
        self.assertFalse(transport.ack_pending)
        self.assertEqual(self.b._tsurami.acks, expected)
        self.assertEqual(self.net.clients[19]._tsurami.acks, expected)

    def test_pause_defers_hit_work_and_invalid_native_capture_does_not_ack(self):
        self.net.start()
        self.net.paused[2] = 1
        self.assertTrue(self.b.send_tsurami_hit(1, 1, 21))
        for _ in range(120):
            self.net.tick(1 / 30)
        self.assertEqual(self.net.status[2]["hits"], [])
        self.assertEqual((self.net.status[3]["paused"],
                          self.net.status[3]["owner"]), (1, 2))
        self.net.paused[2] = 0
        self.net.tick(0.1)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 1, 21]])
        self.net.states[2] = None
        self.net.tick(0.1)
        self.assertEqual(self.a._tsurami.acks, {})
        self.assertTrue(self.a._tsurami.delivered)

    def test_failed_snapshot_and_hit_send_leave_retry_cadence_uncommitted(self):
        self.net.start()
        self.net.now += 0.1
        self.a._tsurami.advertisement_dirty = False
        previous = self.a._tsurami.last_wire
        with mock.patch.object(self.a, "_send_raw", return_value=False):
            self.a.update_tsurami(1, 1, 0, json.dumps(state(tick=9)))
        self.assertEqual(self.a._tsurami.last_wire, previous)
        with mock.patch.object(self.a, "_send_raw", return_value=True) as send:
            self.a.update_tsurami(1, 1, 0, json.dumps(state(tick=9)))
        self.assertEqual(send.call_args.args[0]["op"], "s")
        self.assertEqual(self.a._tsurami.last_wire, self.net.now)
        self.assertTrue(self.b.send_tsurami_hit(1, 1, 23))
        previous = self.b._tsurami.last_hit_wire
        with mock.patch.object(self.b, "_send_raw", return_value=False):
            self.b.update_tsurami(1, 1, 0, "null")
        self.assertEqual(self.b._tsurami.last_hit_wire, previous)
        with mock.patch.object(self.b, "_send_raw", return_value=True) as send:
            self.b.update_tsurami(1, 1, 0, "null")
        self.assertEqual(send.call_args.args[0]["h"], [7, 1, 1, 23])

    def test_save_unload_disconnect_and_same_connection_reentry_clear_old_work(self):
        self.net.start()
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        self.assertTrue(self.b.send_tsurami_hit(1, 1, 2))
        self.b.set_save_loaded(False)
        self.assertIsNone(self.b._tsurami.e)
        self.assertFalse(self.b._tsurami.outgoing)
        self.a.set_local_room(10)
        self.net.flush()
        self.a.set_local_room(anchor_tsurami.ROOM)
        self.a.update_tsurami(1, 2, 0, json.dumps(state()))
        self.net.flush()
        sequence = self.b._tsurami.sequence
        self.net.deliver(self.b, {**old, "q": 999})
        self.assertEqual(self.b._tsurami.sequence, sequence)
        self.a.disconnect()
        self.assertIsNone(self.a._tsurami.e)
        self.assertFalse(self.a._tsurami.cache)

    def test_deferred_snapshot_waits_for_metadata_and_expires_without_it(self):
        self.net.start()
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        packet.update(q=2, d=state(hp=7, tick=4))
        metadata = self.b._player_states[2].pop(anchor_tsurami.METADATA_KEY)
        self.net.deliver(self.b, packet)
        self.assertEqual(self.b._tsurami.state, state())
        self.assertIn(2, self.b._tsurami.pending_packets)
        self.b._player_states[2][anchor_tsurami.METADATA_KEY] = metadata
        self.net.tick(0.01)
        self.assertEqual(self.b._tsurami.state, packet["d"])
        self.assertFalse(self.b._tsurami.pending_packets)
        self.b._player_states[2].pop(anchor_tsurami.METADATA_KEY)
        self.net.deliver(self.b, {**packet, "q": 3})
        self.net.now += anchor_tsurami.LEASE + 0.1
        self.b.update_tsurami(1, 1, 0, "null")
        self.assertFalse(self.b._tsurami.pending_packets)

    def test_changed_snapshots_and_request_flood_respect_ten_hz(self):
        self.net.start()
        self.net.history.clear()
        for frame in range(60):
            self.net.states[2] = state(tick=frame + 1, projectiles=8)
            request = self.b._tsurami._packet(
                self.b._boss_context(), "r", targetClientId=2)
            for _ in range(12):
                self.net.deliver(self.a, request)
            self.net.tick(1 / 60)
        packets = self.net.packets("s")
        self.assertLessEqual(len(packets), 10)
        self.assertTrue(all(b[0] - a[0] >= 0.1 - 1e-8
                            for a, b in zip(packets, packets[1:])))
        self.assertEqual({cid for _, cid, _ in self.net.packets()}, {2})

    def test_maximum_snapshot_ack_and_native_hit_status_fit_declared_budgets(self):
        self.net.start()
        maximal = state(hp=0xffffffff, tick=0xffffffff,
                        projectiles=anchor_tsurami.MAX_PROJECTILES,
                        fill=0xffffffff)
        maximal["r"][0] = 0xffffffff
        transport = self.a._tsurami
        context = dict(self.a._boss_context())
        context.update(cid=0x7fffffff, session=0x7fffffff, team="default")
        transport.e = (0x7fffffff,) * 3
        transport.term = 0x7fffffff
        transport.local = (True, 0x7fffffff, False)
        packet = transport._packet(
            context, "s", q=0x7fffffff, p=1, d=maximal,
            a=[[0x7fffffff] * 4 for _ in range(anchor_tsurami.ACK_ROWS)])
        size = len((json.dumps(packet, separators=(",", ":")) + "\0").encode())
        self.assertLessEqual(size, self.a.HOT_PACKET_MAX_BYTES[anchor_tsurami.PACKET_TYPE])
        status = dict(self.net.status[2])
        status["state"] = maximal
        status["hits"] = [[0x7fffffff] * 4 + [8, 0x7fffffff]
                          for _ in range(anchor_tsurami.HITS_PER_FRAME)]
        self.assertLess(len(json.dumps(status, separators=(",", ":"))),
                        anchor_tsurami.MAX_STATUS_BYTES)


if __name__ == "__main__":
    unittest.main()
