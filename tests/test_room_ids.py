import json
import sys
import tomllib
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))

import anchor_mnsg  # noqa: E402


class FakeSocket:
    def __init__(self) -> None:
        self.sent = []
        self.closed = False

    def settimeout(self, _timeout) -> None:
        pass

    def connect(self, _address) -> None:
        pass

    def sendall(self, data: bytes) -> None:
        self.sent.append(data)

    def close(self) -> None:
        self.closed = True


class FakeThread:
    def __init__(self, **_kwargs) -> None:
        pass

    def start(self) -> None:
        pass


class RoomIdTests(unittest.TestCase):
    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    def test_normalization_adds_exactly_one_hidden_prefix(self) -> None:
        self.assertEqual(anchor_mnsg.normalize_room_id("friends"), "mnsg-friends")
        self.assertEqual(
            anchor_mnsg.normalize_room_id(" mnsg-friends "), "mnsg-friends"
        )
        self.assertEqual(anchor_mnsg.normalize_room_id("mnsg-"), "")
        self.assertEqual(anchor_mnsg.normalize_room_id("\v\f"), "")

    def test_blank_room_is_rejected_before_socket_creation(self) -> None:
        for invalid_room in (" \t\v\f", " mnsg- "):
            with self.subTest(room=invalid_room):
                with mock.patch.object(
                    anchor_mnsg.socket, "socket"
                ) as socket_constructor:
                    self.assertFalse(
                        anchor_mnsg.connect(
                            "example.test", 43383, invalid_room, "Player"
                        )
                    )
                socket_constructor.assert_not_called()

    def test_manifest_keeps_custom_room_config_key_with_blank_default(self) -> None:
        with (ROOT / "mod.toml").open("rb") as manifest_file:
            manifest = tomllib.load(manifest_file)
        options = {
            option["id"]: option
            for option in manifest["manifest"]["config_options"]
        }

        self.assertIn("anchor_room_id_new", options)
        self.assertEqual(options["anchor_room_id_new"]["default"], "")

    def test_handshake_uses_hidden_prefixed_room(self) -> None:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "friends", "Player", team_id="team"
                )
            )

        self.assertEqual(len(fake_socket.sent), 1)
        handshake = json.loads(fake_socket.sent[0].removesuffix(b"\x00"))
        self.assertEqual(handshake["roomId"], "mnsg-friends")
        self.assertEqual(handshake["clientState"]["teamId"], "team")

    def test_room_metadata_invalidates_the_old_transform(self) -> None:
        anchor_mnsg._player_states[7] = {
            "name": "Seven",
            "teamId": "blue",
            "appearanceFlags": 2,
            "roomId": 10,
            "posX": 100,
            "posY": 200,
            "posZ": 300,
            "velX": 10,
            "posSeq": 4,
        }

        anchor_mnsg._merge_client_state(7, {"currentRoomId": 11})

        self.assertEqual(anchor_mnsg._player_states[7]["roomId"], 11)
        self.assertEqual(anchor_mnsg._player_states[7]["name"], "Seven")
        self.assertEqual(anchor_mnsg._player_states[7]["teamId"], "blue")
        self.assertEqual(anchor_mnsg._player_states[7]["appearanceFlags"], 2)
        self.assertNotIn("posX", anchor_mnsg._player_states[7])
        self.assertNotIn("posSeq", anchor_mnsg._player_states[7])

        anchor_mnsg._merge_client_state(
            7,
            {
                "currentRoomId": 11,
                "posX": -1,
                "posY": -2,
                "posZ": -3,
                "posSeq": 5,
            },
        )
        self.assertEqual(anchor_mnsg._player_states[7]["posX"], -1)

        anchor_mnsg._player_states[8] = {
            "name": "Eight",
            "roomId": 20,
            "posX": 1,
            "posY": 2,
            "posZ": 3,
            "velX": 99,
            "posSeq": 40,
            "action": 12,
        }
        anchor_mnsg._merge_client_state(
            8,
            {"currentRoomId": 21, "posX": 4, "posY": 5, "posZ": 6},
        )
        self.assertEqual(anchor_mnsg._player_states[8]["posX"], 4)
        self.assertNotIn("velX", anchor_mnsg._player_states[8])
        self.assertNotIn("posSeq", anchor_mnsg._player_states[8])
        self.assertNotIn("action", anchor_mnsg._player_states[8])

    def test_hot_movement_rejects_reordering_and_accepts_wrap(self) -> None:
        anchor_mnsg._player_states[7] = {
            "name": "Seven",
            "roomId": 10,
            "posX": 100,
            "posY": 0,
            "posZ": 0,
            "posSeq": 10,
            "posT": 1000,
            "animStep100": 75,
            "hasAnimStep": 1,
        }

        self.assertFalse(
            anchor_mnsg._merge_client_state(
                7,
                {"currentRoomId": 9, "posX": 90, "posSeq": 9, "posT": 900},
                enforce_movement_order=True,
            )
        )
        self.assertEqual(anchor_mnsg._player_states[7]["roomId"], 10)
        self.assertEqual(anchor_mnsg._player_states[7]["posX"], 100)
        self.assertEqual(anchor_mnsg._player_states[7]["hasAnimStep"], 1)

        anchor_mnsg._player_states[7]["posSeq"] = 0x7fffffff
        anchor_mnsg._player_states[7]["posT"] = 1100
        self.assertTrue(
            anchor_mnsg._merge_client_state(
                7,
                {"currentRoomId": 10, "posX": 101, "posSeq": 0, "posT": 1200},
                enforce_movement_order=True,
            )
        )
        self.assertEqual(anchor_mnsg._player_states[7]["animStep100"], 0)
        self.assertEqual(anchor_mnsg._player_states[7]["hasAnimStep"], 0)
        self.assertTrue(
            anchor_mnsg._merge_client_state(
                7,
                {"currentRoomId": 10, "posX": 102, "posSeq": 1, "posT": 1300},
                enforce_movement_order=True,
            )
        )
        self.assertEqual(anchor_mnsg._player_states[7]["posX"], 102)

        anchor_mnsg._player_states[7]["posSeq"] = 500
        anchor_mnsg._player_states[7]["posT"] = 2000
        self.assertTrue(
            anchor_mnsg._merge_client_state(
                7,
                {"currentRoomId": 10, "posX": 103, "posSeq": 1, "posT": 2100},
                enforce_movement_order=True,
            )
        )
        self.assertEqual(anchor_mnsg._player_states[7]["posX"], 103)

        self.assertFalse(
            anchor_mnsg._merge_client_state(
                7,
                {"currentRoomId": 10, "posX": 104, "posSeq": 501, "posT": 2050},
                enforce_movement_order=True,
            )
        )
        self.assertEqual(anchor_mnsg._player_states[7]["posX"], 103)

    def test_membership_snapshot_preserves_same_room_hot_state(self) -> None:
        anchor_mnsg._player_states[7] = {
            "name": "Old Seven",
            "roomId": 10,
            "posX": 100,
            "posY": 200,
            "posZ": 300,
            "velX": 30,
            "posSeq": 8,
            "posT": 1000,
            "animStep100": 75,
            "hasAnimStep": 1,
            "appearanceFlags": 2,
            "character": "Goemon",
        }
        anchor_mnsg._player_states[8] = {
            "name": "Gone",
            "roomId": 10,
            "posX": 1,
        }
        anchor_mnsg._player_states[9] = {
            "name": "Nine",
            "roomId": 20,
            "posX": 9,
            "character": "Yae",
        }

        anchor_mnsg._replace_all_client_states([
            {
                "clientId": 7,
                "clientState": {
                    "name": "Seven",
                    "teamId": "blue",
                    "currentRoomId": 10,
                },
            },
            {
                "clientId": 9,
                "clientState": {
                    "name": "Nine",
                    "currentRoomId": 21,
                },
            },
        ])

        seven = anchor_mnsg._player_states[7]
        self.assertEqual(seven["name"], "Seven")
        self.assertEqual(seven["posX"], 100)
        self.assertEqual(seven["posSeq"], 8)
        self.assertEqual(seven["animStep100"], 75)
        self.assertEqual(seven["hasAnimStep"], 1)
        self.assertEqual(seven["appearanceFlags"], 2)
        self.assertEqual(seven["character"], "Goemon")
        self.assertNotIn(8, anchor_mnsg._player_states)
        self.assertNotIn("posX", anchor_mnsg._player_states[9])
        self.assertEqual(anchor_mnsg._player_states[9]["character"], "Yae")

    def test_room_change_resets_the_velocity_baseline(self) -> None:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "friends", "Player", client_id=7
                )
            )

        with mock.patch.object(
            anchor_mnsg.time, "monotonic", side_effect=(100.0, 100.1, 100.11)
        ):
            anchor_mnsg.set_local_room(10)
            self.assertTrue(
                anchor_mnsg.set_position_anim(0, 0, 0, 1, 0, 100, 0, 0, 0)
            )
            self.assertTrue(
                anchor_mnsg.set_position_anim(60, 0, 0, 1, 20, 100, 0, 0, 0)
            )
            anchor_mnsg.set_local_room(11)
            self.assertTrue(
                anchor_mnsg.set_position_anim(1000, 0, 0, 1, 40, 100, 0, 0, 0)
            )

        movement_packets = [
            json.loads(raw.removesuffix(b"\x00"))
            for raw in fake_socket.sent
            if json.loads(raw.removesuffix(b"\x00")).get("type")
            == "MNSG_PLAYER_POS"
        ]
        self.assertNotEqual(movement_packets[1]["velX"], 0)
        self.assertEqual(movement_packets[2]["velX"], 0)

    def test_endpoint_velocity_overrides_packet_interval_average(self) -> None:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "friends", "Player", client_id=7
                )
            )

        with mock.patch.object(
            anchor_mnsg.time, "monotonic", side_effect=(200.0, 200.1)
        ):
            anchor_mnsg.set_local_room(10)
            self.assertTrue(
                anchor_mnsg.set_position_anim(
                    0, 0, 0, 1, 0, 100, 0, 0, 0, 0,
                    111, -222, 333, 444, -555, 666, 0, 75, 1
                )
            )
            self.assertTrue(
                anchor_mnsg.set_position_anim(
                    60, 0, 0, 1, 20, 100, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 1
                )
            )

        movement_packets = [
            json.loads(raw.removesuffix(b"\x00"))
            for raw in fake_socket.sent
            if json.loads(raw.removesuffix(b"\x00")).get("type")
            == "MNSG_PLAYER_POS"
        ]
        self.assertEqual(movement_packets[0]["velX"], 111)
        self.assertEqual(movement_packets[0]["velY"], -222)
        self.assertEqual(movement_packets[0]["velZ"], 333)
        self.assertEqual(movement_packets[0]["rotVelX"], 444)
        self.assertEqual(movement_packets[0]["rotVelY"], -555)
        self.assertEqual(movement_packets[0]["rotVelZ"], 666)
        self.assertEqual(movement_packets[0]["animStep100"], 75)
        self.assertEqual(movement_packets[0]["hasAnimStep"], 1)
        self.assertEqual(movement_packets[1]["velX"], 0)
        self.assertEqual(movement_packets[1]["rotVelY"], 0)
        self.assertEqual(movement_packets[1]["animStep100"], 0)
        self.assertEqual(movement_packets[1]["hasAnimStep"], 1)

    def test_sparse_motion_edge_bypasses_python_rate_limit(self) -> None:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "friends", "Player", client_id=7
                )
            )

        with mock.patch.object(
            anchor_mnsg.time, "monotonic", side_effect=(300.0, 300.01)
        ):
            anchor_mnsg.set_local_room(10)
            self.assertTrue(
                anchor_mnsg.set_position_anim(
                    0, 0, 0, 1, 0, 100, 0, 0, 0, 0,
                    300, 0, 0, 0, 3000, 0
                )
            )
            self.assertTrue(
                anchor_mnsg.set_position_anim(
                    3, 0, 0, 1, 10, 100, 0, 100, 0, 0,
                    0, 0, 0, 0, 0, 0, 1
                )
            )

        movement_packets = [
            json.loads(raw.removesuffix(b"\x00"))
            for raw in fake_socket.sent
            if json.loads(raw.removesuffix(b"\x00")).get("type")
            == "MNSG_PLAYER_POS"
        ]
        self.assertEqual(len(movement_packets), 2)
        self.assertEqual(movement_packets[1]["velX"], 0)
        self.assertEqual(movement_packets[1]["rotVelY"], 0)

    def test_lobby_positions_exposes_wrap_safe_sender_time(self) -> None:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "friends", "Player", client_id=7
                )
            )

        anchor_mnsg._merge_client_state(
            8,
            {
                "name": "Eight",
                "currentRoomId": 1,
                "posX": 10,
                "posY": 20,
                "posZ": 30,
                "posSeq": 4,
                "posT": 0x80000005,
                "rotVelX": 700,
                "rotVelY": -800,
                "rotVelZ": 900,
                "animStep100": 75,
                "hasAnimStep": 1,
            },
        )
        lobby = json.loads(anchor_mnsg.get_lobby_positions_json())

        self.assertEqual(lobby[0]["t"], 5)
        self.assertEqual(lobby[0]["rvx"], 700)
        self.assertEqual(lobby[0]["rvy"], -800)
        self.assertEqual(lobby[0]["rvz"], 900)
        self.assertEqual(lobby[0]["as"], 75)
        self.assertEqual(lobby[0]["ah"], 1)


if __name__ == "__main__":
    unittest.main()
