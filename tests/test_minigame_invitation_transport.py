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
        self.assertTrue(self.client._merge_client_state(cid, payload, True))

    def packet(self, game=1, **changes):
        packet = {"type": "MNSG_MINIGAME_INVITE", "clientId": 2,
                  "targetTeamId": "blue", "game": game, "entered": True,
                  "session": 202, "seq": 1}
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

    def test_entry_exit_reentry_all_three_games(self):
        for game, room in self.client.MINIGAME_ROOMS.items():
            with self.subTest(game=game):
                self.client.set_local_room(room)
                self.assertTrue(self.client.set_minigame_active(game, game))
                wire = self.client._sock.sent[-1]
                self.assertLessEqual(len(wire), 256)  # Includes NUL framing.
                self.assertEqual(json.loads(wire[:-1]), {
                    "type": "MNSG_MINIGAME_INVITE", "clientId": 1,
                    "targetTeamId": "blue", "game": game,
                    "entered": True, "session": 101,
                    "seq": (game - 1) * 2 + 1,
                })
                count = len(self.client._sock.sent)
                self.assertTrue(self.client.set_minigame_active(game, game))
                self.assertEqual(len(self.client._sock.sent), count)
                self.assertTrue(self.client.set_minigame_active(0))
                exit_wire = self.client._sock.sent[-1]
                self.assertLessEqual(len(exit_wire), 256)
                self.assertEqual(json.loads(exit_wire[:-1])["game"], game)
                self.assertFalse(json.loads(exit_wire[:-1])["entered"])
        self.client.set_local_room(self.client.MINIGAME_ROOMS[1])
        self.assertTrue(self.client.set_minigame_active(1, 9))
        self.assertEqual(json.loads(self.client._sock.sent[-1][:-1])["seq"], 7)

    def test_sender_room_and_bounded_payload(self):
        self.assertFalse(self.client.set_minigame_active(1, 1))
        self.assertFalse(self.client._sock.sent)
        self.client.set_local_room(self.client.MINIGAME_ROOMS[1])
        self.assertFalse(self.client.set_minigame_active(1, 0))
        self.assertFalse(self.client.set_minigame_active(4, 1))
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
        self.assertTrue(self.client._receive_minigame_invite(self.packet(seq=4)))
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
        for changes in ({"game": 0}, {"game": 4}, {"clientId": True},
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
            "game": 1, "name": "Remote",
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


if __name__ == "__main__":
    unittest.main()
