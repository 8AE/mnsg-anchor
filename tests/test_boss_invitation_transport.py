"""Boss arena invitation transport/lifecycle tests; no game or server required."""

import importlib.util
import json
import re
import sys
import unittest
from pathlib import Path
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "py" / "anchor_mnsg.py"
sys.path.insert(0, str(MODULE_PATH.parent))


class RecordingSocket:
    def __init__(self, received=()):
        self.sent = []
        self.received = iter(received)

    def sendall(self, data):
        self.sent.append(data)

    def settimeout(self, _timeout):
        pass

    def connect(self, _address):
        pass

    def recv(self, _size):
        return next(self.received, b"")

    def close(self):
        pass


def load_client(cid=1, session=101, team="blue", room=10):
    spec = importlib.util.spec_from_file_location(f"arena_client_{cid}", MODULE_PATH)
    client = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(client)
    client._sock = RecordingSocket()
    client._connected = True
    client._client_id = cid
    client._interaction_session = session
    client._team_id = team
    client._player_name = f"Player {cid}"
    client.set_save_loaded(True)
    client.set_local_room(room)
    client._sock.sent.clear()
    return client


class BossInvitationTransportTests(unittest.TestCase):
    def setUp(self):
        self.clock = mock.patch("time.monotonic", return_value=100.0).start()
        self.addCleanup(mock.patch.stopall)
        self.client = load_client()
        self.addCleanup(self.client.disconnect)
        self.sock = self.client._sock
        self.peer()

    def peer(self, cid=2, **changes):
        payload = {
            "currentRoomId": self.client.CONGO_ROOM, "online": True,
            "isSaveLoaded": True, "teamId": "blue", "name": "Congo guest",
            "posSeq": 4, "posT": 100000, "interactionSession": 202,
            "playerEpoch": 7,
        }
        payload.update(changes)
        self.assertTrue(self.client._merge_client_state(cid, payload, True))

    def packet(self, **changes):
        packet = {
            "type": "MNSG_BOSS_ARENA", "clientId": 2,
            "targetTeamId": "blue", "arena": self.client.CONGO_ARENA,
            "entered": True, "session": 202, "seq": 1,
        }
        packet.update(changes)
        return packet

    def invitation(self):
        result = self.client.get_boss_invitation_json()
        return json.loads(result) if result else None

    def receive(self, **changes):
        return self.client._receive_boss_arena(self.packet(**changes))

    def route_bytes(self, *chunks):
        """Route real framed bytes, preserving state when our fake socket ends."""
        receiver = RecordingSocket(chunks)
        self.client._sock = receiver
        with mock.patch.object(self.client, "_do_disconnect"):
            self.client._recv_loop(receiver)
        self.client._sock = self.sock
        self.assertEqual(receiver.sent, [])

    def route_packets(self, *packets):
        wire = b"".join((json.dumps(p) + "\x00").encode() for p in packets)
        self.route_bytes(wire)

    def current(self, cid=2, session=202, sequence=1):
        return self.client.boss_invitation_is_current(cid, session, sequence)

    def test_sender_tcp_packet_reaches_other_room_client_through_receive_loop(self):
        sender = load_client(2, 202, room=self.client.CONGO_ROOM)
        self.addCleanup(sender.disconnect)
        self.assertTrue(sender.set_boss_arena(sender.CONGO_ARENA))
        self.assertEqual(len(sender._sock.sent), 1)
        raw = sender._sock.sent[0]
        self.assertTrue(raw.endswith(b"\x00"))
        self.assertEqual(json.loads(raw[:-1]), self.packet())
        self.assertNotIn(b"addToQueue", raw)
        self.route_bytes(raw[:11], raw[11:])
        self.assertEqual(self.invitation(), {
            "cid": 2, "session": 202, "seq": 1,
            "arena": sender.CONGO_ARENA, "name": "Congo guest",
        })
        self.assertEqual(self.client._local_room_id, 10)
        self.assertTrue(self.current())
        self.assertFalse(self.client.has_packet())
        self.assertEqual(self.sock.sent, [])

    def test_entry_exit_reentry_are_edges_and_failed_send_retries_sequence(self):
        self.assertTrue(self.client.set_boss_arena(0))
        self.assertEqual(self.sock.sent, [])
        with mock.patch.object(self.client, "_send_raw", side_effect=[False, True]) as send:
            self.assertFalse(self.client.set_boss_arena(self.client.CONGO_ARENA))
            self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
            self.assertEqual(send.call_args_list[0], send.call_args_list[1])
            self.assertEqual(send.call_args.args[0]["seq"], 1)
        for _ in range(10):
            self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        self.assertEqual(self.sock.sent, [])
        self.assertTrue(self.client.set_boss_arena(0))
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        packets = [json.loads(raw[:-1]) for raw in self.sock.sent]
        self.assertEqual([(p["seq"], p["entered"]) for p in packets], [(2, False), (3, True)])

    def test_sender_rejects_unsupported_arena_unloaded_save_and_missing_identity(self):
        for arena in (-1, 5, 0x155, True, "1", None):
            with self.subTest(arena=arena):
                self.assertFalse(self.client.set_boss_arena(arena))
        for field, value in (("_local_save_loaded", False), ("_connected", False),
                             ("_client_id", 0), ("_interaction_session", 0)):
            with self.subTest(field=field), mock.patch.object(self.client, field, value):
                self.assertFalse(self.client.set_boss_arena(self.client.CONGO_ARENA))
        self.assertEqual(self.sock.sent, [])

    def test_same_room_new_native_visit_sends_once_without_requiring_room_id_change(self):
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA, 7))
        for _ in range(10):
            self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA, 7))
        self.assertEqual(len(self.sock.sent), 1)
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA, 8))
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA, 8))
        packets = [json.loads(raw[:-1]) for raw in self.sock.sent]
        self.assertEqual([(p["seq"], p["entered"]) for p in packets], [(1, True), (2, True)])
        self.assertTrue(all("visit" not in p for p in packets))
        for visit in (-1, True, "8", None, 0x80000000):
            with self.subTest(visit=visit):
                self.assertFalse(self.client.set_boss_arena(self.client.CONGO_ARENA, visit))

    def test_receiver_rejects_self_other_teams_and_malformed_packets(self):
        invalid = [{"type": "SET_FLAG"}, {"clientId": 1}, {"targetTeamId": "red"},
                   {"arena": 0}, {"arena": 5}, {"arena": 0x155}, {"arena": True},
                   {"entered": 1}, {"entered": "true"}]
        for field in ("clientId", "session", "seq"):
            invalid.extend({field: value} for value in (0, -1, 0x80000000, True, "2", None))
        for changes in invalid:
            with self.subTest(changes=changes):
                self.assertFalse(self.receive(**changes))
        for value in (None, [], "packet"):
            self.assertFalse(self.client._receive_boss_arena(value))
        for field in self.packet():
            packet = self.packet()
            del packet[field]
            with self.subTest(missing=field):
                self.assertFalse(self.client._receive_boss_arena(packet))
        self.assertIsNone(self.invitation())
        self.assertEqual(self.sock.sent, [])

    def test_receiver_rechecks_actual_peer_team_despite_matching_envelope(self):
        self.client._merge_client_state(2, {"teamId": "red"})
        self.assertFalse(self.receive())
        self.client._merge_client_state(2, {"teamId": "blue", "online": False})
        self.assertFalse(self.receive())
        self.assertIsNone(self.invitation())

    def test_unknown_peer_event_waits_for_membership_and_session(self):
        self.client._player_states.clear()
        self.client._player_movement_order.clear()
        self.assertTrue(self.receive())
        self.assertIsNone(self.invitation())
        self.clock.return_value = 104.0
        self.peer()
        self.assertTrue(self.current())
        self.assertEqual(self.invitation()["name"], "Congo guest")

    def test_initial_snapshot_session_confirms_invitation_without_movement_packet(self):
        self.client._player_states.clear()
        self.client._player_movement_order.clear()
        self.assertTrue(self.receive())
        self.assertIsNone(self.invitation())
        self.route_packets({"type": "ALL_CLIENT_STATE", "state": [{
            "clientId": 2, "clientState": {
                "currentRoomId": self.client.CONGO_ROOM, "teamId": "blue",
                "online": True, "isSaveLoaded": True,
                "interactionSession": 202, "name": "Handshake identity",
            },
        }]})
        self.assertTrue(self.current())
        self.assertEqual(self.invitation()["name"], "Handshake identity")

    def test_movement_before_membership_defers_instead_of_losing_invitation(self):
        self.client._player_states.clear()
        self.client._player_movement_order.clear()
        movement = {"type": "MNSG_PLAYER_POS", "clientId": 2,
                    "currentRoomId": self.client.CONGO_ROOM, "interactionSession": 202,
                    "playerEpoch": 7, "posSeq": 4, "posT": 100000}
        self.route_packets(movement, self.packet())
        self.assertIsNone(self.invitation())
        self.route_packets({"type": "UPDATE_CLIENT_STATE", "state": {
            "clientId": 2, "teamId": "blue", "online": True,
            "isSaveLoaded": True, "name": "Late metadata",
            "currentRoomId": self.client.CONGO_ROOM,
        }})
        self.assertTrue(self.current())
        self.assertEqual(self.invitation()["name"], "Late metadata")

    def test_event_waits_for_arena_room_metadata_and_survives_deferred_dialog(self):
        self.client._merge_client_state(2, {"currentRoomId": 11})
        self.assertTrue(self.receive())
        self.assertIsNone(self.invitation())
        self.client._merge_client_state(2, {"currentRoomId": self.client.CONGO_ROOM})
        self.assertTrue(self.current())
        self.clock.return_value = 700.0
        self.assertIsNotNone(self.invitation())
        self.assertTrue(self.current())

    def test_unconfirmed_event_expires_without_replaying_when_metadata_arrives(self):
        self.client._merge_client_state(2, {"currentRoomId": 11})
        self.assertTrue(self.receive())
        self.clock.return_value += self.client.ARENA_METADATA_WAIT_MS / 1000 + 0.001
        self.assertIsNone(self.invitation())
        self.client._merge_client_state(2, {"currentRoomId": self.client.CONGO_ROOM})
        self.assertFalse(self.current())
        self.assertFalse(self.receive())

    def test_duplicate_and_out_of_order_events_cannot_reopen_dismissed_dialog(self):
        self.assertTrue(self.receive(seq=3))
        self.assertIsNotNone(self.invitation())
        self.client.dismiss_boss_invitation(2, 202, 3)
        self.assertFalse(self.current(sequence=3))
        self.assertIsNone(self.invitation())
        self.assertFalse(self.receive(seq=3))
        self.assertFalse(self.receive(seq=2))
        self.assertFalse(self.receive(seq=1))
        self.assertTrue(self.receive(seq=4, entered=False))
        self.assertIsNone(self.invitation())
        self.assertTrue(self.receive(seq=5))
        self.assertTrue(self.current(sequence=5))
        self.assertEqual(self.sock.sent, [])

    def test_dismissing_previous_dialog_cannot_consume_new_entry(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        self.assertTrue(self.receive(seq=2, entered=False))
        self.assertFalse(self.current())
        self.assertTrue(self.receive(seq=3))
        self.client.dismiss_boss_invitation(2, 202, 1)
        self.client.dismiss_boss_invitation(2, 999, 3)
        self.assertTrue(self.current(sequence=3))

    def test_sender_exit_packet_cancels_pending_dialog_through_receive_loop(self):
        self.route_packets(self.packet(), self.packet(seq=2, entered=False))
        self.assertIsNone(self.invitation())
        self.assertFalse(self.current())
        self.assertFalse(self.client.has_packet())

    def test_old_session_exit_cannot_cancel_a_reconnected_sender_invitation(self):
        self.assertTrue(self.receive())
        # Handshake metadata can precede the first new movement sample, which
        # normally retires the old session. Its delayed exit cannot cancel a
        # different connection's current invitation in that interval.
        self.client._merge_client_state(2, {"interactionSession": 303})
        self.assertTrue(self.receive(session=303))
        self.assertEqual(self.invitation()["session"], 303)
        self.receive(session=202, seq=2, entered=False)
        self.assertTrue(self.client.boss_invitation_is_current(2, 303, 1))

    def test_sender_leave_disconnect_save_unload_and_team_change_invalidate_dialog(self):
        for change in ({"currentRoomId": 11}, {"online": False},
                       {"isSaveLoaded": False}, {"teamId": "red"}):
            with self.subTest(change=change):
                self.client._reset_boss_invitations()
                self.client._player_states.clear()
                self.client._player_movement_order.clear()
                self.peer()
                self.assertTrue(self.receive())
                self.assertTrue(self.current())
                self.route_packets({"type": "UPDATE_CLIENT_STATE", "state": {
                    "clientId": 2, **change,
                }})
                self.assertFalse(self.current())
                self.assertIsNone(self.invitation())

    def test_roster_removal_retires_sender_session_and_prevents_old_event_replay(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        self.client._replace_all_client_states([])
        self.assertFalse(self.current())
        self.assertFalse(self.receive(seq=2))

    def test_sender_reconnect_invalidates_old_dialog_and_accepts_new_session(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        self.peer(interactionSession=303, playerEpoch=1, posSeq=1, posT=101000)
        self.assertFalse(self.current())
        self.assertFalse(self.receive(seq=2))
        self.assertTrue(self.receive(session=303))
        self.assertTrue(self.current(session=303))

    def test_local_save_unload_clears_invitation_and_requires_new_event_after_reload(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        self.assertTrue(self.client.set_save_loaded(False))
        self.assertFalse(self.current())
        self.assertFalse(self.receive(seq=2))
        self.assertTrue(self.client.set_save_loaded(True))
        self.assertIsNone(self.invitation())
        self.assertFalse(self.receive())
        self.assertTrue(self.receive(seq=2))
        self.assertTrue(self.current(sequence=2))

    def test_local_disconnect_resets_queued_events_and_publication_state(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        self.client.disconnect()
        self.assertEqual(self.client.get_boss_invitation_json(), "")
        self.assertFalse(self.current())
        self.assertEqual(self.client._arena_events, {})
        self.assertEqual(self.client._arena_seen, {})
        self.assertEqual(self.client._arena_sequence, 0)
        self.assertEqual(self.client._arena_local_state, ("", 0, 0))
        self.assertFalse(self.receive())

    def test_local_reconnect_starts_new_session_and_publishes_entry_again(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA, 4))
        self.client.disconnect()
        reconnect_socket = RecordingSocket()
        with (
            mock.patch.object(self.client.socket, "socket", return_value=reconnect_socket),
            mock.patch.object(self.client.threading.Thread, "start"),
            mock.patch.object(self.client.secrets, "randbelow", return_value=303),
        ):
            self.assertTrue(self.client.connect("example.test", 43383, "arena-test",
                                                "One", client_id=1))
        self.assertFalse(self.current())
        self.assertIsNone(self.invitation())
        handshake = json.loads(reconnect_socket.sent[0][:-1])
        self.assertEqual(handshake["clientState"]["interactionSession"], 304)
        self.assertTrue(self.client.set_save_loaded(True))
        self.assertTrue(self.client.set_local_room(self.client.CONGO_ROOM))
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA, 4))
        entry = json.loads(reconnect_socket.sent[-1][:-1])
        self.assertEqual((entry["session"], entry["seq"], entry["entered"]), (304, 1, True))

    def test_client_state_cannot_override_hidden_team(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        self.assertTrue(self.client.update_client_state('{"teamId":"red"}'))
        state_packet = json.loads(self.sock.sent[-1][:-1])
        self.assertEqual(state_packet["state"]["teamId"], "blue")
        self.assertTrue(self.current())
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        self.assertEqual(json.loads(self.sock.sent[-1][:-1])["targetTeamId"], "blue")

    def test_players_already_in_arena_do_not_get_join_invitation(self):
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        self.assertTrue(self.receive())
        self.assertIsNone(self.invitation())
        self.assertTrue(self.client.set_boss_arena(0))
        self.assertIsNone(self.invitation())

    def test_all_client_snapshot_same_room_refresh_does_not_resend_entry(self):
        self.assertTrue(self.client.set_local_room(self.client.CONGO_ROOM))
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        self.sock.sent.clear()
        self.route_packets({"type": "ALL_CLIENT_STATE", "state": [{
            "clientId": 1, "self": True, "clientState": {
                "currentRoomId": self.client.CONGO_ROOM, "teamId": "blue",
                "online": True, "isSaveLoaded": True,
            },
        }]})
        self.assertEqual(self.client._local_room_id, -1)
        self.assertTrue(self.client.set_local_room(self.client.CONGO_ROOM))
        self.assertTrue(self.client.set_boss_arena(self.client.CONGO_ARENA))
        packets = [json.loads(raw[:-1]) for raw in self.sock.sent]
        self.assertEqual([p["type"] for p in packets], ["UPDATE_CLIENT_STATE"])

    def test_snapshot_preserves_dismissal_and_cannot_create_an_invitation(self):
        self.assertTrue(self.receive())
        self.client.dismiss_boss_invitation(2, 202, 1)
        snapshot = [{"clientId": 2, "clientState": {
            "currentRoomId": self.client.CONGO_ROOM, "teamId": "blue",
            "online": True, "isSaveLoaded": True, "name": "Congo guest",
        }}]
        self.client._replace_all_client_states(snapshot)
        self.assertIsNone(self.invitation())
        self.client._reset_boss_invitations()
        self.client._replace_all_client_states(snapshot)
        self.assertIsNone(self.invitation())

    def test_stale_snapshot_session_cannot_replace_newer_movement_identity(self):
        self.peer(interactionSession=303, posSeq=1, posT=101000, playerEpoch=1)
        self.assertTrue(self.receive(session=303))
        self.assertTrue(self.current(session=303))
        self.client._replace_all_client_states([{"clientId": 2, "clientState": {
            "currentRoomId": self.client.CONGO_ROOM, "teamId": "blue",
            "online": True, "isSaveLoaded": True, "interactionSession": 202,
        }}])
        self.assertTrue(self.current(session=303))
        self.assertFalse(self.receive(seq=99, session=202))

    def test_multiple_senders_are_independent_and_name_is_json_safe_and_bounded(self):
        self.peer(3, name='A "name"\n' + "é" * 100, interactionSession=303)
        self.assertTrue(self.receive())
        self.clock.return_value += 0.1
        self.assertTrue(self.receive(clientId=3, session=303))
        self.assertEqual(self.invitation()["cid"], 2)
        self.client.dismiss_boss_invitation(2, 202, 1)
        next_invitation = self.invitation()
        self.assertEqual(next_invitation["cid"], 3)
        self.assertEqual(next_invitation["name"], ('A "name"\n' + "é" * 100)[:64])
        self.assertTrue(self.current(cid=3, session=303))

    def test_every_arena_round_trips_real_entry_and_exit_packets(self):
        for arena, room in self.client.BOSS_ARENA_ROOMS.items():
            with self.subTest(arena=arena, room=room):
                self.client._reset_boss_invitations()
                self.client._merge_client_state(2, {"currentRoomId": room})
                sender = load_client(2, 202, room=room)
                self.addCleanup(sender.disconnect)
                self.assertTrue(sender.set_boss_arena(arena, 8))
                self.assertTrue(sender.set_boss_arena(arena, 8))
                self.assertEqual(len(sender._sock.sent), 1)
                raw = sender._sock.sent[0]
                self.assertEqual(json.loads(raw[:-1]), self.packet(arena=arena))
                self.assertLess(len(raw), 160)
                self.route_bytes(raw[:9], raw[9:])
                self.assertEqual(self.invitation()["arena"], arena)
                self.assertTrue(self.current())

                self.assertTrue(sender.set_boss_arena(0))
                self.assertEqual(json.loads(sender._sock.sent[-1][:-1]),
                                 self.packet(arena=arena, entered=False, seq=2))
                self.route_bytes(sender._sock.sent[-1])
                self.assertIsNone(self.invitation())
                self.assertFalse(self.current())
                self.assertFalse(self.receive(arena=arena, seq=1))
                sender.disconnect()

    def test_python_destination_map_matches_explicit_native_catalog_constants(self):
        header = (MODULE_PATH.parents[1] / "include" / "anchor_boss_arenas.h").read_text()
        constants = {name: int(value.rstrip("uU"), 0) for name, value in re.findall(
            r"^#define\s+(ANCHOR_BOSS_(?:ARENA|ROOM)_[A-Z_]+)\s+(0x[0-9a-fA-F]+[uU]?|[0-9]+)\s*$",
            header, re.MULTILINE)}
        names = {name.removeprefix("ANCHOR_BOSS_ARENA_") for name in constants
                 if name.startswith("ANCHOR_BOSS_ARENA_")}
        self.assertEqual(names, {"CONGO", "DHARUMANYO", "TSURAMI", "CONTROL_MACHINE"})
        native_map = {constants[f"ANCHOR_BOSS_ARENA_{name}"]:
                      constants[f"ANCHOR_BOSS_ROOM_{name}"] for name in names}
        self.assertEqual(self.client.BOSS_ARENA_ROOMS, native_map)
        self.assertEqual(native_map, {1: 22, 2: 73, 3: 113, 4: 341})

    def test_only_matching_destination_suppresses_a_recipient(self):
        for local_arena in (0, *self.client.BOSS_ARENA_ROOMS):
            for arena, room in self.client.BOSS_ARENA_ROOMS.items():
                with self.subTest(local=local_arena, destination=arena):
                    self.client._reset_boss_invitations()
                    self.assertTrue(self.client.set_boss_arena(local_arena))
                    self.client._merge_client_state(2, {"currentRoomId": room})
                    self.assertTrue(self.receive(arena=arena))
                    invitation = self.invitation()
                    if local_arena == arena:
                        self.assertIsNone(invitation)
                        self.assertTrue(self.client.set_boss_arena(0))
                        self.assertIsNone(self.invitation())
                    else:
                        self.assertEqual(invitation["arena"], arena)

    def test_each_arena_requires_its_own_source_room_and_keeps_metadata_grace(self):
        for arena, room in self.client.BOSS_ARENA_ROOMS.items():
            wrong_rooms = [10, *(other for other in self.client.BOSS_ARENA_ROOMS.values()
                                 if other != room)]
            for wrong_room in wrong_rooms:
                with self.subTest(arena=arena, wrong_room=wrong_room):
                    self.client._reset_boss_invitations()
                    self.client._merge_client_state(2, {"currentRoomId": wrong_room})
                    self.assertTrue(self.receive(arena=arena))
                    self.assertIsNone(self.invitation())
                    self.assertFalse(self.current())
                    # A new entry can legitimately precede its room metadata.
                    self.client._merge_client_state(2, {"currentRoomId": room})
                    self.assertEqual(self.invitation()["arena"], arena)
                    self.assertTrue(self.current())

    def test_arena_transition_replaces_old_invitation_before_new_room_metadata(self):
        arenas = list(self.client.BOSS_ARENA_ROOMS.items())
        for old_arena, old_room in arenas:
            for new_arena, new_room in arenas:
                if old_arena == new_arena:
                    continue
                with self.subTest(old=old_arena, new=new_arena):
                    self.client._reset_boss_invitations()
                    self.client._merge_client_state(2, {"currentRoomId": old_room})
                    sender = load_client(2, 202, room=old_room)
                    self.addCleanup(sender.disconnect)
                    self.assertTrue(sender.set_boss_arena(old_arena, 1))
                    self.route_bytes(sender._sock.sent[-1])
                    self.assertTrue(self.current())
                    self.assertTrue(sender.set_boss_arena(new_arena, 2))
                    self.assertEqual(json.loads(sender._sock.sent[-1][:-1]),
                                     self.packet(arena=new_arena, seq=2))
                    self.route_bytes(sender._sock.sent[-1])
                    self.assertFalse(self.current())
                    self.assertIsNone(self.invitation())
                    self.client._merge_client_state(2, {"currentRoomId": new_room})
                    self.assertEqual(self.invitation()["arena"], new_arena)
                    self.assertTrue(self.current(sequence=2))
                    self.client.dismiss_boss_invitation(2, 202, 1)
                    self.assertFalse(self.receive(arena=old_arena, seq=1,
                                                  entered=False))
                    self.assertTrue(self.current(sequence=2))
                    self.assertTrue(sender.set_boss_arena(0))
                    self.assertEqual(json.loads(sender._sock.sent[-1][:-1]),
                                     self.packet(arena=new_arena, seq=3, entered=False))
                    self.route_bytes(sender._sock.sent[-1])
                    self.assertIsNone(self.invitation())
                    sender.disconnect()

    def test_failed_arena_transition_preserves_actual_exit_arena_and_sequence(self):
        arenas = list(self.client.BOSS_ARENA_ROOMS)
        for old_arena in arenas:
            for new_arena in arenas:
                if old_arena == new_arena:
                    continue
                with self.subTest(old=old_arena, new=new_arena):
                    self.client._reset_boss_invitations()
                    self.sock.sent.clear()
                    self.assertTrue(self.client.set_boss_arena(old_arena))
                    with mock.patch.object(self.client, "_send_raw", return_value=False):
                        self.assertFalse(self.client.set_boss_arena(new_arena))
                    self.assertEqual(self.client._arena_local_state[1], old_arena)
                    self.assertTrue(self.client.set_boss_arena(0))
                    packet = json.loads(self.sock.sent[-1][:-1])
                    self.assertEqual((packet["arena"], packet["seq"], packet["entered"]),
                                     (old_arena, 2, False))

    def test_confirmed_visit_cannot_revive_after_inter_arena_metadata_round_trip(self):
        for arena, room in self.client.BOSS_ARENA_ROOMS.items():
            for snapshot in (False, True):
                with self.subTest(arena=arena, snapshot=snapshot):
                    self.client._reset_boss_invitations()
                    self.client._merge_client_state(2, {"currentRoomId": room})
                    self.assertTrue(self.receive(arena=arena))
                    self.assertTrue(self.current())
                    other_room = next(r for r in self.client.BOSS_ARENA_ROOMS.values()
                                      if r != room)
                    for destination in (other_room, room):
                        state = {"clientId": 2, "currentRoomId": destination,
                                 "teamId": "blue", "isSaveLoaded": True,
                                 "online": True, "interactionSession": 202}
                        if snapshot:
                            self.route_packets({"type": "ALL_CLIENT_STATE", "state": [state]})
                        else:
                            self.route_packets({"type": "UPDATE_CLIENT_STATE", "state": state})
                    self.assertFalse(self.current())
                    self.assertIsNone(self.invitation())
                    self.assertFalse(self.receive(arena=arena))
                    self.assertTrue(self.receive(arena=arena, seq=2))
                    self.assertTrue(self.current(sequence=2))

    def test_old_observed_session_entry_cannot_replace_new_arena_invitation(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        next_arena = next(a for a in self.client.BOSS_ARENA_ROOMS
                          if a != self.client.CONGO_ARENA)
        self.assertTrue(self.receive(arena=next_arena, session=303))
        self.assertFalse(self.current())
        # New session and room metadata may both follow the event. Until then
        # the old hot-movement session has not been retired by its subsystem.
        self.assertNotIn(202, self.client._retired_interaction_sessions.get(2, ()))
        self.assertFalse(self.receive(seq=99))
        self.assertFalse(self.receive(seq=100, entered=False))
        self.client._merge_client_state(2, {
            "currentRoomId": self.client.BOSS_ARENA_ROOMS[next_arena],
            "interactionSession": 303,
        })
        self.assertEqual(self.invitation()["arena"], next_arena)
        self.assertTrue(self.current(session=303))
        self.client.disconnect()
        self.assertEqual(self.client._arena_retired_sessions, {})
        self.assertEqual(self.client._arena_confirmed_sessions, {})

    def test_expired_unconfirmed_session_does_not_retire_valid_sender_stream(self):
        self.assertTrue(self.receive())
        self.assertTrue(self.current())
        self.assertTrue(self.receive(arena=2, session=303))
        self.assertFalse(self.receive(seq=2))
        self.assertIsNone(self.invitation())
        self.clock.return_value += self.client.ARENA_METADATA_WAIT_MS / 1000 + 0.001
        self.assertIsNone(self.invitation())
        self.assertNotIn(202, self.client._arena_retired_sessions.get(2, ()))
        self.assertTrue(self.receive(seq=3))
        self.assertTrue(self.current(sequence=3))


if __name__ == "__main__":
    unittest.main()
