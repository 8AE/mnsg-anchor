"""Transient minigame invitations; no game or server required."""

import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "py" / "anchor_mnsg.py"
sys.path.insert(0, str(MODULE_PATH.parent))


class RecordingSocket:
    def __init__(self, received=()):
        self.sent = []
        self.received = iter(received)

    def sendall(self, data):
        self.sent.append(data)

    def close(self):
        pass

    def recv(self, _size):
        return next(self.received, b"")


def load_client(room=10):
    spec = importlib.util.spec_from_file_location("minigame_client", MODULE_PATH)
    client = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(client)
    client._sock = RecordingSocket()
    client._connected = True
    client._client_id = 1
    client._interaction_session = 101
    client._world_roster_session = 101
    client._team_id = "blue"
    client.set_save_loaded(True)
    client.set_local_room(room)
    client._sock.sent.clear()
    return client


def verify_native_ebisumaru_invitation(pending_native, ready_native):
    """Bridge real C cold-start captures through invitation Yes preparation.

    The native runner supplies these values; readiness is never manufactured
    here. An unsuccessful native capture is submitted as JSON null, matching
    the coordinator. This checks the same invitation before and after startup.
    """
    assert pending_native is None or pending_native["ready"] == 0
    assert ready_native["ready"] == 1 and ready_native["running"] == 1
    assert {row[0] for row in ready_native["a"]} >= {1, 2}
    host = load_client(room=0x1e1)
    guest = load_client(room=0x166)
    host._client_id = 2
    host._interaction_session = 202
    host._world_roster_session = 202

    def exchange_metadata():
        updates = [json.loads(wire[:-1]) for wire in host._sock.sent
                   if json.loads(wire[:-1]).get("type") == "UPDATE_CLIENT_STATE"]
        assert updates
        assert guest._merge_client_state(2, updates[-1]["state"])
        return updates[-1]["state"][host.anchor_minigame.METADATA_KEY]

    try:
        with mock.patch("time.monotonic", return_value=100.0) as clock:
            pending = json.loads(host.update_minigame(2, 7, json.dumps(pending_native)))
            assert pending["owner"] == 2 and pending["ready"] == 0
            assert exchange_metadata()["r"] == 0
            assert host.set_minigame_active(2, 7)
            invite_wire = host._sock.sent[-1]
            invite = json.loads(invite_wire[:-1])
            assert invite["type"] == "MNSG_MINIGAME_INVITE" and invite["game"] == 2
            assert guest._receive_minigame_invite(invite)
            shown = json.loads(guest.get_minigame_invitation_json())
            identity = (shown["cid"], shown["session"], shown["seq"])
            assert identity == (2, 202, invite["seq"])
            assert shown["gameVisit"] == 7
            # C retains its accepted Yes while this exact prepare gate rejects.
            assert not guest.minigame_prepare_join(2, 2, 202, 7)
            assert guest.minigame_invitation_is_current(*identity)
            assert json.loads(guest.get_minigame_invitation_json()) == shown

            clock.return_value = 100.1
            ready = json.loads(host.update_minigame(2, 7, json.dumps(ready_native)))
            assert ready["ready"] == 1 and ready["isOwner"] == 1
            assert exchange_metadata()["r"] == 1
            assert guest.minigame_invitation_is_current(*identity)
            assert guest.minigame_prepare_join(2, 2, 202, 7)
            assert list(guest._minigame.t) == ready["token"] == [2, 2, 202, 7]
            assert guest._minigame.prepared and not guest._minigame.visit
            # Neither preparing nor retrying needs a second invitation packet.
            invites = [json.loads(wire[:-1]) for wire in host._sock.sent
                       if json.loads(wire[:-1]).get("type") == "MNSG_MINIGAME_INVITE"]
            assert invites == [invite]
            guest.dismiss_minigame_invitation(*identity)
            assert guest.get_minigame_invitation_json() == ""
    finally:
        host.disconnect()
        guest.disconnect()


