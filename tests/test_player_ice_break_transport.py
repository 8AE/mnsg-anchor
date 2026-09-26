import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))

import anchor_mnsg  # noqa: E402


class RecordingSocket:
    def __init__(self, received=()):
        self.sent = []
        self.raw_sent = []
        self.received = iter(received)

    def sendall(self, data):
        self.raw_sent.append(data)
        self.sent.append(json.loads(data.removesuffix(b"\x00")))

    def close(self):
        pass

    def recv(self, _size):
        return next(self.received, b"")


class IceBreakTransportTests(unittest.TestCase):
    def setUp(self):
        anchor_mnsg.disconnect()
        self.clock = mock.patch.object(
            anchor_mnsg.time, "monotonic", return_value=100.0
        ).start()
        self.addCleanup(mock.patch.stopall)
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._interaction_session = 101
        anchor_mnsg._position_seq = 0
        self.assertTrue(anchor_mnsg.set_position_anim(
            10, 20, 30, 5, 100, 1200, 10, 20, 30,
            appearance_flags=anchor_mnsg.APPEARANCE_FROZEN,
            player_epoch=3,
        ))
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(), enforce_movement_order=True
        ))
        self.sock.sent.clear()
        self.sock.raw_sent.clear()

    def tearDown(self):
        anchor_mnsg.disconnect()
        anchor_mnsg._position_seq = 0

    @staticmethod
    def movement(**changes):
        payload = {
            "currentRoomId": 10, "online": True,
            "posX": 100, "posY": 200, "posZ": 300,
            "posSeq": 4, "posT": 100000,
            "playerEpoch": 7, "interactionSession": 202,
            "appearanceFlags": anchor_mnsg.APPEARANCE_FROZEN,
        }
        payload.update(changes)
        return payload

    @staticmethod
    def incoming(**changes):
        packet = {
            "type": "MNSG_PLAYER_ICE_BREAK", "clientId": 2,
            "currentRoomId": 10, "interactionSession": 202,
            "playerEpoch": 7, "breakSeq": 1, "sourcePosSeq": 4,
            "breakT": 100000, "cause": 1,
            "x100": 10000, "y100": 20000, "z100": 30000,
            "quiet": True,
        }
        packet.update(changes)
        return packet

    def test_send_is_small_transient_room_broadcast(self):
        self.assertTrue(anchor_mnsg.send_player_ice_break(
            101, 3, 2, -1200, 3400, 5600
        ))
        packet = self.sock.sent[-1]
        self.assertEqual(packet, {
            "type": "MNSG_PLAYER_ICE_BREAK", "clientId": 1,
            "currentRoomId": 10, "interactionSession": 101,
            "playerEpoch": 3, "breakSeq": 1, "sourcePosSeq": 1,
            "breakT": 100000, "cause": 2,
            "x100": -1200, "y100": 3400, "z100": 5600,
            "quiet": True,
        })
        self.assertNotIn("targetClientId", packet)
        self.assertNotIn("targetTeamId", packet)
        self.assertNotIn("addToQueue", packet)
        self.assertLessEqual(len(self.sock.raw_sent[-1]),
                             anchor_mnsg.HOT_PACKET_MAX_BYTES[packet["type"]])

    def test_sender_rejects_bad_lifetime_coordinates_and_failed_send(self):
        invalid = (
            (100, 3, 1, 0, 0, 0), (101, 2, 1, 0, 0, 0),
            (101, 3, 0, 0, 0, 0), (101, 3, 3, 0, 0, 0),
            (101, 3, 1, True, 0, 0), (101, 3, 1, 1000000001, 0, 0),
        )
        for args in invalid:
            with self.subTest(args=args):
                self.assertFalse(anchor_mnsg.send_player_ice_break(*args))
        with mock.patch.object(anchor_mnsg, "_send_raw", return_value=False):
            self.assertFalse(anchor_mnsg.send_player_ice_break(
                101, 3, 1, 0, 0, 0
            ))
        self.assertEqual(anchor_mnsg._player_ice_break_seq, 0)
        self.assertTrue(anchor_mnsg.send_player_ice_break(
            101, 3, 1, 0, 0, 0
        ))
        self.assertEqual(self.sock.sent[-1]["breakSeq"], 1)

    def test_worst_case_packet_stays_within_budget(self):
        class MaxTimestamp:
            def __mul__(self, _other):
                return anchor_mnsg.PLAYER_SOUND_TIMESTAMP_MAX

        maximum = anchor_mnsg._POSITION_SEQUENCE_MASK
        anchor_mnsg._client_id = maximum
        anchor_mnsg._local_room_id = 0xffff
        anchor_mnsg._interaction_session = maximum
        anchor_mnsg._player_ice_break_seq = maximum - 1
        anchor_mnsg._player_states.clear()
        anchor_mnsg._player_states[maximum] = {
            "roomId": 0xffff, "posX": 1, "posSeq": maximum,
            "playerEpoch": maximum, "interactionSession": maximum,
        }
        self.clock.return_value = MaxTimestamp()
        self.assertTrue(anchor_mnsg.send_player_ice_break(
            maximum, maximum, 2, -1000000000, 1000000000, -1000000000
        ))
        self.assertLessEqual(
            len(self.sock.raw_sent[-1]),
            anchor_mnsg.HOT_PACKET_MAX_BYTES["MNSG_PLAYER_ICE_BREAK"],
        )

    def test_receive_once_for_valid_room_sender_and_lifetime(self):
        packet = self.incoming()
        self.assertTrue(anchor_mnsg._receive_player_ice_break(packet))
        self.assertFalse(anchor_mnsg._receive_player_ice_break(packet))
        self.assertEqual(anchor_mnsg.poll_player_ice_break(),
                         (2, 202, 7, 10, 1, 1, 10000, 20000, 30000))
        self.assertIsNone(anchor_mnsg.poll_player_ice_break())
        self.assertTrue(anchor_mnsg._receive_player_ice_break(
            self.incoming(breakSeq=2, cause=2)
        ))
        self.assertEqual(anchor_mnsg.poll_player_ice_break()[5], 2)
        self.assertFalse(anchor_mnsg._receive_player_ice_break(packet))

    def test_rejects_wrong_route_room_identity_stale_and_shape(self):
        invalid = (
            {"type": "SET_FLAG"}, {"clientId": 1}, {"clientId": 99},
            {"clientId": True}, {"currentRoomId": 11},
            {"interactionSession": 201}, {"playerEpoch": 8},
            {"interactionSession": True}, {"breakSeq": 0},
            {"breakSeq": True}, {"sourcePosSeq": 0},
            {"breakT": 94000}, {"cause": 0}, {"cause": 3},
            {"cause": True}, {"x100": 1000000001},
            {"quiet": False}, {"targetClientId": 1},
            {"targetTeamId": "default"}, {"addToQueue": True},
            {"padding": "x" * 350},
        )
        for changes in invalid:
            with self.subTest(changes=changes):
                self.assertFalse(anchor_mnsg._receive_player_ice_break(
                    self.incoming(**changes)
                ))
        self.assertIsNone(anchor_mnsg.poll_player_ice_break())

    def test_deferred_movement_and_queue_expiration(self):
        self.assertTrue(anchor_mnsg._receive_player_ice_break(
            self.incoming(sourcePosSeq=5, breakT=100100)
        ))
        self.assertIsNone(anchor_mnsg.poll_player_ice_break())
        self.clock.return_value = 100.1
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(posSeq=5, posT=100100),
            enforce_movement_order=True,
        ))
        self.assertEqual(anchor_mnsg.poll_player_ice_break()[4], 1)
        self.assertTrue(anchor_mnsg._receive_player_ice_break(
            self.incoming(breakSeq=2, sourcePosSeq=5, breakT=100100)
        ))
        self.clock.return_value = 101.0
        self.assertIsNone(anchor_mnsg.poll_player_ice_break())

    def test_room_and_disconnect_clear_pending_events(self):
        self.assertTrue(anchor_mnsg._receive_player_ice_break(self.incoming()))
        anchor_mnsg._reset_player_ice_breaks()
        self.assertIsNone(anchor_mnsg.poll_player_ice_break())
        self.assertTrue(anchor_mnsg._receive_player_ice_break(self.incoming()))
        anchor_mnsg.disconnect()
        self.assertIsNone(anchor_mnsg.poll_player_ice_break())

    def test_receive_loop_uses_dedicated_queue_without_echo(self):
        raw = (json.dumps(self.incoming()) + "\x00").encode()
        receiver = RecordingSocket((raw, b""))
        anchor_mnsg._sock = receiver
        with mock.patch.object(anchor_mnsg, "_do_disconnect"):
            anchor_mnsg._recv_loop(receiver)
        self.assertFalse(anchor_mnsg.has_packet())
        self.assertEqual(anchor_mnsg.poll_player_ice_break(),
                         (2, 202, 7, 10, 1, 1, 10000, 20000, 30000))
        self.assertEqual(receiver.sent, [])


if __name__ == "__main__":
    unittest.main()
