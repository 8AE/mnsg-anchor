"""Anchor envelope, heartbeat, and packet-budget regression tests."""

import asyncio
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))
sys.path.insert(0, str(ROOT / "tools"))

import anchor_mnsg  # noqa: E402
import anchor_stress  # noqa: E402


class ByteSocket:
    def __init__(self, received=()) -> None:
        self.sent: list[bytes] = []
        self.received = iter(received)

    def sendall(self, data: bytes) -> None:
        self.sent.append(data)

    def recv(self, _size: int) -> bytes:
        return next(self.received, b"")

    def close(self) -> None:
        pass


class AnchorEnvelopeContractTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()
        self.sock = ByteSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 7
        anchor_mnsg._room_id = "mnsg-contract"
        anchor_mnsg._team_id = "blue"
        anchor_mnsg._player_name = "Contract"
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._local_character = "Goemon"
        anchor_mnsg._local_save_loaded = True
        anchor_mnsg._interaction_session = 101
        anchor_mnsg._position_seq = 0
        anchor_mnsg._last_position_sent = None
        anchor_mnsg._last_position_sent_ms = 0
        anchor_mnsg._last_position_room_id = -1
        anchor_mnsg._last_position_action = -2
        anchor_mnsg._last_position_frame_100 = 0
        anchor_mnsg._last_position_appearance_flags = -1
        anchor_mnsg._last_position_collision_disabled = -1
        anchor_mnsg._last_position_drive = (0, 0)
        anchor_mnsg._last_position_epoch = 0
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states.clear()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    def packets(self) -> list[dict]:
        return [json.loads(data.removesuffix(b"\x00")) for data in self.sock.sent]

    def test_update_client_state_has_root_sender_and_complete_metadata(self):
        self.assertTrue(anchor_mnsg.update_client_state('{"mnsgRace":"started"}'))
        packet = self.packets()[-1]
        self.assertEqual(packet["type"], "UPDATE_CLIENT_STATE")
        self.assertEqual(packet["clientId"], 7)
        self.assertEqual(packet["state"]["clientId"], 7)
        self.assertEqual(packet["state"]["teamId"], "blue")
        self.assertEqual(packet["state"]["currentRoomId"], 10)
        self.assertEqual(packet["state"]["currentCharacter"], "Goemon")
        self.assertTrue(packet["state"]["isSaveLoaded"])

    def test_server_heartbeat_is_consumed_without_client_response(self):
        receiver = ByteSocket((b'{"type":"HEARTBEAT","quiet":true}\x00', b""))
        anchor_mnsg._sock = receiver
        with mock.patch.object(anchor_mnsg, "_do_disconnect"):
            anchor_mnsg._recv_loop(receiver)
        self.assertEqual(receiver.sent, [])
        self.assertFalse(anchor_mnsg.has_packet())

    def test_custom_payload_cannot_override_reserved_envelope(self):
        payload = json.dumps({
            "type": "SPOOF", "clientId": 999, "targetClientId": 3,
            "targetTeamId": "red", "addToQueue": True, "value": 4,
        })
        self.assertTrue(anchor_mnsg.send_custom_packet(
            "TEAM_EVENT", payload, target_team_id="blue"
        ))
        self.assertEqual(self.packets()[-1], {
            "value": 4, "type": "TEAM_EVENT", "clientId": 7,
            "targetTeamId": "blue",
        })

    def test_queueing_requires_the_generic_team_route(self):
        self.assertFalse(anchor_mnsg.send_custom_packet(
            "BAD_ROOM_QUEUE", "{}", add_to_queue=True
        ))
        self.assertFalse(anchor_mnsg.send_custom_packet(
            "BAD_DIRECT_QUEUE", "{}", target_team_id="blue",
            target_client_id=2, add_to_queue=True,
        ))
        self.assertTrue(anchor_mnsg.send_custom_packet(
            "DURABLE", "{}", target_team_id="blue", add_to_queue=True
        ))
        self.assertEqual(len(self.sock.sent), 1)
        self.assertTrue(self.packets()[0]["addToQueue"])

    def test_raw_helper_owns_identity_and_rejects_unsafe_control_packets(self):
        self.assertTrue(anchor_mnsg.send_packet(
            '{"type":"EXTRA","clientId":999,"targetTeamId":"blue"}'
        ))
        self.assertEqual(self.packets()[-1]["clientId"], 7)
        for raw in (
            "{}", "[]", '{"type":"HANDSHAKE"}', '{"type":"HEARTBEAT"}',
            '{"type":"X","targetClientId":0}',
            '{"type":"X","addToQueue":true}',
            '{"type":"X","targetClientId":2,"addToQueue":true}',
        ):
            with self.subTest(raw=raw):
                self.assertFalse(anchor_mnsg.send_packet(raw))

    def test_invalid_or_oversized_json_is_not_sent_or_disconnected(self):
        self.assertFalse(anchor_mnsg._send_raw({
            "type": "BAD", "clientId": 7, "n": float("nan"),
        }))
        with mock.patch.object(anchor_mnsg, "ANCHOR_MAX_PACKET_BYTES", 16):
            self.assertFalse(anchor_mnsg._send_raw({
                "type": "TOO_LARGE", "clientId": 7, "value": "x" * 20,
            }))
        self.assertTrue(anchor_mnsg._connected)
        self.assertEqual(self.sock.sent, [])

    def test_low_level_sender_rejects_missing_or_spoofed_root_identity(self):
        self.assertFalse(anchor_mnsg._send_raw({"type": "EVENT"}))
        self.assertFalse(anchor_mnsg._send_raw({"type": "EVENT", "clientId": 8}))
        self.assertTrue(anchor_mnsg._send_raw({"type": "EVENT", "clientId": 7}))
        self.assertEqual(len(self.sock.sent), 1)

    def test_hot_movement_packet_stays_within_declared_compact_budget(self):
        anchor_mnsg._client_id = 0xFFFFFFFF
        anchor_mnsg._local_room_id = 0xFFFF
        anchor_mnsg._interaction_session = 0x7FFFFFFF
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                2147483647, -2147483648, 2147483647,
                255, 65535, 65535, -32768, 32767, -32768, 7,
                1000000, -1000000, 1000000,
                32767, -32768, 32767, 1, -65535, 1, 1,
                -30000, 30000, 2147483647,
            ))
        self.assertLessEqual(
            len(self.sock.sent[-1]),
            anchor_mnsg.HOT_PACKET_MAX_BYTES["MNSG_PLAYER_POS"],
        )
        self.assertTrue(self.sock.sent[-1].endswith(b"\x00"))


class RecordingWriter:
    def __init__(self) -> None:
        self.sent: list[bytes] = []

    def write(self, data: bytes) -> None:
        self.sent.append(data)

    async def drain(self) -> None:
        await asyncio.sleep(0)


class StressEnvelopeContractTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self) -> None:
        config = anchor_stress.BotConfig(
            "localhost", 43383, "contract", "blue", "Bot",
            0, 0, 0, 10, "Goemon", 5.0,
        )
        self.bot = anchor_stress.AnchorBot(1, config, anchor_stress.WorldState())
        self.bot.connected = True
        self.bot.client_id = 7
        self.bot.interaction_session = 101
        self.writer = RecordingWriter()
        self.bot._writer = self.writer

    async def test_metadata_has_root_sender_and_heartbeat_has_no_echo(self):
        await self.bot.publish_metadata(force=True)
        packet = json.loads(self.writer.sent[-1].removesuffix(b"\x00"))
        self.assertEqual(packet["clientId"], 7)
        before = len(self.writer.sent)
        await self.bot._handle_packet({"type": "HEARTBEAT", "quiet": True})
        self.assertEqual(len(self.writer.sent), before)


if __name__ == "__main__":
    unittest.main()
