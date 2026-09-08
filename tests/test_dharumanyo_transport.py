"""Framed Dharumanyo coordination tests without a game or live server."""

import copy
import json
import unittest
from unittest import mock

from test_boss_invitation_transport import RecordingSocket, load_client
import anchor_congo
import anchor_dharumanyo


def state(lives=12, tick=0, projectiles=0, fill=0):
    root = [fill] * anchor_dharumanyo.ROOT_WORDS
    carrier = [fill] * anchor_dharumanyo.CARRIER_WORDS
    root[0:4] = [1, tick, 0x00CC, 0x08003810]
    carrier[0:4] = [lives, 0, 0x00CC, 0x08003F84]
    records = []
    for index in range(projectiles):
        records.extend([
            index + 1, max(1, tick - index), index & 1,
            0x3F800000 + index, 0xC28C0000, 0x430C0000,
            index * 29, 0x43960000, 0xC3960000, 0, index,
        ])
    return {
        "r": root,
        "c": carrier,
        "s": projectiles,
        "t": tick,
        "p": records,
    }


class Relay:
    """Anchor's transient team/direct routing with controlled delivery."""

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

    def add(self, cid, room=anchor_dharumanyo.ROOM, team="blue", session=None):
        client = load_client(cid, session or cid * 101, team, room)
        self.test.addCleanup(client.disconnect)
        self.clients[cid] = client
        self.states[cid] = state()
        self.ready[cid] = room == anchor_dharumanyo.ROOM
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
                context = client._boss_context()
                members.append({
                    "clientId": cid,
                    "self": receiver is client,
                    "name": client._player_name,
                    "online": True,
                    "teamId": client._team_id,
                    "isSaveLoaded": client._local_save_loaded,
                    "currentRoomId": local.get("roomId", client._local_room_id),
                    "interactionSession": client._interaction_session,
                    anchor_congo.METADATA_KEY:
                        client._congo.advertisement(context),
                    anchor_dharumanyo.METADATA_KEY:
                        client._dharumanyo.advertisement(context),
                })
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
                elif ("targetTeamId" in packet and
                      packet["targetTeamId"] != client._team_id):
                    continue
                self.deliver(client, packet)

    def tick(self, advance=0.0, flush=True):
        self.now += advance
        for cid, client in self.clients.items():
            if not client._connected:
                continue
            self.status[cid] = json.loads(client.update_dharumanyo(
                self.ready[cid], self.visit[cid], self.paused[cid],
                json.dumps(self.states[cid]) if self.states[cid] is not None
                else "null",
            ))
        if flush:
            self.flush()
        return self.status

    def start(self):
        self.tick()
        self.tick(0.36)
        self.tick(0.04)

    def packets(self, op=None):
        return [
            (stamp, cid, packet)
            for stamp, cid, packet in self.history
            if (packet["type"] == anchor_dharumanyo.PACKET_TYPE and
                (op is None or packet["op"] == op))
        ]


