"""Scope, framing and lifecycle tests for transient Impact presentation."""

import copy
import json
import unittest

import anchor_impact as impact


def advertisement(cid, *, stage=0x260, boss=1, visit=1, ready=1):
    return [impact.VERSION, ready, visit, 0, 1, cid * 100,
            0, 0, 0, 0, 0, 0, 0, 1, stage, boss]


def context(cid, count=3):
    return {"cid": cid, "session": cid * 100, "team": "default",
            "connected": True, "loaded": False, "room": 0x1D1,
            "players": {
                other: {"online": True, "teamId": "default",
                        "interactionSession": other * 100, "playerEpoch": 7,
                        "roomId": 0x1D1, "isSaveLoaded": False,
                        impact.METADATA_KEY: advertisement(other)}
                for other in range(1, count + 1)}}


def sample(visible=1, aim=None, attacks=()):
    return {"c": [visible, *(aim if aim is not None else [0x3F800000] * 3 + [0, 512, 0])],
            "a": [[seq, kind, *([0x40000000] * 6 if kind == 1 else [0x2000, 0x2000, 0, 512, 1, 1])] for seq, kind in attacks]}


class ImpactScopeTests(unittest.TestCase):
    def test_exact_native_stage_set(self):
        for stage in [*range(0x21C, 0x224), *range(0x239, 0x23D), 0x260]:
            self.assertTrue(impact._impact_stage(stage), hex(stage))
        for stage in [0, True, 0x21B, *range(0x224, 0x239), 0x23D, 0x261]:
            self.assertFalse(impact._impact_stage(stage), stage)

    def test_all_operations_and_election_require_stage_and_boss(self):
        transport = impact.ImpactTransport()
        transport.set_encounter(0x260, 1)
        ctx = context(1)
        transport.local = (True, 1, False)
        for stage, boss in [(0x220, 1), (0x260, 2)]:
            ctx["players"][2][impact.METADATA_KEY] = advertisement(
                2, stage=stage, boss=boss)
            self.assertIsNone(transport._peer(ctx, 2))
            self.assertNotIn(2, transport._eligible(ctx))
            for op in ["r", "s", "h", "k"]:
                packet = {"type": impact.PACKET_TYPE, "v": impact.VERSION,
                          "clientId": 2, "targetTeamId": "default",
                          "session": 200, "epoch": 7, "visit": 1,
                          "s": stage, "k": boss, "op": op}
                self.assertFalse(transport.receive(ctx, packet, 10.0))
        self.assertEqual(transport.pending_packets, {})

    def test_scope_reset_clears_every_encounter_cache_and_metadata_advances(self):
        transport = impact.ImpactTransport()
        ctx = context(1)
        transport.set_encounter(0x260, 1)
        transport.local = (True, 1, False)
        old = transport.advertisement(ctx)
        transport.state = {"old": True}
        transport.cache[(1, 100, 1)] = {}
        transport.outgoing.append([1, 100, 7, 1, 10])
        transport.pending_packets[2] = ({}, 1)
        transport.set_encounter(0x260, 2)
        new = transport.advertisement(ctx)
        self.assertGreater(new[4], old[4])
        self.assertEqual(impact.merge_metadata(old, new, 100), new)
        self.assertIsNone(transport.state)
        self.assertFalse(transport.cache or transport.outgoing or transport.pending_packets)
        transport.set_encounter(0x221, 2)
        self.assertEqual(transport.room, 0x221)
        transport.set_encounter(0x225, 2)
        self.assertEqual(transport.advertisement(ctx)[14:], [0, 0])

    def test_local_capture_cannot_elect_from_another_encounter(self):
        transport = impact.ImpactTransport()
        transport.set_encounter(0x260, 1)
        supplied = {"s": 0x260, "k": 2, "r": [1] * impact.ROOT_WORDS}
        for now in [1, 2, 3]:
            status, packets = transport.update(context(1), 1, 1, 0, supplied, now)
            self.assertEqual(status["role"], 0)
            self.assertFalse(packets)
        self.assertFalse(transport.capture_ready)


