"""Exercise actual minigame send/receive, graph gates and native bridge."""

import copy
import json
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "py"))
import anchor_minigame as mg
from test_minigame_invitation_transport import load_client, RecordingSocket


def state(phase=4, timer=900, actors=3, **changes):
    value = {"ready": 1, "running": 1, "c": [phase, timer, 7, 2],
             "a": [[1, i, 1, i * 100, -70, 2] for i in range(actors)]}
    value.update(changes)
    return value


def ebi_state(**changes):
    rows = []
    for kind, actor_id in ((1, 1), (2, 2), (3, 3), (5, 4)):
        row = [kind, actor_id, 1] + [0] * (37 if kind == 3 else 29)
        row[8:11] = [0x3f800000] * 3
        row[14] = 0x3f800000
        row[17] = 1
        row[19] = 3 << 24
        if kind == 3:
            row[36] = 2
        rows.append(row)
    value = {"ready": 1, "running": 1,
             "c": [2, 1, 0, 10, 100, 2900, 0, 4, 0, 1, 0, 0, 3], "a": rows}
    value.update(changes)
    return value


def goemon_state(visuals=1, core=6, **changes):
    """Goemon v2 wire widths; production C captures are tested by its harness."""
    rows = []
    for actor_id in range(core):
        row = [1, actor_id, 1] + [0] * 28
        row[3:11] = [1, 1, 0, 12, actor_id * 30, 100, 0, 2048]
        row[11:18] = [actor_id * 102400, 0, -102400, 0, 1024, 1024, 1024]
        row[18:20] = [1, 1]
        row[24:27] = [1024] * 3
        row[29:31] = [102400, 1]
        rows.append(row)
    for slot in range(visuals):
        rows.append([2, slot + 1, 1, slot // 3 + 1, (slot // 3 + 1) * 101,
                     slot % 3, 1, 0, 102400, 204800, -102400, 0, 0, 0,
                     1, 17, 4096, 2048, -4096, 1])
    value = {"ready": 1, "running": 1, "c": [2, 1, 0, 2900, 0], "a": rows}
    value.update(changes)
    return value


class Peer:
    def __init__(self, cid, roster, game=1, room=10, team="blue"):
        self.cid, self.session, self.team = cid, cid * 101, team
        self.game, self.room, self.roster = game, room, roster
        self.character = {1: "Goemon", 2: "Ebisumaru", 3: "Sasuke"}[game]
        self.engine = mg.MinigameTransport()
        self.engine._scope(self.context())
        self.sent = []
        self.meta(100)

    def context(self):
        return {"cid": self.cid, "session": self.session, "team": self.team,
                "connected": True, "loaded": True, "room": self.room,
                "character": self.character, "players": self.roster}

    def meta(self, now):
        self.roster[self.cid] = {"online": True, "isSaveLoaded": True, "teamId": self.team,
            "roomId": self.room, "interactionSession": self.session, "_minigameReceived": now,
            "currentCharacter": self.character,
            mg.METADATA_KEY: self.engine.advertisement(self.context())}

    def update(self, supplied=None, now=100, success=True, visit=7):
        status, packets = self.engine.update(self.context(), self.game, visit, supplied, now)
        self.meta(now)
        self.sent.extend(packets)
        for packet in packets:
            self.engine.packet_send_result(packet, success)
        return status, packets


def make_native_status_fixture(native, game=3, local_ready=1):
    """Round-trip a production C capture through the actual Python transport.

    Native harnesses export state_encode JSON, then import this helper and
    feed its returned JSON to production status_decode and their real adapter.
    No controller/object words are invented by this helper.
    """
    roster = {}
    host = Peer(2, roster, game, mg.ROOMS[game])
    guest = Peer(1, roster, game, 10)
    _, packets = host.update(native)
    assert packets and mg.native_state(native), "Native fixture is not a complete graph"
    for packet in packets:
        assert guest.engine.receive(guest.context(), packet, 100)
    assert guest.engine.prepare_join(guest.context(), game, 2, 202, 7, 100)
    guest.room = mg.ROOMS[game]
    status, _ = guest.update({**native, "ready": local_ready}, now=100.1, visit=17)
    assert not status["isOwner"] and status["running"] == native["running"]
    assert status["c"] == native["c"] and status["a"] == native["a"]
    return status


