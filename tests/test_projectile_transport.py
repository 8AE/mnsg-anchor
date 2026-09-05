import asyncio
import json
import socket
import sys
import threading
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'py'))
sys.path.insert(0, str(ROOT / 'tools'))
import anchor_mnsg
import anchor_stress


class RecordingSocket:
    def __init__(self, received=()):
        self.sent = []
        self.received = iter(received)

    def sendall(self, data):
        self.sent.append(json.loads(data.removesuffix(b'\x00')))

    def recv(self, _size):
        return next(self.received, b'')

    def close(self):
        pass


class ProjectileTransportTests(unittest.TestCase):
    def setUp(self):
        anchor_mnsg.disconnect()
        self.clock = mock.patch.object(anchor_mnsg.time, 'monotonic', return_value=100.0).start()
        self.addCleanup(mock.patch.stopall)
        self.sock = RecordingSocket()
        anchor_mnsg._sock = self.sock
        anchor_mnsg._connected = True
        anchor_mnsg._client_id = 1
        anchor_mnsg._local_room_id = 10
        anchor_mnsg._interaction_session = 101
        self.assertTrue(self.position())
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(), True))
        self.sock.sent.clear()
        while anchor_mnsg.has_packet():
            anchor_mnsg.poll_packet()

    def tearDown(self):
        anchor_mnsg.disconnect()

    @staticmethod
    def position(epoch=3, appearance=0):
        return anchor_mnsg.set_position_anim(
            10, 20, 30, 5, 100, 1200, 10, 20, 30,
            appearance, 0, 0, 0, 0, 0, 0, 0, 75, 1, 0, 0, 0, epoch,
        )

    @staticmethod
    def movement(**changes):
        result = {'currentRoomId': 10, 'online': True,
                  'posX': 100, 'posY': 200, 'posZ': 300,
                  'posSeq': 4, 'posT': 100000,
                  'playerEpoch': 7, 'interactionSession': 202}
        result.update(changes)
        return result

    @staticmethod
    def entry(**changes):
        result = {'id': 1, 'kind': 2, 'x100': 12345, 'y100': -100, 'z100': 500,
                  'vx100': -100, 'vy100': 200, 'vz100': 300,
                  'rx': -32768, 'ry': 32767, 'rz': 0, 'scale100000': 10000}
        result.update(changes)
        return result

    def packet(self, entry=None, **changes):
        result = {'type': 'MNSG_PROJECTILE_SPAWN', 'clientId': 2,
                  'currentRoomId': 10, 'interactionSession': 202, 'ownerEpoch': 7,
                  'spawn': list((self.entry() if entry is None else entry).values())}
        result.update(changes)
        return result

    def send(self, entry=None, session=101, epoch=3):
        return anchor_mnsg.send_projectile_spawn_json(
            session, epoch, json.dumps(self.entry() if entry is None else entry))

    def rows(self, epoch=3):
        return json.loads(anchor_mnsg.get_projectile_spawns_json(epoch))

    def stats(self):
        return json.loads(anchor_mnsg.get_projectile_spawn_stats_json())

    def test_sender_serializes_compact_array_and_assigned_identity(self):
        self.assertTrue(self.send())
        self.assertEqual(self.sock.sent[-1], self.packet(
            clientId=1, interactionSession=101, ownerEpoch=3, quiet=True))
        self.assertEqual(self.sock.sent[-1]['spawn'], [1, 2, 12345, -100, 500, -100, 200, 300, -32768, 32767, 0, 10000])
        self.assertNotIn('eventId', self.sock.sent[-1])
        self.assertNotIn('projectiles', self.sock.sent[-1])
        self.assertNotIn('addToQueue', self.sock.sent[-1])
        self.assertEqual(self.stats()['sent'], 1)

    def test_source_retry_keeps_same_id_and_success_does_not_echo_twice(self):
        with mock.patch.object(anchor_mnsg, '_send_raw', side_effect=[False, True]) as send:
            self.assertFalse(self.send())
            self.assertTrue(self.send())
            self.assertTrue(self.send())
            self.assertEqual(send.call_count, 2)
            self.assertEqual(send.call_args_list[0], send.call_args_list[1])
        self.assertFalse(self.send(self.entry(x100=999)))
        self.assertEqual(self.stats()['sent'], 1)

    def test_distinct_throws_at_identical_positions_are_never_throttled(self):
        for event_id in (1, 2, 3):
            self.assertTrue(self.send(self.entry(id=event_id)))
        self.assertEqual([p['spawn'][0] for p in self.sock.sent], [1, 2, 3])

    def test_source_session_accessor_and_lagging_epoch_room_gates(self):
        self.assertEqual(anchor_mnsg.get_projectile_session(), 101)
        self.assertFalse(self.send(epoch=4))
        self.assertFalse(self.send(session=102))
        self.assertFalse(self.send(session=True))
        anchor_mnsg.set_local_room(11)
        self.assertFalse(self.send())
        self.assertTrue(self.position(epoch=4))
        self.assertTrue(self.send(epoch=4))
        anchor_mnsg._interaction_session = 303
        anchor_mnsg._player_states[1]['interactionSession'] = 303
        self.assertFalse(self.send(epoch=4))
        self.assertTrue(self.send(epoch=4, session=303))
        anchor_mnsg.disconnect()
        self.assertEqual(anchor_mnsg.get_projectile_session(), 0)
        self.assertFalse(self.send(epoch=4, session=303))
        self.assertFalse(anchor_mnsg.has_packet())

    def test_peek_retries_until_correct_ack_and_retains_other_throws(self):
        for event_id in (1, 2):
            self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=event_id))))
        self.assertEqual(self.rows(), self.rows())
        self.clock.return_value = 100.125
        self.assertEqual(self.rows()[0], {'cid': 2, 'session': 202, 'epoch': 7, 'age': 125, **self.entry()})
        self.assertFalse(anchor_mnsg.ack_projectile_spawn(2, 201, 7, 1))
        self.assertFalse(anchor_mnsg.ack_projectile_spawn(2, 202, 8, 1))
        self.assertTrue(anchor_mnsg.ack_projectile_spawn(2, 202, 7, 1))
        self.assertFalse(anchor_mnsg.ack_projectile_spawn(2, 202, 7, 1))
        self.assertEqual([row['id'] for row in self.rows()], [2])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.assertEqual(self.stats()['acked'], 1)
        self.assertEqual(self.stats()['received'], 2)
        self.assertEqual(self.sock.sent, [])
        self.assertFalse(anchor_mnsg.has_packet())
        self.assertIsNone(anchor_mnsg.poll_player_hit())

    def test_receive_window_preserves_reordered_distinct_throws_and_wrap(self):
        for event_id in (100, 102, 101, 99):
            self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=event_id))))
        self.assertEqual([row['id'] for row in self.rows()], [100, 102, 101, 99])
        for event_id in (102, 101, 99, 38):
            self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=event_id))))
        anchor_mnsg._projectile_seen.clear()
        anchor_mnsg._projectile_spawns.clear()
        for event_id in (0x7fffffff, 1, 0x7ffffffe):
            self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=event_id))))
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=0x7fffffff))))

    def test_expiry_drops_pending_without_forgetting_seen_ids(self):
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.clock.return_value = 100.750
        self.assertEqual(self.rows()[0]['age'], 750)
        self.clock.return_value = 100.751
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg.ack_projectile_spawn(2, 202, 7, 1))
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.assertEqual(self.stats()['expired'], 1)

    def test_bounded_fifo_returns_sixteen_without_consuming_and_tracks_overflow(self):
        for event_id in range(1, 65):
            self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=event_id))))
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=65))))
        batch = self.rows()
        self.assertEqual(len(batch), 16)
        self.assertEqual(self.stats()['pending'], 64)
        self.assertEqual(self.stats()['overflow'], 1)
        for row in batch:
            self.assertTrue(anchor_mnsg.ack_projectile_spawn(row['cid'], row['session'], row['epoch'], row['id']))
        self.assertEqual([row['id'] for row in self.rows()], list(range(17, 33)))
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=65))))

    def test_retry_rotation_prevents_sixteen_blocked_throws_starving_the_next(self):
        for event_id in range(1, 18):
            self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=event_id))))
        self.assertEqual([row['id'] for row in self.rows()], list(range(1, 17)))
        # None of the first batch could allocate; the next throw still gets
        # an immediate opportunity without acknowledging or losing any retry.
        self.assertEqual([row['id'] for row in self.rows()], [17, *range(1, 16)])
        self.assertEqual(self.stats()['pending'], 17)
        self.assertTrue(anchor_mnsg.ack_projectile_spawn(2, 202, 7, 17))
        self.assertEqual(self.stats()['pending'], 16)

    def test_unknown_self_wrong_room_session_epoch_and_offline_senders_rejected(self):
        anchor_mnsg._retire_interaction_session(2, 201)
        for changes in ({'clientId': 3}, {'clientId': 1}, {'currentRoomId': 11},
                        {'interactionSession': 201}, {'ownerEpoch': 6},
                        {'clientId': True}, {'currentRoomId': '10'}):
            with self.subTest(changes=changes):
                self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(**changes)))
        anchor_mnsg._player_states[2]['online'] = False
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet()))
        anchor_mnsg._player_states[2]['online'] = True
        del anchor_mnsg._player_states[2]['posX']
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.assertEqual(self.rows(), [])

    def test_throw_before_new_epoch_movement_waits_then_promotes_once(self):
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(ownerEpoch=8)))
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg.ack_projectile_spawn(2, 202, 8, 1))
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2))))
        self.assertEqual([row['id'] for row in self.rows()], [2])
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(playerEpoch=8, posSeq=5, posT=100010), True))
        self.assertEqual([(row['epoch'], row['id']) for row in self.rows()], [(8, 1)])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(ownerEpoch=8)))
        self.assertTrue(anchor_mnsg.ack_projectile_spawn(2, 202, 8, 1))
        self.assertEqual(self.stats()['deferred'], 1)

    def test_future_connection_and_room_entry_wait_for_exact_hot_identity(self):
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(interactionSession=303, ownerEpoch=1)))
        self.assertEqual(self.rows(), [])
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(interactionSession=303, playerEpoch=1, posSeq=1, posT=1), True))
        self.assertEqual(self.rows()[0]['session'], 303)
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(currentRoomId=11, interactionSession=303, playerEpoch=2, posSeq=2, posT=2), True))
        self.assertEqual(self.rows(), [])
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2), interactionSession=303, ownerEpoch=3)))
        anchor_mnsg._merge_client_state(2, {'currentRoomId': 10})
        self.assertEqual(self.rows(), [])
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(interactionSession=303, playerEpoch=3, posSeq=3, posT=3), True))
        self.assertEqual([(row['epoch'], row['id']) for row in self.rows()], [(3, 2)])

    def test_expired_or_superseded_future_throw_never_becomes_visible(self):
        future = self.packet(ownerEpoch=8)
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(future))
        self.clock.return_value = 100.751
        self.assertEqual(self.rows(), [])
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(playerEpoch=8, posSeq=5, posT=100800), True))
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(future))
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2), ownerEpoch=9)))
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(playerEpoch=10, posSeq=6, posT=100810), True))
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2), ownerEpoch=9)))

    def test_owner_life_and_session_changes_clear_pending_and_reject_retired_sender(self):
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(playerEpoch=8, posSeq=5, posT=100010), True))
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2))))
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(ownerEpoch=8)))
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(interactionSession=303, playerEpoch=1, posSeq=1, posT=1), True))
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(ownerEpoch=8)))
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(interactionSession=303, ownerEpoch=1)))
        self.assertFalse(anchor_mnsg._merge_client_state(2, self.movement(posSeq=100, posT=200000), True))

    def test_local_generation_change_and_disconnect_retire_pending(self):
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.assertEqual(self.rows(epoch=4), [])
        self.assertTrue(self.position(epoch=4))
        self.assertEqual(self.rows(epoch=4), [])
        self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet()))
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2))))
        anchor_mnsg.disconnect()
        self.assertEqual(self.rows(), [])
        self.assertFalse(anchor_mnsg._projectile_seen)
        self.assertFalse(anchor_mnsg._projectile_sent)

    def test_membership_gap_preserves_and_accepts_throws_but_real_room_exit_drops(self):
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet()))
        members = [{'clientId': cid, 'self': cid == 1,
                    'clientState': {'currentRoomId': 10, 'name': str(cid)}} for cid in (1, 2)]
        anchor_mnsg._replace_all_client_states(members)
        anchor_mnsg._local_room_id = -1
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=2))))
        self.assertEqual(len(self.rows()), 2)
        anchor_mnsg.set_local_room(10)
        self.assertEqual(len(self.rows()), 2)
        anchor_mnsg._merge_client_state(2, {'currentRoomId': 11})
        self.assertEqual(self.rows(), [])
        anchor_mnsg._merge_client_state(2, {'currentRoomId': 10})
        self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(self.entry(id=3))))
        self.assertEqual(self.rows(), [])
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(posSeq=5, posT=100010), True))
        self.assertEqual([row['id'] for row in self.rows()], [3])
        anchor_mnsg.set_local_room(11)
        self.assertEqual(self.rows(), [])

    def test_strict_twelve_field_schema_rejects_bad_shapes_types_and_bounds(self):
        invalid = [None, [], self.entry(extra=0), {k: v for k, v in self.entry().items() if k != 'scale100000'}]
        for key, (low, high) in anchor_mnsg._PROJECTILE_SPAWN_LIMITS.items():
            for bad in (low - 1, high + 1, True, '1', 1.0, None):
                invalid.append(self.entry(**{key: bad}))
        for entry in invalid:
            with self.subTest(entry=entry):
                self.assertFalse(anchor_mnsg.send_projectile_spawn_json(101, 3, json.dumps(entry)))
                if isinstance(entry, dict):
                    self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(entry)))
        for spawn in ([], list(self.entry().values()) + [0], self.entry(), 'invalid'):
            self.assertFalse(anchor_mnsg._receive_projectile_spawn(self.packet(spawn=spawn)))
        self.assertFalse(anchor_mnsg.send_projectile_spawn_json(101, 3, '['))
        self.assertFalse(anchor_mnsg.send_projectile_spawn_json(101, 3, ' ' * 513))
        self.assertEqual(self.sock.sent, [])

    def test_boundary_values_and_compact_readable_getter(self):
        for index in (0, 1):
            entry = {key: limits[index] for key, limits in anchor_mnsg._PROJECTILE_SPAWN_LIMITS.items()}
            self.assertTrue(self.send(entry))
            self.assertTrue(anchor_mnsg._receive_projectile_spawn(self.packet(entry)))
        self.assertNotIn(' ', anchor_mnsg.get_projectile_spawns_json(3))
        self.assertEqual(len(self.rows()), 2)

    def test_actual_loopback_tcp_dispatch_and_legacy_snapshot_drop(self):
        # Exercise null framing and the live receive thread through actual TCP.
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            try:
                listener.bind(('127.0.0.1', 0))
            except PermissionError:
                self.skipTest('This sandbox does not allow loopback TCP listeners')
            listener.listen(1)
            client = socket.create_connection(listener.getsockname(), timeout=1)
            server, _address = listener.accept()
        self.addCleanup(client.close)
        self.addCleanup(server.close)
        anchor_mnsg._sock = client
        packets = [{'type': 'MNSG_PROJECTILES', 'projectiles': [self.entry()]},
                   self.packet(), self.packet(self.entry(id=2)), self.packet()]
        handled = threading.Event()
        receive = anchor_mnsg._receive_projectile_spawn
        count = 0
        def observe(packet):
            nonlocal count
            result = receive(packet)
            count += 1
            if count == 3:
                handled.set()
            return result
        with mock.patch.object(anchor_mnsg, '_receive_projectile_spawn', side_effect=observe), mock.patch.object(anchor_mnsg, '_do_disconnect'):
            worker = threading.Thread(target=anchor_mnsg._recv_loop, args=(client,), daemon=True)
            worker.start()
            data = b''.join((json.dumps(p) + '\x00').encode() for p in packets)
            server.sendall(data[:13])
            server.sendall(data[13:])
            try:
                self.assertTrue(handled.wait(1), 'TCP events were not dispatched')
                self.assertEqual([row['id'] for row in self.rows()], [1, 2])
                self.assertFalse(anchor_mnsg.has_packet())
                self.assertIsNone(anchor_mnsg.poll_player_hit())
            finally:
                server.shutdown(socket.SHUT_WR)
                worker.join(1)
                self.assertFalse(worker.is_alive())

    def test_recovery_bitmap_preserved_without_per_blink_packets(self):
        self.assertTrue(self.position(appearance=7))
        self.assertEqual(self.sock.sent[-1]['appearanceFlags'], 7)
        self.assertFalse(self.position(appearance=7))
        self.assertTrue(self.position(appearance=3))
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(appearanceFlags=7, posSeq=5, posT=100010), True))
        self.assertEqual(json.loads(anchor_mnsg.get_lobby_positions_json())[0]['ap'], 7)
        self.assertTrue(anchor_mnsg._merge_client_state(2, self.movement(posSeq=6, posT=100020), True))
        self.assertEqual(anchor_mnsg._player_states[2]['appearanceFlags'], 3)

    def test_stress_throw_and_recovery_controls_use_current_wire(self):
        async def exercise():
            config = anchor_stress.BotConfig('host', 1, 'test', 'team', 'bot', 4, 5, 6, 10, 'Sasuke', 15)
            controller = anchor_stress.StressController(config, 1)
            bot = controller.bots[0]
            bot.connected, bot.client_id, bot.interaction_session = True, 2, 202
            bot.player_epoch = 7
            bot._send = mock.AsyncMock()
            bot.appearance_flags = 3
            with mock.patch('builtins.print'):
                await controller.handle_command('recovery all on')
                self.assertEqual(bot._send.call_args.args[0]['appearanceFlags'], 7)
                await controller.handle_command('recovery all off')
                self.assertEqual(bot._send.call_args.args[0]['appearanceFlags'], 3)
                await controller.handle_command('throw all 2 100 200 -300')
                packet = bot._send.call_args.args[0]
                self.assertEqual(packet['spawn'], [1, 2, 400, 500, 600, 100, 200, -300, 0, 0, 0, 10000])
                self.assertTrue(anchor_mnsg._receive_projectile_spawn(packet))
                await bot._handle_packet(packet)
                self.assertEqual((bot.projectile_sent, bot.projectile_received), (1, 1))
                await controller.handle_command('status')
        asyncio.run(exercise())


if __name__ == '__main__':
    unittest.main()
