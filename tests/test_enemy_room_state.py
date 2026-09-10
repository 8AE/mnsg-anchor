"""Occupied-room dead-enemy bitmap metadata used by enemy synchronization."""

import json
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


class EnemyRoomStateTests(unittest.TestCase):
    def setUp(self) -> None:
        anchor_mnsg.disconnect()

    def tearDown(self) -> None:
        anchor_mnsg.disconnect()

    def connect(self, client_id: int = 7) -> FakeSocket:
        fake_socket = FakeSocket()
        with (
            mock.patch.object(
                anchor_mnsg.socket, "socket", return_value=fake_socket
            ),
            mock.patch.object(anchor_mnsg.threading, "Thread", FakeThread),
        ):
            self.assertTrue(
                anchor_mnsg.connect(
                    "example.test", 43383, "enemy-room", "Player", client_id
                )
            )
        return fake_socket

    @staticmethod
    def packets(fake_socket: FakeSocket) -> list[dict]:
        return [
            json.loads(raw.removesuffix(b"\x00")) for raw in fake_socket.sent
        ]

    def add_peer(self, cid: int, **fields) -> None:
        state = {
            "name": f"Player{cid}",
            "online": True,
            "isSaveLoaded": True,
            "teamId": anchor_mnsg.DEFAULT_TEAM_ID,
            "self": False,
        }
        state.update(fields)
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states[cid] = state

    # ── Local publish ───────────────────────────────────────────────────

    def test_publish_carries_normalized_bitmap_on_every_metadata_update(
        self,
    ) -> None:
        fake_socket = self.connect(client_id=1)

        self.assertTrue(anchor_mnsg.set_enemy_room_state(0x49, 0x1234, "0100"))
        self.assertEqual(anchor_mnsg._local_enemy_room, 0x49)
        self.assertEqual(anchor_mnsg._local_enemy_sig, 0x1234)
        self.assertEqual(anchor_mnsg._local_enemy_bits, "01")

        # Anchor replaces client state; a later unrelated metadata update must
        # re-send the enemy-room record for a late room entrant.
        self.assertTrue(anchor_mnsg.set_character("Yae"))
        state = self.packets(fake_socket)[-1]["state"]
        self.assertEqual(state["er"], 0x49)
        self.assertEqual(state["es"], 0x1234)
        self.assertEqual(state["eb"], "01")

    def test_publish_normalizes_case_and_strips_only_high_zero_bytes(
        self,
    ) -> None:
        fake_socket = self.connect(client_id=1)

        self.assertTrue(anchor_mnsg.set_enemy_room_state(5, 9, "AB00"))
        self.assertEqual(anchor_mnsg._local_enemy_bits, "ab")

        # Leading zero bytes are part of the low-index bitmap and must remain.
        self.assertTrue(anchor_mnsg.set_enemy_room_state(5, 9, "00CD"))
        self.assertEqual(anchor_mnsg._local_enemy_bits, "00cd")

        state = self.packets(fake_socket)[-1]["state"]
        self.assertEqual(state["eb"], "00cd")

    def test_publish_rejects_invalid_values_without_sending(self) -> None:
        fake_socket = self.connect(client_id=1)
        self.assertTrue(anchor_mnsg.set_enemy_room_state(5, 9, "01"))
        before = len(fake_socket.sent)

        invalid = (
            (-1, 9, "01"),
            (0x10000, 9, "01"),
            (5, -1, "01"),
            (5, 9, "0"),
            (5, 9, "zz"),
            (5, 9, "0" * 66),
            (5, 9, 1234),
        )
        for room_id, signature, bits in invalid:
            with self.subTest(room_id=room_id, signature=signature, bits=bits):
                self.assertFalse(
                    anchor_mnsg.set_enemy_room_state(room_id, signature, bits)
                )

        self.assertEqual(len(fake_socket.sent), before)

    # ── Remote query ────────────────────────────────────────────────────

    def test_query_ors_matching_peers_and_filters_mismatches(self) -> None:
        self.connect(client_id=1)
        self.add_peer(
            2, roomId=0x49, er=0x49, es=0x1234, eb="01"
        )
        self.add_peer(
            3, roomId=0x49, er=0x49, es=0x1234, eb="02"
        )
        # Wrong signature, room, team, save, online, and self are all excluded.
        self.add_peer(4, roomId=0x49, er=0x49, es=0x9999, eb="04")
        self.add_peer(5, roomId=0x50, er=0x50, es=0x1234, eb="08")
        self.add_peer(
            6, roomId=0x49, er=0x49, es=0x1234, eb="10", teamId="other"
        )
        self.add_peer(
            8, roomId=0x49, er=0x49, es=0x1234, eb="20", isSaveLoaded=False
        )
        self.add_peer(
            9, roomId=0x49, er=0x49, es=0x1234, eb="40", online=False
        )
        self.add_peer(10, roomId=0x49, er=0x49, es=0x1234, eb="80")
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._player_states[10]["self"] = True

        self.assertEqual(
            anchor_mnsg.get_enemy_room_state(0x49, 0x1234), "03"
        )
        self.assertEqual(anchor_mnsg.get_enemy_room_state(0x49, 0x9999), "04")
        self.assertEqual(anchor_mnsg.get_enemy_room_state(0x4A, 0x1234), "")

    def test_query_returns_empty_for_no_matching_peer(self) -> None:
        self.connect(client_id=1)
        self.assertEqual(anchor_mnsg.get_enemy_room_state(0x49, 0x1), "")
        self.assertEqual(anchor_mnsg.get_enemy_room_state(-1, 0x1), "")
        self.assertEqual(anchor_mnsg.get_enemy_room_state(0x49, -1), "")

    # ── Incoming state merge ────────────────────────────────────────────

    def test_all_state_snapshot_exposes_enemy_bitmap(self) -> None:
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
                        "isSaveLoaded": True,
                        "teamId": anchor_mnsg.DEFAULT_TEAM_ID,
                        "currentRoomId": 0x49,
                        "er": 0x49,
                        "es": 0x1234,
                        "eb": "0f",
                    },
                },
            ]
        )

        with anchor_mnsg._player_states_lock:
            peer = anchor_mnsg._player_states[9]
        self.assertEqual(
            (peer["er"], peer["es"], peer["eb"]), (0x49, 0x1234, "0f")
        )
        self.assertEqual(
            anchor_mnsg.get_enemy_room_state(0x49, 0x1234), "0f"
        )

    def test_incremental_merge_preserves_enemy_bitmap(self) -> None:
        self.connect(client_id=1)
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._merge_client_state(
                2,
                {
                    "name": "Two",
                    "online": True,
                    "isSaveLoaded": True,
                    "teamId": anchor_mnsg.DEFAULT_TEAM_ID,
                    "currentRoomId": 0x49,
                    "er": 0x49,
                    "es": 0x1234,
                    "eb": "0f",
                },
            )
            # A following metadata-only update must not erase the bitmap.
            anchor_mnsg._merge_client_state(2, {"currentCharacter": "Sasuke"})

        self.assertEqual(
            anchor_mnsg.get_enemy_room_state(0x49, 0x1234), "0f"
        )

    def test_invalid_incoming_bitmap_is_not_stored(self) -> None:
        self.connect(client_id=1)
        self.add_peer(2, roomId=0x49, er=0x49, es=0x1234, eb="01")
        with anchor_mnsg._player_states_lock:
            anchor_mnsg._merge_client_state(
                2, {"eb": "not-hex", "es": 0x1234, "er": 0x49}
            )
            self.assertIsNone(anchor_mnsg._player_states[2]["eb"])

    # ── Answer election ─────────────────────────────────────────────────

    def test_request_election_picks_lowest_matching_occupant(self) -> None:
        self.connect(client_id=5)
        anchor_mnsg._client_id = 5
        self.add_peer(2, roomId=0x49, er=0x49, es=0x1234)
        self.add_peer(9, roomId=0x49, er=0x49, es=0x1234)

        self.assertFalse(
            anchor_mnsg._should_handle_enemy_state_request(
                {"clientId": 9, "r": 0x49, "s": 0x1234}
            )
        )
        anchor_mnsg._client_id = 2
        self.assertTrue(
            anchor_mnsg._should_handle_enemy_state_request(
                {"clientId": 9, "r": 0x49, "s": 0x1234}
            )
        )

    def test_request_election_defaults_to_handling_when_unconfirmed(self) -> None:
        # No handshake yet: a local client id of zero must not suppress the
        # response, otherwise a fresh room entrant never learns the bitmap.
        anchor_mnsg._client_id = 0
        self.assertTrue(
            anchor_mnsg._should_handle_enemy_state_request(
                {"clientId": 9, "r": 0x49, "s": 0x1234}
            )
        )

    # ── Lifecycle ───────────────────────────────────────────────────────

    def test_disconnect_and_new_connection_clear_local_enemy_state(self) -> None:
        self.connect()
        self.assertTrue(anchor_mnsg.set_enemy_room_state(0x49, 0x1, "01"))
        self.assertEqual(anchor_mnsg._local_enemy_bits, "01")

        anchor_mnsg.disconnect()
        self.assertEqual(anchor_mnsg._local_enemy_room, -1)
        self.assertEqual(anchor_mnsg._local_enemy_sig, 0)
        self.assertEqual(anchor_mnsg._local_enemy_bits, "")

        fake_socket = self.connect(client_id=8)
        handshake = self.packets(fake_socket)[0]["clientState"]
        self.assertEqual(handshake["er"], -1)
        self.assertEqual(handshake["es"], 0)
        self.assertEqual(handshake["eb"], "")

    # ── Live-enemy authority election ───────────────────────────────────

    def test_authority_is_self_when_alone(self) -> None:
        self.connect(client_id=4)
        anchor_mnsg._local_room_id = 0x49
        self.assertEqual(anchor_mnsg.get_enemy_authority(), 4)

    def test_authority_is_lowest_matching_room_occupant(self) -> None:
        self.connect(client_id=4)
        anchor_mnsg._local_room_id = 0x49
        self.add_peer(2, roomId=0x49)
        self.add_peer(9, roomId=0x49)
        self.add_peer(1, roomId=0x50)              # different room
        self.add_peer(3, roomId=0x49, isSaveLoaded=False)
        self.add_peer(5, roomId=0x49, teamId="other")
        self.add_peer(6, roomId=0x49, online=False)
        self.assertEqual(anchor_mnsg.get_enemy_authority(), 2)

    def test_authority_uses_self_room_when_self_state_is_missing(self) -> None:
        self.connect(client_id=8)
        anchor_mnsg._local_room_id = 0x49
        self.add_peer(2, roomId=0x49)
        self.assertEqual(anchor_mnsg.get_enemy_authority(), 2)

    def test_authority_is_zero_without_identity_or_room(self) -> None:
        self.connect(client_id=4)
        self.assertEqual(anchor_mnsg.get_enemy_authority(), 0)
        anchor_mnsg._local_room_id = 0x49
        anchor_mnsg._client_id = 0
        self.assertEqual(anchor_mnsg.get_enemy_authority(), 0)


if __name__ == "__main__":
    unittest.main()