class MinigameSyncTests(unittest.TestCase):
    def pair(self, game=1, supplied=None):
        roster = {}
        guest = Peer(1, roster, game)
        host = Peer(2, roster, game, mg.ROOMS[game])
        _, packets = host.update(supplied or state())
        for packet in packets:
            self.assertTrue(guest.engine.receive(guest.context(), packet, 100))
        self.assertTrue(guest.engine.prepare_join(guest.context(), game, host.cid, host.session, 7, 100))
        guest.room = mg.ROOMS[game]
        guest.meta(100)
        return host, guest

    def test_late_join_all_games_keeps_controller_dialog_progress_timer(self):
        for game in mg.ROOMS:
            with self.subTest(game=game):
                host, guest = self.pair(game)
                status, packets = guest.update(state(phase=0, timer=0), now=100.1, visit=17)
                self.assertEqual(status["c"], [4, 900, 7, 2])
                self.assertEqual(status["a"], state()["a"])
                self.assertEqual(status["token"], [game, 2, 202, 7])
                self.assertEqual(status["isOwner"], 0)
                self.assertEqual(status["ready"], 1)
                self.assertEqual(status["running"], 1)
                self.assertEqual(packets, [])

    def test_prepare_uses_metadata_before_graph_and_waits_for_complete_ready(self):
        roster = {}
        host, guest = Peer(2, roster, room=mg.ROOMS[1]), Peer(1, roster)
        _, packets = host.update(state())
        self.assertTrue(guest.engine.prepare_join(guest.context(), 1, 2, 202, 7, 100))
        self.assertTrue(guest.engine.prepare_join(guest.context(), 1, 2, 202, 7, 100.1))
        guest.room = mg.ROOMS[1]
        out, _ = guest.update(state(), now=100.1, visit=17)
        self.assertEqual(out["ready"], 0)
        self.assertEqual(out["isOwner"], 0)
        self.assertTrue(guest.engine.receive(guest.context(), packets[0], 100.2))
        out, _ = guest.update(state(ready=0), now=100.3, visit=17)
        self.assertEqual(out["ready"], 0)
        self.assertEqual(out["c"], state()["c"])
        out, _ = guest.update(state(), now=100.4, visit=17)
        self.assertEqual(out["ready"], 1)
        # Fresh owner metadata must not adopt a cached graph from an older
        # authority generation while the new complete graph is still in flight.
        guest.engine.visit = 0
        guest.engine.prepared = False
        roster[2][mg.METADATA_KEY]["g"] = 2
        self.assertTrue(guest.engine.prepare_join(guest.context(), 1, 2, 202, 7, 100.5))
        self.assertIsNone(guest.engine.graph)

    def test_prepared_context_survives_ordinary_frames_and_cancels_source_change_expiry(self):
        host, guest = self.pair()
        guest.room = 10
        guest.engine.prepare_room = 10
        ctx = guest.context()
        guest.engine.update(ctx, 0, 0, None, 100.2)
        self.assertEqual(guest.engine.t, host.engine.t)
        guest.engine.room_changed(ctx, 11)
        self.assertIsNone(guest.engine.t)
        self.assertTrue(guest.engine.prepare_join(ctx, 1, 2, 202, 7, 100.3))
        guest.engine.update(ctx, 0, 0, None, 131)
        self.assertIsNone(guest.engine.t)

    def test_invalid_stale_host_and_offline_debug(self):
        roster = {}
        host, guest = Peer(2, roster, room=mg.ROOMS[1]), Peer(1, roster)
        host.update(state())
        for changes in ({"online": False}, {"isSaveLoaded": False}, {"teamId": "red"},
                        {"roomId": 10}, {"interactionSession": 999}):
            saved = copy.deepcopy(roster[2])
            roster[2].update(changes)
            self.assertFalse(guest.engine.prepare_join(guest.context(), 1, 2, 202, 7, 100))
            roster[2] = saved
        self.assertFalse(guest.engine.prepare_join(guest.context(), 1, 2, 999, 7, 100))
        self.assertFalse(guest.engine.prepare_join(guest.context(), 1, 2, 202, 8, 100))
        self.assertFalse(guest.engine.prepare_join(guest.context(), 1, 2, 202, 7, 104))
        ctx = {**guest.context(), "connected": False, "loaded": False}
        self.assertTrue(guest.engine.prepare_join(ctx, 1, 0, 0, 0, 104))
        self.assertFalse(guest.engine.prepare_join(ctx, 1, 2, 0, 0, 104))

    def test_debug_selects_active_or_starts_own(self):
        host, guest = self.pair()
        guest.engine.reset()
        self.assertTrue(guest.engine.prepare_join(guest.context(), 1, 0, 0, 0, 100))
        self.assertEqual(guest.engine.t, host.engine.t)
        guest.engine.reset()
        guest.roster[2]["online"] = False
        self.assertTrue(guest.engine.prepare_join(guest.context(), 1, 0, 0, 0, 100))
        out, _ = guest.update(state(), now=100, visit=20)
        self.assertEqual(out["isOwner"], 1)
        self.assertEqual(out["token"], [1, 1, 101, 20])

    def test_atomic_chunks_reordered_incomplete_duplicate_and_header_mismatch(self):
        big = state(actors=0)
        big["a"] = [[1, i, 1] + [-2147483648] * 29 for i in range(32)]
        host, guest = self.pair(supplied=big)
        self.assertGreater(len(host.sent), 1)
        _, packets = host.update(big, now=100.3)
        previous = copy.deepcopy(guest.engine.graph)
        for p in reversed(packets[1:]):
            self.assertTrue(guest.engine.receive(guest.context(), p, 100.3))
            self.assertEqual(guest.engine.graph, previous)
        self.assertTrue(guest.engine.receive(guest.context(), packets[-1], 100.31))
        self.assertTrue(guest.engine.receive(guest.context(), packets[0], 100.32))
        self.assertEqual(guest.engine.graph["k"], 2)
        self.assertFalse(guest.engine.receive(guest.context(), packets[0], 100.33))
        _, newer = host.update(big, now=100.6)
        self.assertTrue(guest.engine.receive(guest.context(), newer[0], 100.6))
        malformed = copy.deepcopy(newer[1])
        malformed["c"][0] += 1
        self.assertFalse(guest.engine.receive(guest.context(), malformed, 100.61))
        self.assertEqual(guest.engine.graph["k"], 2)

    def test_duplicate_actor_graph_and_bounded_pending_expiry(self):
        host, guest = self.pair()
        _, packets = host.update(state(), now=100.3)
        p = copy.deepcopy(packets[0])
        p["a"].append(p["a"][0])
        self.assertFalse(guest.engine.receive(guest.context(), p, 100.3))
        for k in range(3, 10):
            p = copy.deepcopy(packets[0])
            p.update(k=k, n=2, p=0)
            self.assertTrue(guest.engine.receive(guest.context(), p, 100.3))
        self.assertEqual(len(guest.engine.pending), 2)
        guest.engine._prune(102)
        self.assertEqual(guest.engine.pending, {})

    def test_wrong_identity_team_room_session_and_older_authority_rejected(self):
        host, guest = self.pair()
        _, packets = host.update(state(), now=100.3)
        p = packets[0]
        for changes in ({"clientId": 99}, {"targetTeamId": "red"}, {"targetClientId": 1},
                        {"addToQueue": False}, {"o": [2, 999, 7]}, {"o": [2, 202, 99]},
                        {"g": 0}, {"k": True}, {"t": [2, 2, 202, 7]}, {"c": [True]}):
            self.assertFalse(guest.engine.receive(guest.context(), {**p, **changes}, 100.3), changes)
        guest.roster[2]["roomId"] = 10
        self.assertFalse(guest.engine.receive(guest.context(), p, 100.3))

    def test_cadence_bytes_idle_edges_and_failed_retry(self):
        roster = {}
        host = Peer(2, roster, room=mg.ROOMS[1])
        _, packets = host.update(state(running=0))
        self.assertLessEqual(sum(mg.wire_bytes(p) for p in packets), mg.MAX_GRAPH_BYTES)
        self.assertEqual(host.update(state(running=0), now=100.2)[1], [])
        self.assertEqual(len(host.update(state(running=0), now=101)[1]), 1)
        self.assertEqual(host.update(state(timer=20, edge=1), now=101.03)[1], [])
        self.assertEqual(len(host.update(state(timer=20, edge=1), now=101.05)[1]), 1)
        _, failed = host.update(state(timer=10), now=101.3, success=False)
        last_tick = host.engine.tick
        self.assertEqual(host.update(state(timer=10), now=101.33)[1], [])
        _, retry = host.update(state(timer=10), now=101.35)
        self.assertEqual(failed, retry)
        self.assertEqual(host.engine.tick, last_tick + 1)
        for p in host.sent:
            self.assertEqual(p["targetTeamId"], "blue")
            self.assertEqual(p["clientId"], 2)
            self.assertNotIn("addToQueue", p)
            self.assertLessEqual(mg.wire_bytes(p), mg.PACKET_BYTES)

    def test_handoff_keeps_origin_seed_and_rejects_old_host(self):
        host, guest = self.pair()
        guest.update(state(), now=100.1, visit=17)
        guest.roster[2]["online"] = False
        out, packets = guest.update(state(phase=0, timer=0), now=102.9, visit=17)
        self.assertEqual(out["isOwner"], 0)
        out, packets = guest.update(state(phase=0, timer=0), now=103.1, visit=17)
        self.assertEqual(out["isOwner"], 1)
        self.assertEqual(out["takeover"], 1)
        self.assertEqual(out["generation"], 2)
        self.assertEqual(out["token"], [1, 2, 202, 7])
        self.assertEqual(packets[0]["c"], state()["c"])
        self.assertEqual(out["running"], 1)
        # Returning origin cannot overwrite the newly owned generation.
        host.meta(103.2)
        _, old = host.update(state(timer=5), now=103.2)
        self.assertFalse(guest.engine.receive(guest.context(), old[0], 103.2))

    def test_handoff_old_tick_does_not_freeze_new_generation_and_retries_native_adoption(self):
        host, guest = self.pair()
        for i in range(1, 11):
            _, packets = host.update(state(timer=900-i), now=100 + i * .21)
            for p in packets:
                self.assertTrue(guest.engine.receive(guest.context(), p, 100 + i * .21))
        self.assertEqual(guest.engine.graph["k"], 11)
        guest.update(state(), now=102.2, visit=17)
        guest.roster[2]["online"] = False
        out, packets = guest.update(state(phase=0, timer=0), now=105.2, visit=17)
        self.assertEqual((out["generation"], out["tick"], out["takeover"]), (2, 0, 1))
        self.assertEqual(packets[0]["k"], 1)
        seed = out["c"]
        failed, _ = guest.update(state(phase=0, timer=0, ready=0), now=105.3, visit=17)
        self.assertEqual((failed["tick"], failed["takeover"], failed["c"]), (1, 1, seed))
        retry, packets = guest.update(state(phase=0, timer=0), now=105.5, visit=17)
        self.assertEqual(packets[0]["c"], seed)
        self.assertEqual(retry["takeover"], 1)
        adopted, _ = guest.update(state(adoptedGeneration=2), now=105.6, visit=17)
        self.assertEqual((adopted["generation"], adopted["tick"], adopted["takeover"]), (2, 2, 0))
        self.assertNotIn("c", adopted)

    def test_lower_explicit_ready_joiner_wins_and_unready_graph_cannot_takeover(self):
        host, guest = self.pair()
        guest.update(state(), now=100.1, visit=17)
        third = Peer(3, guest.roster)
        for p in host.sent:
            self.assertTrue(third.engine.receive(third.context(), p, 100))
        self.assertTrue(third.engine.prepare_join(third.context(), 1, 2, 202, 7, 100))
        third.room = mg.ROOMS[1]
        third.update(state(), now=100.1, visit=19)
        guest.roster[2]["online"] = False
        out, _ = third.update(state(), now=103.1, visit=19)
        self.assertEqual(out["isOwner"], 0)
        out, packets = guest.update(state(), now=103.1, visit=17)
        self.assertEqual(out["isOwner"], 1)
        for p in packets:
            self.assertTrue(third.engine.receive(third.context(), p, 103.1))
        self.assertEqual(third.engine.g, 2)
        self.assertEqual(third.engine.o[0], 1)
        host, guest = self.pair()
        guest.roster[2]["online"] = False
        guest.engine.graph = None
        out, _ = guest.update(state(), now=104, visit=17)
        self.assertEqual(out["isOwner"], 0)
        self.assertEqual(out["ready"], 0)

    def test_input_retry_ack_dedupe_and_terminal_idempotency(self):
        host, guest = self.pair()
        guest.update(state(), now=100.1, visit=17)
        out, packets = guest.update(state(inputs=[[1, 2, 33]]), now=100.2, visit=17)
        request = packets[0]
        self.assertEqual(request["type"], mg.INPUT_TYPE)
        self.assertEqual(request["targetClientId"], 2)
        self.assertTrue(host.engine.receive(host.context(), request, 100.2))
        self.assertFalse(host.engine.receive(host.context(), request, 100.21))
        out, replies = host.update(state(), now=100.3)
        self.assertEqual(out["inputs"], [[1, 101, 17, 1, 2, 33]])
        self.assertEqual(host.engine.acks, {})
        out, _ = host.update(state(ready=0), now=100.4)
        self.assertEqual(out["inputs"], [])
        out, replies = host.update(state(inputAck=[[1, 101, 17, 1]]), now=100.6)
        self.assertEqual(out["inputs"], [])
        for reply in replies:
            self.assertTrue(guest.engine.receive(guest.context(), reply, 100.6))
        self.assertEqual(guest.engine.input_pending, {})
        self.assertEqual(guest.update(state(inputs=[[1, 2, 33]]), now=100.65, visit=17)[1], [])
        self.assertEqual(host.update(state(), now=100.7)[0]["inputs"], [])
        _, result_packets = guest.update(state(result=2), now=100.8, visit=17)
        result = result_packets[0]
        self.assertTrue(host.engine.receive(host.context(), result, 100.8))
        self.assertFalse(host.engine.receive(host.context(), result, 100.81))
        self.assertEqual(host.engine.terminal, 0)
        self.assertEqual(guest.update(state(result=2), now=101.0, visit=17)[1], [])
        _, terminal = host.update(state(result=1), now=101.1)
        for p in terminal:
            self.assertTrue(guest.engine.receive(guest.context(), p, 101.1))
        self.assertEqual(guest.engine.terminal, 1)

    def test_native_shape_limits_and_metadata_order(self):
        for changes in ({"c": [False]}, {"c": [0] * 33}, {"a": [[1, 0, 0, 0x80000000]]},
                        {"a": [[1, 0, 0]] * 2}, {"inputs": [[1, 0]]}, {"ready": True}):
            self.assertIsNone(mg.native_state(state(**changes)))
        host, _ = self.pair()
        new = host.engine.advertisement(host.context())
        old = {**new, "q": new["q"] - 1, "r": 0}
        self.assertEqual(mg.merge_metadata(new, old, 202), new)
        self.assertIsNone(mg.metadata(new, 303))
        host.room = 10
        host.engine.room_changed(host.context(), 10)
        exit_meta = host.engine.advertisement(host.context())
        self.assertGreater(exit_meta["q"], new["q"])
        self.assertEqual(mg.merge_metadata(new, exit_meta, 202)["t"], [])

    def test_verified_pool_capacity_64_rows_byte_budget_and_65_rejection(self):
        big = state(actors=0)
        big["a"] = [[1, i, 1] + [-2147483648] * 29 for i in range(64)]
        big["c"] = [-2147483648] * 32
        host, guest = self.pair(supplied=big)
        self.assertEqual(len(guest.engine.graph["a"]), 64)
        self.assertLessEqual(len(host.sent), 8)
        self.assertLessEqual(sum(mg.wire_bytes(p) for p in host.sent), 32768)
        self.assertTrue(all(mg.wire_bytes(p) <= 4096 for p in host.sent))
        big["a"].append([1, 64, 1])
        self.assertIsNone(mg.native_state(big))

    def test_33_senders_rotate_bounded_receipts_and_prune_session_churn(self):
        host, observer = self.pair()
        participants, requests = [], []
        for cid in range(3, 36):
            peer = Peer(cid, host.roster)
            for p in host.sent:
                self.assertTrue(peer.engine.receive(peer.context(), p, 100))
            self.assertTrue(peer.engine.prepare_join(peer.context(), 1, 2, 202, 7, 100))
            peer.room = mg.ROOMS[1]
            peer.update(state(), now=100.1, visit=cid+100)
            _, packets = peer.update(state(inputs=[[1, 2, cid]]), now=100.2, visit=cid+100)
            requests.append(packets[0])
            participants.append(peer)
        for i, request in enumerate(requests):
            self.assertEqual(host.engine.receive(host.context(), request, 100.2), i < 32)
        delivered, _ = host.update(state(), now=100.3)
        self.assertEqual(len(delivered["inputs"]), 32)
        host.update(state(inputAck=[row[:4] for row in delivered["inputs"]]), now=100.5)
        self.assertTrue(host.engine.receive(host.context(), requests[-1], 100.6))
        delivered, _ = host.update(state(), now=100.7)
        self.assertEqual(delivered["inputs"], [[35, 3535, 135, 1, 2, 35]])
        host.update(state(inputAck=[delivered["inputs"][0][:4]]), now=100.9)
        observed = set()
        for now in (101.1, 101.3):
            _, packets = host.update(state(), now=now)
            self.assertTrue(packets)
            self.assertLessEqual(len(packets[0]["h"]), 32)
            observed.update(tuple(row[:3]) for row in packets[0]["h"])
            for p in packets:
                self.assertTrue(observer.engine.receive(observer.context(), p, now))
        self.assertEqual(len(observed), 33)
        self.assertEqual(len(observer.engine.graph["ackHistory"]), 33)
        self.assertEqual(len(host.engine.input_seen), 33)
        old = requests[0]
        peer = host.roster[3]
        peer["interactionSession"] += 1
        peer[mg.METADATA_KEY]["s"] += 1
        fresh = {**old, "s": peer["interactionSession"]}
        self.assertFalse(host.engine.receive(host.context(), old, 101.4))
        self.assertTrue(host.engine.receive(host.context(), fresh, 101.4))
        self.assertNotIn((3, 303, 103), host.engine.input_seen)
        self.assertEqual(len(host.engine.input_seen), 33)

    def test_visit_rejoin_does_not_consume_new_input_with_old_receipt(self):
        host, guest = self.pair()
        guest.update(state(), now=100.1, visit=17)
        _, requests = guest.update(state(inputs=[[7, 2, 33]]), now=100.2, visit=17)
        old_request = requests[0]
        self.assertTrue(host.engine.receive(host.context(), old_request, 100.2))
        host.update(state(), now=100.3)
        _, receipts = host.update(state(inputAck=[[1, 101, 17, 7]]), now=100.5)
        old_receipt = copy.deepcopy(receipts[0])
        guest.engine.reset()
        guest.room = 10
        guest.meta(100.6)
        self.assertTrue(guest.engine.prepare_join(guest.context(), 1, 2, 202, 7, 100.6))
        guest.room = mg.ROOMS[1]
        guest.update(state(), now=100.7, visit=18)
        self.assertTrue(guest.engine.receive(guest.context(), old_receipt, 100.7))
        _, request = guest.update(state(inputs=[[1, 2, 44]]), now=100.8, visit=18)
        self.assertEqual(request[0]["i"][0], 1)
        self.assertFalse(host.engine.receive(host.context(), old_request, 100.8))
        self.assertTrue(host.engine.receive(host.context(), request[0], 100.8))
        self.assertNotIn((1, 101, 17), host.engine.acks)
        out, _ = host.update(state(inputAck=[[1, 101, 17, 7]]), now=100.9)
        self.assertEqual(out["inputs"], [[1, 101, 18, 1, 2, 44]])

    def test_full_width_64_actor_graph_with_32_receipts_adapts_ack_budget(self):
        big = state(actors=0)
        big["a"] = [[1, i, 1] + [-2147483648] * 29 for i in range(64)]
        big["c"] = [-2147483648] * 32
        host, _ = self.pair(supplied=big)
        host.cid = mg.MAX_INT
        host.session = mg.MAX_INT
        host.engine.t = (1, mg.MAX_INT, mg.MAX_INT, mg.MAX_INT)
        host.engine.o = host.engine.t[1:]
        host.engine.acks = {(mg.MAX_INT-i, mg.MAX_INT, mg.MAX_INT): mg.MAX_INT for i in range(32)}
        packets = host.engine._chunks(host.context(), mg.native_state(big), mg.MAX_INT)
        self.assertTrue(packets)
        self.assertLessEqual(len(packets), 8)
        self.assertTrue(all(mg.wire_bytes(p) <= 4096 for p in packets))
        self.assertLessEqual(sum(mg.wire_bytes(p) for p in packets), 32768)
        self.assertGreaterEqual(len(packets[0]["h"]), 1)
        self.assertLess(len(packets[0]["h"]), 32)
        self.assertEqual(sum(len(p["a"]) for p in packets), 64)

    def test_ordered_input_window_keeps_three_pose_streams_fresh_and_preserves_hits(self):
        host, guest = self.pair()
        guest.update(state(), now=100.1, visit=17)
        sequence, captured, accepted, latest = 0, [], [], [-1, -1, -1]
        hits = []
        max_pending = 0
        for frame in range(1, 301):
            now = 100.1 + frame / 30
            if frame % 6 == 0:
                for stream in range(3):
                    sequence += 1
                    captured.append([sequence, 2, frame, stream])
            if frame in (30, 150):
                sequence += 1
                captured.append([sequence, 3, frame, 99])
            # Native capture can present target requests before older poses.
            _, packets = guest.update(state(inputs=list(reversed(captured[-20:]))), now=now, visit=17)
            for p in packets:
                host.engine.receive(host.context(), p, now)
            status, packets = host.update(state(inputAck=accepted), now=now)
            accepted = [row[:4] for row in status["inputs"]]
            for row in status["inputs"]:
                if row[4] == 2:
                    latest[row[6]] = row[5]
                elif row[4] == 3:
                    hits.append(row[5])
            for p in packets:
                guest.engine.receive(guest.context(), p, now)
            max_pending = max(max_pending, len(guest.engine.input_pending))
            if frame > 30:
                self.assertLessEqual((frame-min(latest))/30, .6)
        self.assertLessEqual(max_pending, 12)
        self.assertGreaterEqual(host.engine.input_seen[(1, 101, 17)], sequence-6)
        self.assertEqual(hits, [30, 150])
        self.assertLessEqual(len(guest.engine.input_pending), 12)


