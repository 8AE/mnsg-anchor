"""Durable multiplayer location metadata for the native Japan map."""

import json
import math
import sys
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


class WorldMapLocationTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    def connect(self, client_id: int = 7) -> FakeSocket:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(anchor_mnsg.socket, "socket", return_value=fake_socket),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "world-map", "Player", client_id
                )
            )
        return fake_socket

    @staticmethod
    def packets(fake_socket: FakeSocket) -> list[dict]:
        return [
            json.loads(raw.removesuffix(b"\x00")) for raw in fake_socket.sent
        ]

    def test_map_transition_retains_last_successful_gameplay_position(self) -> None:
        fake_socket = self.connect()

        self.assertTrue(anchor_mnsg.set_local_room(0x130))
        self.assertTrue(anchor_mnsg.set_position(101, -22, 303))
        self.assertTrue(anchor_mnsg.set_local_room(anchor_mnsg.WORLD_MAP_ROOM_ID))

        packet = self.packets(fake_socket)[-1]
        self.assertEqual(packet["type"], "UPDATE_CLIENT_STATE")
        self.assertEqual(
            packet["state"]["currentRoomId"], anchor_mnsg.WORLD_MAP_ROOM_ID
        )
        self.assertEqual(packet["state"]["mnsgMapRoomId"], 0x130)
        self.assertEqual(packet["state"]["mnsgMapX"], 10100)
        self.assertEqual(packet["state"]["mnsgMapY"], -2200)
        self.assertEqual(packet["state"]["mnsgMapZ"], 30300)
        self.assertEqual(anchor_mnsg._last_position_room_id, 0x130)

    def test_exact_native_location_wins_and_survives_replacement_updates(self) -> None:
        fake_socket = self.connect()
        self.assertTrue(anchor_mnsg.set_local_room(0x130))
        self.assertTrue(anchor_mnsg.set_position(101, 12, -10))

        self.assertTrue(
            anchor_mnsg.set_world_map_location(0x130, 101.234, -9.876)
        )
        # A frame-end gameplay sample racing the map transition must not
        # replace F170's exact source tuple after it has been captured.
        self.assertTrue(
            anchor_mnsg.set_position_anim(999, 12, 999, 4, 0, 0, 0, 0, 0)
        )
        self.assertTrue(anchor_mnsg.set_local_room(anchor_mnsg.WORLD_MAP_ROOM_ID))
        self.assertTrue(
            anchor_mnsg.update_client_state(
                json.dumps(
                    {
                        "currentCharacter": "Yae",
                        "mnsgMapRoomId": -1,
                        "mnsgMapX": 0,
                        "mnsgMapY": 0,
                        "mnsgMapZ": 0,
                    }
                )
            )
        )

        state = self.packets(fake_socket)[-1]["state"]
        self.assertEqual(state["currentRoomId"], anchor_mnsg.WORLD_MAP_ROOM_ID)
        self.assertEqual(
            (
                state["mnsgMapRoomId"],
                state["mnsgMapX"],
                state["mnsgMapY"],
                state["mnsgMapZ"],
            ),
            (0x130, 10123, 1200, -988),
        )

    def test_exact_setter_rejects_invalid_or_unbounded_native_values(self) -> None:
        fake_socket = self.connect()
        initial_count = len(fake_socket.sent)

        invalid_calls = (
            (True, 1.0, 2.0),
            (-1, 1.0, 2.0),
            (0x10000, 1.0, 2.0),
            (anchor_mnsg.WORLD_MAP_ROOM_ID, 1.0, 2.0),
            (0x130, math.nan, 2.0),
            (0x130, math.inf, 2.0),
            (0x130, 1.0e308, 2.0),
            (0x130, 1.0, -1.0e308),
        )
        for room_id, x, z in invalid_calls:
            with self.subTest(room_id=room_id, x=x, z=z):
                self.assertFalse(
                    anchor_mnsg.set_world_map_location(room_id, x, z)
                )

        self.assertEqual(len(fake_socket.sent), initial_count)
        self.assertIsNone(anchor_mnsg._local_map_snapshot)

    def test_all_state_snapshot_exposes_simultaneous_map_viewers(self) -> None:
        self.connect(client_id=1)
        anchor_mnsg._replace_all_client_states(
            [
                {
                    "clientId": 1,
                    "self": True,
                    "clientState": {"name": "Self", "online": True},
                },
                {
                    "clientId": 9,
                    "clientState": {
                        "name": "Nine",
                        "online": True,
                        "currentRoomId": anchor_mnsg.WORLD_MAP_ROOM_ID,
                        "mnsgMapRoomId": 0x130,
                        "mnsgMapX": 10123,
                        "mnsgMapY": 1200,
                        "mnsgMapZ": -988,
                    },
                },
                {
                    "clientId": 4,
                    "clientState": {
                        "name": "Four",
                        "online": True,
                        "currentRoomId": anchor_mnsg.WORLD_MAP_ROOM_ID,
                        "mnsgMapRoomId": 0x141,
                        "mnsgMapX": -250,
                        "mnsgMapY": 0,
                        "mnsgMapZ": 600,
                    },
                },
            ]
        )

        rows = json.loads(anchor_mnsg.get_lobby_positions_json())
        self.assertEqual([row["cid"] for row in rows], [4, 9])
        self.assertEqual(
            [(row["mr"], row["mx"], row["my"], row["mz"], row["mhp"])
             for row in rows],
            [
                (0x141, -250, 0, 600, 1),
                (0x130, 10123, 1200, -988, 1),
            ],
        )

    def test_incremental_state_merge_preserves_complete_map_record(self) -> None:
        self.connect(client_id=1)
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._merge_client_state(
                2,
                {
                    "name": "Two",
                    "online": True,
                    "currentRoomId": anchor_mnsg.WORLD_MAP_ROOM_ID,
                    "mnsgMapRoomId": 0x130,
                    "mnsgMapX": 125,
                    "mnsgMapY": -250,
                    "mnsgMapZ": 375,
                },
            )
            anchor_mnsg._merge_client_state(2, {"currentCharacter": "Sasuke"})

        row = json.loads(anchor_mnsg.get_lobby_positions_json())[0]
        self.assertEqual(
            (row["mr"], row["mx"], row["my"], row["mz"], row["mhp"]),
            (0x130, 125, -250, 375, 1),
        )

    def test_non_map_fallback_and_missing_legacy_data_are_explicit(self) -> None:
        self.connect(client_id=1)
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states.update(
                {
                    2: {
                        "name": "Moving",
                        "online": True,
                        "roomId": 0x130,
                        "posX": 12,
                        "posY": -3,
                        "posZ": 4,
                    },
                    3: {
                        "name": "Partial",
                        "online": True,
                        "roomId": 0x131,
                        "posX": 12,
                    },
                    4: {
                        "name": "Legacy Map Viewer",
                        "online": True,
                        "roomId": anchor_mnsg.WORLD_MAP_ROOM_ID,
                        "posX": 99,
                        "posY": 88,
                        "posZ": 77,
                    },
                    5: {
                        "name": "Offline",
                        "online": False,
                        "roomId": 0x130,
                        "posX": 1,
                        "posY": 2,
                        "posZ": 3,
                    },
                }
            )

        rows = json.loads(anchor_mnsg.get_lobby_positions_json())
        self.assertEqual([row["cid"] for row in rows], [2, 3, 4])
        self.assertEqual(
            (rows[0]["mr"], rows[0]["mx"], rows[0]["my"], rows[0]["mz"],
             rows[0]["mhp"]),
            (0x130, 1200, -300, 400, 1),
        )
        for row in rows[1:]:
            self.assertEqual(
                (row["mr"], row["mx"], row["my"], row["mz"], row["mhp"]),
                (-1, 0, 0, 0, 0),
            )

    def test_disconnect_and_new_connection_clear_local_snapshot(self) -> None:
        self.connect()
        self.assertTrue(anchor_mnsg.set_local_room(0x130))
        self.assertTrue(anchor_mnsg.set_position(1, 2, 3))
        self.assertIsNotNone(anchor_mnsg._local_map_snapshot)

        anchor_mnsg.disconnect()
        self.assertIsNone(anchor_mnsg._local_map_snapshot)
        self.assertFalse(anchor_mnsg._local_map_snapshot_explicit)

        anchor_mnsg._local_map_snapshot = (0x130, 100, 200, 300)
        anchor_mnsg._local_map_snapshot_explicit = True
        fake_socket = self.connect(client_id=8)
        handshake_state = self.packets(fake_socket)[0]["clientState"]
        self.assertIsNone(anchor_mnsg._local_map_snapshot)
        self.assertFalse(anchor_mnsg._local_map_snapshot_explicit)
        self.assertEqual(
            (
                handshake_state["mnsgMapRoomId"],
                handshake_state["mnsgMapX"],
                handshake_state["mnsgMapY"],
                handshake_state["mnsgMapZ"],
            ),
            (-1, 0, 0, 0),
        )


if __name__ == "__main__":
    unittest.main()
