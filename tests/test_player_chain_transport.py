"""Native chain endpoints stay on the ordered transient position route."""

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


class PlayerChainTransportTests(unittest.TestCase):
    def setUp(self) -> None:
        # Production keeps this sequence monotonic across disconnects. Restore
        # its incoming value after each isolated sender fixture.
        self.previous_position_seq = anchor_mnsg._position_seq
        anchor_mnsg.disconnect()
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._local_character = "Goemon"
        anchor_mnsg._team_id = "blue"
        anchor_mnsg._interaction_session = 5
        anchor_mnsg._position_seq = 0
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states.clear()
            anchor_mnsg._player_movement_order.clear()
            anchor_mnsg._retired_interaction_sessions.clear()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()
        anchor_mnsg._position_seq = self.previous_position_seq

    def packets(self) -> list[dict]:
        self.assertTrue(all(wire.endswith(b"\x00") for wire in self.sock.sent))
        return [json.loads(wire[:-1]) for wire in self.sock.sent]

    @staticmethod
    def send(phase: object = 1, x: object = 1000, **changes: object) -> bool:
        options = {
            "player_epoch": 8, "chain_phase": phase,
            "chain_x100": x, "chain_y100": 2000, "chain_z100": 3000,
        }
        options.update(changes)
        return anchor_mnsg.set_position_anim(
            10, 20, 30, 0x71, 100, 1200, 0, 0, 0, **options,
        )

    @staticmethod
    def movement(seq: int, room: int = 10, session: int = 7,
                 **changes: object) -> dict:
        payload = {
            "currentRoomId": room, "online": True,
            "posX": 100 + seq, "posY": 200, "posZ": 300,
            "posSeq": seq, "posT": 1000 + seq,
            "action": 0x73, "playerEpoch": 8,
            "interactionSession": session,
        }
        payload.update(changes)
        return payload

    def row(self) -> dict:
        rows = json.loads(anchor_mnsg.get_lobby_positions_json())
        return next(row for row in rows if row["cid"] == 2)

    def chain(self) -> list[int]:
        row = self.row()
        return [row[key] for key in ("cp", "cx", "cy", "cz")]

    def peer(self) -> None:
        anchor_mnsg._merge_client_state(2, {
            "currentCharacter": "Goemon", "currentRoomId": 10,
            "isSaveLoaded": True,
        })

    def test_start_phase_stop_edges_bypass_throttle_but_endpoint_does_not(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", side_effect=(
                100.0, 100.01, 100.02, 100.03, 100.04, 100.2, 100.21)):
            self.assertTrue(self.send(phase=0))
            self.assertTrue(self.send())
            self.assertFalse(self.send(x=1100))
            self.assertTrue(self.send(phase=3, x=1100))
            self.assertFalse(self.send(phase=3, x=1200))
            self.assertTrue(self.send(phase=3, x=1200))
            self.assertTrue(self.send(phase=0))
        packets = self.packets()
        self.assertEqual([p.get("chain") for p in packets], [
            None, [1, 1000, 2000, 3000], [3, 1100, 2000, 3000],
            [3, 1200, 2000, 3000], None,
        ])
        self.assertTrue(all(p["type"] == "MNSG_PLAYER_POS" and p["quiet"]
                            and p["clientId"] == 1 for p in packets))
        self.assertTrue(all(not any(key in p for key in (
            "targetClientId", "targetTeamId", "addToQueue")) for p in packets))
        self.assertEqual([p["posSeq"] for p in packets], [1, 2, 3, 4, 5])

    def test_invalid_sender_values_keep_position_and_clear_chain(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            for phase, x in ((True, 100), (5, 100), (-1, 100),
                             (1, True), (1, 1.0), (1, "100"),
                             (1, 1000000001), (1, -1000000001)):
                self.assertTrue(self.send(phase, x, force_motion_edge=1))
            self.assertTrue(self.send(collision_disabled=1, force_motion_edge=1))
            self.assertTrue(self.send(player_epoch=0, force_motion_edge=1))
        self.assertTrue(all("chain" not in p for p in self.packets()))
        anchor_mnsg._local_character = "Sasuke"
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=101.0):
            self.assertTrue(self.send())
        self.assertNotIn("chain", self.packets()[-1])

    def test_failed_edge_send_does_not_consume_phase_or_sequence(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(self.send(phase=0))
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.01):
            with mock.patch.object(anchor_mnsg, "_send_raw", return_value=False):
                self.assertFalse(self.send())
            self.assertEqual(anchor_mnsg._last_position_chain_phase, 0)
            self.assertEqual(anchor_mnsg._position_seq, 1)
            self.assertTrue(self.send())
        self.assertEqual(self.packets()[-1]["posSeq"], 2)

    def test_receive_roundtrip_legacy_invalid_and_reordering(self) -> None:
        self.peer()
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(self.send(3, -1000000000))
        payload = self.packets()[0]
        payload.update(clientId=2, interactionSession=7)
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, payload, enforce_movement_order=True))
        self.assertEqual(self.chain(), [3, -1000000000, 2000, 3000])
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(2, posT=100001), enforce_movement_order=True))
        self.assertEqual(self.chain(), [0, 0, 0, 0])
        self.assertFalse(anchor_mnsg._merge_client_state(
            2, payload, enforce_movement_order=True))
        for seq, value in enumerate((True, "bad", [1, 2, 3],
                                    [1, 2, 3, 4, 5], [True, 1, 2, 3],
                                    [3, 1.0, 2, 3], [3, 1000000001, 2, 3]), 3):
            self.assertTrue(anchor_mnsg._merge_client_state(
                2, self.movement(seq, posT=100000 + seq, chain=value),
                enforce_movement_order=True))
            self.assertEqual(self.chain(), [0, 0, 0, 0])
            self.assertEqual(self.row()["x"], 100 + seq)

    def test_room_identity_character_and_script_edges_retire_chain(self) -> None:
        self.peer()
        for seq in (1, 2):
            anchor_mnsg._merge_client_state(
                2, self.movement(seq, chain=[3, 100, 200, 300]),
                enforce_movement_order=True)
        self.assertEqual(self.chain(), [3, 100, 200, 300])
        anchor_mnsg._merge_client_state(2, {"currentRoomId": 11})
        self.assertEqual(self.chain(), [0, 0, 0, 0])
        anchor_mnsg._merge_client_state(
            2, self.movement(3, room=11, chain=[3, 100, 200, 300]),
            enforce_movement_order=True)
        self.assertEqual(self.chain(), [3, 100, 200, 300])
        anchor_mnsg._merge_client_state(2, {"interactionSession": 8})
        self.assertEqual(self.chain(), [0, 0, 0, 0])
        anchor_mnsg._merge_client_state(
            2, self.movement(1, room=11, session=8, chain=[3, 100, 200, 300]),
            enforce_movement_order=True)
        self.assertEqual(self.chain(), [3, 100, 200, 300])
        self.assertFalse(anchor_mnsg._merge_client_state(
            2, self.movement(100, room=11, session=7, chain=[3, 999, 999, 999]),
            enforce_movement_order=True))
        self.assertEqual(self.chain(), [3, 100, 200, 300])
        anchor_mnsg._merge_client_state(2, {"currentCharacter": "Yae"})
        self.assertEqual(self.chain(), [0, 0, 0, 0])
        anchor_mnsg._merge_client_state(2, {"currentCharacter": "Goemon"})
        self.assertTrue(anchor_mnsg._merge_client_state(
            2, self.movement(2, room=11, session=8,
                             chain=[3, 100, 200, 300], collisionDisabled=1),
            enforce_movement_order=True))
        self.assertEqual(self.chain(), [0, 0, 0, 0])

    def test_metadata_epoch_fence_and_save_unload_prevent_stale_chain(self) -> None:
        self.peer()
        anchor_mnsg._merge_client_state(
            2, self.movement(1, chain=[3, 100, 200, 300]),
            enforce_movement_order=True)
        self.assertEqual(self.chain(), [3, 100, 200, 300])
        anchor_mnsg._merge_client_state(2, {"playerEpoch": 9})
        self.assertEqual(self.chain(), [0, 0, 0, 0])
        anchor_mnsg._merge_client_state(
            2, self.movement(2, chain=[3, 100, 200, 300]),
            enforce_movement_order=True)
        self.assertEqual(self.chain(), [0, 0, 0, 0])
        anchor_mnsg._merge_client_state(
            2, self.movement(3, playerEpoch=9, chain=[3, 100, 200, 300]),
            enforce_movement_order=True)
        self.assertEqual(self.chain(), [3, 100, 200, 300])
        anchor_mnsg._merge_client_state(2, {"isSaveLoaded": False})
        self.assertEqual(self.chain(), [0, 0, 0, 0])

    def test_all_native_chain_actions_and_roster_identity_fence(self) -> None:
        self.peer()
        for seq, action in enumerate(range(0x70, 0x7C), 1):
            anchor_mnsg._merge_client_state(
                2, self.movement(seq, action=action, chain=[1, 100, 200, 300]),
                enforce_movement_order=True)
            self.assertEqual(self.chain(), [1, 100, 200, 300])
        anchor_mnsg._replace_all_client_states([{
            "clientId": 2, "clientState": {
                "currentRoomId": 10, "interactionSession": 9,
                "currentCharacter": "Goemon", "online": True,
            },
        }])
        self.assertEqual(self.chain(), [0, 0, 0, 0])

    def test_packet_budget_with_full_negative_endpoints(self) -> None:
        anchor_mnsg._client_id = 0xFFFFFFFF
        anchor_mnsg._local_room_id = 0xFFFF
        anchor_mnsg._interaction_session = 0x7FFFFFFF
        anchor_mnsg._position_seq = 0x7FFFFFFE
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=2147483647.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                -2147483648, -2147483648, -2147483648,
                0x7A, 65535, 65535, -32768, -32768, -32768,
                appearance_flags=255,
                velocity_x=-1000000, velocity_y=-1000000,
                velocity_z=-1000000, angular_velocity_x=-32768,
                angular_velocity_y=-32768, angular_velocity_z=-32768,
                force_motion_edge=1, animation_step_100=-65535,
                has_animation_step=1, drive_x=-30000, drive_z=-30000,
                player_epoch=0x7FFFFFFF, player_health=255,
                chain_phase=3, chain_x100=-1000000000,
                chain_y100=-1000000000, chain_z100=-1000000000,
            ))
        self.assertEqual(self.packets()[-1]["chain"],
                         [3, -1000000000, -1000000000, -1000000000])
        self.assertLessEqual(len(self.sock.sent[-1]),
                             anchor_mnsg.HOT_PACKET_MAX_BYTES["MNSG_PLAYER_POS"])


if __name__ == "__main__":
    unittest.main()