class EbisumaruActionTests(unittest.TestCase):
    def pair(self):
        host, guest = MinigameSyncTests().pair(2, ebi_state())
        guest.update(ebi_state(), now=100.01, visit=17)
        for peer in (host, guest):
            peer.roster[peer.cid]["currentCharacter"] = "Ebisumaru"
        return host, guest

    def deliver(self, host, guest, supplied, now):
        _, packets = host.update(supplied, now=now)
        self.assertTrue(packets)
        for packet in packets:
            self.assertTrue(guest.engine.receive(guest.context(), packet, now))
        self.assertEqual(host.engine.graph["a"], guest.engine.graph["a"])
        return packets

    def test_motion_omitted_until_full_but_phase_health_target_and_hp_are_edges(self):
        host, guest = self.pair()
        moving = ebi_state()
        moving["c"][2] += 3
        moving["c"][3] += 3
        moving["c"][5] -= 3
        for row in moving["a"]:
            row[3] = 0x42000000
            row[11] = 0x40800000
            row[19] += 5  # Native life clock is not an HP edge.
        moving["a"][2][32:36] = [0x3f000000] * 4
        moving["a"][2][37:40] = [0x42000000] * 3
        moving["a"][3][12] ^= 0x20  # Sweet blink advances locally.
        self.assertEqual(host.update(moving, now=100.2)[1], [])
        moving["c"][12] = 2
        action = self.deliver(host, guest, moving, 100.21)[0]
        self.assertEqual(action["type"], mg.ACTION_TYPE)
        self.assertEqual((action["b"], action["a"], action["z"]), (1, [], []))
        self.assertEqual(guest.engine.graph["c"][12], 2)
        self.assertEqual(guest.update(moving, now=100.22, visit=17)[0]["fullTick"], 1)
        self.assertEqual(guest.engine.graph["a"], ebi_state()["a"])
        moving["a"][2][17] = 4
        moving["a"][2][19] = 2 << 24
        moving["a"][2][36] = 1
        action = self.deliver(host, guest, moving, 100.27)[0]
        self.assertEqual(action["a"], [moving["a"][2]])
        self.assertEqual(guest.engine.graph["a"][0], ebi_state()["a"][0])
        checkpoint = self.deliver(host, guest, moving, 101.0)[0]
        self.assertEqual(checkpoint["type"], mg.PACKET_TYPE)
        self.assertEqual(guest.engine.graph["a"], moving["a"])
        self.assertEqual(guest.update(moving, now=101.01, visit=17)[0]["fullTick"], checkpoint["k"])
        self.assertNotIn("fullTick", checkpoint)

    def test_lifetimes_tombstones_and_directions_produce_complete_census(self):
        host, guest = self.pair()
        current = ebi_state()
        current["a"].pop()
        current["a"][2][2] = 2
        current["a"][0][14] = -1082130432  # -1.0, turn direction.
        packets = self.deliver(host, guest, current, 100.05)
        self.assertEqual(packets[0]["z"], [[3, 3, 1], [5, 4, 1]])
        self.assertEqual({tuple(r[:3]) for r in guest.engine.graph["a"]},
                         {(1, 1, 1), (2, 2, 1), (3, 3, 2)})
        stopped = copy.deepcopy(current)
        stopped["a"][0][14] = -2147483648  # -0.0 is stopped.
        self.assertEqual(self.deliver(host, guest, stopped, 100.1)[0]["a"], [stopped["a"][0]])

    def test_lost_base_waits_for_next_full_and_does_not_consume_delta_census(self):
        host, guest = self.pair()
        changed = ebi_state()
        changed["a"][2][17] = 2
        _, lost = host.update(changed, now=100.05)
        changed["c"][12] = 2
        _, newer = host.update(changed, now=100.1)
        old = copy.deepcopy(guest.engine.graph)
        self.assertFalse(guest.engine.receive(guest.context(), newer[0], 100.1))
        self.assertEqual(guest.engine.graph, old)
        self.assertTrue(guest.engine.receive(guest.context(), lost[0], 100.11))
        self.assertTrue(guest.engine.receive(guest.context(), newer[0], 100.12))
        guest.engine.cache.clear()
        self.assertFalse(guest.engine.receive(guest.context(), newer[0], 100.13))
        recovered = self.deliver(host, guest, changed, 101.0)
        self.assertEqual(recovered[0]["type"], mg.PACKET_TYPE)

    def test_multipart_actions_are_atomic_and_malformed_tombstones_rollback(self):
        many = ebi_state()
        many["a"] = [[3, i+1, 1] + [2147483647] * 37 for i in range(32)]
        host, guest = MinigameSyncTests().pair(2, many)
        changed = copy.deepcopy(many)
        for row in changed["a"]:
            row[17] += -1
        _, packets = host.update(changed, now=100.05)
        self.assertGreater(len(packets), 1)
        prior = copy.deepcopy(guest.engine.graph)
        for packet in reversed(packets[1:]):
            self.assertTrue(guest.engine.receive(guest.context(), packet, 100.05))
            self.assertEqual(guest.engine.graph, prior)
        bad = copy.deepcopy(packets[0])
        bad["c"][12] = 1
        self.assertFalse(guest.engine.receive(guest.context(), bad, 100.06))
        self.assertEqual(guest.engine.graph, prior)
        for packet in packets:
            self.assertTrue(guest.engine.receive(guest.context(), packet, 100.07))
        self.assertEqual(guest.engine.graph["a"], changed["a"])
        changed["c"][12] = 2
        _, packets = host.update(changed, now=100.1)
        prior = copy.deepcopy(guest.engine.graph)
        for tombstones in ([[3, 999, 1]], [[3, 1, 1]] * 2, [[3, True, 1]]):
            self.assertFalse(guest.engine.receive(guest.context(), {**packets[0], "z": tombstones}, 100.1))
            self.assertEqual(guest.engine.graph, prior)
        self.assertTrue(guest.engine.receive(guest.context(), packets[0], 100.1))

    def test_failed_send_keeps_baseline_tick_full_timer_and_retries_at_floor(self):
        host, guest = self.pair()
        changed = ebi_state()
        changed["a"][2][17] = 2
        _, failed = host.update(changed, now=100.05, success=False)
        self.assertEqual((host.engine.tick, host.engine.last_full_wire), (1, 100))
        self.assertEqual(host.update(changed, now=100.08)[1], [])
        _, retry = host.update(changed, now=100.1)
        self.assertEqual(failed, retry)
        self.assertTrue(guest.engine.receive(guest.context(), retry[0], 100.1))
        prior = copy.deepcopy(host.engine.graph)
        _, failed_full = host.update(changed, now=101, success=False)
        self.assertEqual(failed_full[0]["type"], mg.PACKET_TYPE)
        self.assertEqual(host.engine.last_full_wire, 100)
        self.assertEqual(host.engine.graph, prior)
        _, retry_full = host.update(changed, now=101.05)
        self.assertEqual(failed_full, retry_full)
        self.assertEqual(host.engine.last_full_wire, 101.05)

    def test_partial_action_send_failure_never_advances_published_graph(self):
        many = ebi_state()
        many["a"] = [[3, i+1, 1] + [2147483647] * 37 for i in range(32)]
        host, guest = MinigameSyncTests().pair(2, many)
        changed = copy.deepcopy(many)
        for row in changed["a"]:
            row[17] -= 1
        prior = copy.deepcopy(host.engine.graph)
        _, packets = host.engine.update(host.context(), 2, 7, changed, 100.05)
        self.assertGreater(len(packets), 1)
        for index, packet in enumerate(packets):
            host.engine.packet_send_result(packet, index != 1)
        self.assertEqual(host.engine.graph, prior)
        self.assertEqual(host.engine.tick, 1)
        self.assertEqual(host.engine.last_full_wire, 100)
        _, retry = host.update(changed, now=100.1)
        self.assertEqual(packets, retry)
        for packet in retry:
            self.assertTrue(guest.engine.receive(guest.context(), packet, 100.1))
        self.assertEqual(host.engine.graph["a"], guest.engine.graph["a"])

    def test_owner_takeover_always_publishes_full_seed_before_local_actions(self):
        host, guest = self.pair()
        changed = ebi_state()
        changed["c"][12] = 1
        self.deliver(host, guest, changed, 100.05)
        guest.roster[2]["online"] = False
        out, packets = guest.update(ebi_state(), now=103.1, visit=17)
        self.assertEqual((out["generation"], out["takeover"]), (2, 1))
        self.assertEqual(packets[0]["type"], mg.PACKET_TYPE)
        self.assertNotIn("b", packets[0])
        self.assertEqual(packets[0]["c"][12], 1)
        adopted = copy.deepcopy(changed)
        adopted["adoptedGeneration"] = 2
        guest.update(adopted, now=103.11, visit=17)
        adopted["c"][12] = 0
        _, packets = guest.update(adopted, now=103.15, visit=17)
        self.assertEqual(packets[0]["type"], mg.ACTION_TYPE)
        self.assertEqual(packets[0]["b"], 1)

    def test_accepted_receipts_only_and_history_survives_rotating_windows(self):
        host, guest = self.pair()
        _, packets = guest.update(ebi_state(inputs=[[1, 2, 3, 1, 1]]), now=100.05, visit=17)
        self.assertTrue(host.engine.receive(host.context(), packets[0], 100.05))
        admitted, _ = host.update(ebi_state(), now=100.06)
        self.assertEqual(admitted["localAck"], 0)
        self.assertEqual(guest.update(ebi_state(), now=100.07, visit=17)[0]["localAck"], 0)
        packets = self.deliver(host, guest, ebi_state(inputAck=[[1, 101, 17, 1]]), 100.11)
        self.assertEqual(guest.update(ebi_state(), now=100.12, visit=17)[0]["localAck"], 1)
        # Published window may omit this participant, committed history may not.
        host.engine.ack_cursor = 1
        for cid in range(3, 36):
            peer = Peer(cid, host.roster, 2, mg.ROOMS[2])
            peer.engine.t, peer.engine.o, peer.engine.g = host.engine.t, host.engine.o, host.engine.g
            peer.engine.visit, peer.engine.local_ready = cid+10, True
            peer.meta(100.13)
            host.engine.acks[(cid, peer.session, cid+10)] = 1
        changed = ebi_state()
        changed["c"][12] = 2
        action = self.deliver(host, guest, changed, 100.17)[0]
        self.assertNotIn([1, 101, 17, 1], action["h"])
        self.assertEqual(guest.update(ebi_state(), now=100.18, visit=17)[0]["localAck"], 1)
        own, _ = host.update(ebi_state(inputs=[[1, 2, 3, 1, 1]]), now=100.2)
        self.assertEqual(own["localAck"], 0)
        own, _ = host.update(ebi_state(inputAck=[[2, 202, 7, 1]]), now=100.25)
        self.assertEqual(own["localAck"], 1)

    def test_follower_receipt_changes_only_after_complete_new_tick_commit(self):
        many = ebi_state()
        many["a"] = [[3, i+1, 1] + [2147483647] * 37 for i in range(32)]
        host, guest = MinigameSyncTests().pair(2, many)
        guest.update(many, now=100.01, visit=17)
        _, requests = guest.update({**many, "inputs": [[1, 2, 1, 1, 1]]}, now=100.05, visit=17)
        self.assertTrue(host.engine.receive(host.context(), requests[0], 100.05))
        changed = copy.deepcopy(many)
        changed["inputAck"] = [[1, 101, 17, 1]]
        for row in changed["a"]:
            row[17] -= 1
        _, packets = host.update(changed, now=100.1)
        self.assertGreater(len(packets), 1)
        prior = copy.deepcopy(guest.engine.graph)
        for packet in packets[:-1]:
            self.assertTrue(guest.engine.receive(guest.context(), packet, 100.1))
            self.assertEqual(guest.engine.graph, prior)
            self.assertEqual(guest.update(many, now=100.11, visit=17)[0]["localAck"], 0)
            self.assertEqual(guest.update(many, now=100.11, visit=17)[0]["fullTick"], 1)
        self.assertTrue(guest.engine.receive(guest.context(), packets[-1], 100.12))
        self.assertEqual(guest.update(many, now=100.13, visit=17)[0]["localAck"], 1)
        committed = copy.deepcopy(guest.engine.graph)
        forged_duplicate = copy.deepcopy(packets[0])
        forged_duplicate["h"] = [[1, 101, 17, 99]]
        self.assertFalse(guest.engine.receive(guest.context(), forged_duplicate, 100.14))
        self.assertEqual(guest.engine.graph, committed)
        self.assertEqual(guest.update(many, now=100.15, visit=17)[0]["localAck"], 1)
        self.assertEqual(guest.update(many, now=100.15, visit=17)[0]["fullTick"], 1)

    def test_members_filter_exact_lifecycle_character_generation_and_terminal(self):
        host, guest = self.pair()
        status, _ = host.update(ebi_state(), now=100.02)
        self.assertEqual(status["members"], [1, 2])
        mutations = (("online", False), ("teamId", "red"), ("roomId", 10),
                     ("interactionSession", 909), ("currentCharacter", "Goemon"))
        for key, value in mutations:
            saved = copy.deepcopy(host.roster[1])
            host.roster[1][key] = value
            self.assertEqual(host.update(ebi_state(), now=100.03)[0]["members"], [2])
            host.roster[1] = saved
        for key, value in (("r", 0), ("x", 1), ("g", 2), ("o", [1, 101, 17]), ("t", [2, 2, 202, 9])):
            saved = copy.deepcopy(host.roster[1][mg.METADATA_KEY])
            host.roster[1][mg.METADATA_KEY][key] = value
            self.assertEqual(host.update(ebi_state(), now=100.04)[0]["members"], [2])
            host.roster[1][mg.METADATA_KEY] = saved
        self.assertEqual(host.update(ebi_state(result=2), now=100.05)[0]["members"], [])

    def test_member_status_sorted_unique_bounded_and_legacy_games_keep_cadence(self):
        host, guest = self.pair()
        for cid in range(3, 75):
            host.roster[cid] = {**copy.deepcopy(host.roster[1]), "interactionSession": cid*101}
            host.roster[cid][mg.METADATA_KEY].update(s=cid*101, visit=cid+10)
        members = host.update(ebi_state(), now=100.02)[0]["members"]
        self.assertEqual(members, list(range(1, 65)))
        for game in (1, 3):
            host, guest = MinigameSyncTests().pair(game)
            self.assertEqual(host.update(state(timer=890), now=100.19)[1], [])
            _, packets = host.update(state(timer=890), now=100.2)
            self.assertEqual(packets[0]["type"], mg.PACKET_TYPE)
            self.assertNotIn("b", packets[0])
            self.assertEqual(len(host.update(state(edge=1), now=100.25)[1]), 1)

    def test_width_cadence_bytes_and_envelope_limits(self):
        host, guest = self.pair()
        largest = ebi_state()
        largest["a"] = [[3, i+1, 1] + [-2147483648] * 37 for i in range(64)]
        self.assertIsNotNone(mg.native_state(largest))
        self.assertIsNone(mg.native_state({**largest, "a": [[3, 1, 1] + [0] * 38]}))
        self.assertIsNone(mg.native_state({**largest, "c": [0] * 33}))
        packets = host.engine._chunks(host.context(), mg.native_state(largest), 2)
        self.assertTrue(packets)
        self.assertLessEqual(len(packets), 8)
        self.assertLessEqual(sum(mg.wire_bytes(p) for p in packets), 32768)
        self.assertTrue(all(mg.wire_bytes(p) <= 4096 for p in packets))
        self.assertEqual(sum(len(p["a"]) for p in packets), 64)
        for frame in range(1, 30):
            moving = ebi_state()
            moving["a"][2][3] += frame
            moving["a"][2][32] += frame
            self.assertEqual(host.update(moving, now=100+frame/30)[1], [])
        packets = self.deliver(host, guest, moving, 101)
        for packet in host.sent:
            self.assertEqual(packet["clientId"], 2)
            self.assertEqual(packet["targetTeamId"], "blue")
            self.assertNotIn("targetClientId", packet)
            self.assertNotIn("addToQueue", packet)
        changed = copy.deepcopy(moving)
        changed["c"][12] = 2
        _, pending = host.update(changed, now=101.05)
        action = pending[0]
        previous = copy.deepcopy(guest.engine.graph)
        for changes in ({"g": 2}, {"o": [2, 999, 7]}, {"targetTeamId": "red"},
                        {"clientId": 77}, {"targetClientId": 1}, {"addToQueue": False},
                        {"b": True}, {"b": action["k"]}, {"t": [1, 2, 202, 7]}):
            self.assertFalse(guest.engine.receive(guest.context(), {**action, **changes}, 101.06))
            self.assertEqual(guest.engine.graph, previous)
        self.assertTrue(guest.engine.receive(guest.context(), action, 101.07))

    def test_tombstone_only_header_also_obeys_individual_packet_budget(self):
        host, _ = self.pair()
        host.cid = host.session = mg.MAX_INT
        host.team = "t" * 64
        host.engine.t = (2, mg.MAX_INT, mg.MAX_INT, mg.MAX_INT)
        host.engine.o = host.engine.t[1:]
        host.engine.graph["a"] = [[3, mg.MAX_INT-i, mg.MAX_INT] + [0] * 37 for i in range(64)]
        host.engine.acks = {(mg.MAX_INT-i, mg.MAX_INT, mg.MAX_INT): mg.MAX_INT for i in range(32)}
        empty = ebi_state(a=[], c=[2]+[mg.MAX_INT]*11+[3])
        packets = host.engine._chunks(host.context(), mg.native_state(empty), 2, True)
        self.assertTrue(packets)
        self.assertEqual(packets[0]["a"], [])
        self.assertEqual(len(packets[0]["z"]), 64)
        self.assertTrue(all(mg.wire_bytes(packet) <= mg.PACKET_BYTES for packet in packets))

    def test_large_lifetime_replacement_falls_back_to_complete_checkpoint(self):
        roster = {}
        host = Peer(mg.MAX_INT, roster, 2, mg.ROOMS[2], "t"*64)
        host.session = mg.MAX_INT
        before = ebi_state(c=[2]+[mg.MAX_INT]*11+[3])
        before["a"] = [[3 if i<2 else 4, mg.MAX_INT-i, mg.MAX_INT] + [-2147483648]*(37 if i<2 else 29)
                       for i in range(64)]
        host.update(before, visit=mg.MAX_INT)
        previous = copy.deepcopy(host.engine.graph)
        after = copy.deepcopy(before)
        for row in after["a"]:
            row[2] -= 1
        self.assertEqual(host.engine._chunks(host.context(), mg.native_state(after), 2, True), [])
        _, failed = host.update(after, now=100.05, success=False, visit=mg.MAX_INT)
        self.assertTrue(failed)
        self.assertEqual(failed[0]["type"], mg.PACKET_TYPE)
        self.assertEqual(host.engine.graph, previous)
        self.assertEqual((host.engine.tick, host.engine.last_full_wire), (1, 100))
        _, packets = host.update(after, now=100.1, visit=mg.MAX_INT)
        self.assertEqual(packets, failed)
        self.assertTrue(all(mg.wire_bytes(p) <= mg.PACKET_BYTES for p in packets))
        self.assertLessEqual(sum(mg.wire_bytes(p) for p in packets), mg.MAX_GRAPH_BYTES)
        self.assertEqual(host.engine.graph["a"], after["a"])
        self.assertEqual((host.engine.tick, host.engine.last_full_wire, host.engine.graph["fullTick"]), (2, 100.1, 2))

    def test_identical_full_refreshes_full_tick_but_partial_full_cannot(self):
        many = ebi_state()
        many["a"] = [[3, i+1, 1] + [2147483647]*37 for i in range(32)]
        host, guest = MinigameSyncTests().pair(2, many)
        guest.update(many, now=100.01, visit=17)
        for second in range(1, 4):
            now = 100+second
            _, packets = host.update(many, now=now)
            old_tick = guest.update(many, now=now, visit=17)[0]["fullTick"]
            for packet in packets[:-1]:
                self.assertTrue(guest.engine.receive(guest.context(), packet, now))
                self.assertEqual(guest.update(many, now=now, visit=17)[0]["fullTick"], old_tick)
            self.assertTrue(guest.engine.receive(guest.context(), packets[-1], now))
            self.assertEqual(guest.update(many, now=now, visit=17)[0]["fullTick"], packets[0]["k"])
            self.assertEqual(guest.engine.graph["a"], many["a"])


