"""Remote nameplate health uses the ordered, latest-value movement cache."""

import json
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))
import anchor_mnsg  # noqa: E402


class RecordingSocket:
    def __init__(self):
        self.sent = []

    def sendall(self, data):
        self.sent.append(data)

    def close(self):
        pass


class PlayerHealthTransportTests(unittest.TestCase):
    def setUp(self):
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
        anchor_mnsg._merge_client_state(2, {
            "currentRoomId": 10, "currentCharacter": "Sasuke",
            "online": True, "isSaveLoaded": True, "teamId": "blue",
        })

    def tearDown(self):
        anchor_mnsg.disconnect()

    def send(self, health=-1, **changes):
        kwargs = {"player_epoch": 8, "player_health": health}
        kwargs.update(changes)
        return anchor_mnsg.set_position_anim(
            10, 20, 30, 0x9B, 100, 1200, 0, 0, 0, **kwargs
        )

    @staticmethod
    def movement(seq, **changes):
        packet = {
            "currentRoomId": 10, "posX": 10, "posY": 20, "posZ": 30,
            "posSeq": seq, "posT": 1000 + seq, "action": 0x9B,
            "playerEpoch": 8, "interactionSession": 7,
        }
        packet.update(changes)
        return packet

    def receive(self, seq, **changes):
        return anchor_mnsg._merge_client_state(
            2, self.movement(seq, **changes), enforce_movement_order=True
        )

    def row(self):
        return next(row for row in json.loads(
            anchor_mnsg.get_lobby_positions_json()) if row["cid"] == 2)

    def packets(self):
        return [json.loads(wire[:-1]) for wire in self.sock.sent]

    def test_stationary_health_changes_keep_existing_floor_and_envelope(self):
        with mock.patch.object(anchor_mnsg.time, "monotonic",
                               side_effect=(100.0, 100.01, 100.2, 100.4)):
            self.assertTrue(self.send(10))
            self.assertFalse(self.send(9))
            self.assertTrue(self.send(9))
            self.assertTrue(self.send(0))
        packets = self.packets()
        self.assertEqual([p["playerHealth"] for p in packets], [10, 9, 0])
        self.assertEqual([p["posSeq"] for p in packets], [1, 2, 3])
        for p, wire in zip(packets, self.sock.sent):
            self.assertEqual(p["type"], "MNSG_PLAYER_POS")
            self.assertEqual(p["clientId"], 1)
            self.assertTrue(p["quiet"])
            self.assertNotIn("targetTeamId", p)
            self.assertNotIn("targetClientId", p)
            self.assertNotIn("addToQueue", p)
            self.assertTrue(wire.endswith(b"\0"))

    def test_sender_legacy_and_invalid_health_preserve_movement(self):
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            for bad in (-1, 256, -2, True, False, 9.0, "9", None):
                self.assertTrue(self.send(bad, force_motion_edge=1))
                self.assertNotIn("playerHealth", self.packets()[-1])
            self.assertTrue(self.send(255, force_motion_edge=1))
        self.assertEqual(self.packets()[-1]["playerHealth"], 255)

    def test_failed_send_does_not_advance_sequence_or_suppress_retry(self):
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            with mock.patch.object(anchor_mnsg, "_send_raw", return_value=False):
                self.assertFalse(self.send(9))
            self.assertEqual(anchor_mnsg._position_seq, 0)
            self.assertTrue(self.send(9))
        self.assertEqual(self.packets()[0]["playerHealth"], 9)
        self.assertEqual(self.packets()[0]["posSeq"], 1)

    def test_receive_valid_missing_invalid_and_ordering(self):
        for seq, value in enumerate((0, 9, 10, 255), 1):
            self.assertTrue(self.receive(seq, playerHealth=value))
            self.assertEqual(self.row()["h"], value)
        self.assertFalse(self.receive(3, playerHealth=0))
        self.assertEqual(self.row()["h"], 255)
        self.assertTrue(self.receive(5))
        self.assertEqual(self.row()["h"], -1)
        for seq, bad in enumerate((-1, 256, True, False, "9", 9.0, None), 6):
            self.assertTrue(self.receive(seq, playerHealth=bad))
            self.assertEqual(self.row()["h"], -1)
            self.assertEqual(self.row()["hp"], 1)

    def test_session_and_epoch_never_restore_stale_health(self):
        self.assertTrue(self.receive(1, playerHealth=10))
        self.assertTrue(self.receive(2, playerEpoch=9, playerHealth=8))
        self.assertFalse(self.receive(3, playerEpoch=8, playerHealth=10))
        self.assertEqual(self.row()["h"], 8)
        self.assertTrue(self.receive(1, interactionSession=8))
        self.assertEqual(self.row()["h"], -1)
        self.assertFalse(self.receive(4, playerHealth=10))
        self.assertEqual(self.row()["h"], -1)

    def test_metadata_room_character_save_and_offline_invalidate(self):
        for metadata in ({"currentRoomId": 11}, {"currentCharacter": "Goemon"},
                         {"playerEpoch": 9}, {"interactionSession": 8},
                         {"isSaveLoaded": False}, {"online": False}):
            anchor_mnsg._player_states.pop(2, None)
            anchor_mnsg._player_movement_order.clear()
            anchor_mnsg._retired_interaction_sessions.clear()
            anchor_mnsg._merge_client_state(2, {
                "currentRoomId": 10, "currentCharacter": "Sasuke",
                "online": True, "isSaveLoaded": True,
            })
            self.assertTrue(self.receive(1, playerHealth=9))
            anchor_mnsg._merge_client_state(2, metadata)
            self.assertEqual(anchor_mnsg._player_states[2].get("playerHealth", -1), -1)

    def test_roster_preserves_current_health_only_for_compatible_loaded_peer(self):
        self.assertTrue(self.receive(1, playerHealth=9))
        def snapshot(**changes):
            state = {"currentRoomId": 10, "currentCharacter": "Sasuke",
                     "online": True, "isSaveLoaded": True,
                     "interactionSession": 7, "name": "Updated name"}
            state.update(changes)
            anchor_mnsg._replace_all_client_states([
                {"clientId": 2, "clientState": state}])
        snapshot()
        self.assertEqual(self.row()["h"], 9)
        snapshot(isSaveLoaded=False)
        self.assertEqual(self.row()["h"], -1)
        snapshot()
        self.assertEqual(self.row()["h"], -1)
        self.assertTrue(self.receive(2, playerHealth=7))
        snapshot(interactionSession=8)
        self.assertEqual(self.row()["h"], -1)

    def test_metadata_identity_fence_rejects_late_old_context_health(self):
        self.assertTrue(self.receive(1, playerHealth=9))
        anchor_mnsg._merge_client_state(2, {"interactionSession": 8})
        self.assertTrue(self.receive(2, playerHealth=7))
        self.assertEqual(self.row()["h"], -1)
        self.assertTrue(self.receive(1, interactionSession=8, playerHealth=6))
        self.assertEqual(self.row()["h"], 6)
        anchor_mnsg._merge_client_state(2, {"playerEpoch": 9})
        self.assertTrue(self.receive(2, interactionSession=8, playerHealth=5))
        self.assertEqual(self.row()["h"], -1)
        self.assertTrue(self.receive(3, interactionSession=8, playerEpoch=9,
                                     playerHealth=4))
        self.assertEqual(self.row()["h"], 4)
        self.assertTrue(self.receive(4, interactionSession=8, playerEpoch=10,
                                     playerHealth=3))
        self.assertEqual(self.row()["h"], 3)

    def test_epoch_only_fence_is_bound_to_predecessor_session(self):
        self.assertTrue(self.receive(1, playerHealth=9))
        anchor_mnsg._merge_client_state(2, {"playerEpoch": 9})
        self.assertEqual(anchor_mnsg._player_states[2]["_healthContext"], (7, 9))
        self.assertTrue(self.receive(2, playerHealth=7))
        self.assertEqual(self.row()["h"], -1)
        self.assertTrue(self.receive(1, interactionSession=8, playerEpoch=1,
                                     playerHealth=6))
        self.assertEqual(self.row()["h"], 6)

    def test_roster_fence_and_stale_snapshots_allow_current_successors(self):
        self.assertTrue(self.receive(1, playerHealth=9))
        def snapshot(session):
            anchor_mnsg._replace_all_client_states([{
                "clientId": 2, "clientState": {
                    "currentRoomId": 10, "currentCharacter": "Sasuke",
                    "online": True, "isSaveLoaded": True,
                    "interactionSession": session,
                },
            }])
        snapshot(8)
        snapshot(7)  # Older roster metadata cannot remove the pending fence.
        self.assertTrue(self.receive(2, playerHealth=7))
        self.assertEqual(self.row()["h"], -1)
        self.assertTrue(self.receive(1, interactionSession=8, playerHealth=6))
        self.assertEqual(self.row()["h"], 6)
        snapshot(7)  # Retired metadata cannot erase current health.
        self.assertEqual(self.row()["h"], 6)
        self.assertTrue(self.receive(2, interactionSession=8, playerHealth=5))
        self.assertEqual(self.row()["h"], 5)
        self.assertTrue(self.receive(1, interactionSession=9, playerHealth=4))
        self.assertEqual(self.row()["h"], 4)

    def test_getter_requires_complete_position_and_defends_bad_cached_types(self):
        self.assertTrue(self.receive(1, playerHealth=9))
        anchor_mnsg._player_states[2].pop("posY")
        self.assertEqual(self.row()["h"], -1)
        anchor_mnsg._player_states[2]["posY"] = 20
        anchor_mnsg._player_states[2]["playerHealth"] = True
        self.assertEqual(self.row()["h"], -1)

    def test_maximum_packet_stays_within_existing_640_byte_budget(self):
        anchor_mnsg._client_id = 0xFFFFFFFF
        anchor_mnsg._local_room_id = 0xFFFF
        anchor_mnsg._interaction_session = 0x7FFFFFFF
        anchor_mnsg._position_seq = 0x7FFFFFFE
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=2147483.647):
            self.assertTrue(anchor_mnsg.set_position_anim(
                2147483647, -2147483648, 2147483647,
                0x9B, 65535, 65535, -32768, 32767, -32768,
                appearance_flags=255,
                velocity_x=2147483647, velocity_y=-2147483648,
                velocity_z=2147483647, angular_velocity_x=32767,
                angular_velocity_y=-32768, angular_velocity_z=32767,
                force_motion_edge=1, animation_step_100=-65535,
                has_animation_step=1, collision_disabled=1,
                drive_x=-30000, drive_z=30000, player_epoch=0x7FFFFFFF,
                jet_velocity_100=10000, player_health=255,
            ))
        self.assertEqual(self.packets()[-1]["playerHealth"], 255)
        self.assertLessEqual(len(self.sock.sent[-1]), 640)


if __name__ == "__main__":
    unittest.main()
