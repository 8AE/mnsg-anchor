"""Appearance bitmap bit 3 (alternative Ebisumaru skin) transport tests."""

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


class RecordingSocket:
    def __init__(self) -> None:
        self.sent: list[bytes] = []

    def sendall(self, data: bytes) -> None:
        self.sent.append(data)

    def close(self) -> None:
        pass


class AppearanceSkinFlagTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._team_id = "blue"
        anchor_mnsg._interaction_session = 1
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

    def test_bit_three_is_defined_and_part_of_the_mask(self) -> None:
        self.assertEqual(anchor_mnsg.APPEARANCE_ALTERNATIVE_EBISUMARU, 1 << 3)
        self.assertEqual(anchor_mnsg.APPEARANCE_MASK, 0b1111)
        self.assertTrue(anchor_mnsg.APPEARANCE_MASK & (1 << 3))

    def test_set_position_anim_sends_the_alternative_bit(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                0, 0, 0, 1, 0, 100, 0, 0, 0,
                appearance_flags=anchor_mnsg.APPEARANCE_ALTERNATIVE_EBISUMARU,
            ))
        packet = self.packets()[-1]
        self.assertEqual(packet["type"], "MNSG_PLAYER_POS")
        self.assertEqual(packet["appearanceFlags"], 1 << 3)
        self.assertTrue(packet["appearanceFlags"] & (1 << 3))

    def test_set_position_anim_carries_all_four_bits(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                0, 0, 0, 1, 0, 100, 0, 0, 0, appearance_flags=15,
            ))
        self.assertEqual(self.packets()[-1]["appearanceFlags"], 15)

    def test_merge_stores_and_lobby_exposes_bit_three(self) -> None:
        self.assertTrue(anchor_mnsg._merge_client_state(2, {"appearanceFlags": 15}))
        self.assertEqual(anchor_mnsg._player_states[2]["appearanceFlags"], 15)
        lobby = json.loads(anchor_mnsg.get_lobby_positions_json())
        self.assertEqual(lobby[0]["ap"], 15)
        self.assertTrue(lobby[0]["ap"] & (1 << 3))

    def test_old_peer_mask_drops_bit_three_without_raising(self) -> None:
        with mock.patch.object(anchor_mnsg, "APPEARANCE_MASK", 7):
            self.assertTrue(
                anchor_mnsg._merge_client_state(2, {"appearanceFlags": 15})
            )
            self.assertEqual(anchor_mnsg._player_states[2]["appearanceFlags"], 7)
        lobby = json.loads(anchor_mnsg.get_lobby_positions_json())
        self.assertEqual(lobby[0]["ap"] & (1 << 3), 0)

    def test_legacy_movement_payload_derives_bits_without_bit_three(self) -> None:
        self.assertTrue(anchor_mnsg._merge_client_state(
            2,
            {
                "currentRoomId": 10,
                "posX": 100, "posY": 200, "posZ": 300,
                "posSeq": 1, "posT": 1000,
                "suddenImpact": True, "modelScale100000": 5000,
            },
            enforce_movement_order=True,
        ))
        flags = anchor_mnsg._player_states[2]["appearanceFlags"]
        self.assertEqual(flags & anchor_mnsg.APPEARANCE_SUDDEN_IMPACT, 1)
        self.assertEqual(flags & anchor_mnsg.APPEARANCE_MINI_EBISUMARU, 2)
        self.assertEqual(flags & (1 << 3), 0)

    def test_appearance_change_bypasses_the_movement_floor(self) -> None:
        with mock.patch.object(
            anchor_mnsg.time, "monotonic",
            side_effect=(100.0, 100.01, 100.02),
        ):
            self.assertTrue(
                anchor_mnsg.set_position_anim(0, 0, 0, 1, 0, 100, 0, 0, 0)
            )
            self.assertTrue(anchor_mnsg.set_position_anim(
                1, 0, 0, 1, 1, 100, 0, 0, 0, appearance_flags=1 << 3,
            ))
            self.assertFalse(anchor_mnsg.set_position_anim(
                2, 0, 0, 1, 2, 100, 0, 0, 0, appearance_flags=1 << 3,
            ))
        self.assertEqual(
            [p["appearanceFlags"] for p in self.packets()], [0, 1 << 3]
        )

    def test_worst_case_appearance_value_stays_within_the_hot_budget(self) -> None:
        anchor_mnsg._client_id = 0xFFFFFFFF
        anchor_mnsg._local_room_id = 0xFFFF
        anchor_mnsg._interaction_session = 0x7FFFFFFF
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(anchor_mnsg.set_position_anim(
                2147483647, -2147483648, 2147483647,
                255, 65535, 65535, -32768, 32767, -32768,
                appearance_flags=15,
                velocity_x=1000000, velocity_y=-1000000, velocity_z=1000000,
                angular_velocity_x=32767, angular_velocity_y=-32768,
                angular_velocity_z=32767, force_motion_edge=1,
                animation_step_100=-65535, has_animation_step=1,
                collision_disabled=1, drive_x=-30000, drive_z=30000,
                player_epoch=2147483647,
            ))
        wire = self.sock.sent[-1]
        self.assertLessEqual(
            len(wire), anchor_mnsg.HOT_PACKET_MAX_BYTES["MNSG_PLAYER_POS"]
        )
        self.assertTrue(wire.endswith(b"\x00"))
        self.assertEqual(self.packets()[-1]["appearanceFlags"], 15)


class StressAppearanceCommandTests(unittest.IsolatedAsyncioTestCase):
    @staticmethod
    def _controller() -> "anchor_stress.StressController":
        config = anchor_stress.BotConfig(
            host="example.test", port=43383, room_id="test", team_id="blue",
            name_prefix="Bot", start_x=0, start_y=0, start_z=0,
            start_room=10, character="Ebisumaru", rate_hz=5,
        )
        return anchor_stress.StressController(config, 1)

    async def test_alternative_command_toggles_bit_three_and_masks(self) -> None:
        controller = self._controller()
        bot = controller.bots[0]
        with mock.patch.object(bot, "_send", new_callable=mock.AsyncMock):
            await bot.set_state(
                appearance_flags=anchor_mnsg.APPEARANCE_HURT_RECOVERY
            )
            await controller._command_alternative(["1", "on"])
            self.assertEqual(
                bot.appearance_flags,
                anchor_mnsg.APPEARANCE_HURT_RECOVERY | anchor_mnsg.APPEARANCE_ALTERNATIVE_EBISUMARU,
            )
            self.assertEqual(
                bot.appearance_flags & anchor_mnsg.APPEARANCE_MASK,
                bot.appearance_flags,
            )
            await controller._command_alternative(["1", "off"])
            self.assertEqual(
                bot.appearance_flags, anchor_mnsg.APPEARANCE_HURT_RECOVERY
            )
            self.assertEqual(bot.appearance_flags & (1 << 3), 0)

    async def test_alternative_command_rejects_unknown_toggle(self) -> None:
        controller = self._controller()
        with self.assertRaises(ValueError):
            await controller._command_alternative(["1", "maybe"])

    async def test_stress_tool_mask_carries_bit_three(self) -> None:
        # The imported mask must be the widened one, not the old 7-bit fallback.
        self.assertEqual(anchor_stress.APPEARANCE_MASK, 15)


if __name__ == "__main__":
    unittest.main()
