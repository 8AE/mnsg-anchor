"""Exercise native JSON exchange through the production receive loop and metadata merge."""
import json
import unittest
from unittest import mock
from test_boss_invitation_transport import load_client, RecordingSocket
from test_world_transport import row
import anchor_world as world
from test_world_dynamic import actor


class WorldBridgeTests(unittest.TestCase):
    def test_world_waits_for_current_handshake_membership(self):
        c=load_client(1,101,'blue',0x12e);self.addCleanup(c.disconnect)
        c._world_roster_session=100
        sample=json.dumps({'a':[row()],'d':'00'*32})
        self.assertFalse(json.loads(c.update_world(0x12e,42,1,sample))['a'])
        self.assertIsNone(c._world.scope)
        self.assertFalse(any(json.loads(p[:-1]).get('type')=='MNSG_WORLD' for p in c._sock.sent))
        c._replace_all_client_states([{'clientId':1,'self':True,'clientState':{
            'interactionSession':101,'teamId':'blue','currentRoomId':0x12e,
            'online':True,'isSaveLoaded':True}}])
        c.update_world(0x12e,42,1,sample)
        self.assertEqual(c._world_roster_session,101)
        self.assertIsNotNone(c._world.scope)
        self.assertTrue(any(json.loads(p[:-1]).get('type')=='MNSG_WORLD' for p in c._sock.sent))

    def test_child_snapshots_use_production_framing_and_skip_event_fifo(self):
        a=load_client(1,101,'blue',0x12e);b=load_client(2,202,'blue',0x12e)
        self.addCleanup(a.disconnect);self.addCleanup(b.disconnect)
        with mock.patch('time.monotonic',return_value=100):
            for c in (a,b):
                c.update_world(0x12e,42,1,'{}')
            b.update_world_actors('{}')
            a.update_world_actors(json.dumps({'a':[actor()]}))
            wire=b''.join(a._sock.sent)
            receiver=RecordingSocket([wire[:21],wire[21:]])
            b._sock=receiver
            with mock.patch.object(b,'_do_disconnect'):b._recv_loop(receiver)
            b._sock=RecordingSocket()
            answer=json.loads(b.update_world_actors('{}'))
            self.assertEqual(answer['a'][0][:4],[0x7ffffffd,42,0x12f,1])
            queued=[]
            while not b._recv_queue.empty():queued.append(json.loads(b._recv_queue.get_nowait()))
            self.assertFalse(any(p.get('type')=='MNSG_WORLD_ACTORS' for p in queued))
        b.disconnect();self.assertFalse(b._world_actors.peers)

    def test_framed_packets_metadata_and_off_event_queue(self):
        a=load_client(1,101,'blue',0x12e);b=load_client(2,202,'blue',0x12e)
        self.addCleanup(a.disconnect);self.addCleanup(b.disconnect)
        sample=json.dumps({'a':[row()],'d':'00'*32})
        with mock.patch('time.monotonic',return_value=100):
            a.update_world(0x12e,42,1,sample)
            b.update_world(0x12e,42,1,sample)
            frames=list(a._sock.sent)
            self.assertTrue(any(json.loads(p[:-1]).get('type')=='MNSG_WORLD' for p in frames))
            wire=b''.join(frames)
            receiver=RecordingSocket([wire[:15],wire[15:81],wire[81:]])
            b._sock=receiver
            with mock.patch.object(b,'_do_disconnect'):
                b._recv_loop(receiver)
            self.assertEqual(b._player_states[1]['worldSync'],[world.VERSION,101,1,0x12e,42])
            b._sock=RecordingSocket()
            answer=json.loads(b.update_world(0x12e,42,1,sample))
            self.assertEqual(answer['a'][0][0],1)
            self.assertEqual(answer['a'][0][2:2+world.INSTANCE],row()[:world.INSTANCE])
            self.assertGreater(answer['a'][0][-1],0)
            queued=[]
            while not b._recv_queue.empty():queued.append(json.loads(b._recv_queue.get_nowait()))
            self.assertFalse(any(p.get('type')=='MNSG_WORLD' for p in queued))
        b.disconnect();self.assertFalse(b._world.peers)

    def test_malformed_json_and_room_exit_release_native(self):
        c=load_client(1,101,'blue',0x12e);self.addCleanup(c.disconnect)
        self.assertEqual(json.loads(c.update_world(0x12e,42,1,'[NaN]'))['a'],[])
        self.assertEqual(json.loads(c.update_world(0,0,0,'{}'))['a'],[])
        self.assertIsNone(c._world.scope)

if __name__=='__main__':unittest.main()
