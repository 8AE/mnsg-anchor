"""Single-team configuration and roster regression tests."""

import inspect
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
        self.sent: list[bytes] = []

    def settimeout(self, _timeout) -> None:
        pass

    def connect(self, _address) -> None:
        pass

    def sendall(self, data: bytes) -> None:
        self.sent.append(data)

    def close(self) -> None:
        pass


class FakeThread:
    def __init__(self, **_kwargs) -> None:
        pass

    def start(self) -> None:
        pass


class SingleTeamTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states.clear()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    def connect(self, *, client_id: int = 7) -> FakeSocket:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "single-team", "Player", client_id
                )
            )
        return fake_socket

    def test_user_configuration_has_no_team_name(self) -> None:
        with (ROOT / "mod.toml").open("rb") as manifest_file:
            manifest = tomllib.load(manifest_file)

        option_ids = {
            option["id"] for option in manifest["manifest"]["config_options"]
        }
        self.assertNotIn("anchor_team_id", option_ids)
        self.assertNotIn("Team Name", (ROOT / "mod.toml").read_text())
        self.assertNotIn("Team Name", (ROOT / "README.md").read_text())

    def test_connect_api_has_no_team_argument_and_handshake_uses_default(self) -> None:
        self.assertNotIn("team_id", inspect.signature(anchor_mnsg.connect).parameters)
        self.assertFalse(hasattr(anchor_mnsg, "set_team"))

        fake_socket = self.connect()
        handshake = json.loads(fake_socket.sent[0].removesuffix(b"\x00"))

        self.assertEqual(handshake["clientState"]["teamId"], "default")
        self.assertEqual(anchor_mnsg.get_team_id(), "default")

    def test_client_uis_do_not_expose_team_controls_or_headers(self) -> None:
        ui_sources = (
            ROOT / "src" / "ui" / "startup_multiplayer_ui.c",
            ROOT / "src" / "ui" / "anchor_connect_ui.c",
            ROOT / "src" / "ui" / "anchor_ui.c",
            ROOT / "src" / "ui" / "race_lobby_ui.c",
            ROOT / "src" / "ui" / "startup_race_ui.c",
        )
        forbidden = ('"Team Name"', '"Team: "', '"Winning Team: "', "anchor_team_id")

        for source_path in ui_sources:
            source = source_path.read_text()
            for marker in forbidden:
                with self.subTest(source=source_path.name, marker=marker):
                    self.assertNotIn(marker, source)

    def test_client_state_cannot_override_default_team(self) -> None:
        fake_socket = self.connect()

        self.assertTrue(
            anchor_mnsg.update_client_state(
                json.dumps({"teamId": "legacy-team", "currentRoomId": 10})
            )
        )
        packet = json.loads(fake_socket.sent[-1].removesuffix(b"\x00"))

        self.assertEqual(packet["state"]["teamId"], "default")
        self.assertEqual(anchor_mnsg.get_team_id(), "default")

    def test_player_info_is_flat_and_sorted_by_client_id(self) -> None:
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states.update(
                {
                    9: {"name": "Nine", "teamId": "legacy-red", "online": True},
                    4: {"name": "Offline", "teamId": "legacy-blue", "online": False},
                    2: {"name": "Two", "teamId": "legacy-blue", "online": True},
                }
            )

        players = json.loads(anchor_mnsg.get_player_info_json())

        self.assertEqual([player["cid"] for player in players], [2, 9])
        self.assertEqual([player["n"] for player in players], ["Two", "Nine"])
        self.assertTrue(all("t" not in player for player in players))

    def test_transfer_target_requires_fresh_safe_remote_movement(self) -> None:
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 9
        sample = {
            "name": "Two",
            "online": True,
            "isSaveLoaded": True,
            "currentRoomId": 0x161,
            "posX": -123,
            "posY": 45,
            "posZ": 678,
            "posSeq": 1,
            "posT": 1000,
            "collisionDisabled": 0,
            "playerEpoch": 7,
            "interactionSession": 11,
        }
        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
            self.assertTrue(anchor_mnsg._merge_client_state(
                2, sample, enforce_movement_order=True
            ))
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states[9] = {
                "name": "Local", "online": True, "self": True,
                "isSaveLoaded": True,
            }

        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=104.999):
            target = json.loads(anchor_mnsg.get_transfer_target_json(2))
            players = json.loads(anchor_mnsg.get_player_info_json())
        self.assertEqual(target, {
            "cid": 2, "room": 0x161, "x": -123, "y": 45, "z": 678,
        })
        self.assertEqual(players[0]["ct"], 1)
        self.assertEqual(players[0]["self"], 0)
        self.assertEqual((players[1]["cid"], players[1]["self"], players[1]["ct"]),
                         (9, 1, 0))

        with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=105.001):
            self.assertEqual(anchor_mnsg.get_transfer_target_json(2), "{}")
        self.assertEqual(anchor_mnsg.get_transfer_target_json(9), "{}")

    def test_transfer_target_rejects_scripted_world_map_and_wide_coordinates(self) -> None:
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        base = {
            "name": "Two",
            "online": True,
            "isSaveLoaded": True,
            "currentRoomId": 0x130,
            "posX": 1,
            "posY": 2,
            "posZ": 3,
            "posSeq": 1,
            "posT": 1000,
            "collisionDisabled": 0,
            "playerEpoch": 4,
            "interactionSession": 8,
        }

        rejected = (
            {"collisionDisabled": 1},
            {"currentRoomId": anchor_mnsg.WORLD_MAP_ROOM_ID},
            {"currentRoomId": anchor_mnsg.TRANSFER_ROOM_MAX + 1},
            {"posX": anchor_mnsg.TRANSFER_COORD_MAX + 1},
            {"posY": anchor_mnsg.TRANSFER_COORD_MIN - 1},
            {"playerEpoch": 0},
            {"interactionSession": 0},
            {"isSaveLoaded": False},
        )
        for index, changes in enumerate(rejected, start=1):
            with self.subTest(changes=changes):
                with anchor_mnsg._player_states_lock:
                    anchor_mnsg._player_states.clear()
                    anchor_mnsg._player_movement_order.clear()
                    anchor_mnsg._retired_interaction_sessions.clear()
                sample = {**base, **changes, "posSeq": index}
                with mock.patch.object(anchor_mnsg.time, "monotonic", return_value=100.0):
                    self.assertTrue(anchor_mnsg._merge_client_state(
                        2, sample, enforce_movement_order=True
                    ))
                    self.assertEqual(anchor_mnsg.get_transfer_target_json(2), "{}")

    def test_race_lobby_is_flat_and_sorted_by_client_id(self) -> None:
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 9
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states.update(
                {
                    9: {
                        "name": "Nine",
                        "teamId": "legacy-red",
                        "mnsgRace": "lobby",
                        "online": True,
                    },
                    4: {
                        "name": "Offline",
                        "teamId": "legacy-blue",
                        "online": False,
                    },
                    2: {
                        "name": "Two",
                        "teamId": "legacy-blue",
                        "mnsgRace": "started",
                        "online": True,
                    },
                }
            )

        players = json.loads(anchor_mnsg.get_race_lobby_json())

        self.assertEqual([player["cid"] for player in players], [2, 9])
        self.assertEqual([player["n"] for player in players], ["Two", "Nine"])
        self.assertTrue(all("t" not in player for player in players))


if __name__ == "__main__":
    unittest.main()