class GoemonActionTests(unittest.TestCase):
    def pair(self, supplied=None):
        current = supplied or goemon_state()
        host, guest = MinigameSyncTests().pair(1, current)
        guest.update(current, now=100.01, visit=17)
        return host, guest

    def deliver(self, host, guest, supplied, now, kind=mg.ACTION_TYPE):
        _, packets = host.update(supplied, now=now)
        self.assertTrue(packets)
        for packet in packets:
            self.assertEqual(packet["type"], kind)
            self.assertTrue(guest.engine.receive(guest.context(), packet, now))
        self.assertEqual(host.engine.graph["a"], guest.engine.graph["a"])
        return packets

    def test_exact_game_version_and_controller_width_select_action_transport(self):
        for game, controller in ((1, [1, 1, 0, 2900, 0]), (1, [2, 1, 0, 2900]),
                                 (1, [2, 1, 0, 2900, 0, 0]), (3, [2, 1, 0, 2900, 0])):
            with self.subTest(game=game, controller=controller):
                before = goemon_state(c=controller)
                host, _ = MinigameSyncTests().pair(game, before)
                changed = copy.deepcopy(before)
                changed["a"][0][30] += 1
                self.assertEqual(host.update(changed, now=100.05)[1], [])
                _, packets = host.update(changed, now=100.2)
                self.assertEqual(packets[0]["type"], mg.PACKET_TYPE)
                self.assertNotIn("b", packets[0])
        self.assertTrue(mg.action_v2(1, goemon_state()["c"]))
        self.assertTrue(mg.action_v2(2, ebi_state()["c"]))
        self.assertFalse(mg.action_v2(1, ebi_state()["c"]))

    def test_motion_clocks_child_pose_and_flight_sequence_wait_for_full(self):
        host, guest = self.pair()
        moving = goemon_state()
        moving["c"][2:4] = [20, 2870]
        for row in moving["a"][:6]:
            for index in (6, 9, *range(11, 18), *range(20, 29)):
                row[index] += 23
        for index in (*range(8, 15), 16, 17, 18):
            moving["a"][6][index] += 7
        self.assertEqual(host.update(moving, now=100.2)[1], [])
        # A controller edge must not smuggle unrelated continuously moving rows.
        moving["c"][1] = 5
        packet = self.deliver(host, guest, moving, 100.25)[0]
        self.assertEqual((packet["a"], packet["z"]), ([], []))
        self.assertEqual(guest.engine.graph["a"], goemon_state()["a"])
        self.assertEqual(guest.engine.graph["fullTick"], 1)
        self.deliver(host, guest, moving, 101, mg.PACKET_TYPE)
        self.assertEqual(guest.engine.graph["a"], moving["a"])
        self.assertEqual(guest.engine.graph["fullTick"], 3)

    def test_target_phase_schedule_completion_child_and_restart_edges(self):
        for index in (3, 4, 5, 7, 8, 10, 18, 19, 29, 30):
            with self.subTest(index=index):
                host, guest = self.pair()
                changed = goemon_state()
                changed["a"][2][index] += 1
                packet = self.deliver(host, guest, changed, 100.05)[0]
                self.assertEqual(packet["a"], [changed["a"][2]])
        current = goemon_state()
        current["a"][0][4] = 4
        host, guest = self.pair(current)
        for count in range(1, 4):
            current["a"][0][6] = 0  # Repeated native phase-4 HIT reset.
            current["a"][0][30] += 1
            packet = self.deliver(host, guest, current, 100 + count * .05)[0]
            self.assertEqual(packet["a"], [current["a"][0]])
            self.assertEqual(packet["a"][0][4], 4)

    def test_visual_launch_stop_pop_lifetime_and_held_anchor_edges(self):
        for index in (1, 2, 3, 4, 5, 6, 7, 15, 19):
            with self.subTest(index=index):
                host, guest = self.pair()
                changed = goemon_state()
                changed["a"][6][index] += 1
                packet = self.deliver(host, guest, changed, 100.05)[0]
                self.assertEqual(packet["a"], [changed["a"][6]])
                expected = [goemon_state()["a"][6][:3]] if index in (1, 2) else []
                self.assertEqual(packet["z"], expected)
        current = goemon_state()
        current["a"][6][7] = 1
        host, guest = self.pair(current)
        current["a"][6][14] += 1
        self.assertEqual(self.deliver(host, guest, current, 100.2)[0]["a"], [current["a"][6]])
        # First free-flight input fixes the launch serial. Checkpoint poseSeq
        # and velocity updates during that flight cannot restart it.
        current["a"][6][7] = 0
        current["a"][6][19] += 1
        self.deliver(host, guest, current, 100.25)
        for frame in range(1, 15):
            current["a"][6][14] += 1
            current["a"][6][8] += 4096
            current["a"][6][17] -= 1
            self.assertEqual(host.update(current, now=100.25 + frame / 30)[1], [])
        current["a"][6][6] = 0
        self.assertEqual(self.deliver(host, guest, current, 100.8)[0]["a"], [current["a"][6]])
        current["a"].pop()
        packet = self.deliver(host, guest, current, 100.85)[0]
        self.assertEqual((packet["a"], packet["z"]), ([], [[2, 1, 1]]))

    def test_core_topology_changes_publish_full_under_floor_and_keep_native_order(self):
        intro = goemon_state(core=0, c=[2, 3, 12, -1, 0], running=0)
        host, guest = self.pair(intro)
        gameplay = goemon_state()
        self.assertEqual(host.update(gameplay, now=100.049)[1], [])
        packets = self.deliver(host, guest, gameplay, 100.05, mg.PACKET_TYPE)
        self.assertNotIn("b", packets[0])
        self.assertEqual(guest.engine.graph["a"], gameplay["a"])
        self.assertEqual([row[:3] for row in guest.engine.graph["a"][:6]],
                         [[1, i, 1] for i in range(6)])
        gameplay["a"][2][30] += 1
        self.deliver(host, guest, gameplay, 100.1)
        self.assertEqual(guest.engine.graph["a"], gameplay["a"])
        # Generation replacement is another core identity-set change.
        gameplay["a"][4][2] += 1
        self.deliver(host, guest, gameplay, 100.15, mg.PACKET_TYPE)
        self.deliver(host, guest, intro, 100.2, mg.PACKET_TYPE)
        self.assertEqual(guest.engine.graph["a"], intro["a"])

    def test_sparse_composes_only_published_rows_and_visual_tombstones(self):
        host, guest = self.pair(goemon_state(visuals=2))
        current = goemon_state(visuals=2)
        current["a"][0][11] += 100
        current["a"][1][30] += 1
        current["a"][6][14] += 1
        current["a"].pop()
        packet = self.deliver(host, guest, current, 100.05)[0]
        self.assertEqual(packet["a"], [current["a"][1]])
        self.assertEqual(packet["z"], [[2, 2, 1]])
        expected = goemon_state()["a"]
        expected[1] = current["a"][1]
        self.assertEqual(guest.engine.graph["a"], expected)
        current["a"].append(goemon_state(visuals=2)["a"][-1])
        current["a"][-1][2] = 2
        self.deliver(host, guest, current, 100.1)
        self.assertEqual(guest.engine.graph["a"][:6], expected[:6])
        self.deliver(host, guest, current, 101, mg.PACKET_TYPE)
        self.assertEqual(guest.engine.graph["a"], current["a"])

    def test_missing_base_recovery_duplicate_reorder_version_game_and_generation(self):
        host, guest = self.pair()
        changed = goemon_state()
        changed["a"][0][30] += 1
        _, lost = host.update(changed, now=100.05)
        changed["a"][1][30] += 1
        _, newer = host.update(changed, now=100.1)
        previous = copy.deepcopy(guest.engine.graph)
        self.assertFalse(guest.engine.receive(guest.context(), newer[0], 100.1))
        for updates in ({"c": [1, 1, 0, 2900, 0]}, {"c": [2, 1, 0, 2900]},
                        {"c": ebi_state()["c"]}, {"t": [3, 2, 202, 7]}, {"g": 2},
                        {"targetTeamId": "red"}, {"targetClientId": 1}, {"addToQueue": False},
                        {"clientId": 77}, {"b": True}):
            self.assertFalse(guest.engine.receive(guest.context(), {**lost[0], **updates}, 100.11), updates)
            self.assertEqual(guest.engine.graph, previous)
        # Core additions/removals are forbidden in sparse packets too.
        forged = {**lost[0], "z": [[1, 5, 1]]}
        self.assertFalse(guest.engine.receive(guest.context(), forged, 100.11))
        self.assertEqual(guest.engine.graph, previous)
        self.assertTrue(guest.engine.receive(guest.context(), lost[0], 100.12))
        self.assertTrue(guest.engine.receive(guest.context(), newer[0], 100.13))
        self.assertFalse(guest.engine.receive(guest.context(), lost[0], 100.14))
        self.assertFalse(guest.engine.receive(guest.context(), newer[0], 100.14))
        guest.engine.cache.clear()
        changed["a"][2][30] += 1
        _, missing = host.update(changed, now=100.2)
        self.assertFalse(guest.engine.receive(guest.context(), missing[0], 100.2))
        self.deliver(host, guest, changed, 101, mg.PACKET_TYPE)
        self.assertEqual(guest.engine.graph["a"], changed["a"])

    def largest(self):
        current = goemon_state(visuals=18)
        current["c"][2:4] = [32767, 3000]
        for row in current["a"]:
            row[2] = mg.MAX_INT
            if row[0] == 1:
                row[6:9] = [32767] * 3
                for index in (9, 10, 11, 12, 13, 15, 16, 17, 20, 21, 22, 24, 25, 26, 29):
                    row[index] = -33554432
                row[30] = mg.MAX_INT
            else:
                row[3:5] = [mg.MAX_INT - row[1], mg.MAX_INT]
                for index in (8, 9, 10, 16, 17, 18):
                    row[index] = -33554432
                row[14:16] = [mg.MAX_INT, mg.MAX_INT]
                row[11:14] = [1023] * 3
                row[19] = mg.MAX_INT
        return current

    def test_native_width_worst_case_bytes_partial_failure_and_atomic_retry(self):
        before = self.largest()
        self.assertEqual([len(row) for row in before["a"]], [31] * 6 + [20] * 18)
        self.assertIsNotNone(mg.native_state(before))
        host, guest = self.pair(before)
        self.assertGreater(len(host.sent), 1)
        changed = copy.deepcopy(before)
        for row in changed["a"]:
            row[30 if row[0] == 1 else 19] -= 1
        previous = copy.deepcopy(host.engine.graph)
        _, packets = host.engine.update(host.context(), 1, 7, changed, 100.05)
        self.assertGreater(len(packets), 1)
        for index, packet in enumerate(packets):
            host.engine.packet_send_result(packet, index != 1)
        self.assertEqual((host.engine.tick, host.engine.last_full_wire), (1, 100))
        self.assertEqual(host.engine.graph, previous)
        self.assertEqual(host.update(changed, now=100.08)[1], [])
        _, retry = host.update(changed, now=100.1)
        self.assertEqual(packets, retry)
        prior_guest = copy.deepcopy(guest.engine.graph)
        for packet in reversed(retry[1:]):
            self.assertTrue(guest.engine.receive(guest.context(), packet, 100.1))
            self.assertEqual(guest.engine.graph, prior_guest)
        self.assertTrue(guest.engine.receive(guest.context(), retry[-1], 100.11))
        self.assertTrue(guest.engine.receive(guest.context(), retry[0], 100.12))
        self.assertEqual(host.engine.graph["a"], guest.engine.graph["a"])
        prior_host = copy.deepcopy(host.engine.graph)
        _, failed_full = host.update(changed, now=101, success=False)
        self.assertEqual(host.engine.graph, prior_host)
        self.assertEqual(host.engine.last_full_wire, 100)
        retry_full = self.deliver(host, guest, changed, 101.05, mg.PACKET_TYPE)
        self.assertEqual(failed_full, retry_full)
        for packets in (host.sent[:len(failed_full)], retry, retry_full):
            self.assertLessEqual(len(packets), mg.MAX_PARTS)
            self.assertLessEqual(sum(mg.wire_bytes(packet) for packet in packets), mg.MAX_GRAPH_BYTES)
            self.assertTrue(all(mg.wire_bytes(packet) <= mg.PACKET_BYTES for packet in packets))

    def test_motion_only_one_hz_edges_at_floor_and_running_receipt_result_cues(self):
        host, guest = self.pair()
        current = goemon_state()
        fulls = []
        for frame in range(1, 151):
            current["c"][3] -= 1
            current["a"][0][6] += 1
            current["a"][0][11] += 1024
            current["a"][6][14] += 1
            _, packets = host.update(current, now=100 + frame / 30)
            fulls.extend(packets)
        self.assertEqual(len(fulls), 5)
        self.assertTrue(all(p["type"] == mg.PACKET_TYPE for p in fulls))
        sent_at = []
        for millisecond in range(1, 1001):
            current["a"][0][30] += 1
            now = 105 + millisecond / 1000
            _, packets = host.update(current, now=now)
            if packets:
                sent_at.append(now)
        self.assertEqual(len(sent_at), 20)
        self.assertTrue(all(b - a + 1e-9 >= mg.FLOOR for a, b in zip(sent_at, sent_at[1:])))
        host, guest = self.pair()
        current = goemon_state(running=0)
        self.assertEqual(self.deliver(host, guest, current, 100.05)[0]["r"], 0)
        host.update({**current, "inputs": [[1, 1, 0, 1]]}, now=100.06)
        current["inputAck"] = [[2, 202, 7, 1]]
        self.assertEqual(self.deliver(host, guest, current, 100.1)[0]["h"], [[2, 202, 7, 1]])
        current["c"][4] = 1
        self.assertEqual(self.deliver(host, guest, current, 100.15)[0]["c"][4], 1)
        current["result"] = 1
        _, packets = host.update(current, now=100.2)
        action = next(packet for packet in packets if packet["type"] == mg.ACTION_TYPE)
        self.assertEqual(action["x"], 1)
        for packet in host.sent:
            self.assertEqual((packet["clientId"], packet.get("targetTeamId")), (2, "blue"))
            self.assertTrue(packet["quiet"])
            self.assertNotIn("targetClientId", packet)
            self.assertNotIn("addToQueue", packet)


