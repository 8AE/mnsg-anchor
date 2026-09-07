"""Actual framed client-to-client Congo flow, without a game or live service."""

import copy
import json
import struct
import unittest
from unittest import mock

from test_boss_invitation_transport import load_client, RecordingSocket
import anchor_congo


def state(hp=12, tick=0):
    root = [1, 20, hp] + [0] * 21
    root[14:17] = [1065353216] * 3  # Native IEEE754 scale 1.0.
    root[21:24] = [42, 50, 0]
    return {"r": root, "p": [word for part in range(6) for word in (part, 0, 0, 0)],
            "s": 0, "t": tick, "f": []}


def flame_state(tick=90, flames=8):
    bits = lambda f: struct.unpack(">I", struct.pack(">f", f))[0]
    result = state(hp=17, tick=tick)
    result["r"][8:11] = [bits(20.5), bits(-70), bits(140.25)]
    for part in range(6):
        result["p"][part * 4 + 1] = bits((tick % 90) / 3)
    result["f"] = [word for i in range(flames) for word in
                   (i + 1, max(0, tick - 6), 0x10, bits(i * 8.5), bits(-70), bits(140), i * 29 % 1024)]
    return result


class Relay:
    """Anchor's transient team/direct routing, with explicit delivery control."""
    def __init__(self, test):
        self.test = test
        self.clients = {}
        self.status = {}
        self.states = {}
        self.ready = {}
        self.visit = {}
        self.paused = {}
        self.now = 100.0
        self.history = []
        self.clock = mock.patch("time.monotonic", side_effect=lambda: self.now)
        self.clock.start()
        test.addCleanup(self.clock.stop)

    def add(self, cid, room=22, team="blue", session=None):
        client = load_client(cid, session or cid * 101, team, room)
        self.test.addCleanup(client.disconnect)
        self.clients[cid] = client
        self.states[cid] = state()
        self.ready[cid] = room == 22
        self.visit[cid] = 1
        self.paused[cid] = 0
        client._player_states[cid]["playerEpoch"] = 7
        client._player_states[cid]["roomId"] = room
        self.roster()
        return client

    def roster(self):
        for receiver in self.clients.values():
            members = []
            for cid, client in self.clients.items():
                local = client._player_states.get(cid, {})
                members.append({"clientId": cid, "self": receiver is client,
                                "name": client._player_name, "online": True,
                                "teamId": client._team_id, "isSaveLoaded": client._local_save_loaded,
                                "currentRoomId": local.get("roomId", client._local_room_id),
                                "interactionSession": client._interaction_session,
                                "mnsgCongo": client._congo.advertisement(client._congo_context())})
            receiver._replace_all_client_states(members)
            for cid in self.clients:
                receiver._player_states[cid]["playerEpoch"] = 7

    def deliver(self, client, packet):
        wire = (json.dumps(packet, separators=(",", ":")) + "\0").encode()
        old = client._sock
        receiver = RecordingSocket((wire[:13], wire[13:]))
        client._sock = receiver
        with mock.patch.object(client, "_do_disconnect"):
            client._recv_loop(receiver)
        client._sock = old

    def collect(self):
        result = []
        for cid, client in self.clients.items():
            sock = client._sock
            if not sock:
                continue
            sent, sock.sent = sock.sent, []
            for raw in sent:
                self.test.assertTrue(raw.endswith(b"\0"))
                packet = json.loads(raw[:-1])
                self.history.append((self.now, cid, packet))
                result.append((cid, packet))
        return result

    def flush(self):
        for cid, packet in self.collect():
            for other, client in self.clients.items():
                if cid == other:
                    continue
                if "targetClientId" in packet:
                    if packet["targetClientId"] != other:
                        continue
                elif "targetTeamId" in packet and packet["targetTeamId"] != client._team_id:
                    continue
                self.deliver(client, packet)

    def tick(self, advance=0.0, flush=True):
        self.now += advance
        for cid, client in self.clients.items():
            if not client._connected:
                continue
            self.status[cid] = json.loads(client.update_congo(
                self.ready[cid], self.visit[cid], self.paused[cid], json.dumps(self.states[cid])))
        if flush:
            self.flush()
        return self.status

    def start(self):
        self.tick()
        self.tick(0.36)
        self.tick(0.04)

    def packets(self, op=None):
        return [(t, cid, p) for t, cid, p in self.history
                if p["type"] == "MNSG_CONGO" and (op is None or p["op"] == op)]