class ImpactPlayerTests(unittest.TestCase):
    def setUp(self):
        self.sender = impact.ImpactPlayerTransport()
        self.receiver = impact.ImpactPlayerTransport()
        self.a, self.b = context(1), context(2)
        self.receiver.update(self.b, 1, 0x260, 1, 1, sample(), 100)

    def send(self, value=None, now=100, sent=True):
        status, packets = self.sender.update(
            self.a, 1, 0x260, 1, 1, value or sample(), now)
        for packet in packets:
            self.sender.packet_send_result(packet, sent)
        return status, packets

    def drain(self, now=100):
        return self.receiver.update(
            self.b, 1, 0x260, 1, 1, sample(), now)[0]

    def test_latest_cursor_is_coalesced_and_two_peers_stay_visible(self):
        _, packets = self.send()
        packet = packets[0]
        for q in range(1, 11):
            newer = {**packet, "q": q, "t": 100000 + q,
                     "c": [1] + [0x3F800000 + q] * 3 + [q, 512, 0]}
            self.assertTrue(self.receiver.receive(self.b, newer, 100 + q / 100))
        peer = {**packet, "clientId": 3, "session": 300, "q": 1}
        self.assertTrue(self.receiver.receive(self.b, peer, 100.2))
        rows = self.drain(100.2)["c"]
        self.assertEqual([row[0] for row in rows], [1, 3])
        self.assertEqual(rows[0][2], 10)
        self.assertEqual(rows[0][4:], [0x3F80000A] * 3 + [10, 512, 0])
        self.assertFalse(self.receiver.receive(self.b, packet, 100.3))
        self.assertEqual(len(self.drain(100.3)["c"]), 2)

    def test_visibility_off_bypasses_cadence_and_attack_is_once(self):
        _, packets = self.send(sample(attacks=[(1, 1)]))
        packet = packets[0]
        self.assertTrue(self.receiver.receive(self.b, packet, 100))
        self.assertFalse(self.receiver.receive(self.b, packet, 100.001))
        self.assertEqual(len(self.drain()["a"]), 1)
        self.assertEqual(self.drain()["a"], [])
        replay = {**packet, "q": 2, "t": 100010}
        self.assertTrue(self.receiver.receive(self.b, replay, 100.01))
        self.assertEqual(self.drain(100.01)["a"], [])
        _, packets = self.send(sample(visible=0), 100.001)
        self.assertEqual(len(packets), 1)
        off = {**packets[0], "q": 3, "t": 100020}
        self.assertTrue(self.receiver.receive(self.b, off, 100.02))
        self.assertEqual(self.drain(100.02)["c"][0][3], 0)

    def test_failed_send_retains_event_and_does_not_commit_baseline(self):
        status, packets = self.send(sample(attacks=[(1, 2)]), sent=False)
        self.assertEqual(status["accepted"], 1)
        self.assertIsNone(self.sender.published)
        self.assertEqual(len(self.sender.pending), 1)
        self.assertLess(self.sender.last_wire, 0)
        _, retry = self.send(sample(attacks=[(1, 2)]), 100.001)
        self.assertEqual(retry[0]["a"], packets[0]["a"])
        self.assertEqual(len(self.sender.pending), 0)
        self.assertEqual(self.sender.last_wire, 100.001)
        self.assertEqual(self.send(sample(), 100.002)[1], [])

    def test_scope_session_visit_and_sender_validation(self):
        _, packets = self.send()
        packet = packets[0]
        bad_fields = [
            ("clientId", 99), ("clientId", True), ("clientId", {}),
            ("session", 101), ("visit", 2),
            ("s", 0x220), ("k", 2), ("targetTeamId", "other"),
            ("targetClientId", 2), ("addToQueue", False),
            ("v", 1), ("q", True), ("q", 0), ("t", -1),
            ("c", [1] + [0x7F800000] * 6),
            ("a", [[1, 4] + [0] * 6]),
        ]
        for key, value in bad_fields:
            with self.subTest(key=key, value=value):
                self.assertFalse(self.receiver.receive(self.b, {**packet, key: value}, 100))
        for field, value in [("online", False), ("interactionSession", 101),
                             ("teamId", "other")]:
            ctx = copy.deepcopy(self.b)
            ctx["players"][1][field] = value
            self.assertFalse(self.receiver.receive(ctx, packet, 100))
        ctx = copy.deepcopy(self.b)
        ctx["players"][1][impact.METADATA_KEY] = advertisement(1, boss=2)
        self.assertFalse(self.receiver.receive(ctx, packet, 100))
        self.assertTrue(self.receiver.receive(self.b, packet, 100))

    def test_expiry_drops_aim_events_and_retains_sequence_tombstone(self):
        _, packets = self.send(sample(attacks=[(1, 1)]))
        packet = packets[0]
        self.receiver.receive(self.b, packet, 100)
        expired = self.drain(102)
        self.assertEqual(expired["c"], [])
        self.assertEqual(expired["a"], [])
        self.assertFalse(self.receiver.receive(self.b, packet, 102))

    def test_scope_reset_drops_queued_visuals_and_local_events(self):
        _, packets = self.send(sample(attacks=[(1, 2)]), sent=False)
        self.receiver.receive(self.b, packets[0], 100)
        status, _ = self.receiver.update(self.b, 1, 0x260, 2, 2, sample(), 100.01)
        self.assertEqual(status["c"], [])
        self.assertEqual(status["a"], [])
        self.assertFalse(self.receiver.receive(self.b, packets[0], 100.02))
        status, _ = self.sender.update(self.a, 0, 0, 0, 0, None, 100.02)
        self.assertEqual(status["accepted"], 0)
        self.assertFalse(self.sender.pending)

    def test_same_encounter_new_visit_drops_inputs_and_old_send_commit(self):
        _, packets = self.send(sample(attacks=[(1, 2)]), sent=False)
        old = packets[0]
        self.a["players"][1][impact.METADATA_KEY] = advertisement(1, visit=2)
        self.sender.update(self.a, 1, 0x260, 1, 2, sample(), 100.01)
        self.sender.packet_send_result(old, True)
        self.assertEqual(self.sender.accepted, 0)
        self.assertFalse(self.sender.pending)
        self.assertIsNone(self.sender.published)
        self.assertEqual(self.sender.visit, 2)

    def test_cursor_mesh_pose_and_control_types_are_distinct(self):
        self.assertTrue(impact._cursor([1, 0, 0, 0, 65535, 512, 0]))
        self.assertFalse(impact._cursor([1, 0, 0, 0, 0x3F800000, 512, 0]))
        self.assertFalse(impact._cursor([1, 0x7FC00000, 0, 0, 0, 512, 0]))
        self.assertTrue(impact._attack([1, 2, 0x2000, 0x2000, 1000, 600, 1, 1]))
        self.assertFalse(impact._attack([1, 2, 0x2000, 0x2000, 1000, 600, 0, 1]))
        self.assertFalse(impact._attack([1, 2, 0x2000, True, 1000, 600, 1, 1]))
        self.assertFalse(impact._attack([1, 3, 0, 0, 0, 0, 0, 0]))

    def test_bounded_packet_queue_peer_count_and_frame_drain(self):
        self.a, self.b = context(1, 30), context(2, 30)
        _, packets = self.send(sample(aim=[0xFF7FFFFF] * 3 + [65535] * 3,
                                      attacks=[(i, 1) for i in range(1, 17)]))
        packet = packets[0]
        self.assertEqual(len(packet["a"]), impact.PLAYER_EVENTS_PER_PACKET)
        self.assertLessEqual(len(json.dumps(packet, separators=(",", ":")).encode()) + 1,
                             impact.PLAYER_PACKET_BYTES)
        self.assertEqual(packet["targetTeamId"], "default")
        self.assertEqual(packet["clientId"], 1)
        self.assertNotIn("addToQueue", packet)
        self.assertNotIn("targetClientId", packet)
        for cid in range(1, 31):
            row = {**packet, "clientId": cid, "session": cid * 100}
            self.receiver.receive(self.b, row, 100)
        self.assertLessEqual(len(self.receiver.remote), impact.PLAYER_MAX_PEERS)
        self.assertLessEqual(len(self.receiver.incoming), impact.PLAYER_EVENT_QUEUE)
        self.assertEqual(len(self.drain()["a"]), impact.PLAYER_EVENTS_PER_FRAME)

    def test_cadence_is_ten_hz_with_event_edges_and_keepalive(self):
        _, packets = self.send()
        self.assertEqual(len(packets), 1)
        self.assertFalse(self.send(sample(aim=[0] * 6), 100.01)[1])
        self.assertTrue(self.send(sample(aim=[0] * 6), 100.1)[1])
        self.assertFalse(self.send(sample(aim=[0] * 6), 100.2)[1])
        self.assertTrue(self.send(sample(aim=[0] * 6, attacks=[(1, 1)]), 100.21)[1])
        self.assertFalse(self.send(sample(aim=[0] * 6), 100.3)[1])
        self.assertTrue(self.send(sample(aim=[0] * 6), 101)[1])


if __name__ == "__main__":
    unittest.main()
