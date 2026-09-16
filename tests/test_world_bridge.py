"""Exercise native JSON exchange through the production receive loop and metadata merge."""
import json
import unittest
from unittest import mock
from test_boss_invitation_transport import load_client, RecordingSocket
from test_world_transport import row


class WorldBridgeTests(unittest.TestCase):
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
            self.assertEqual(b._player_states[1]['worldSync'],[1,101,1,0x12e,42])
            b._sock=RecordingSocket()
            answer=json.loads(b.update_world(0x12e,42,1,sample))
            self.assertEqual(answer['a'][0][0],1)
            self.assertEqual(answer['a'][0][2:],row())
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