class CongoTransportTests(unittest.TestCase):
    bandwidth_measurements = []
    def setUp(self):
        self.net = Relay(self)
        self.a = self.net.add(2)
        self.b = self.net.add(3)

    def test_single_authority_actual_framed_flow_and_codec(self):
        self.net.start()
        self.assertEqual([self.net.status[c]["role"] for c in (2, 3)], [1, 2])
        self.assertEqual(self.net.status[3]["encounter"], [2, 202, 1])
        self.assertEqual(self.net.status[3]["state"], state())
        self.assertEqual({cid for _, cid, _ in self.net.packets("s")}, {2})
        self.assertTrue(all("addToQueue" not in p for _, _, p in self.net.packets()))
        self.assertTrue(all(p.get("quiet") for _, _, p in self.net.packets()))

    def test_changed_state_cap_and_request_flood_coalescing(self):
        self.net.start()
        self.net.history.clear()
        for frame in range(60):
            self.net.states[2] = state(tick=frame + 1)
            request = self.b._congo._packet(self.b._congo_context(), "r", targetClientId=2)
            for _ in range(20):
                self.net.deliver(self.a, request)
            self.net.tick(1 / 60)
        packets = self.net.packets("s")
        self.assertLessEqual(len(packets), 10)
        self.assertTrue(all(b[0] - a[0] >= 0.1 - 1e-8 for a, b in zip(packets, packets[1:])))
        self.assertEqual({cid for _, cid, _ in self.net.packets()}, {2})

    def test_transient_flag_checkpoint_survives_stream_without_extra_packets(self):
        self.net.start()
        self.net.history.clear()
        for frame in range(120):
            checkpoint = state(hp=17, tick=frame + 1)
            # Native status packs observed event B into bit 1, independently
            # of the recovery bit and HP thresholds. Preserve both 2 and 3.
            checkpoint["r"][6] = 2 | ((frame // 12) & 1)
            self.net.states[2] = checkpoint
            self.net.tick(1 / 60)
        packets = self.net.packets()
        self.assertEqual(len(packets), 20)
        self.assertEqual({(cid, p["op"]) for _, cid, p in packets}, {(2, "s")})
        self.assertEqual({p["d"]["r"][6] for _, _, p in packets}, {2, 3})
        latest = packets[-1][2]["d"]
        self.assertEqual(latest["r"][6], 3)
        self.assertEqual(self.b._congo.state, latest)
        revision = self.b._congo.revision
        self.net.deliver(self.b, packets[0][2])
        self.assertEqual(self.b._congo.state, latest)
        self.assertEqual(self.b._congo.revision, revision)
        # The bridge returns the same persistent flag; it is not a separately
        # replayed event or inferred from the deliberately non-threshold HP.
        status = json.loads(self.b.update_congo(1, 1, 0, "null"))
        self.assertEqual(status["state"]["r"][6], 3)
        self.assertEqual(status["state"]["r"][2], 17)

    def test_unchanged_state_keepalive_and_owner_pause(self):
        self.net.start()
        self.net.history.clear()
        self.net.paused[2] = 1
        for _ in range(150):
            self.net.tick(1 / 30)
        self.assertEqual(len(self.net.packets("s")), 1)
        self.assertLessEqual(len(self.net.packets("k")), 5)
        self.assertEqual(self.net.status[3]["paused"], 1)
        self.assertEqual(self.net.status[3]["owner"], 2)
        self.assertEqual(self.net.status[3]["term"], 1)

    def test_solo_has_no_state_stream_and_lower_cid_late_join_is_follower(self):
        self.b.set_local_room(10)
        self.net.ready[3] = False
        self.net.flush()
        self.net.start()
        for frame in range(90):
            self.net.states[2] = state(tick=frame)
            self.net.tick(1 / 30)
        self.assertEqual(self.net.packets("s"), [])
        late = self.net.add(1)
        self.net.tick()
        self.net.tick(0.1)
        self.net.tick(0.1)
        self.assertEqual(self.net.status[1]["owner"], 2)
        self.assertEqual(self.net.status[1]["role"], 2)
        self.assertEqual(self.net.status[1]["state"], state(tick=89))
        self.assertEqual(self.net.status[2]["term"], 1)
        self.assertFalse(late._congo.incoming)

    def test_late_entry_uses_latest_cached_team_state(self):
        self.b.set_local_room(10)
        self.net.ready[3] = False
        peer = self.net.add(4)
        self.net.flush()
        self.net.start()
        self.net.states[2] = state(hp=4, tick=99)
        self.net.tick(0.1)
        self.assertTrue(self.b._congo.cache)
        self.b.set_local_room(22)
        self.net.ready[3] = True
        self.net.visit[3] = 2
        self.net.flush()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["state"], state(hp=4, tick=99))
        self.assertEqual(self.net.status[3]["role"], 2)

    def test_reordered_and_duplicate_snapshots_cannot_rollback(self):
        self.net.start()
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        self.net.states[2] = state(hp=5, tick=4)
        self.net.tick(0.1)
        new = copy.deepcopy(self.net.packets("s")[-1][2])
        revision = self.b._congo.revision
        self.net.deliver(self.b, old)
        self.net.deliver(self.b, new)
        self.assertEqual(self.b._congo.state, state(hp=5, tick=4))
        self.assertEqual(self.b._congo.revision, revision)

    def test_departure_migrates_existing_checkpoint_and_fences_old_owner(self):
        self.net.start()
        self.net.states[2] = state(hp=6, tick=17)
        self.net.tick(0.1)
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["role"], 1)
        self.assertEqual(self.net.status[3]["term"], 2)
        self.assertEqual(self.net.status[3]["encounter"], [2, 202, 1])
        self.assertEqual(self.net.status[3]["state"], state(hp=6, tick=17))
        self.net.deliver(self.b, old)
        self.assertEqual(self.b._congo.owner, 3)

    def test_stalled_owner_migrates_after_lease_not_while_keepalive_arrives(self):
        self.net.start()
        self.net.now += 3.1
        result = json.loads(self.b.update_congo(1, 1, 0, json.dumps(state(hp=12))))
        self.assertEqual(result["role"], 1)
        self.assertEqual(result["term"], 2)
        self.assertEqual(result["encounter"], [2, 202, 1])

    def test_two_client_stalled_owner_receives_successor_and_converges(self):
        self.net.start()
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        self.net.now += 3.1
        successor = json.loads(self.b.update_congo(1, 1, 0, "null"))
        self.assertEqual(successor["role"], 1)
        self.net.flush()
        resumed = json.loads(self.a.update_congo(1, 1, 0, json.dumps(state(tick=999))))
        self.assertEqual(resumed["role"], 2)
        self.assertEqual(resumed["owner"], 3)
        self.assertEqual(resumed["term"], 2)
        self.net.deliver(self.b, old)
        self.assertEqual(self.b._congo.owner, 3)
        self.assertEqual(self.b._congo.term, 2)

    def test_same_room_roster_refresh_preserves_role_and_new_native_visit_joins(self):
        self.net.start()
        before = self.a._congo.e
        self.net.roster()
        self.a._local_room_id = -1
        self.a.set_local_room(22)
        self.net.tick(0.01)
        self.assertEqual(self.a._congo.e, before)
        self.assertEqual(self.net.status[2]["role"], 1)
        self.net.visit[3] = 2
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["encounter"], list(before))
        self.assertEqual(self.net.status[3]["role"], 2)

    def test_invalid_schema_never_changes_checkpoint_or_acks(self):
        self.net.start()
        original = copy.deepcopy(self.net.packets("s")[-1][2])
        bad_values = [[], {}, {**state(), "extra": 1}, {**state(), "r": [0] * 23},
                      {**state(), "p": [True] * 24}, {**state(), "s": 0x100000000},
                      {**state(), "t": float("nan")}, {**state(), "f": [0] * 8},
                      {**state(), "f": [0] * 231}]
        for bad in bad_values:
            packet = copy.deepcopy(original)
            packet.update(q=99, d=bad, a=[[3, 303, 7, 9]])
            self.net.deliver(self.b, packet)
            self.assertEqual(self.b._congo.state, state())
            self.assertEqual(self.b._congo.acks, {})

    def test_hits_ack_only_after_next_native_state_and_retry_dedup(self):
        self.net.start()
        self.assertTrue(self.b.send_congo_hit(1, 2))
        self.net.tick(0.1)
        hit_packet = self.net.packets("h")[-1][2]
        self.net.deliver(self.a, hit_packet)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 2]])
        self.assertEqual(self.a._congo.acks, {})
        self.net.states[2] = state(hp=10)
        self.net.deliver(self.a, hit_packet)
        self.net.tick(0.1)
        self.assertEqual(self.net.status[2]["hits"], [])
        self.assertEqual(self.a._congo.acks[(3, 303, 7)], 1)
        self.assertFalse(self.b._congo.outgoing)
        self.net.deliver(self.a, hit_packet)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[2]["hits"], [])

    def test_paused_owner_defers_hit_work_and_invalid_local_state_does_not_ack(self):
        self.net.start()
        self.net.paused[2] = 1
        self.assertTrue(self.b.send_congo_hit(1, 1))
        self.net.tick(0.1)
        self.net.tick(0.1)
        self.assertEqual(self.net.status[2]["hits"], [])
        self.net.paused[2] = 0
        self.net.tick(0.1)
        self.assertEqual(len(self.net.status[2]["hits"]), 1)
        self.net.states[2] = {}
        self.net.tick(0.1)
        self.assertEqual(self.a._congo.acks, {})
        self.assertTrue(self.a._congo.delivered)

    def test_hit_ack_checkpoint_survives_migration(self):
        self.net.start()
        self.assertTrue(self.b.send_congo_hit(1, 1))
        self.net.tick(0.1)
        old_hit = copy.deepcopy(self.net.packets("h")[-1][2])
        self.net.tick(0.01)
        self.net.states[2] = state(hp=11)
        self.net.tick(0.1)
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["state"], state(hp=11))
        self.assertEqual(self.b._congo.acks[(3, 303, 7)], 1)
        self.assertTrue(self.b.send_congo_hit(1, 1))
        self.net.tick(0.1)
        self.assertEqual(self.net.status[3]["hits"], [])

    def test_team_save_unload_and_disconnect_clear_local_encounter(self):
        self.net.start()
        self.b.set_save_loaded(False)
        self.assertIsNone(self.b._congo.e)
        self.assertFalse(self.b._congo.outgoing)
        self.a.set_team("red")
        self.assertIsNone(self.a._congo.e)
        self.a.disconnect()
        self.assertIsNone(self.a._congo.e)
        self.assertFalse(self.a._congo.cache)

    def test_wrong_team_session_room_and_unknown_sender_rejected(self):
        self.net.start()
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        packet["q"] += 1
        for changes in ({"clientId": 999}, {"targetTeamId": "red"}, {"session": 999},
                        {"clientId": 3}, {"targetClientId": 99}):
            self.net.deliver(self.b, {**packet, **changes})
            self.assertEqual(self.b._congo.sequence, 1)
        self.b._merge_client_state(2, {"currentRoomId": 10})
        self.net.deliver(self.b, packet)
        self.assertEqual(self.b._congo.sequence, 1)

    def test_snapshot_before_ready_metadata_is_deferred_and_not_enqueued(self):
        self.net.start()
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        packet["q"] = 2
        packet["d"] = state(hp=7)
        source = self.b._player_states[2]
        saved = source.pop("mnsgCongo")
        self.net.deliver(self.b, packet)
        self.assertEqual(self.b._congo.state, state())
        self.assertIn(2, self.b._congo.pending_packets)
        source["mnsgCongo"] = saved
        self.net.tick(0.01)
        self.assertEqual(self.b._congo.state, state(hp=7))
        self.assertFalse(self.b._congo.pending_packets)
        while self.b.has_packet():
            self.assertNotEqual(json.loads(self.b.poll_packet())["type"], "MNSG_CONGO")

    def test_old_native_visit_snapshot_rejected_after_same_connection_reentry(self):
        self.net.start()
        old = copy.deepcopy(self.net.packets("s")[-1][2])
        old["q"] = 999
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        self.a.set_local_room(22)
        self.net.ready[2] = True
        self.net.visit[2] = 2
        # Even without a follower game frame between exit/reentry, the receive
        # path retains the departure edge and old packets keep their visit.
        self.a.update_congo(1, 2, 0, json.dumps(state()))
        self.net.flush()
        previous = self.b._congo.sequence
        self.net.deliver(self.b, old)
        self.assertEqual(self.b._congo.sequence, previous)

    def test_queued_hit_is_discarded_after_source_leaves_or_epoch_changes(self):
        for edge in ("room", "epoch"):
            with self.subTest(edge=edge):
                self.a._congo.incoming.clear()
                self.net.start()
                self.assertTrue(self.b.send_congo_hit(1, 2))
                self.net.tick(0.1)
                if edge == "room":
                    self.b.set_local_room(10)
                    self.net.flush()
                else:
                    self.a._player_states[3]["playerEpoch"] = 8
                result = json.loads(self.a.update_congo(1, 1, 0, json.dumps(state())))
                self.assertEqual(result["hits"], [])
                if edge == "room":
                    self.b.set_local_room(22)
                    self.net.visit[3] += 1
                    self.net.flush()

    def test_takeover_null_or_stale_input_never_overwrites_checkpoint(self):
        self.net.start()
        self.net.states[2] = state(hp=3, tick=60)
        self.net.tick(0.1)
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        before = self.b._congo.revision
        result = json.loads(self.b.update_congo(1, 1, 0, json.dumps(state(hp=12))))
        self.assertEqual(result["state"], state(hp=3, tick=60))
        self.assertGreater(result["revision"], before)
        self.assertEqual(result["hits"], [])
        for _ in range(3):
            self.net.now += 0.1
            result = json.loads(self.b.update_congo(1, 1, 0, "null"))
            self.assertEqual(result["state"], state(hp=3, tick=60))
            self.assertEqual(result["hits"], [])
        result = json.loads(self.b.update_congo(1, 1, 0, json.dumps(state(hp=3, tick=61))))
        self.assertEqual(result["state"], state(hp=3, tick=61))

    def test_metadata_retained_by_character_update_and_stale_snapshot(self):
        self.net.start()
        current = list(self.b._player_states[2]["mnsgCongo"])
        self.a.set_character("Goemon")
        self.net.flush()
        self.assertEqual(self.b._player_states[2]["mnsgCongo"], current)
        stale = list(current)
        stale[1], stale[4], stale[6:13] = 0, 0, [0] * 7
        self.b._merge_client_state(2, {"mnsgCongo": stale})
        self.assertEqual(self.b._player_states[2]["mnsgCongo"], current)
        self.assertTrue(self.b._congo._peer(self.b._congo_context(), 2))

    def test_many_players_still_have_one_snapshot_publisher(self):
        for cid in range(4, 37):
            self.net.add(cid)
        self.net.start()
        for frame in range(6):
            self.net.states[2] = state(tick=frame)
            self.net.tick(0.1)
        self.assertEqual(sum(s["role"] == 1 for s in self.net.status.values()), 1)
        self.assertEqual(sum(s["role"] == 2 for s in self.net.status.values()), 34)
        self.assertEqual({cid for _, cid, _ in self.net.packets("s")}, {2})
        self.assertLessEqual(len(self.net.packets("s")), 7)

    def test_intro_readiness_does_not_elect_an_owner_without_native_capture(self):
        self.net.states[2] = None
        self.net.states[3] = None
        for _ in range(150):
            self.net.tick(1 / 30)
        self.assertEqual({s["role"] for s in self.net.status.values()}, {0})
        self.assertEqual({s["owner"] for s in self.net.status.values()}, {0})
        self.assertEqual(self.net.packets(), [])
        self.net.states[2] = state()
        self.net.states[3] = state()
        self.net.tick()
        self.net.tick(0.36)
        self.net.tick(0.01)
        self.assertEqual([self.net.status[c]["role"] for c in (2, 3)], [1, 2])

    def test_intro_late_join_can_adopt_active_fight_without_local_capture(self):
        self.net.start()
        self.net.states[2] = state(hp=7, tick=80)
        self.net.tick(0.1)
        late = self.net.add(1)
        self.net.states[1] = None
        self.net.tick()
        self.net.tick(0.1)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[1]["role"], 2)
        self.assertEqual(self.net.status[1]["state"], state(hp=7, tick=80))
        self.assertEqual(self.net.status[1]["owner"], 2)
        self.assertFalse(late._congo.capture_ready)

    def test_damage_whitelist_and_maximum_status_fit_native_codec(self):
        self.net.start()
        for amount in (0, 5, 6, 7, 9, 255, True, 1.0):
            self.assertFalse(self.b.send_congo_hit(1, amount))
            packet = self.b._congo._packet(self.b._congo_context(), "h", targetClientId=2,
                                           h=[7, 1, amount])
            self.net.deliver(self.a, packet)
        self.assertFalse(self.a._congo.incoming)
        maximal = flame_state(flames=32)
        self.assertIsNotNone(anchor_congo.validate_state(maximal))
        self.net.states[2] = maximal
        self.net.tick(0.1)
        status = copy.deepcopy(self.net.status[2])
        status["hits"] = [[0x7fffffff] * 4 + [8] for _ in range(32)]
        encoded = json.dumps(status, separators=(",", ":"))
        self.assertLess(len(encoded.encode()), 8192)
        self.assertEqual(set(status), {"role", "owner", "term", "revision", "paused", "encounter", "state", "hits"})
        self.assertIs(type(status["paused"]), int)

    def test_ack_and_visit_history_is_bounded_by_current_members(self):
        self.net.start()
        for epoch in range(8, 108):
            self.a._congo.acks[(3, 303, epoch - 1)] = 2
            self.a._player_states[3]["playerEpoch"] = epoch
            self.b._player_states[3]["playerEpoch"] = epoch
            self.net.tick(0.01)
            self.assertLessEqual(len(self.a._congo.acks), 1)
        self.assertEqual(self.a._congo.acks, {})
        for visit in range(2, 102):
            self.a._congo._retire((3, 303, visit))
            self.a._congo.unavailable[(3, 303)] = visit
        self.assertEqual(len(self.a._congo.retired), 1)
        self.assertEqual(len(self.a._congo.unavailable), 1)

    def test_traffic_bytes_do_not_multiply_publishers_with_player_count(self):
        measurements = []
        for count in (7, 35):
            net = Relay(self)
            for cid in range(1, count + 1):
                net.add(cid)
                net.states[cid] = flame_state()
            net.start()
            net.history.clear()
            for frame in range(60):
                net.states[1] = flame_state(tick=91 + frame)
                net.tick(1 / 30)
            packets = net.packets()
            byte_count = sum(len((json.dumps(p, separators=(",", ":")) + "\0").encode()) for _, _, p in packets)
            measurements.append({"players": count, "seconds": 2,
                                 "packets": len(packets), "upstream_bytes": byte_count})
            self.assertEqual({cid for _, cid, _ in packets}, {1})
            self.assertLessEqual(len(packets), 20)
            self.assertEqual({p["op"] for _, _, p in packets}, {"s"})
        self.assertEqual(measurements[0]["upstream_bytes"], measurements[1]["upstream_bytes"])
        type(self).bandwidth_measurements = measurements


if __name__ == "__main__":
    unittest.main()