def verify_native_yae_invitation(ready_native):
    """Relay a production native Mermaid capture into the invitation path.

    The observer starts as Goemon in ordinary Kegon. Story and reward flags
    are deliberately absent from transport context: accepting an existing
    native challenge depends on the chosen host session, not that save state.
    """
    assert ready_native["ready"] == 1 and ready_native["running"] == 1
    host, guest = load_client(0x154), load_client(0x154)
    host._client_id = 2
    host._interaction_session = host._world_roster_session = 202
    cursor = 0

    def relay():
        nonlocal cursor
        wire = b"".join(host._sock.sent[cursor:])
        cursor = len(host._sock.sent)
        receiver = RecordingSocket([wire[i:i + 17] for i in range(0, len(wire), 17)])
        original = guest._sock
        guest._sock = receiver
        try:
            with mock.patch.object(guest, "_do_disconnect"):
                guest._recv_loop(receiver)
        finally:
            guest._sock = original
        assert receiver.sent == []

    try:
        with mock.patch("time.monotonic", return_value=100.0) as clock:
            assert host.set_character("Yae") and guest.set_character("Goemon")
            pending = json.loads(host.update_minigame(4, 7, "null"))
            assert pending["owner"] == 2 and pending["ready"] == 0
            assert host.update_client_state('{"collisionDisabled":1}')
            assert host.set_minigame_active(4, 7)
            relay()
            shown = json.loads(guest.get_minigame_invitation_json())
            identity = (shown["cid"], shown["session"], shown["seq"])
            assert identity == (2, 202, 1) and shown["gameVisit"] == 7
            assert guest._local_character == "Goemon"
            assert guest._player_states[2]["collisionDisabled"] == 1
            assert not guest.minigame_prepare_join(4, 2, 202, 7)
            assert guest.minigame_invitation_is_current(*identity)

            clock.return_value = 100.1
            ready = json.loads(host.update_minigame(4, 7, json.dumps(ready_native)))
            assert ready["ready"] == 1 and ready["isOwner"] == 1
            relay()
            assert guest.minigame_invitation_is_current(*identity)
            assert not guest.minigame_prepare_join(4, 2, 303, 7)
            assert not guest.minigame_prepare_join(4, 2, 202, 8)
            assert guest.minigame_prepare_join(4, 2, 202, 7)
            assert list(guest._minigame.t) == ready["token"] == [4, 2, 202, 7]
            assert guest._minigame.prepared and guest._minigame.visit == 0
            assert guest._minigame.graph["c"] == ready_native["c"]
            invites = [json.loads(wire[:-1]) for wire in host._sock.sent
                       if json.loads(wire[:-1])["type"] == "MNSG_MINIGAME_INVITE"]
            assert len(invites) == 1
            guest.dismiss_minigame_invitation(*identity)
            assert guest.get_minigame_invitation_json() == ""
    finally:
        host.disconnect()
        guest.disconnect()


