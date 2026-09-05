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
        self.sent = []

    def sendall(self, data: bytes) -> None:
        self.sent.append(json.loads(data.removesuffix(b"\x00")))

    def close(self) -> None:
        pass


class CollisionTransportTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._team_id = "blue"
        anchor_mnsg._position_seq = 0

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    @staticmethod
    def send_sample(collision_disabled: int = 0) -> bool:
        return anchor_mnsg.set_position_anim(
            100, 200, 300, 5, 120, 1000, 10, 20, 30,
            2, 111, -222, 333, 444, -555, 666, 0, 75, 1,
            collision_disabled,
        )

    @staticmethod
    def receive_sample(seq: int, collision_disabled=0, room: int = 10) -> bool:
        payload = {
            "currentRoomId": room,
            "posX": 100 + seq,
            "posY": 200,
            "posZ": 300,
            "posSeq": seq,
            "posT": 1000 + seq,
        }
        if collision_disabled is not None:
            payload["collisionDisabled"] = collision_disabled
        return anchor_mnsg._merge_client_state(2, payload, enforce_movement_order=True)

    def test_both_cutscene_edges_bypass_the_interval_with_animation_intact(self) -> None:
        with mock.patch.object(
            anchor_mnsg.time, "monotonic",
            side_effect=(100.0, 100.01, 100.02, 100.03, 100.04),
        ):
            self.assertTrue(self.send_sample())
            self.assertFalse(self.send_sample())
            self.assertTrue(self.send_sample(1))
            self.assertFalse(self.send_sample(1))
            self.assertTrue(self.send_sample())

        self.assertEqual([p["collisionDisabled"] for p in self.sock.sent], [0, 1, 0])
        self.assertEqual([p["posSeq"] for p in self.sock.sent], [1, 2, 3])
        for packet in self.sock.sent:
            self.assertEqual(packet["appearanceFlags"], 2)
            self.assertEqual(packet["action"], 5)
            self.assertEqual(packet["animFrame100"], 120)
            self.assertEqual(packet["animFrameCount100"], 1000)
            self.assertEqual(packet["animStep100"], 75)
            self.assertEqual(packet["hasAnimStep"], 1)
            self.assertEqual(packet["velY"], -222)
            self.assertEqual(packet["rotVelY"], -555)
        self.assertEqual(anchor_mnsg._player_states[1]["collisionDisabled"], 0)

    def test_failed_transition_is_retried_without_advancing_the_send_baseline(self) -> None:
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(self.send_sample())
            with mock.patch.object(anchor_mnsg, "_send_raw", return_value=False):
                self.assertFalse(self.send_sample(1))
            self.assertEqual(anchor_mnsg._last_position_collision_disabled, 0)
            self.assertEqual(anchor_mnsg._position_seq, 1)
            self.assertTrue(self.send_sample(1))
        self.assertEqual([p["collisionDisabled"] for p in self.sock.sent], [0, 1])

    def test_legacy_sender_defaults_to_collision_and_disconnect_resets_gate(self) -> None:
        with mock.patch.object(
            anchor_mnsg.time, "monotonic", side_effect=(100.0, 100.01)
        ):
            self.assertTrue(self.send_sample(1))
            self.assertTrue(anchor_mnsg.set_position_anim(
                100, 200, 300, 5, 120, 1000, 10, 20, 30, 2
            ))
        self.assertEqual(self.sock.sent[-1]["collisionDisabled"], 0)
        anchor_mnsg.disconnect()
        self.assertEqual(anchor_mnsg._last_position_collision_disabled, -1)

    def test_reordered_sample_cannot_toggle_gate_and_legacy_sample_clears_it(self) -> None:
        self.assertTrue(self.receive_sample(10, 1))
        self.assertFalse(self.receive_sample(9, 0))
        self.assertEqual(anchor_mnsg._player_states[2]["collisionDisabled"], 1)
        self.assertEqual(anchor_mnsg._player_states[2]["posX"], 110)
        self.assertTrue(self.receive_sample(11, None))
        self.assertEqual(anchor_mnsg._player_states[2]["collisionDisabled"], 0)

    def test_membership_preserves_hot_gate_and_room_change_invalidates_it(self) -> None:
        self.assertTrue(self.receive_sample(10, 1))
        anchor_mnsg._replace_all_client_states([{
            "clientId": 2,
            "clientState": {
                "name": "Two", "currentRoomId": 10, "collisionDisabled": 0,
                "posX": -999, "posSeq": 1,
            },
        }])
        state = anchor_mnsg._player_states[2]
        self.assertEqual(state["collisionDisabled"], 1)
        self.assertEqual(state["posX"], 110)
        self.assertEqual(state["posSeq"], 10)
        lobby = json.loads(anchor_mnsg.get_lobby_positions_json())
        self.assertEqual(lobby[0]["cd"], 1)
        self.assertEqual(lobby[0]["hp"], 1)

        anchor_mnsg._merge_client_state(2, {"currentRoomId": 11})
        self.assertNotIn("collisionDisabled", state)
        self.assertNotIn("posX", state)
        lobby = json.loads(anchor_mnsg.get_lobby_positions_json())
        self.assertEqual(lobby[0]["cd"], 0)
        self.assertEqual(lobby[0]["hp"], 0)
        self.assertTrue(self.receive_sample(11, 1, room=11))
        self.assertEqual(state["collisionDisabled"], 1)

        anchor_mnsg._replace_all_client_states([{
            "clientId": 2, "clientState": {"currentRoomId": 12},
        }])
        self.assertNotIn("collisionDisabled", anchor_mnsg._player_states[2])

    def test_gate_survives_an_atomic_room_change_and_normalizes_to_boolean(self) -> None:
        self.assertTrue(self.receive_sample(10, 0))
        self.assertTrue(self.receive_sample(11, 7, room=11))
        state = anchor_mnsg._player_states[2]
        self.assertEqual(state["roomId"], 11)
        self.assertEqual(state["posX"], 111)
        self.assertEqual(state["collisionDisabled"], 1)


