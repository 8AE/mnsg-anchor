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
        self.raw_sent = []
        self.received = iter(received)

    def sendall(self, data: bytes) -> None:
        self.raw_sent.append(data)
        self.sent.append(json.loads(data.removesuffix(b"\x00")))

    def recv(self, _size) -> bytes:
        return next(self.received, b"")

    def close(self) -> None:
        pass


class FailingSocket(RecordingSocket):
    def sendall(self, _data: bytes) -> None:
        raise OSError("injected send failure")


class PlayerSoundTransportTests(unittest.TestCase):
    def setUp(self) -> None:
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
        self.assertTrue(self.send_position())
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(), enforce_movement_order=True
        ))
        self.sock.sent.clear()
        self.sock.raw_sent.clear()
        while anchor_mnsg.has_packet():
            anchor_mnsg.poll_packet()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    @staticmethod
    def send_position(epoch=3):
        return anchor_mnsg.set_position_anim(
            10, 20, 30, 5, 100, 1200, 10, 20, 30,
            0, 0, 0, 0, 0, 0, 0, 0, 75, 1, 0, 0, 0, epoch,
        )

    @staticmethod
    def movement(**changes):
        payload = {
            "currentRoomId": 10, "online": True,
            "posX": 100, "posY": 200, "posZ": 300,
            "posSeq": 4, "posT": 100000,
            "playerEpoch": 7, "interactionSession": 202,
        }
        payload.update(changes)
        return payload

    @staticmethod
    def sound(**changes):
        packet = {
            "type": "MNSG_PLAYER_SOUND", "clientId": 2,
            "currentRoomId": 10, "interactionSession": 202,
            "playerEpoch": 7, "soundSeq": 1, "sourcePosSeq": 4,
            "soundT": 100000, "soundIds": [0x212, 0x25B], "quiet": True,
        }
        packet.update(changes)
        return packet

    def test_sender_batches_native_queue_and_uses_quiet_room_broadcast(self):
        sounds = [0x104, 0x207, 0x21A, 0x36A]
        self.assertTrue(anchor_mnsg.send_player_sounds(101, 3, sounds))
        packet = self.sock.sent[-1]
        self.assertEqual(packet, {
            "type": "MNSG_PLAYER_SOUND", "clientId": 1,
            "currentRoomId": 10, "interactionSession": 101,
            "playerEpoch": 3, "soundSeq": 1, "sourcePosSeq": 1,
            "soundT": 100000, "soundIds": sounds, "quiet": True,
        })
        self.assertNotIn("targetClientId", packet)
        self.assertNotIn("targetTeamId", packet)
        self.assertNotIn("addToQueue", packet)
        self.assertLessEqual(len(self.sock.raw_sent[-1]),
                             anchor_mnsg.HOT_PACKET_MAX_BYTES[packet["type"]])

    def test_sender_strictly_rejects_music_stops_loops_duplicates_and_bad_lifecycle(self):
        invalid_batches = (
            [], [0x100] * 9, [0xFF], [0x8000], [0x8212], [0x26D],
            [0x212, 0x212], [True], [1.5], ["0x212"], (0x212,), None,
        )
        for sounds in invalid_batches:
            with self.subTest(sounds=sounds):
                self.assertFalse(anchor_mnsg.send_player_sounds(101, 3, sounds))
        self.assertFalse(anchor_mnsg.send_player_sounds(True, 3, [0x212]))
        self.assertFalse(anchor_mnsg.send_player_sounds(0, 3, [0x212]))
        self.assertFalse(anchor_mnsg.send_player_sounds(100, 3, [0x212]))
        self.assertFalse(anchor_mnsg.send_player_sounds(101, True, [0x212]))
        self.assertFalse(anchor_mnsg.send_player_sounds(101, 2, [0x212]))
        anchor_mnsg._player_states[1]["roomId"] = 11
        self.assertFalse(anchor_mnsg.send_player_sounds(101, 3, [0x212]))
        anchor_mnsg._local_room_id = 0x10000
        anchor_mnsg._player_states[1]["roomId"] = 0x10000
        self.assertFalse(anchor_mnsg.send_player_sounds(101, 3, [0x212]))
        self.assertEqual(self.sock.sent, [])

    def test_worst_case_sender_packet_stays_within_hot_budget(self):
        class MaxTimestamp:
            def __mul__(self, _other):
                return anchor_mnsg.PLAYER_SOUND_TIMESTAMP_MAX

        maximum = anchor_mnsg._POSITION_SEQUENCE_MASK
        anchor_mnsg._client_id = maximum
        anchor_mnsg._local_room_id = 0xffff
        anchor_mnsg._interaction_session = maximum
        anchor_mnsg._player_sound_seq = maximum - 1
        anchor_mnsg._player_states.clear()
        anchor_mnsg._player_states[maximum] = {
            "online": True, "roomId": 0xffff, "posX": 1,
            "posSeq": maximum, "playerEpoch": maximum,
            "interactionSession": maximum,
        }
        self.clock.return_value = MaxTimestamp()
        sounds = list(range(0x7ff8, 0x8000))
        self.assertTrue(anchor_mnsg.send_player_sounds(
            maximum, maximum, sounds
        ))
        raw = self.sock.raw_sent[-1]
        self.assertEqual(len(raw), 281)
        self.assertLessEqual(
            len(raw), anchor_mnsg.HOT_PACKET_MAX_BYTES["MNSG_PLAYER_SOUND"]
        )

    def test_failed_send_does_not_advance_sequence(self):
        anchor_mnsg._sock = FailingSocket()
        self.assertFalse(anchor_mnsg.send_player_sounds(101, 3, [0x212]))
        self.assertEqual(anchor_mnsg._player_sound_seq, 0)

    def test_receiver_rejects_bad_envelopes_and_unsafe_cues(self):
        invalid = (
            {"type": "SET_FLAG"}, {"clientId": 1}, {"clientId": 99},
            {"clientId": True}, {"currentRoomId": 11},
            {"currentRoomId": True}, {"interactionSession": 0},
            {"playerEpoch": 0}, {"soundSeq": 0}, {"soundSeq": True},
            {"sourcePosSeq": 0}, {"sourcePosSeq": True}, {"soundT": 0},
            {"soundT": True},
            {"soundT": anchor_mnsg.PLAYER_SOUND_TIMESTAMP_MAX + 1},
            {"soundIds": []}, {"soundIds": [0xFF]},
            {"soundIds": [0x8212]}, {"soundIds": [0x26D]},
            {"soundIds": [0x212, 0x212]}, {"soundIds": [True]},
        )
        for changes in invalid:
            with self.subTest(changes=changes):
                self.assertFalse(anchor_mnsg._receive_player_sound(
                    self.sound(**changes)
                ))
        self.assertIsNone(anchor_mnsg.poll_player_sound())
        self.assertEqual(self.sock.sent, [])

    def test_deduplicates_batches_but_accepts_reordering_and_sequence_wrap(self):
        self.assertTrue(anchor_mnsg._receive_player_sound(self.sound(soundSeq=2)))
        self.assertTrue(anchor_mnsg._receive_player_sound(
            self.sound(soundSeq=1, soundIds=[0x28A])
        ))
        self.assertFalse(anchor_mnsg._receive_player_sound(self.sound(soundSeq=2)))
        self.assertEqual(anchor_mnsg.poll_player_sound(), (2, 202, 7, 0x212, 500))
        self.assertEqual(anchor_mnsg.poll_player_sound(), (2, 202, 7, 0x25B, 500))
        self.assertEqual(anchor_mnsg.poll_player_sound(), (2, 202, 7, 0x28A, 500))
        anchor_mnsg._reset_player_sounds()
        self.assertTrue(anchor_mnsg._receive_player_sound(
            self.sound(soundSeq=0x7fffffff, soundIds=[0x212])
        ))
        self.assertTrue(anchor_mnsg._receive_player_sound(
            self.sound(soundSeq=1, soundIds=[0x25B])
        ))

    def test_future_sound_waits_for_exact_movement_generation(self):
        future = self.sound(sourcePosSeq=5, soundT=100010)
        self.assertTrue(anchor_mnsg._receive_player_sound(future))
        self.assertIsNone(anchor_mnsg.poll_player_sound())
        self.assertEqual(len(anchor_mnsg._player_sounds), 2)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(posSeq=5, posT=100010),
            enforce_movement_order=True,
        ))
        self.assertEqual(anchor_mnsg.poll_player_sound(), (2, 202, 7, 0x212, 500))

    def test_unconfirmed_sessions_and_epochs_are_rejected_without_state_growth(self):
        for generation in range(1, 257):
            self.assertFalse(anchor_mnsg._receive_player_sound(self.sound(
                interactionSession=1000 + generation,
                playerEpoch=2000 + generation,
                soundSeq=generation,
            )))
        self.assertEqual(len(anchor_mnsg._player_sounds), 0)
        self.assertEqual(anchor_mnsg._player_sound_seen, {})

        self.assertTrue(anchor_mnsg._receive_player_sound(self.sound()))
        self.assertIsNotNone(anchor_mnsg.poll_player_sound())
        self.assertIsNotNone(anchor_mnsg.poll_player_sound())
        self.assertEqual(len(anchor_mnsg._player_sound_seen), 1)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(interactionSession=303, playerEpoch=1,
                             posSeq=1, posT=100010),
            enforce_movement_order=True,
        ))
        self.assertEqual(anchor_mnsg._player_sound_seen, {})

    def test_queue_is_atomic_bounded_and_expires_by_local_receive_time(self):
        batch = [0x300 + index for index in range(8)]
        for sequence in range(1, 9):
            self.assertTrue(anchor_mnsg._receive_player_sound(
                self.sound(soundSeq=sequence, soundIds=batch)
            ))
        self.assertEqual(len(anchor_mnsg._player_sounds), 64)
        overflow = self.sound(soundSeq=9, soundIds=[0x400])
        self.assertFalse(anchor_mnsg._receive_player_sound(overflow))
        self.assertIsNotNone(anchor_mnsg.poll_player_sound())
        self.assertTrue(anchor_mnsg._receive_player_sound(overflow))
        self.clock.return_value = 100.5
        self.assertIsNone(anchor_mnsg.poll_player_sound())
        self.assertEqual(len(anchor_mnsg._player_sounds), 0)

    def test_poll_reports_remaining_original_receive_deadline(self):
        self.assertTrue(anchor_mnsg._receive_player_sound(
            self.sound(soundIds=[0x212])
        ))
        self.clock.return_value = 100.125
        self.assertEqual(
            anchor_mnsg.poll_player_sound(), (2, 202, 7, 0x212, 375)
        )

    def test_poll_revalidates_room_player_lifecycle_and_local_lifecycle(self):
        for field, value in (("roomId", 11), ("playerEpoch", 8),
                             ("interactionSession", 303), ("online", False)):
            with self.subTest(field=field):
                self.assertTrue(anchor_mnsg._receive_player_sound(self.sound()))
                anchor_mnsg._player_states[2][field] = value
                self.assertIsNone(anchor_mnsg.poll_player_sound())
                anchor_mnsg._player_states[2].update(self.movement())
                anchor_mnsg._player_states[2]["roomId"] = 10
                anchor_mnsg._reset_player_sounds()
        self.assertTrue(anchor_mnsg._receive_player_sound(self.sound()))
        anchor_mnsg._player_states[1]["playerEpoch"] = 4
        self.assertIsNone(anchor_mnsg.poll_player_sound())

    def test_actual_receive_loop_uses_dedicated_queue_without_echo(self):
        raw = (json.dumps(self.sound()) + "\x00").encode()
        receiver = RecordingSocket((raw, b""))
        anchor_mnsg._sock = receiver
        with mock.patch.object(anchor_mnsg, "_do_disconnect"):
            anchor_mnsg._recv_loop(receiver)
        self.assertFalse(anchor_mnsg.has_packet())
        self.assertEqual(anchor_mnsg.poll_player_sound(), (2, 202, 7, 0x212, 500))
        self.assertEqual(receiver.sent, [])

    def test_disconnect_and_room_change_clear_transient_sound_state(self):
        self.assertTrue(anchor_mnsg._receive_player_sound(self.sound()))
        anchor_mnsg.set_local_room(11)
        self.assertIsNone(anchor_mnsg.poll_player_sound())
        self.assertEqual(anchor_mnsg._player_sound_seen, {})
        anchor_mnsg.disconnect()
        self.assertEqual(anchor_mnsg._player_sound_seq, 0)
        self.assertEqual(len(anchor_mnsg._player_sounds), 0)


if __name__ == "__main__":
    unittest.main()