class MinigameInvitationTransportTests(unittest.TestCase):
    def setUp(self):
        self.clock = mock.patch("time.monotonic", return_value=100.0).start()
        self.addCleanup(mock.patch.stopall)
        self.client = load_client()
        self.addCleanup(self.client.disconnect)
        self.peer_sequence = 3

    def peer(self, game=1, cid=2, session=202, **changes):
        self.peer_sequence += 1
        payload = {
            "currentRoomId": self.client.MINIGAME_ROOMS[game],
            "online": True, "isSaveLoaded": True, "teamId": "blue",
            "name": "Remote", "posSeq": self.peer_sequence,
            "posT": 100000 + self.peer_sequence,
            "interactionSession": session, "playerEpoch": 7,
        }
        payload.update(changes)
        if game == 4:
            payload.setdefault("currentCharacter", "Yae")
            payload.setdefault("mg", {"v": 1, "s": session, "q": self.peer_sequence,
                "r": 1, "t": [4, cid, session, 7], "o": [cid, session, 7],
                "g": 1, "visit": 7, "x": 0})
        self.assertTrue(self.client._merge_client_state(cid, payload, True))

    def packet(self, game=1, **changes):
        packet = {"type": "MNSG_MINIGAME_INVITE", "clientId": 2,
                  "targetTeamId": "blue", "game": game, "entered": True,
                  "session": 202, "seq": 1}
        if game == 4:
            packet["visit"] = 7
        packet.update(changes)
        return packet

    def route_bytes(self, *chunks):
        original = self.client._sock
        receiver = RecordingSocket(chunks)
        self.client._sock = receiver
        try:
            with mock.patch.object(self.client, "_do_disconnect"):
                self.client._recv_loop(receiver)
        finally:
            self.client._sock = original
        self.assertEqual(receiver.sent, [])

    def test_entry_exit_reentry_all_games(self):
        for game, room in self.client.MINIGAME_ROOMS.items():
            with self.subTest(game=game):
                self.client.set_local_room(room)
                if game == 4:
                    self.client.set_character("Yae")
                    self.client.update_minigame(4, game, json.dumps({
                        "ready": 1, "running": 1, "c": [4], "a": []}))
                self.assertTrue(self.client.set_minigame_active(game, game))
                wire = self.client._sock.sent[-1]
                self.assertLessEqual(len(wire), 256)  # Includes NUL framing.
                self.assertEqual(json.loads(wire[:-1]), {
                    "type": "MNSG_MINIGAME_INVITE", "clientId": 1,
                    "targetTeamId": "blue", "game": game,
                    "entered": True, "session": 101,
                    "seq": (game - 1) * 2 + 1,
                    "visit": game,
                })
                count = len(self.client._sock.sent)
                self.assertTrue(self.client.set_minigame_active(game, game))
                self.assertEqual(len(self.client._sock.sent), count)
                self.assertTrue(self.client.set_minigame_active(0))
                exit_wire = next(wire for wire in reversed(self.client._sock.sent)
                                 if json.loads(wire[:-1])["type"] == "MNSG_MINIGAME_INVITE")
                self.assertLessEqual(len(exit_wire), 256)
                self.assertEqual(json.loads(exit_wire[:-1])["game"], game)
                self.assertFalse(json.loads(exit_wire[:-1])["entered"])
        self.client.set_local_room(self.client.MINIGAME_ROOMS[1])
        self.assertTrue(self.client.set_minigame_active(1, 9))
        self.assertEqual(json.loads(self.client._sock.sent[-1][:-1])["seq"], 9)

    def test_sender_room_and_bounded_payload(self):
        self.assertFalse(self.client.set_minigame_active(1, 1))
        self.assertFalse(self.client._sock.sent)
        self.client.set_local_room(self.client.MINIGAME_ROOMS[1])
        self.assertFalse(self.client.set_minigame_active(1, 0))
        self.assertFalse(self.client.set_minigame_active(5, 1))
        self.assertTrue(self.client.set_minigame_active(1, 1))
        self.assertLessEqual(len(self.client._sock.sent[-1]), 256)
        maximal = {"type": "MNSG_MINIGAME_INVITE", "clientId": 0x7fffffff,
                   "targetTeamId": "t" * 64, "game": 3, "entered": True,
                   "session": 0x7fffffff, "seq": 0x7fffffff}
        serialized = (json.dumps(maximal, separators=(",", ":")) + "\0").encode()
        self.assertLessEqual(len(serialized), 256)
        self.assertEqual(self.client.HOT_PACKET_MAX_BYTES["MNSG_MINIGAME_INVITE"], 256)

    def test_failed_send_retries_and_same_room_new_visit(self):
        self.client.set_local_room(self.client.MINIGAME_ROOMS[3])
        with mock.patch.object(self.client, "_send_raw", side_effect=[False, True]) as send:
            self.assertFalse(self.client.set_minigame_active(3, 4))
            self.assertTrue(self.client.set_minigame_active(3, 4))
            self.assertEqual(send.call_args_list[0], send.call_args_list[1])
        self.assertTrue(self.client.set_minigame_active(3, 5))
        self.assertEqual(json.loads(self.client._sock.sent[-1][:-1])["seq"], 2)

    def test_atomic_room_character_metadata_and_failed_send_retry(self):
        self.assertTrue(self.client.set_character("Yae"))
        self.client._sock.sent.clear()
        room = self.client.MINIGAME_ROOMS[2]
        with mock.patch.object(self.client, "_send_raw", side_effect=[False, True]) as send:
            self.assertFalse(self.client.set_local_room_character(room, "Ebisumaru"))
            self.assertTrue(self.client._local_room_character_pending)
            self.assertFalse(self.client.set_minigame_active(2, 1))
            self.assertTrue(self.client.set_local_room_character(room, "Ebisumaru"))
            self.assertEqual(send.call_args_list[0], send.call_args_list[1])
        self.assertFalse(self.client._local_room_character_pending)
        state = send.call_args.args[0]["state"]
        self.assertEqual(state["currentRoomId"], room)
        self.assertEqual(state["currentCharacter"], "Ebisumaru")
        self.assertTrue(self.client.set_minigame_active(2, 1))

        self.client._sock.sent.clear()
        self.assertTrue(self.client.set_local_room_character(10, "Yae"))
        packet = json.loads(self.client._sock.sent[0][:-1])
        self.assertEqual(packet["type"], "UPDATE_CLIENT_STATE")
        self.assertEqual(packet["state"]["currentRoomId"], 10)
        self.assertEqual(packet["state"]["currentCharacter"], "Yae")
        self.assertEqual(len(self.client._sock.sent), 1)

    def test_receive_all_games_and_local_same_game_suppression(self):
        for game in self.client.MINIGAME_ROOMS:
            with self.subTest(game=game):
                self.peer(game=game)
                self.assertTrue(self.client._receive_minigame_invite(
                    self.packet(game=game, seq=game)))
                invitation = json.loads(self.client.get_minigame_invitation_json())
                self.assertEqual(invitation["game"], game)
                self.assertTrue(self.client.minigame_invitation_is_current(2, 202, game))
                self.client.dismiss_minigame_invitation(2, 202, game)
        self.client.set_local_room(self.client.MINIGAME_ROOMS[1])
        self.assertTrue(self.client.set_minigame_active(1, 1))
        self.peer(game=1)
        self.assertTrue(self.client._receive_minigame_invite(self.packet(seq=5)))
        self.assertEqual(self.client.get_minigame_invitation_json(), "")

    def test_metadata_grace_then_confirmation_and_exit(self):
        self.assertTrue(self.client._receive_minigame_invite(self.packet()))
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        self.peer()
        self.assertEqual(json.loads(self.client.get_minigame_invitation_json())["cid"], 2)
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(entered=False, seq=2)))
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        self.assertFalse(self.client._receive_minigame_invite(self.packet(seq=1)))

    def test_departure_blocks_only_stale_matching_transfer_target(self):
        for game, room in self.client.MINIGAME_ROOMS.items():
            with self.subTest(game=game):
                self.peer(game=game, posX=111, posY=-222, posZ=333)
                entry_seq = game * 10
                self.assertTrue(self.client._receive_minigame_invite(
                    self.packet(game=game, seq=entry_seq)))
                self.client.dismiss_minigame_invitation(2, 202, entry_seq)
                self.assertEqual(json.loads(self.client.get_transfer_target_json(2))["room"],
                                 room)  # Declining does not hide a fresh target.

                self.assertTrue(self.client._receive_minigame_invite(
                    self.packet(game=game, entered=False, seq=entry_seq + 1)))
                self.assertEqual(self.client._minigame_departures[2], (202, game))
                self.assertEqual(self.client.get_transfer_target_json(2), "{}")
                roster = json.loads(self.client.get_player_info_json())
                self.assertEqual(next(row["ct"] for row in roster if row["cid"] == 2), 0)

                self.assertTrue(self.client._receive_minigame_invite(
                    self.packet(game=game, seq=entry_seq + 2)))
                self.assertNotIn(2, self.client._minigame_departures)
                self.assertEqual(json.loads(self.client.get_transfer_target_json(2))["room"],
                                 room)

                self.assertTrue(self.client._receive_minigame_invite(
                    self.packet(game=game, entered=False, seq=entry_seq + 3)))
                self.peer(game=game, currentRoomId=10, posX=-123, posY=456,
                          posZ=-789)
                self.assertNotIn(2, self.client._minigame_departures)
                self.assertEqual(json.loads(self.client.get_transfer_target_json(2)),
                                 {"cid": 2, "room": 10, "x": -123,
                                  "y": 456, "z": -789})

    def test_departure_obsoletes_on_new_session_and_reset(self):
        self.peer(game=1, posX=1, posY=2, posZ=3)
        self.assertFalse(self.client._receive_minigame_invite(
            self.packet(entered=False, seq=0)))
        self.assertFalse(self.client._minigame_departures)
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(entered=False, seq=1)))
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        for malformed in (None, "not-a-session"):
            self.client._prune_minigame_departure(2, {
                "online": True, "isSaveLoaded": True,
                "roomId": self.client.MINIGAME_ROOMS[1],
                "interactionSession": malformed,
            })
            self.assertEqual(self.client._minigame_departures[2], (202, 1))
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(session=303, seq=1)))
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.peer(game=1, session=303, posX=4, posY=5, posZ=6)
        self.assertFalse(self.client._minigame_departures)
        self.assertEqual(json.loads(self.client.get_transfer_target_json(2))["room"],
                         self.client.MINIGAME_ROOMS[1])
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(session=303, entered=False, seq=2)))
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.client._reset_minigame_invitations()
        self.assertFalse(self.client._minigame_departures)

    def test_other_game_entry_keeps_old_room_departure_blocked(self):
        self.peer(game=1, posX=11, posY=22, posZ=33)
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(game=1, entered=False, seq=1)))
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(game=2, seq=2)))
        self.assertEqual(self.client._minigame_departures[2], (202, 1))
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")

        self.peer(game=2, posX=44, posY=55, posZ=66)
        self.assertNotIn(2, self.client._minigame_departures)
        self.assertEqual(json.loads(self.client.get_transfer_target_json(2)),
                         {"cid": 2, "room": self.client.MINIGAME_ROOMS[2],
                          "x": 44, "y": 55, "z": 66})

    def test_malformed_stale_room_team_and_disconnect(self):
        self.peer()
        for changes in ({"game": 0}, {"game": 5}, {"clientId": True},
                        {"seq": 0}, {"session": -1}, {"entered": 1},
                        {"targetTeamId": "red"}, {"clientId": 1},
                        {"targetClientId": 1}, {"targetClientId": None},
                        {"addToQueue": True}, {"addToQueue": False}):
            with self.subTest(changes=changes):
                self.assertFalse(self.client._receive_minigame_invite(
                    self.packet(**changes)))
        self.assertTrue(self.client._receive_minigame_invite(self.packet()))
        self.assertFalse(self.client._receive_minigame_invite(self.packet()))
        self.peer(currentRoomId=10)
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        self.peer()
        self.assertTrue(self.client._receive_minigame_invite(self.packet(seq=2)))
        self.assertTrue(self.client.get_minigame_invitation_json())
        self.peer(online=False)
        self.assertEqual(self.client.get_minigame_invitation_json(), "")

    def test_framed_dispatch_rejects_direct_queue_and_oversize(self):
        self.peer()
        for malformed in (self.packet(targetClientId=1),
                          self.packet(addToQueue=False),
                          self.packet(padding="x" * 180)):
            wire = (json.dumps(malformed, separators=(",", ":")) + "\0").encode()
            self.route_bytes(wire[:13], wire[13:])
            self.assertEqual(self.client._minigame_seen, {})
            self.assertEqual(self.client.get_minigame_invitation_json(), "")
        sender = load_client(room=self.client.MINIGAME_ROOMS[1])
        self.addCleanup(sender.disconnect)
        sender._client_id = 2
        sender._interaction_session = 202
        self.assertTrue(sender.set_minigame_active(1, 1))
        wire = sender._sock.sent[-1]
        self.assertTrue(wire.endswith(b"\0"))
        self.assertLessEqual(len(wire), 256)
        self.assertNotIn(b"targetClientId", wire)
        self.assertNotIn(b"addToQueue", wire)
        self.route_bytes(wire[:11], wire[11:])
        self.assertEqual(json.loads(self.client.get_minigame_invitation_json()), {
            "cid": 2, "session": 202, "seq": 1,
            "game": 1, "gameVisit": 1, "name": "Remote",
        })

    def test_save_unload_clears_and_emits_exit(self):
        self.client.set_local_room(self.client.MINIGAME_ROOMS[2])
        self.assertTrue(self.client.set_minigame_active(2, 1))
        self.client.set_save_loaded(False)
        invitations = [json.loads(wire[:-1]) for wire in self.client._sock.sent
                       if b'MNSG_MINIGAME_INVITE' in wire]
        self.assertFalse(invitations[-1]["entered"])
        self.assertFalse(self.client._minigame_events)

    def test_reconnect_session_and_expired_candidate(self):
        self.peer()
        self.assertTrue(self.client._receive_minigame_invite(self.packet()))
        self.assertTrue(self.client.get_minigame_invitation_json())
        self.peer(session=303)
        self.assertFalse(self.client.minigame_invitation_is_current(2, 202, 1))
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(session=303, seq=1)))
        self.assertTrue(self.client.get_minigame_invitation_json())
        self.assertFalse(self.client._receive_minigame_invite(
            self.packet(session=202, seq=2)))
        self.client._reset_minigame_invitations()
        self.assertFalse(self.client._minigame_events)
        self.assertFalse(self.client._minigame_seen)
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(session=404, seq=1)))
        self.clock.return_value = 106.0
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        self.peer(session=404)
        self.assertEqual(self.client.get_minigame_invitation_json(), "")

    def test_yae_same_room_observer_requires_explicit_active_host(self):
        self.client.set_local_room(0x154)
        self.client._sock.sent.clear()
        self.assertFalse(self.client.set_minigame_active(4, 7))
        self.assertEqual(self.client._sock.sent, [])
        empty = {"v": 1, "s": 202, "q": 100, "r": 0, "t": []}
        self.peer(game=4, mg=empty)
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        self.assertTrue(self.client._receive_minigame_invite(self.packet(game=4, visit=7)))
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        active = {"v": 1, "s": 202, "q": 101, "r": 0,
                  "t": [4, 2, 202, 7], "o": [2, 202, 7], "g": 1, "visit": 7, "x": 0}
        self.peer(game=4, mg=active)
        invitation = json.loads(self.client.get_minigame_invitation_json())
        self.assertEqual((invitation["game"], invitation["gameVisit"]), (4, 7))
        self.assertFalse(self.client.minigame_prepare_join(4, 2, 202, 7))
        self.assertTrue(self.client.minigame_invitation_is_current(2, 202, 1))
        self.peer(game=4, mg={**active, "q": 102, "r": 1})
        self.assertFalse(self.client.minigame_prepare_join(4, 2, 202, 8))
        self.assertTrue(self.client.minigame_prepare_join(4, 2, 202, 7))
        self.assertEqual(self.client._minigame.t, (4, 2, 202, 7))
        # Ordinary source-room frames while C waits to launch cannot retire
        # the accepted session before its first native adapter exchange.
        self.assertTrue(self.client.set_minigame_active(0))
        self.assertTrue(self.client.set_minigame_active(0))
        self.assertEqual(self.client._minigame.t, (4, 2, 202, 7))
        self.assertEqual(self.client._minigame.visit, 0)

    def test_yae_framed_invitation_allows_goemon_observer_before_host_ready(self):
        verify_native_yae_invitation({"ready": 1, "running": 1,
            "c": [1, 7, 41, 1499, 0, 0, 0, 1, 0], "a": []})

    def test_yae_rejects_unfenced_or_old_visit_invitation(self):
        self.peer(game=4)
        missing = self.packet(game=4)
        missing.pop("visit")
        self.assertFalse(self.client._receive_minigame_invite(missing))
        self.assertTrue(self.client._receive_minigame_invite(self.packet(game=4, visit=7)))
        self.assertTrue(self.client.get_minigame_invitation_json())
        self.peer(game=4, mg={"v": 1, "s": 202, "q": 100, "r": 1,
            "t": [4, 2, 202, 8], "o": [2, 202, 8], "g": 1, "visit": 8, "x": 0})
        self.assertFalse(self.client.minigame_invitation_is_current(2, 202, 1))
        self.assertEqual(self.client.get_minigame_invitation_json(), "")
        self.assertTrue(self.client._receive_minigame_invite(self.packet(game=4, visit=8, seq=2)))
        self.assertEqual(json.loads(self.client.get_minigame_invitation_json())["gameVisit"], 8)

    def test_yae_player_list_distinguishes_pending_challenge_and_ordinary_kegon(self):
        active = {"v": 1, "s": 202, "q": 100, "r": 0,
                  "t": [4, 2, 202, 7], "o": [2, 202, 7], "g": 1, "visit": 7, "x": 0}
        self.peer(game=4, mg=active, posX=12, posY=-222, posZ=-333, collisionDisabled=1)
        self.assertEqual(json.loads(self.client.get_transfer_target_json(2)),
                         {"cid": 2, "room": 0x154, "x": 12, "y": -222, "z": -333,
                          "game": 4, "session": 202, "gameVisit": 7})
        self.assertFalse(self.client.minigame_prepare_join(4, 2))
        self.peer(game=4, mg={**active, "q": 101}, posX=12, posY=-222, posZ=-333,
                  collisionDisabled=1, teamId="red")
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.peer(game=4, mg={"v": 1, "s": 202, "q": 101, "r": 0, "t": []},
                  posX=12, posY=-222, posZ=-333, collisionDisabled=1)
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.peer(game=4, mg={"v": 1, "s": 202, "q": 102, "r": 0, "t": []},
                  posX=12, posY=-222, posZ=-333, collisionDisabled=0)
        ordinary = json.loads(self.client.get_transfer_target_json(2))
        self.assertNotIn("game", ordinary)
        self.assertEqual(ordinary["room"], 0x154)
        self.assertNotIn("session", ordinary)
        self.assertNotIn("gameVisit", ordinary)
        self.assertFalse(self.client.minigame_prepare_join(4, 2))

    def test_yae_player_list_identity_pins_selected_peer_and_session_origin(self):
        self.peer(game=4, cid=3, session=303, posX=12, posY=-222, posZ=-333,
            collisionDisabled=1, mg={"v": 1, "s": 303, "q": 100, "r": 1,
                "t": [4, 2, 202, 7], "o": [2, 202, 7], "g": 1, "visit": 17, "x": 0})
        target = json.loads(self.client.get_transfer_target_json(3))
        self.assertEqual((target["cid"], target["session"], target["gameVisit"]), (3, 303, 7))
        self.assertTrue(self.client.minigame_prepare_join(
            4, target["cid"], target["session"], target["gameVisit"]))
        self.assertEqual(self.client._minigame.t, (4, 2, 202, 7))

    def test_yae_player_list_pinned_target_rejects_new_host_visit_and_session(self):
        self.peer(game=4, posX=12, posY=-222, posZ=-333, collisionDisabled=1)
        target = json.loads(self.client.get_transfer_target_json(2))
        selected = (target["cid"], target["session"], target["gameVisit"])
        self.assertTrue(self.client.minigame_prepare_join(4, *selected))
        self.peer(game=4, posX=12, posY=-222, posZ=-333, collisionDisabled=1,
            mg={"v": 1, "s": 202, "q": 100, "r": 1,
                "t": [4, 2, 202, 8], "o": [2, 202, 8], "g": 1, "visit": 8, "x": 0})
        self.assertFalse(self.client.minigame_prepare_join(4, *selected))
        self.assertEqual(self.client._minigame.t, (4, 2, 202, 7))
        self.peer(game=4, session=303, posX=12, posY=-222, posZ=-333, collisionDisabled=1,
            mg={"v": 1, "s": 303, "q": 101, "r": 1,
                "t": [4, 2, 303, 9], "o": [2, 303, 9], "g": 1, "visit": 9, "x": 0})
        self.assertFalse(self.client.minigame_prepare_join(4, *selected))
        refreshed = json.loads(self.client.get_transfer_target_json(2))
        self.assertEqual((refreshed["session"], refreshed["gameVisit"]), (303, 9))
        self.assertEqual(self.client._minigame.t, (4, 2, 202, 7))

    def test_yae_player_list_identity_not_exposed_for_stale_offline_or_non_yae(self):
        self.peer(game=4, posX=12, posY=-222, posZ=-333, collisionDisabled=1)
        self.assertEqual(json.loads(self.client.get_transfer_target_json(2))["session"], 202)
        self.clock.return_value += self.client.TRANSFER_TARGET_MAX_AGE_MS / 1000 + 0.001
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.clock.return_value = 100.0
        self.peer(game=4, posX=12, posY=-222, posZ=-333, collisionDisabled=1,
                  currentCharacter="Goemon")
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.peer(game=4, posX=12, posY=-222, posZ=-333, collisionDisabled=1, online=False)
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")

    def test_yae_exit_retires_same_room_metadata_and_retries_failed_metadata_send(self):
        self.client.set_local_room_character(0x154, "Yae")
        self.client.update_minigame(4, 7, '{"ready":1,"running":1,"c":[4],"a":[]}')
        self.assertTrue(self.client.set_minigame_active(4, 7))
        self.client._sock.sent.clear()
        original_send = self.client._send_raw

        def send(packet):
            return False if packet["type"] == "UPDATE_CLIENT_STATE" else original_send(packet)

        with mock.patch.object(self.client, "_send_raw", side_effect=send):
            self.assertFalse(self.client.set_minigame_active(0))
        self.assertIsNone(self.client._minigame.t)
        self.assertTrue(self.client._minigame_yae_exit_metadata_pending)
        self.assertTrue(self.client.set_minigame_active(0))
        self.assertFalse(self.client._minigame_yae_exit_metadata_pending)
        packets = [json.loads(wire[:-1]) for wire in self.client._sock.sent]
        self.assertEqual(sum(packet["type"] == "MNSG_MINIGAME_INVITE" for packet in packets), 1)
        self.assertFalse(packets[0]["entered"])
        self.assertEqual(packets[-1]["state"]["mg"]["t"], [])

    def test_yae_explicit_retirement_handles_entry_invite_that_never_sent(self):
        self.client.set_local_room_character(0x154, "Yae")
        self.client.update_minigame(4, 7, '{"ready":1,"running":1,"c":[4],"a":[]}')
        with mock.patch.object(self.client, "_send_raw", return_value=False):
            self.assertFalse(self.client.set_minigame_active(4, 7))
            self.client.update_minigame(0, 0, "null")
        self.assertIsNone(self.client._minigame.t)
        self.assertTrue(self.client._minigame_yae_exit_metadata_pending)
        # The entry edge never committed, so native retirement sends metadata
        # without manufacturing an invitation exit for the unannounced visit.
        self.client._sock.sent.clear()
        self.assertTrue(self.client.set_minigame_active(0))
        self.assertTrue(self.client.set_minigame_active(0))
        packets = [json.loads(wire[:-1]) for wire in self.client._sock.sent]
        self.assertEqual([packet["type"] for packet in packets], ["UPDATE_CLIENT_STATE"])
        self.assertEqual(packets[0]["state"]["mg"]["t"], [])

    def test_yae_confirmed_invitation_and_departure_expire_when_host_stays_kegon(self):
        self.peer(game=4, posX=12, posY=-222, posZ=-333)
        self.assertTrue(self.client._receive_minigame_invite(self.packet(game=4, visit=7)))
        self.assertTrue(self.client.get_minigame_invitation_json())
        self.assertTrue(self.client._receive_minigame_invite(
            self.packet(game=4, entered=False, seq=2)))
        self.assertEqual(self.client.get_transfer_target_json(2), "{}")
        self.peer(game=4, mg={"v": 1, "s": 202, "q": 100, "r": 0, "t": []},
                  posX=12, posY=-222, posZ=-333)
        self.assertNotIn(2, self.client._minigame_departures)
        self.assertNotIn("game", json.loads(self.client.get_transfer_target_json(2)))
        self.assertEqual(self.client.get_minigame_invitation_json(), "")


if __name__ == "__main__":
    unittest.main()
