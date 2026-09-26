import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))

import anchor_mnsg  # noqa: E402


class RecordingSocket:
    def __init__(self, received=()):
        self.sent = []
        self.received = iter(received)

    def sendall(self, data):
        self.sent.append(data)

    def recv(self, _size):
        return next(self.received, b"")

    def close(self):
        pass


class PlayerCubeTransportTests(unittest.TestCase):
    def setUp(self):
        anchor_mnsg.disconnect()
        self.clock = mock.patch.object(anchor_mnsg.time, "monotonic",
                                       return_value=100.0).start()
        self.addCleanup(mock.patch.stopall)
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._interaction_session = 101
        anchor_mnsg._player_states[1] = self.state(101, 3, 1,
                                                    anchor_mnsg.APPEARANCE_FROZEN)
        anchor_mnsg._player_states[2] = self.state(202, 7, 4,
                                                    anchor_mnsg.APPEARANCE_FROZEN)

    def tearDown(self):
        anchor_mnsg.disconnect()

    @staticmethod
    def state(session, epoch, sequence, appearance):
        return {"online": True, "roomId": 10, "posX": 100, "posY": 200,
                "posZ": 300, "posSeq": sequence, "posT": 100000,
                "interactionSession": session, "playerEpoch": epoch,
                "appearanceFlags": appearance, "collisionDisabled": 0}

    @staticmethod
    def incoming(op=1, sequence=1, carry_id=55, **changes):
        packet = {
            "type": "MNSG_PLAYER_CUBE_CTRL", "clientId": 2,
            "targetClientId": 1, "roomId": 10,
            "sourceSession": 202, "targetSession": 101,
            "sourceEpoch": 7, "targetEpoch": 3,
            "sourcePosSeq": 4, "cubeOp": op, "carryId": carry_id,
            "controlSeq": sequence, "controlT": 100000,
            "x100": 100, "y100": 200, "z100": 300,
            "vx100": 10, "vy100": 20, "vz100": 30,
            "rx": 1, "ry": 2, "rz": 3, "quiet": True,
        }
        packet.update(changes)
        return packet

    def sent_packets(self):
        return [json.loads(wire.removesuffix(b"\x00")) for wire in self.sock.sent]

    def accept_position(self, cid, sequence, appearance, *, session=None,
                        epoch=None, room=10):
        old = anchor_mnsg._player_states[cid]
        return anchor_mnsg._merge_client_state(cid, {
            "currentRoomId": room, "online": True,
            "posX": 100, "posY": 200, "posZ": 300,
            "posSeq": sequence, "posT": 100000 + sequence,
            "interactionSession": (session if session is not None else
                                   old["interactionSession"]),
            "playerEpoch": epoch if epoch is not None else old["playerEpoch"],
            "appearanceFlags": appearance,
        }, enforce_movement_order=True)

    def test_direct_envelope_numeric_tuple_and_roles(self):
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            anchor_mnsg.PLAYER_CUBE_REQUEST, 2, 7, 55, 100, 200, 300,
            10, 20, 30, 1, 2, 3, source_epoch=3))
        packet = self.sent_packets()[-1]
        self.assertEqual(packet["clientId"], 1)
        self.assertEqual(packet["targetClientId"], 2)
        self.assertEqual(packet["roomId"], 10)
        self.assertEqual((packet["sourceSession"], packet["targetSession"]),
                         (101, 202))
        self.assertEqual((packet["sourceEpoch"], packet["targetEpoch"]), (3, 7))
        self.assertEqual(packet["sourcePosSeq"], 1)
        self.assertEqual(packet["controlSeq"], 1)
        self.assertEqual(packet["carryId"], 55)
        self.assertEqual(packet["ry"], 2)
        self.assertTrue(packet["quiet"])
        self.assertNotIn("targetTeamId", packet)
        self.assertNotIn("addToQueue", packet)
        self.assertTrue(self.sock.sent[-1].endswith(b"\x00"))

        for op in (anchor_mnsg.PLAYER_CUBE_GRANT,
                   anchor_mnsg.PLAYER_CUBE_POSE,
                   anchor_mnsg.PLAYER_CUBE_THROW,
                   anchor_mnsg.PLAYER_CUBE_IMPACT,
                   anchor_mnsg.PLAYER_CUBE_CANCEL):
            self.assertTrue(anchor_mnsg.send_player_cube_control(op, 2, 7, 55))
        self.assertEqual([p["cubeOp"] for p in self.sent_packets()],
                         list(range(1, 7)))

        self.assertTrue(anchor_mnsg._receive_player_cube_control(self.incoming()))
        self.assertEqual(anchor_mnsg.poll_player_cube_control(),
                         (1, 2, 1, 10, 202, 101, 7, 3, 55, 1, 4,
                          100, 200, 300, 10, 20, 30, 1, 2, 3))

    def test_carrier_victim_direction_and_carried_phase(self):
        remote = anchor_mnsg._player_states[2]
        local = anchor_mnsg._player_states[1]
        local["appearanceFlags"] = 0  # Carrier sends to frozen victim.
        for op in (1, 3, 4, 5, 6):
            self.assertTrue(anchor_mnsg.send_player_cube_control(op, 2, 7,
                                                                  55 + op))
        self.assertFalse(anchor_mnsg.send_player_cube_control(2, 2, 7, 55))
        local["appearanceFlags"] = anchor_mnsg.APPEARANCE_FROZEN
        remote["appearanceFlags"] = 0
        for op in (1, 3, 4, 5, 6):
            self.assertTrue(anchor_mnsg._receive_player_cube_control(
                self.incoming(op=op, sequence=op, carry_id=100 + op)))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=2, sequence=7)))

        # Frozen owner grants toward an unfrozen carrier.
        local["appearanceFlags"] = anchor_mnsg.APPEARANCE_FROZEN
        remote["appearanceFlags"] = 0
        self.assertTrue(anchor_mnsg.send_player_cube_control(2, 2, 7, 90))
        local["appearanceFlags"] = 0
        remote["appearanceFlags"] = anchor_mnsg.APPEARANCE_FROZEN
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=2, sequence=8)))
        remote["appearanceFlags"] = 0
        self.assertFalse(anchor_mnsg.send_player_cube_control(1, 2, 7, 91))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=1, sequence=9)))

        # Bit 5 marks a carried victim: flight controls remain deliverable.
        local["appearanceFlags"] = 0
        remote["appearanceFlags"] = anchor_mnsg.APPEARANCE_CARRIED
        self.assertFalse(anchor_mnsg.send_player_cube_control(1, 2, 7, 92))
        for op in (3, 4, 5):
            self.assertTrue(anchor_mnsg.send_player_cube_control(op, 2, 7,
                                                                  100 + op))
        local["appearanceFlags"] = anchor_mnsg.APPEARANCE_CARRIED
        remote["appearanceFlags"] = 0
        for op in (3, 4, 5):
            self.assertTrue(anchor_mnsg._receive_player_cube_control(
                self.incoming(op=op, sequence=10 + op, carry_id=200 + op)))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=1, sequence=16)))

        # Cancellation still clears state after the native thaw edge.
        local["appearanceFlags"] = 0
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=6, sequence=17)))

    def test_receive_rejects_malformed_stale_and_wrong_lifetime(self):
        invalid = (
            {"clientId": 1}, {"clientId": 99}, {"clientId": True},
            {"targetClientId": 3}, {"targetClientId": True},
            {"roomId": 11}, {"roomId": True},
            {"sourceSession": 203}, {"targetSession": 102},
            {"sourceEpoch": 8}, {"targetEpoch": 4},
            {"sourcePosSeq": 5}, {"sourcePosSeq": True},
            {"controlSeq": 0}, {"controlSeq": True},
            {"controlT": 99000}, {"controlT": True},
            {"carryId": 0}, {"carryId": True},
            {"cubeOp": 7}, {"cubeOp": True},
            {"quiet": False},
            {"x100": 1000000001}, {"x100": 1.0},
            {"vy100": 1000001}, {"rx": -32769},
            {"targetTeamId": "default"}, {"addToQueue": True},
            {"padding": "x" * 512},
        )
        for changes in invalid:
            with self.subTest(changes=changes):
                self.assertFalse(anchor_mnsg._receive_player_cube_control(
                    self.incoming(**changes)))
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())
        self.assertEqual(self.sock.sent, [])

    def test_sender_rejects_bad_identity_and_numeric_values(self):
        invalid = ((1, 1, 3, 55), (1, 99, 7, 55), (1, 2, 8, 55),
                   (1, 2, 7, 0), (True, 2, 7, 55), (7, 2, 7, 55))
        for args in invalid:
            with self.subTest(args=args):
                self.assertFalse(anchor_mnsg.send_player_cube_control(*args))
        self.assertFalse(anchor_mnsg.send_player_cube_control(1, 2, 7, 55,
                                                       x100=float("nan")))
        self.assertFalse(anchor_mnsg.send_player_cube_control(1, 2, 7, 55,
                                                       source_epoch=4))
        anchor_mnsg._player_states[2]["roomId"] = 11
        self.assertFalse(anchor_mnsg.send_player_cube_control(1, 2, 7, 55))
        self.assertEqual(self.sock.sent, [])

    def test_ordering_wrap_dedup_event_fifo_and_latest_pose(self):
        max_seq = anchor_mnsg._POSITION_SEQUENCE_MASK
        anchor_mnsg._player_cube_seen[2] = (202, 7, 101, 3, max_seq - 1)
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=max_seq)))
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=1, x100=1)))
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=2, x100=2)))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=1)))
        self.assertEqual(len(anchor_mnsg._player_cube_poses), 1)
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 1)
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[11], 2)
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())
        self.assertFalse(anchor_mnsg.has_packet())

        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=3)))
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=4, sequence=4)))
        self.assertFalse(anchor_mnsg._player_cube_poses)
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 4)
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=5)))
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 3)
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=5, sequence=6)))
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 5)
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=7)))
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())

    def test_pose_cadence_edges_size_and_receive_age(self):
        self.assertTrue(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))
        self.clock.return_value = 100.05
        self.assertFalse(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))
        self.assertTrue(anchor_mnsg.send_player_cube_control(2, 2, 7, 55))
        self.assertTrue(anchor_mnsg.send_player_cube_control(3, 2, 7, 56))
        self.clock.return_value = 100.10
        self.assertTrue(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))
        self.assertEqual([p["cubeOp"] for p in self.sent_packets()],
                         [3, 2, 3, 3])
        self.assertTrue(anchor_mnsg.send_player_cube_control(4, 2, 7, 55))
        self.clock.return_value = 100.21
        self.assertTrue(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))
        self.assertTrue(anchor_mnsg.send_player_cube_control(5, 2, 7, 55))
        self.assertFalse(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))

        self.clock.return_value = 100.20
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            3, 2, 7, 57, 1000000000, -1000000000, 1000000000,
            1000000, -1000000, 1000000, -32768, 32767, -32768))
        self.assertLessEqual(len(self.sock.sent[-1]), 512)
        worst = {"type": "MNSG_PLAYER_CUBE_CTRL", "quiet": True}
        for field, (low, high) in anchor_mnsg._PLAYER_CUBE_FIELDS.items():
            worst[field] = low if len(str(low)) > len(str(high)) else high
        worst_wire = (json.dumps(worst, separators=(",", ":")) + "\x00").encode()
        self.assertEqual(len(worst_wire), 472)
        self.assertLessEqual(len(worst_wire), 512)

        self.clock.return_value = 100.0
        self.assertTrue(anchor_mnsg._receive_player_cube_control(self.incoming()))
        self.clock.return_value = 100.501
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())

    def test_failed_send_does_not_advance_sequence_or_pose_baseline(self):
        with mock.patch.object(anchor_mnsg, "_send_raw", return_value=False):
            self.assertFalse(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))
        self.assertEqual(anchor_mnsg._player_cube_seq, 0)
        self.assertFalse(anchor_mnsg._player_cube_pose_sent)
        self.assertTrue(anchor_mnsg.send_player_cube_control(3, 2, 7, 55))
        self.assertEqual(self.sent_packets()[-1]["controlSeq"], 1)

    def test_stale_source_sample_and_reconnect_reset(self):
        anchor_mnsg._player_states[2]["_positionReceivedMs"] = 97999
        self.assertFalse(anchor_mnsg._receive_player_cube_control(self.incoming()))
        anchor_mnsg._player_states[2]["_positionReceivedMs"] = 100000
        self.assertTrue(anchor_mnsg._receive_player_cube_control(self.incoming()))
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 1)
        anchor_mnsg._player_states[2].update(
            interactionSession=303, playerEpoch=8, posSeq=1, posT=100100)
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=2)))
        self.clock.return_value = 100.1
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=1, sourceSession=303, sourceEpoch=8,
                          sourcePosSeq=1, controlT=100100)))
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[4], 303)

    def test_receive_loop_uses_dedicated_control_queue_and_room_reset(self):
        raw = (json.dumps(self.incoming()) + "\x00").encode()
        receiver = RecordingSocket((raw, b""))
        anchor_mnsg._sock = receiver
        with mock.patch.object(anchor_mnsg, "_do_disconnect"):
            anchor_mnsg._recv_loop(receiver)
        self.assertFalse(anchor_mnsg.has_packet())
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 1)
        self.assertEqual(receiver.sent, [])

        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=2)))
        anchor_mnsg._reset_player_cube()
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())
        self.assertFalse(anchor_mnsg._player_cube_seen)

    def test_same_room_metadata_refresh_keeps_active_carry(self):
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3)))
        anchor_mnsg._local_room_id = -1
        # A roster refresh can erase only the broadcast baseline; the local
        # player's known room and in-flight pose are still current.
        anchor_mnsg.set_local_room(10)
        self.assertEqual(anchor_mnsg.poll_player_cube_control()[0], 3)
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=3, sequence=2)))
        # A newer local POS can already report the destination room before
        # the UPDATE_CLIENT_STATE metadata call catches up.
        anchor_mnsg._player_states[1]["roomId"] = 11
        anchor_mnsg.set_local_room(11)
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())

    def test_push_sender_latches_remote_freeze_generation_and_rate(self):
        remote = anchor_mnsg._player_states[2]
        remote["appearanceFlags"] = 0
        self.assertFalse(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, vx100=1200))
        self.assertTrue(self.accept_position(2, 5,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, 100, 200, 300, 1200, 0, -1200))
        packet = self.sent_packets()[-1]
        self.assertEqual(packet["targetFreezeSeq"], 5)
        self.assertEqual((packet["targetClientId"], packet["clientId"]), (2, 1))
        self.assertNotIn("targetTeamId", packet)
        self.assertNotIn("addToQueue", packet)
        self.assertEqual(len(self.sock.sent), 1)
        self.assertLessEqual(len(self.sock.sent[-1]), 512)
        worst = {"type": "MNSG_PLAYER_CUBE_CTRL", "quiet": True,
                 "targetFreezeSeq": anchor_mnsg._POSITION_SEQUENCE_MASK}
        for field, (low, high) in anchor_mnsg._PLAYER_CUBE_FIELDS.items():
            worst[field] = low if len(str(low)) > len(str(high)) else high
        worst_wire = (json.dumps(worst, separators=(",", ":")) + "\x00").encode()
        self.assertEqual(len(worst_wire), 501)
        self.assertLessEqual(len(worst_wire), 512)
        self.clock.return_value = 100.05
        self.assertFalse(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 2, vx100=100))
        self.clock.return_value = 100.10
        self.assertFalse(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, vx100=100))
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 2, vx100=100))
        self.assertEqual([p["cubeOp"] for p in self.sent_packets()], [7, 7])
        self.assertEqual(len(anchor_mnsg._player_cube_poses), 0)
        self.clock.return_value = 100.20
        for velocity in ((1201, 0, 0), (0, 0, -1201),
                         (0, 0, 0), (100, 1, 0)):
            with self.subTest(velocity=velocity):
                self.assertFalse(anchor_mnsg.send_player_cube_control(
                    7, 2, 7, 3, vx100=velocity[0], vy100=velocity[1],
                    vz100=velocity[2]))
        remote["collisionDisabled"] = 1
        self.assertFalse(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 3, vx100=100))
        remote["collisionDisabled"] = 0

    def test_push_local_generation_and_failed_send_retry(self):
        local = anchor_mnsg._player_states[1]
        local["appearanceFlags"] = 0
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=7, targetFreezeSeq=2, vy100=0)))
        self.assertTrue(anchor_mnsg.set_position_anim(
            100, 200, 300, 0, 0, 0, 0, 0, 0,
            appearance_flags=anchor_mnsg.APPEARANCE_FROZEN,
            player_epoch=3))
        self.assertEqual(anchor_mnsg._player_cube_freeze[1], (101, 3, 10, 1))
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(op=7, targetFreezeSeq=1, vy100=0)))
        event = anchor_mnsg.poll_player_cube_control()
        self.assertEqual(len(event), 20)
        self.assertEqual(event[0], 7)
        self.assertEqual(event[8], 55)

        remote = anchor_mnsg._player_states[2]
        remote["appearanceFlags"] = 0
        self.assertTrue(self.accept_position(2, 5,
            anchor_mnsg.APPEARANCE_FROZEN))
        with mock.patch.object(anchor_mnsg, "_send_raw", return_value=False):
            self.assertFalse(anchor_mnsg.send_player_cube_control(
                7, 2, 7, 1, vx100=100))
        self.assertFalse(anchor_mnsg._player_cube_push_sent)
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, vx100=100))

    def test_push_thaw_refreeze_and_snapshot_conflicts(self):
        remote = anchor_mnsg._player_states[2]
        remote["appearanceFlags"] = 0
        self.assertTrue(self.accept_position(2, 5,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertEqual(anchor_mnsg._player_cube_freeze[2][-1], 5)
        snapshot = [
            {"clientId": 1, "self": True, "clientState": {
                "currentRoomId": 10, "interactionSession": 101}},
            {"clientId": 2, "clientState": {
                "currentRoomId": 10, "interactionSession": 202,
                "playerEpoch": 7, "appearanceFlags":
                anchor_mnsg.APPEARANCE_FROZEN}},
        ]
        anchor_mnsg._replace_all_client_states(snapshot)
        self.assertEqual(anchor_mnsg._player_cube_freeze[2][-1], 5)
        snapshot[1]["clientState"]["interactionSession"] = 999
        anchor_mnsg._replace_all_client_states(snapshot)
        self.assertNotIn(2, anchor_mnsg._player_cube_freeze)
        self.assertFalse(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, vx100=100))
        snapshot[1]["clientState"]["interactionSession"] = 202
        snapshot[1]["clientState"]["appearanceFlags"] = 0
        anchor_mnsg._replace_all_client_states(snapshot)
        self.assertNotIn(2, anchor_mnsg._player_cube_freeze)
        self.assertTrue(self.accept_position(2, 6, 0))
        self.assertTrue(self.accept_position(2, 7,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertEqual(anchor_mnsg._player_cube_freeze[2][-1], 7)
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 2, vx100=100))
        self.assertEqual(self.sent_packets()[-1]["targetFreezeSeq"], 7)

    def test_push_snapshot_first_freeze_waits_for_ordered_position(self):
        remote = anchor_mnsg._player_states[2]
        remote["appearanceFlags"] = 0
        snapshot = [
            {"clientId": 1, "self": True, "clientState": {
                "currentRoomId": 10, "interactionSession": 101}},
            {"clientId": 2, "clientState": {
                "currentRoomId": 10, "interactionSession": 202,
                "playerEpoch": 7,
                "appearanceFlags": anchor_mnsg.APPEARANCE_FROZEN}},
        ]
        anchor_mnsg._replace_all_client_states(snapshot)
        self.assertEqual(anchor_mnsg._player_states[2]["posSeq"], 4)
        self.assertNotIn(2, anchor_mnsg._player_cube_freeze)
        self.assertFalse(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, vx100=100))
        self.assertTrue(self.accept_position(2, 5,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertEqual(anchor_mnsg._player_cube_freeze[2][-1], 5)
        self.assertTrue(self.accept_position(2, 6,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertEqual(anchor_mnsg._player_cube_freeze[2][-1], 5)
        self.assertTrue(anchor_mnsg.send_player_cube_control(
            7, 2, 7, 1, vx100=100))
        self.assertEqual(self.sent_packets()[-1]["targetFreezeSeq"], 5)

        # A conflicting roster edge retires this generation. A later frozen
        # POS in the same ordered freeze cannot silently rearm it.
        snapshot[1]["clientState"]["appearanceFlags"] = 0
        anchor_mnsg._replace_all_client_states(snapshot)
        self.assertNotIn(2, anchor_mnsg._player_cube_freeze)
        self.assertTrue(self.accept_position(2, 7,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertNotIn(2, anchor_mnsg._player_cube_freeze)
        self.assertTrue(self.accept_position(2, 8, 0))
        self.assertTrue(self.accept_position(2, 9,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertEqual(anchor_mnsg._player_cube_freeze[2][-1], 9)

    def test_push_receive_rejects_generation_spoof_age_cadence_and_shape(self):
        local = anchor_mnsg._player_states[1]
        local["appearanceFlags"] = 0
        self.assertTrue(self.accept_position(1, 2,
            anchor_mnsg.APPEARANCE_FROZEN))
        valid = dict(op=7, targetFreezeSeq=2, vy100=0,
                     vx100=1200, vz100=-1200)
        for changes in (
                {"targetFreezeSeq": 1}, {"targetFreezeSeq": 0},
                {"targetFreezeSeq": True}, {"targetFreezeSeq": 2.0},
                {"vx100": 1201}, {"vz100": -1201},
                {"vx100": 0, "vz100": 0}, {"vy100": 1},
                {"targetClientId": 3}, {"clientId": 1},
                {"roomId": 11}, {"sourceSession": 303},
                {"targetEpoch": 4}, {"controlT": 99000},
                {"padding": "x" * 512},
                {"targetTeamId": "default"}, {"addToQueue": True},
        ):
            with self.subTest(changes=changes):
                self.assertFalse(anchor_mnsg._receive_player_cube_control(
                    self.incoming(**{**valid, **changes})))
        local["collisionDisabled"] = 1
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(**valid)))
        local["collisionDisabled"] = 0
        anchor_mnsg._player_states[2]["collisionDisabled"] = 1
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(**valid)))
        anchor_mnsg._player_states[2]["collisionDisabled"] = 0
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(**valid)))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=2, **valid)))
        self.clock.return_value = 100.05
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=2, carry_id=56, **valid)))
        self.clock.return_value = 100.10
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=2, carry_id=56, **valid)))
        self.assertEqual([anchor_mnsg.poll_player_cube_control()[8]
                          for _ in range(2)], [55, 56])
        self.assertIsNone(anchor_mnsg.poll_player_cube_control())

        self.assertTrue(self.accept_position(1, 3, 0))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=3, carry_id=57, **valid)))
        self.assertTrue(self.accept_position(1, 4,
            anchor_mnsg.APPEARANCE_FROZEN))
        self.assertFalse(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=3, carry_id=57, **valid)))
        self.assertTrue(anchor_mnsg._receive_player_cube_control(
            self.incoming(sequence=3, carry_id=57,
                          **{**valid, "targetFreezeSeq": 4})))


if __name__ == "__main__":
    unittest.main()