class MinigameBridgeTests(unittest.TestCase):
    def setUp(self):
        self.clock = mock.patch("time.monotonic", return_value=100).start()
        self.addCleanup(mock.patch.stopall)
        self.client = load_client(0x1e0)
        self.addCleanup(self.client.disconnect)

    def test_complete_metadata_on_every_update_and_native_bridge_retry(self):
        status = json.loads(self.client.update_minigame(1, 7, json.dumps(state())))
        self.assertEqual(status["isOwner"], 1)
        wires = [json.loads(x[:-1]) for x in self.client._sock.sent]
        self.assertEqual([p["type"] for p in wires], ["UPDATE_CLIENT_STATE", mg.PACKET_TYPE])
        self.client.set_character("Goemon")
        m = json.loads(self.client._sock.sent[-1][:-1])["state"][mg.METADATA_KEY]
        self.assertEqual(m["t"], [1, 1, 101, 7])
        self.assertEqual(m["r"], 1)
        self.client.set_save_loaded(False)
        m = json.loads(self.client._sock.sent[-1][:-1])["state"][mg.METADATA_KEY]
        self.assertEqual(m["t"], [])

    def test_receiver_dispatch_coalesces_and_heartbeat_has_no_echo(self):
        host, guest = MinigameSyncTests().pair()
        payload = {**host.roster[2], "currentRoomId": 0x1e0}
        self.assertTrue(self.client._merge_client_state(2, payload))
        self.client.set_local_room(10)
        self.assertTrue(self.client.minigame_prepare_join(1, 2, 202, 7))
        framed = b"".join((json.dumps(p, separators=(",", ":")) + "\0").encode() for p in host.sent)
        framed += b'{"type":"HEARTBEAT"}\0'
        original = self.client._sock
        receiver = RecordingSocket([framed[:20], framed[20:]])
        self.client._sock = receiver
        with mock.patch.object(self.client, "_do_disconnect"):
            self.client._recv_loop(receiver)
        self.client._sock = original
        self.assertEqual(receiver.sent, [])
        self.assertIsNotNone(self.client._minigame.graph)
        self.assertTrue(self.client._recv_queue.empty())

    def test_actual_two_client_action_dispatch_framing_health_and_receipt_bridge(self):
        host = load_client(mg.ROOMS[2])
        self.addCleanup(host.disconnect)
        host._client_id, host._interaction_session = 2, 202
        host.set_local_room_character(mg.ROOMS[2], "Ebisumaru")
        host._sock.sent.clear()
        host.update_minigame(2, 7, json.dumps(ebi_state()))
        metadata = next(json.loads(w[:-1])["state"] for w in reversed(host._sock.sent)
                        if json.loads(w[:-1])["type"] == "UPDATE_CLIENT_STATE")
        self.client._merge_client_state(2, metadata)
        self.client.set_local_room_character(10, "Ebisumaru")
        self.assertTrue(self.client.minigame_prepare_join(2, 2, 202, 7))
        self.client.set_local_room_character(mg.ROOMS[2], "Ebisumaru")

        def route(wires):
            framed = b"".join(wires)
            original = self.client._sock
            receiver = RecordingSocket([framed[:19], framed[19:]])
            self.client._sock = receiver
            with mock.patch.object(self.client, "_do_disconnect"):
                self.client._recv_loop(receiver)
            self.client._sock = original
            self.assertEqual(receiver.sent, [])
            self.assertTrue(self.client._recv_queue.empty())

        route([wire for wire in host._sock.sent if mg.PACKET_TYPE.encode() in wire])
        status = json.loads(self.client.update_minigame(2, 17, json.dumps(ebi_state())))
        self.assertEqual(status["members"], [1, 2])
        metadata = next(json.loads(w[:-1])["state"] for w in reversed(self.client._sock.sent)
                        if json.loads(w[:-1])["type"] == "UPDATE_CLIENT_STATE")
        host._merge_client_state(1, metadata)
        self.clock.return_value = 100.05
        self.client._sock.sent.clear()
        self.client.update_minigame(2, 17, json.dumps(ebi_state(inputs=[[1, 2, 3, 1, 1]])))
        request = next(json.loads(w[:-1]) for w in self.client._sock.sent if mg.INPUT_TYPE.encode() in w)
        self.assertTrue(host._minigame.receive(host._boss_context(), request, 100.05))
        changed = ebi_state(inputAck=[[1, 101, 17, 1]])
        changed["c"][12] = 2
        host._sock.sent.clear()
        host.update_minigame(2, 7, json.dumps(changed))
        actions = [wire for wire in host._sock.sent if mg.ACTION_TYPE.encode() in wire]
        self.assertTrue(actions)
        for wire in actions:
            self.assertTrue(wire.endswith(b"\0"))
            self.assertLessEqual(len(wire), 4096)
        route(actions)
        status = json.loads(self.client.update_minigame(2, 17, json.dumps(ebi_state())))
        self.assertEqual((status["localAck"], status["c"][12]), (1, 2))
        self.assertEqual(status["a"], ebi_state()["a"])
        self.assertEqual(self.client.HOT_PACKET_MAX_BYTES[mg.ACTION_TYPE], 4096)

    def test_room_exit_disconnect_and_team_change_clear_session(self):
        self.client.update_minigame(1, 7, json.dumps(state()))
        self.client.set_local_room(10)
        self.assertIsNone(self.client._minigame.t)
        self.client.set_local_room(0x1e0)
        self.client.update_minigame(1, 8, json.dumps(state()))
        self.client._team_id = "red"
        self.client.update_minigame(1, 9, json.dumps(state()))
        self.assertEqual(self.client._minigame.t, (1, 1, 101, 9))
        self.client.disconnect()
        self.assertIsNone(self.client._minigame.t)


if __name__ == "__main__":
    unittest.main()