class DharumanyoTransportTests(unittest.TestCase):
    bandwidth_measurements = []

    def setUp(self):
        self.net = Relay(self)
        self.a = self.net.add(2)
        self.b = self.net.add(3)

    def test_exact_checkpoint_schema_and_maximum_projectiles(self):
        maximal = state(
            lives=0xFFFFFFFF,
            tick=0xFFFFFFFF,
            projectiles=anchor_dharumanyo.MAX_PROJECTILES,
            fill=0xFFFFFFFF,
        )
        self.assertIs(anchor_dharumanyo.validate_state(maximal), maximal)
        self.assertEqual(len(maximal["r"]), 29)
        self.assertEqual(len(maximal["c"]), 9)
        self.assertEqual(len(maximal["p"]), 16 * 11)
        self.assertLessEqual(
            len(json.dumps(maximal, separators=(",", ":")).encode()),
            anchor_dharumanyo.MAX_STATE_BYTES,
        )
        bad = [
            None,
            [],
            {},
            {**state(), "extra": 1},
            {**state(), "r": [0] * 28},
            {**state(), "r": [0] * 30},
            {**state(), "c": [0] * 8},
            {**state(), "c": [0] * 10},
            {**state(), "p": [0]},
            {**state(), "p": [0] * (16 * 11 + 11)},
            {**state(), "r": [True] + [0] * 28},
            {**state(), "c": [-1] + [0] * 8},
            {**state(), "s": 0x100000000},
            {**state(), "t": 1.0},
        ]
        for value in bad:
            with self.subTest(value=value):
                self.assertIsNone(anchor_dharumanyo.validate_state(value))
        advertisement = [1, 1, 1, 0, 1, 101, 0, 0, 0, 0, 0, 0, 0, 1]
        self.assertEqual(anchor_dharumanyo.metadata(advertisement), advertisement)
        self.assertIsNone(anchor_dharumanyo.metadata([2, *advertisement[1:]]))

    def test_framed_election_uses_distinct_versioned_transient_packets(self):
        self.net.start()
        self.assertEqual([self.net.status[c]["role"] for c in (2, 3)], [1, 2])
        self.assertEqual(self.net.status[3]["encounter"], [2, 202, 1])
        self.assertEqual(self.net.status[3]["state"], state())
        packets = self.net.packets()
        self.assertEqual({cid for _, cid, _ in packets}, {2})
        self.assertTrue(all(packet["v"] == 1 for _, _, packet in packets))
        self.assertTrue(all(packet["targetTeamId"] == "blue"
                            for _, _, packet in packets))
        self.assertTrue(all("addToQueue" not in packet
                            for _, _, packet in packets))
        self.assertTrue(all(packet.get("quiet") for _, _, packet in packets))
        while self.b.has_packet():
            self.assertNotEqual(
                json.loads(self.b.poll_packet())["type"],
                anchor_dharumanyo.PACKET_TYPE,
            )

    def test_boss_metadata_coexists_and_transport_state_is_independent(self):
        self.net.start()
        congo_before = self.a._congo.advertisement(self.a._boss_context())
        self.assertIsNone(self.a._congo.e)
        self.assertIsNotNone(self.a._dharumanyo.e)
        self.a.update_client_state("{}")
        packet = json.loads(self.a._sock.sent[-1][:-1])
        metadata = packet["state"]
        self.assertEqual(
            anchor_congo.metadata(metadata[anchor_congo.METADATA_KEY]),
            congo_before,
        )
        self.assertIsNotNone(anchor_dharumanyo.metadata(
            metadata[anchor_dharumanyo.METADATA_KEY]
        ))
        self.assertIsNone(self.a._congo.e)
        self.assertIsNotNone(self.a._dharumanyo.e)

    def test_late_join_requests_and_adopts_latest_checkpoint(self):
        self.net.start()
        self.net.states[2] = state(lives=7, tick=80, projectiles=3)
        self.net.tick(0.1)
        late = self.net.add(4)
        self.net.states[4] = None
        for advance in (0.0, 0.1, 0.1):
            self.net.tick(advance)
        self.assertEqual(self.net.status[4]["role"], 2)
        self.assertEqual(self.net.status[4]["owner"], 2)
        self.assertEqual(
            self.net.status[4]["state"],
            state(lives=7, tick=80, projectiles=3),
        )
        self.assertFalse(late._dharumanyo.capture_ready)

    def test_departure_and_lease_expiry_handoff_existing_checkpoint(self):
        self.net.start()
        checkpoint = state(lives=8, tick=45, projectiles=2)
        self.net.states[2] = checkpoint
        self.net.tick(0.1)
        self.a.set_local_room(10)
        self.net.ready[2] = False
        self.net.flush()
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["role"], 1)
        self.assertEqual(self.net.status[3]["term"], 2)
        self.assertEqual(self.net.status[3]["state"], checkpoint)

        other = Relay(self)
        owner = other.add(2)
        follower = other.add(3)
        other.start()
        other.now += anchor_dharumanyo.LEASE + 0.1
        result = json.loads(follower.update_dharumanyo(
            1, 1, 0, json.dumps(state())
        ))
        self.assertEqual(result["role"], 1)
        self.assertEqual(result["term"], 2)
        self.assertEqual(result["encounter"], [2, 202, 1])
        self.assertIsNotNone(owner._dharumanyo.e)

    def test_wrong_type_version_identity_team_session_and_room_are_rejected(self):
        self.net.start()
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        packet["q"] += 1
        previous = self.b._dharumanyo.sequence
        mutations = (
            {"type": anchor_congo.PACKET_TYPE},
            {"v": 2},
            {"clientId": 999},
            {"targetTeamId": "red"},
            {"session": 999},
            {"clientId": 3},
            {"targetClientId": 99},
        )
        for changes in mutations:
            with self.subTest(changes=changes):
                self.net.deliver(self.b, {**packet, **changes})
                self.assertEqual(self.b._dharumanyo.sequence, previous)
        self.b._merge_client_state(2, {"currentRoomId": 10})
        self.net.deliver(self.b, packet)
        self.assertEqual(self.b._dharumanyo.sequence, previous)

    def test_unit_hit_is_direct_retried_deduplicated_and_acked(self):
        self.net.start()
        self.assertTrue(self.b.send_dharumanyo_hit(1))
        self.assertFalse(self.b._dharumanyo.send_hit(
            self.b._boss_context(), 2, 2, self.net.now + 0.1
        ))
        self.net.tick(0.1)
        hit_packet = self.net.packets("h")[-1][2]
        self.assertEqual(hit_packet["targetClientId"], 2)
        self.assertEqual(hit_packet["h"], [7, 1, 1])
        self.net.deliver(self.a, hit_packet)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 1]])
        self.assertEqual(self.a._dharumanyo.acks, {})
        self.net.deliver(self.a, hit_packet)
        self.net.states[2] = state(lives=11, tick=1)
        self.net.tick(0.1)
        self.assertEqual(self.a._dharumanyo.acks[(3, 303, 7)], 1)
        self.assertFalse(self.b._dharumanyo.outgoing)
        self.net.deliver(self.a, hit_packet)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[2]["hits"], [])

    def test_state_rejects_more_than_one_bounded_ack_slice(self):
        self.net.start()
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        previous_sequence = self.b._dharumanyo.sequence
        previous_state = copy.deepcopy(self.b._dharumanyo.state)
        previous_acks = dict(self.b._dharumanyo.acks)
        packet.update(
            q=previous_sequence + 1,
            d=state(lives=1, tick=999),
            a=[[3, 303, 7, index + 1]
               for index in range(anchor_dharumanyo.ACK_ROWS + 1)],
        )
        self.net.deliver(self.b, packet)
        self.assertEqual(self.b._dharumanyo.sequence, previous_sequence)
        self.assertEqual(self.b._dharumanyo.state, previous_state)
        self.assertEqual(self.b._dharumanyo.acks, previous_acks)

    def test_failed_state_send_retries_without_committing_ack_or_cadence(self):
        self.net.start()
        key = (3, 303, 7)
        self.a._dharumanyo.acks[key] = 4
        self.a._dharumanyo.ack_pending.add(key)
        self.a._dharumanyo.ack_revision += 1
        self.a._dharumanyo.advertisement_dirty = False
        published = self.a._dharumanyo.published
        last_wire = self.a._dharumanyo.last_wire
        self.net.now += anchor_dharumanyo.INTERVAL

        with mock.patch.object(self.a, "_send_raw", return_value=False) as send:
            self.a.update_dharumanyo(1, 1, 0, json.dumps(state(tick=4)))
        failed_packet = send.call_args.args[0]
        self.assertEqual(failed_packet["a"], [[3, 303, 7, 4]])
        self.assertIn(key, self.a._dharumanyo.ack_pending)
        self.assertEqual(self.a._dharumanyo.published, published)
        self.assertEqual(self.a._dharumanyo.last_wire, last_wire)

        with mock.patch.object(self.a, "_send_raw", return_value=True) as send:
            self.a.update_dharumanyo(1, 1, 0, json.dumps(state(tick=4)))
        retried_packet = send.call_args.args[0]
        self.assertEqual(retried_packet["a"], failed_packet["a"])
        self.assertGreater(retried_packet["q"], failed_packet["q"])
        self.assertNotIn(key, self.a._dharumanyo.ack_pending)
        self.assertEqual(self.a._dharumanyo.last_wire, self.net.now)

    def test_large_roster_rotates_ack_history_inside_hot_packet_budget(self):
        transport = anchor_dharumanyo.DharumanyoTransport()
        encounter = (1, 101, 1)
        players = {
            1: {
                "online": True, "isSaveLoaded": True, "teamId": "blue",
                "roomId": anchor_dharumanyo.ROOM,
                "interactionSession": 101, "playerEpoch": 7,
            },
        }
        ack_values = {}
        for index in range(512):
            cid = 1_000_000_000 + index
            session = 1_500_000_000 + index
            epoch = 2_000_000_000 + index
            players[cid] = {
                "online": True, "isSaveLoaded": True, "teamId": "blue",
                "roomId": anchor_dharumanyo.ROOM,
                "interactionSession": session, "playerEpoch": epoch,
                anchor_dharumanyo.METADATA_KEY: [
                    1, 1, 1, 0, 1, session,
                    0, 0, 0, 0, 0, 0, 0, 0,
                ],
            }
            ack_values[(cid, session, epoch)] = 0x7fffffff
        context = {
            "cid": 1, "session": 101, "team": "blue",
            "connected": True, "loaded": True,
            "room": anchor_dharumanyo.ROOM, "players": players,
        }
        maximal = state(
            lives=0xffffffff, tick=0xffffffff,
            projectiles=anchor_dharumanyo.MAX_PROJECTILES,
            fill=0xffffffff,
        )
        transport.scope = (101, "blue")
        transport.local = (True, 1, False)
        transport.e = encounter
        transport.term = 1
        transport.owner = 1
        transport.owner_session = 101
        transport.owner_visit = 1
        transport.role = 1
        transport.state = maximal
        transport.capture_ready = True
        transport.acks = dict(ack_values)
        transport.ack_pending = set(ack_values)
        transport.ack_revision = 1

        now = 100.0
        sent_keys = set()
        state_packets = []
        while transport.ack_pending:
            _, packets = transport.update(
                context, True, 1, False, maximal, now
            )
            self.assertEqual(len(packets), 1)
            packet = packets[0]
            self.assertEqual(packet["op"], "s")
            self.assertLessEqual(len(packet["a"]), anchor_dharumanyo.ACK_ROWS)
            self.assertLessEqual(
                len((json.dumps(packet, separators=(",", ":")) + "\0").encode()),
                8 * 1024,
            )
            sent_keys.update(tuple(row[:3]) for row in packet["a"])
            state_packets.append(packet)
            transport.packet_send_result(packet, True)
            now += anchor_dharumanyo.INTERVAL

        self.assertEqual(sent_keys, set(ack_values))
        self.assertEqual(len(state_packets), 8)
        _, packets = transport.update(context, True, 1, False, maximal, now)
        self.assertEqual(packets, [])

        requester_key = next(iter(ack_values))
        requester = requester_key[0]
        request = {
            "type": anchor_dharumanyo.PACKET_TYPE,
            "v": anchor_dharumanyo.VERSION,
            "clientId": requester,
            "targetClientId": 1,
            "targetTeamId": "blue",
            "session": requester_key[1],
            "op": "r",
            "e": list(encounter),
            "term": 1,
            "visit": 1,
            "quiet": True,
        }
        replayed = set()
        for _ in range(8):
            self.assertTrue(transport.receive(context, request, now - 0.01))
            _, packets = transport.update(
                context, True, 1, False, maximal, now
            )
            self.assertEqual(len(packets), 1)
            packet = packets[0]
            replayed.update(tuple(row[:3]) for row in packet["a"])
            transport.packet_send_result(packet, True)
            now += anchor_dharumanyo.INTERVAL
        self.assertEqual(replayed, set(ack_values))

    def test_ack_slices_merge_and_prevent_replay_after_owner_handoff(self):
        transport = anchor_dharumanyo.DharumanyoTransport()
        encounter = (1, 101, 1)
        players = {
            1: {
                "online": True, "isSaveLoaded": True, "teamId": "blue",
                "roomId": anchor_dharumanyo.ROOM,
                "interactionSession": 101, "playerEpoch": 7,
                anchor_dharumanyo.METADATA_KEY: [
                    1, 1, 1, 0, 1, 101,
                    *encounter, 1, 1, 101, 1, 1,
                ],
            },
            2: {
                "online": True, "isSaveLoaded": True, "teamId": "blue",
                "roomId": anchor_dharumanyo.ROOM,
                "interactionSession": 202, "playerEpoch": 7,
            },
        }
        ack_rows = []
        for cid in range(3, 133):
            session = cid * 101
            epoch = 7
            players[cid] = {
                "online": True, "isSaveLoaded": True, "teamId": "blue",
                "roomId": anchor_dharumanyo.ROOM,
                "interactionSession": session, "playerEpoch": epoch,
                anchor_dharumanyo.METADATA_KEY: [
                    1, 1, 1, 0, 1, session,
                    0, 0, 0, 0, 0, 0, 0, 0,
                ],
            }
            ack_rows.append([cid, session, epoch, 1])
        context = {
            "cid": 2, "session": 202, "team": "blue",
            "connected": True, "loaded": True,
            "room": anchor_dharumanyo.ROOM, "players": players,
        }
        transport._scope(context)
        transport.local = (True, 1, False)
        for index in range(0, len(ack_rows), anchor_dharumanyo.ACK_ROWS):
            packet = {
                "type": anchor_dharumanyo.PACKET_TYPE,
                "v": anchor_dharumanyo.VERSION,
                "clientId": 1,
                "targetTeamId": "blue",
                "session": 101,
                "op": "s",
                "e": list(encounter),
                "term": 1,
                "visit": 1,
                "quiet": True,
                "q": index // anchor_dharumanyo.ACK_ROWS + 1,
                "p": 0,
                "d": state(tick=index),
                "a": ack_rows[index:index + anchor_dharumanyo.ACK_ROWS],
            }
            self.assertTrue(transport.receive(context, packet, 100.0 + index))

        self.assertEqual(len(transport.acks), len(ack_rows))
        players[1]["online"] = False
        status, packets = transport.update(
            context, True, 1, False, state(tick=200), 300.0
        )
        self.assertEqual((status["role"], status["owner"], status["term"]),
                         (1, 2, 2))
        self.assertEqual(len(transport.ack_pending), len(ack_rows))
        for packet in packets:
            transport.packet_send_result(packet, False)

        old_hit = {
            "type": anchor_dharumanyo.PACKET_TYPE,
            "v": anchor_dharumanyo.VERSION,
            "clientId": 3,
            "targetClientId": 2,
            "targetTeamId": "blue",
            "session": 303,
            "op": "h",
            "e": list(encounter),
            "term": 2,
            "visit": 1,
            "quiet": True,
            "h": [7, 1, 1],
        }
        self.assertTrue(transport.receive(context, old_hit, 300.01))
        self.assertFalse(transport.incoming)

    def test_paused_owner_defers_hit_until_resumed(self):
        self.net.start()
        self.net.paused[2] = 1
        self.assertTrue(self.b.send_dharumanyo_hit(1))
        self.net.tick(0.1)
        self.net.tick(0.1)
        self.assertEqual(self.net.status[2]["hits"], [])
        self.assertEqual(self.net.status[3]["paused"], 1)
        self.net.paused[2] = 0
        self.net.tick(0.1)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 1]])

    def test_save_unload_team_change_room_leave_and_disconnect_reset(self):
        self.net.start()
        self.b.set_save_loaded(False)
        self.assertIsNone(self.b._dharumanyo.e)
        self.assertFalse(self.b._dharumanyo.outgoing)
        self.a.set_team("red")
        self.assertIsNone(self.a._dharumanyo.e)
        self.a.disconnect()
        self.assertIsNone(self.a._dharumanyo.e)
        self.assertFalse(self.a._dharumanyo.cache)

    def test_traffic_is_one_ten_hz_publisher_at_seven_and_35_players(self):
        measurements = []
        for count in (7, 35):
            net = Relay(self)
            for cid in range(1, count + 1):
                net.add(cid)
                net.states[cid] = state(projectiles=16)
            net.start()
            net.history.clear()
            for frame in range(60):
                net.states[1] = state(
                    lives=12, tick=frame + 1, projectiles=16
                )
                net.tick(1 / 30)
            packets = net.packets()
            sizes = [
                len((json.dumps(packet, separators=(",", ":")) + "\0").encode())
                for _, _, packet in packets
            ]
            measurements.append({
                "players": count,
                "seconds": 2,
                "packets": len(packets),
                "upstream_bytes": sum(sizes),
            })
            self.assertEqual({cid for _, cid, _ in packets}, {1})
            self.assertLessEqual(len(packets), 20)
            self.assertTrue(all(
                size <= 8 * 1024 for size in sizes
            ))
            self.assertEqual({packet["op"] for _, _, packet in packets}, {"s"})
        self.assertEqual(
            measurements[0]["upstream_bytes"],
            measurements[1]["upstream_bytes"],
        )
        type(self).bandwidth_measurements = measurements


if __name__ == "__main__":
    unittest.main()
