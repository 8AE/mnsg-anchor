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

        self.assertNotIn("anchor_room_id_new", options)
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


if __name__ == "__main__":
    unittest.main()
