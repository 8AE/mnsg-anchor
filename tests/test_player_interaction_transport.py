import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))

import anchor_mnsg  # noqa: E402


class RecordingSocket:
    def __init__(self, received=()) -> None:
        self.sent = []
        self.received = iter(received)

    def settimeout(self, _timeout) -> None:
        pass

    def connect(self, _address) -> None:
        pass

    def sendall(self, data: bytes) -> None:
        self.sent.append(json.loads(data.removesuffix(b"\x00")))

    def recv(self, _size) -> bytes:
        return next(self.received, b"")

    def close(self) -> None:
        pass


class PlayerInteractionTransportTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()
        self.clock = mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0).start()
        self.addCleanup(mock.patch.stopall)
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._interaction_session = 101
        anchor_mnsg._position_seq = 0
        self.assertTrue(self.send_position(900, 0, 3))
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(), enforce_movement_order=True
        ))
        self.sock.sent.clear()
        while anchor_mnsg.has_packet():
            anchor_mnsg.poll_packet()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    @staticmethod
    def send_position(drive_x=0, drive_z=0, epoch=3):
        return anchor_mnsg.set_position_anim(
            10, 20, 30, 5, 100, 1200, 10, 20, 30,
            2, 0, 0, 0, 0, 0, 0, 0, 75, 1, 0, drive_x, drive_z, epoch,
        )

    @staticmethod
    def movement(**changes):
        payload = {
            "currentRoomId": 10, "online": True,
            "posX": 100, "posY": 200, "posZ": 300,
            "posSeq": 4, "posT": 100000, "driveX": 1200, "driveZ": -3400,
            "playerEpoch": 7, "interactionSession": 202,
        }
        payload.update(changes)
        return payload

    @staticmethod
    def hit(**changes):
        packet = {
            "type": "MNSG_PLAYER_HIT", "clientId": 2, "targetClientId": 1,
            "roomId": 10, "sourceSession": 202, "targetSession": 101,
            "sourceEpoch": 7, "targetEpoch": 3, "sourcePosSeq": 4,
            "hitSeq": 1, "hitT": 100000,
            "hitX": 50000.25, "hitY": 20.5, "hitZ": -30.75,
        }
        packet.update(changes)
        return packet

    def test_hit_sender_serializes_assigned_identities_and_preserves_projectile_point(self):
        self.assertTrue(anchor_mnsg.send_player_hit(2, 7, 50000.25, 20.5, -30.75))
        packet = self.sock.sent[-1]
        self.assertEqual(packet["type"], "MNSG_PLAYER_HIT")
        self.assertEqual(packet["clientId"], 1)
        self.assertEqual(packet["targetClientId"], 2)
        self.assertEqual(packet["sourceSession"], 101)
        self.assertEqual(packet["targetSession"], 202)
        self.assertEqual(packet["sourceEpoch"], 3)
        self.assertEqual(packet["targetEpoch"], 7)
        self.assertEqual(packet["sourcePosSeq"], 1)
        self.assertEqual(packet["hitSeq"], 1)
        self.assertEqual(packet["hitX"], 50000.25)
        self.assertNotIn("addToQueue", packet)
        self.assertNotIn("targetTeamId", packet)

    def test_hit_sender_rejects_self_unknown_room_and_old_lifecycle(self):
        self.assertFalse(anchor_mnsg.send_player_hit(1, 3, 1, 2, 3))
        self.assertFalse(anchor_mnsg.send_player_hit(99, 7, 1, 2, 3))
        self.assertFalse(anchor_mnsg.send_player_hit(2, 6, 1, 2, 3))
        anchor_mnsg._player_states[2]["roomId"] = 11
        self.assertFalse(anchor_mnsg.send_player_hit(2, 7, 1, 2, 3))
        self.assertEqual(self.sock.sent, [])

    def test_sender_and_receiver_reject_scripted_players_and_source_epoch_lag(self):
        self.assertFalse(anchor_mnsg.send_player_hit(2, 7, 1, 2, 3, source_epoch=4))
        self.assertTrue(anchor_mnsg.send_player_hit(2, 7, 1, 2, 3, source_epoch=3))
        for cid in (1, 2):
            with self.subTest(cid=cid):
                anchor_mnsg._player_states[cid]["collisionDisabled"] = 1
                self.assertFalse(anchor_mnsg.send_player_hit(2, 7, 1, 2, 3))
                self.assertFalse(anchor_mnsg._receive_player_hit(self.hit()))
                anchor_mnsg._player_states[cid]["collisionDisabled"] = 0
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit()))
        anchor_mnsg._player_states[2]["collisionDisabled"] = 1
        self.assertIsNone(anchor_mnsg.poll_player_hit())

    def test_receiver_rejects_spoofed_self_target_room_session_lifecycle_and_bad_point(self):
        invalid = (
            {"clientId": 1}, {"clientId": 99}, {"targetClientId": 3},
            {"roomId": 11}, {"sourceSession": 303}, {"targetSession": 303},
            {"sourceEpoch": 6}, {"targetEpoch": 2}, {"hitSeq": 0},
            {"sourcePosSeq": 5}, {"hitT": 99000},
            {"hitX": float("nan")}, {"hitY": float("inf")},
            {"hitZ": -10000001}, {"hitX": "invalid"},
        )
        for changes in invalid:
            with self.subTest(changes=changes):
                self.assertFalse(anchor_mnsg._receive_player_hit(self.hit(**changes)))
        self.assertIsNone(anchor_mnsg.poll_player_hit())
        self.assertEqual(self.sock.sent, [])

    def test_receiver_deduplicates_without_echoing_or_using_general_queue(self):
        packet = self.hit()
        self.assertTrue(anchor_mnsg._receive_player_hit(packet))
        self.assertFalse(anchor_mnsg._receive_player_hit(packet))
        self.assertEqual(anchor_mnsg.poll_player_hit(), (2, 3, 50000.25, 20.5, -30.75))
        self.assertFalse(anchor_mnsg._receive_player_hit(packet))
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit(hitSeq=2)))
        self.assertFalse(anchor_mnsg._receive_player_hit(self.hit(hitSeq=1)))
        self.assertFalse(anchor_mnsg.has_packet())
        self.assertEqual(self.sock.sent, [])

    def test_actual_receive_loop_routes_hits_to_dedicated_queue(self):
        raw = (json.dumps(self.hit()) + "\x00").encode()
        receiver = RecordingSocket((raw, b""))
        anchor_mnsg._sock = receiver
        # The socket ends after one packet; retain state to inspect routing.
        with mock.patch.object(anchor_mnsg, "_do_disconnect"):
            anchor_mnsg._recv_loop(receiver)
        self.assertFalse(anchor_mnsg.has_packet())
        self.assertEqual(anchor_mnsg.poll_player_hit(), (2, 3, 50000.25, 20.5, -30.75))
        self.assertEqual(receiver.sent, [])

    def test_hit_queue_is_bounded_and_expires_on_local_receive_time(self):
        for sequence in range(1, 41):
            self.assertTrue(anchor_mnsg._receive_player_hit(self.hit(hitSeq=sequence, hitX=sequence)))
        self.assertEqual(len(anchor_mnsg._player_hits), 32)
        self.assertEqual(anchor_mnsg.poll_player_hit()[2], 9)
        self.clock.return_value = 100.501
        self.assertIsNone(anchor_mnsg.poll_player_hit())
        self.assertEqual(len(anchor_mnsg._player_hits), 0)

    def test_new_movement_preserves_queued_hit_but_new_life_invalidates_it(self):
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit()))
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(posSeq=5, posT=100030), enforce_movement_order=True
        ))
        self.assertIsNotNone(anchor_mnsg.poll_player_hit())
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit(hitSeq=2)))
        anchor_mnsg._player_states[1]["playerEpoch"] = 4
        self.assertIsNone(anchor_mnsg.poll_player_hit())

    def test_reconnect_session_rejects_old_movement_hits_and_queued_events(self):
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit()))
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(interactionSession=303, playerEpoch=1, posSeq=1, posT=1000),
            enforce_movement_order=True,
        ))
        self.assertIsNone(anchor_mnsg.poll_player_hit())
        self.assertFalse(anchor_mnsg._receive_player_hit(self.hit(hitSeq=2)))
        self.assertFalse(anchor_mnsg._merge_client_state(
            2, self.movement(posSeq=99, posT=200000), enforce_movement_order=True
        ))
        self.assertEqual(anchor_mnsg._player_states[2]["interactionSession"], 303)
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit(
            sourceSession=303, sourceEpoch=1, sourcePosSeq=1, hitT=1000
        )))

    def test_repeated_reconnects_cannot_reactivate_a_retired_session(self):
        for session in range(303, 313):
            self.assertTrue(anchor_mnsg._merge_client_state(
                2, self.movement(interactionSession=session, playerEpoch=1,
                                 posSeq=1, posT=1000),
                enforce_movement_order=True,
            ))
        self.assertFalse(anchor_mnsg._merge_client_state(
            2, self.movement(posSeq=999, posT=200000), enforce_movement_order=True
        ))
        self.assertEqual(anchor_mnsg._player_states[2]["interactionSession"], 312)

    def test_membership_preserves_hot_drive_and_session_and_room_change_clears_them(self):
        anchor_mnsg._replace_all_client_states([{
            "clientId": 2, "clientState": {
                "currentRoomId": 10, "driveX": 0, "driveZ": 0,
                "playerEpoch": 1, "interactionSession": 999,
            },
        }])
        state = anchor_mnsg._player_states[2]
        self.assertEqual((state["driveX"], state["driveZ"]), (1200, -3400))
        self.assertEqual((state["playerEpoch"], state["interactionSession"]), (7, 202))
        compact = json.loads(anchor_mnsg.get_lobby_positions_json())[0]
        self.assertEqual((compact["dx"], compact["dz"], compact["pe"], compact["ps"]),
                         (1200, -3400, 7, 202))
        anchor_mnsg._merge_client_state(2, {"currentRoomId": 11})
        for field in ("driveX", "driveZ", "playerEpoch", "interactionSession"):
            self.assertNotIn(field, state)
        self.assertFalse(anchor_mnsg._merge_client_state(
            2, self.movement(posSeq=3, posT=99999), enforce_movement_order=True
        ))

    def test_legacy_movement_clears_drive_and_interaction_capability(self):
        legacy = self.movement(posSeq=5, posT=100030)
        for field in ("driveX", "driveZ", "playerEpoch", "interactionSession"):
            legacy.pop(field)
        self.assertTrue(anchor_mnsg._merge_client_state(2, legacy, enforce_movement_order=True))
        state = anchor_mnsg._player_states[2]
        for field in ("driveX", "driveZ", "playerEpoch", "interactionSession"):
            self.assertEqual(state[field], 0)

    def test_drive_stop_bypasses_throttle_and_fields_remain_bounded(self):
        self.clock.return_value = 100.01
        self.assertFalse(self.send_position(900, 0))
        self.assertTrue(self.send_position(0, 0))
        self.clock.return_value = 100.1
        self.assertTrue(self.send_position(40000, -50000))
        packet = self.sock.sent[-1]
        self.assertEqual((packet["driveX"], packet["driveZ"]), (30000, -30000))
        self.assertEqual((packet["playerEpoch"], packet["interactionSession"]), (3, 101))
        self.assertEqual((packet["animStep100"], packet["hasAnimStep"], packet["appearanceFlags"]), (75, 1, 2))
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(driveX=99999, driveZ=-99999, posSeq=5, posT=100100),
            enforce_movement_order=True,
        ))
        self.assertEqual((anchor_mnsg._player_states[2]["driveX"],
                          anchor_mnsg._player_states[2]["driveZ"]), (30000, -30000))

    def test_disconnect_clears_hits_and_new_connection_gets_a_new_token(self):
        self.assertTrue(anchor_mnsg._receive_player_hit(self.hit()))
        anchor_mnsg.disconnect()
        self.assertIsNone(anchor_mnsg.poll_player_hit())
        self.assertEqual(anchor_mnsg._interaction_session, 0)
        self.assertEqual(anchor_mnsg._player_hit_seen, {})
        first = RecordingSocket()
        second = RecordingSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", side_effect=(first, second)),
            mock.patch.object(anchor_mnsg.threading.Thread, "start"),
            mock.patch.object(anchor_mnsg.secrets, "randbelow", side_effect=(123, 456)),
        ):
            self.assertTrue(anchor_mnsg.connect("example.test", 43383, "test", "One", client_id=1))
            self.assertEqual(anchor_mnsg._interaction_session, 124)
            anchor_mnsg.disconnect()
            self.assertTrue(anchor_mnsg.connect("example.test", 43383, "test", "One", client_id=1))
            self.assertEqual(anchor_mnsg._interaction_session, 457)
        self.assertEqual(first.sent[0]["clientState"]["interactionSession"], 124)
        self.assertEqual(second.sent[0]["clientState"]["interactionSession"], 457)


if __name__ == "__main__":
    unittest.main()
