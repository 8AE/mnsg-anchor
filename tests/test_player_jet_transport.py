"""Native Sasuke jet velocity on the existing hot position snapshot."""

import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))

import anchor_mnsg  # noqa: E402


class RecordingSocket:
    def __init__(self) -> None:
        self.sent: list[bytes] = []

    def sendall(self, data: bytes) -> None:
        self.sent.append(data)

    def close(self) -> None:
        pass


class PlayerJetTransportTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._local_character = "Sasuke"
        anchor_mnsg._team_id = "blue"
        anchor_mnsg._interaction_session = 5
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
            anchor_mnsg._player_movement_order.clear()
            anchor_mnsg._retired_interaction_sessions.clear()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    def packets(self) -> list[dict]:
        return [json.loads(wire.removesuffix(b"\x00")) for wire in self.sock.sent]

    @staticmethod
    def send(action: int = 0x9B, speed: object = 250,
             force_motion_edge: int = 0) -> bool:
        return anchor_mnsg.set_position_anim(
            10, 20, 30, action, 100, 1200, 0, 0, 0,
            force_motion_edge=force_motion_edge,
            player_epoch=8,
            jet_velocity_100=speed,
        )

    @staticmethod
    def movement(seq: int, room: int = 10, session: int = 7,
                 **changes: object) -> dict:
        payload = {
            "currentRoomId": room, "online": True,
            "posX": 100 + seq, "posY": 200, "posZ": 300,
            "posSeq": seq, "posT": 1000 + seq,
            "action": 0x9B, "playerEpoch": 8,
            "interactionSession": session,
        }
        payload.update(changes)
        return payload

    def row(self) -> dict:
        rows = json.loads(anchor_mnsg.get_lobby_positions_json())
        return next(row for row in rows if row["cid"] == 2)

    def test_sender_one_optional_field_and_unchanged_cadence(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic",
                               side_effect=(100.0, 100.01, 100.2, 100.21)):
            self.assertTrue(self.send(speed=250))
            self.assertFalse(self.send(speed=300))
            self.assertTrue(self.send(speed=300))
            self.assertTrue(self.send(action=0, speed=300))
        packets = self.packets()
        self.assertEqual(len(packets), 3)
        self.assertEqual([p.get("jetVelocity100") for p in packets],
                         [250, 300, None])
        self.assertTrue(all(p["type"] == "MNSG_PLAYER_POS" and p["quiet"]
                            for p in packets))
        self.assertEqual([p["posSeq"] for p in packets], [1, 2, 3])
        self.assertEqual(anchor_mnsg._player_states[1]["jetVelocity100"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)

    def test_sender_rejects_invalid_values_without_dropping_position(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            for bad in ("250", True, 250.0, None, -10001, 10001):
                self.assertTrue(self.send(speed=bad, force_motion_edge=1))
        self.assertEqual(len(self.packets()), 6)
        self.assertTrue(all("jetVelocity100" not in p for p in self.packets()))
        anchor_mnsg._local_character = "Goemon"
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=101.0):
            self.assertTrue(self.send(speed=250))
        self.assertNotIn("jetVelocity100", self.packets()[-1])

    def test_sender_packet_budget_and_legacy_default(self) -> None:
        anchor_mnsg._client_id = 0xFFFFFFFF
        anchor_mnsg._local_room_id = 0xFFFF
        anchor_mnsg._interaction_session = 0x7FFFFFFF
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                2147483647, -2147483648, 2147483647,
                0x9B, 65535, 65535, -32768, 32767, -32768,
                appearance_flags=255,
                velocity_x=1000000, velocity_y=-1000000,
                velocity_z=1000000, angular_velocity_x=32767,
                angular_velocity_y=-32768, angular_velocity_z=32767,
                force_motion_edge=1, animation_step_100=-65535,
                has_animation_step=1, collision_disabled=1,
                drive_x=-30000, drive_z=30000,
                player_epoch=0x7FFFFFFF, jet_velocity_100=10000,
            ))
        self.assertEqual(self.packets()[-1]["jetVelocity100"], 10000)
        self.assertLessEqual(len(self.sock.sent[-1]),
                             anchor_mnsg.HOT_PACKET_MAX_BYTES["MNSG_PLAYER_POS"])
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=101.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                1, 2, 3, 0x9B, 0, 100, 0, 0, 0,
            ))
        self.assertNotIn("jetVelocity100", self.packets()[-1])

    def test_receive_fresh_missing_invalid_and_reordered(self) -> None:
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, {"currentCharacter": "Sasuke", "currentRoomId": 10},
        ))
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(1, jetVelocity100=250),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"], 250)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(2), enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)
        self.assertFalse(anchor_mnsg._merge_client_state(
            2, self.movement(1, jetVelocity100=500),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)
        for seq, bad in enumerate(("250", True, 250.0, None, -10001, 10001), 3):
            self.assertTrue(anchor_mnsg._merge_client_state(
                2, self.movement(seq, jetVelocity100=bad),
                enforce_movement_order=True,
            ))
            self.assertEqual(anchor_mnsg._player_states[2]["posX"], 100 + seq)
            self.assertEqual(self.row()["jv"],
                             anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)

    def test_receive_identity_room_and_character_reset(self) -> None:
        anchor_mnsg._merge_client_state(
            2, {"currentCharacter": "Sasuke", "currentRoomId": 10},
        )
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(1, jetVelocity100=250),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"], 250)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(1, session=8), enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(2, session=8, jetVelocity100=500),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"], 500)
        anchor_mnsg._merge_client_state(2, {"currentRoomId": 11})
        self.assertEqual(self.row()["hp"], 0)
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(3, room=11, session=8, jetVelocity100=750),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"], 750)
        anchor_mnsg._merge_client_state(2, {"currentCharacter": "Goemon"})
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)

    def test_metadata_and_roster_identity_edges_clear_jet(self) -> None:
        anchor_mnsg._merge_client_state(
            2, {"currentCharacter": "Sasuke", "currentRoomId": 10},
        )
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(1, jetVelocity100=250),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"], 250)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, {"interactionSession": 8},
        ))
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(1, session=8, jetVelocity100=500),
            enforce_movement_order=True,
        ))
        self.assertEqual(self.row()["jv"], 500)
        anchor_mnsg._replace_all_client_states([{
            "clientId": 2,
            "clientState": {
                "currentRoomId": 10, "interactionSession": 9,
                "currentCharacter": "Sasuke", "online": True,
            },
        }])
        self.assertEqual(self.row()["jv"],
                         anchor_mnsg.JET_VELOCITY_100_UNAVAILABLE)


if __name__ == "__main__":
    unittest.main()