class StressCollisionTests(unittest.IsolatedAsyncioTestCase):
    async def test_bot_packets_manual_gate_and_follow_mirror_cutscene_state(self) -> None:
        config = anchor_stress.BotConfig(
            host="example.test", port=43383, room_id="test", team_id="blue",
            name_prefix="Bot", start_x=0, start_y=0, start_z=0,
            start_room=10, character="Goemon", rate_hz=5,
        )
        controller = anchor_stress.StressController(config, 1, follow="2")
        bot = controller.bots[0]
        with mock.patch.object(bot, "_send", new_callable=mock.AsyncMock) as send:
            await bot.publish_position()
            self.assertEqual(send.call_args.args[0]["collisionDisabled"], 0)
            await bot.set_state(collision_disabled=1)
            self.assertEqual(send.call_args.args[0]["collisionDisabled"], 1)

            await controller.world.update_from_position_packet({
                "clientId": 2, "currentRoomId": 10,
                "posX": 5, "posY": 6, "posZ": 7, "collisionDisabled": 0,
            })
            await controller._apply_follow()
            await bot.publish_position()
            self.assertEqual(send.call_args.args[0]["collisionDisabled"], 0)
            await controller.world.update_from_position_packet({
                "clientId": 2, "currentRoomId": 10,
                "posX": 5, "posY": 6, "posZ": 7, "collisionDisabled": 1,
            })
            await controller._apply_follow()
            await bot.publish_position()
            self.assertEqual(send.call_args.args[0]["collisionDisabled"], 1)

    async def test_stress_membership_preserves_gate_and_legacy_movement_clears_it(self) -> None:
        world = anchor_stress.WorldState()
        sample = {"clientId": 2, "currentRoomId": 10, "posX": 1, "collisionDisabled": 1}
        await world.update_from_position_packet(sample)
        await world.update_from_all_client_state({"state": [{
            "clientId": 2, "clientState": {"currentRoomId": 10},
        }]})
        self.assertEqual((await world.snapshot())[0].collision_disabled, 1)
        sample.pop("collisionDisabled")
        await world.update_from_position_packet(sample)
        self.assertEqual((await world.snapshot())[0].collision_disabled, 0)


if __name__ == "__main__":
    unittest.main()
