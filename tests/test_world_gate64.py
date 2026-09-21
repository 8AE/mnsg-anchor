"""Room 0x14B gate64: kind 10 schema, phase authority, receipt, room scope.

The transport owns presence, the room scope, the completion cross-check,
bootstrap/receipt and phase-only authority. The native adapter owns the
phase-specific model, resource, collision and camera binding, and it keeps the
local busy flag purely local.
"""
import copy
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'py'))
import anchor_world as w


def gate_row(index=0,phase=1,timer=0,busy=0,slot=1,paused=0,complete=None,
             body=(0,0,0),child=(0,0,0)):
    r=[0]*w.WORDS
    r[0]=index;r[1]=w.GATE64_ENTITY;r[2]=w.GATE64
    r[3]=busy
    r[4:7]=list(body)
    r[10]=slot
    r[w.WG64_TIMER]=timer
    r[w.WG64_PHASE]=phase
    r[17:20]=list(child)
    r[23]=150
    r[w.WG64_COMPLETE]=int(phase==10) if complete is None else complete
    r[38]=paused
    return r


class Gate64TransportTests(unittest.TestCase):
    def setUp(self):
        self.a=w.WorldTransport();self.b=w.WorldTransport()
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,
                     room=w.GATE64_ROOM,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        self.tick(self.a,self.ca,0)
        self.tick(self.b,self.cb,0)
        self.advertise()

    def tick(self,t,c,now,rows=None,dead='00'*32,visit=1,signature=42):
        return t.update(c,c['room'],signature,visit,
                        rows if rows is not None else [gate_row()],dead,now)

    def advertise(self):
        self.ca['players'][2]=dict(online=True,isSaveLoaded=True,teamId=self.ca['team'],
            roomId=self.ca['room'],interactionSession=20,
            worldSync=self.b.advertisement(self.cb))
        self.cb['players'][1]=dict(online=True,isSaveLoaded=True,teamId=self.cb['team'],
            roomId=self.cb['room'],interactionSession=10,
            worldSync=self.a.advertisement(self.ca))

    def send(self,t,c,receiver,rc,now,rows=None,visit=1):
        _,packets=self.tick(t,c,now,rows,visit=visit)
        self.assertTrue(packets)
        for packet in packets:self.assertTrue(receiver.receive(rc,packet,now+.01))
        t.sent(packets,True,now)
        return packets

    def entry(self,transport,context,now,row=None):
        result,_=self.tick(transport,context,now,
                           [row if row is not None else gate_row()])
        self.assertEqual(len(result['a']),1)
        return result['a'][0]

    def test_gate64_room_scope_and_packet_envelope(self):
        row=gate_row(phase=4,body=(1200,-340,50),child=(0,900,0))
        packets=self.send(self.a,self.ca,self.b,self.cb,1,[row])
        wire=packets[0]['a'][0]
        self.assertEqual(wire[1],w.GATE64_ENTITY)
        self.assertEqual(wire[2],w.GATE64)
        self.assertEqual(wire[w.WG64_PHASE],4)
        self.assertEqual(wire[w.WG64_COMPLETE],0)
        self.assertEqual(wire[w.INSTANCE],0)
        self.assertEqual(wire[w.RECEIPT],0)
        # Gate64 has no input channel, so the envelope stays empty.
        self.assertEqual(packets[0]['u'],[])
        self.assertLessEqual(len(json.dumps(packets[0],separators=(',',':')).encode())+1,
                             w.PACKET_BYTES)
        self.assertLessEqual(len(packets),w.MAX_PARTS)
        # A non-empty input list is only legal in the crane and bridge rooms.
        bad=copy.deepcopy(packets[0]);bad['u']=[[0,1]]
        self.assertFalse(self.b.receive(self.cb,bad,1.01))

    def test_gate64_is_rejected_outside_its_room(self):
        self.ca['room']=self.cb['room']=0x12e
        self.a.reset();self.b.reset()
        self.tick(self.a,self.ca,0,[]);self.tick(self.b,self.cb,0,[]);self.advertise()
        result,packets=self.tick(self.a,self.ca,1,[gate_row(phase=3)])
        self.assertFalse(result['a']);self.assertFalse(packets)
        # A kind 10 row smuggled into another room never becomes gate state.
        def plain(index=0):
            r=[0]*w.WORDS
            r[0]=index;r[1]=0x3e0;r[2]=2;r[18]=1;r[26:29]=[1000]*3
            return r
        packets=self.send(self.a,self.ca,self.b,self.cb,1.1,[plain()])
        smuggled=copy.deepcopy(packets[0]);smuggled['a'][0]=list(gate_row(phase=3))
        self.assertFalse(self.b.receive(self.cb,smuggled,1.11))
        fresh=self.tick(self.a,self.ca,1.2,[plain(index=1)])[1]
        self.assertTrue(self.b.receive(self.cb,fresh[0],1.21))

    def test_gate64_phase_only_authority_ignores_timer(self):
        self.assertLess(w.controller_progress(gate_row(phase=1)),
                        w.controller_progress(gate_row(phase=3)))
        self.assertEqual(w.controller_progress(gate_row(phase=1)),
                         w.controller_progress(gate_row(phase=2)))
        # The native timer never feeds authority.
        self.assertEqual(w.controller_progress(gate_row(phase=6,timer=-1)),
                         w.controller_progress(gate_row(phase=6,timer=32767)))
        # A completed gate outranks a running one even from the higher id.
        self.send(self.a,self.ca,self.b,self.cb,1,[gate_row(phase=5)])
        self.send(self.b,self.cb,self.a,self.ca,1.02,[gate_row(phase=10)])
        result,_=self.tick(self.a,self.ca,1.04,[gate_row(phase=5)])
        self.assertEqual(self.a.owners[0],2)
        offer=result['a'][0]
        self.assertEqual(offer[0],2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WG64_PHASE],10)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WG64_COMPLETE],1)
        # Equal phases fall back to the lower client id.
        result,_=self.tick(self.b,self.cb,1.06,[gate_row(phase=5)])
        self.assertEqual(result['a'][0][0],1)

    def test_gate64_late_entry_bootstraps_then_acknowledges(self):
        live=gate_row(phase=7,timer=12,busy=1)
        self.b.reset()
        self.tick(self.b,self.cb,.5,[]);self.advertise()
        self.send(self.a,self.ca,self.b,self.cb,1,[live])
        fresh=gate_row(phase=7,timer=12,busy=0)
        fresh[w.INSTANCE]=77
        result,packets=self.tick(self.b,self.cb,1.02,[fresh])
        # Nothing is published before the native application acknowledges it.
        self.assertNotIn(0,self.b.established)
        self.assertTrue(all(not p['a'] for p in packets))
        offer=result['a'][0]
        self.assertTrue(offer[w.DELIVERY_ROW+w.RECEIPT] & w.BOOTSTRAP)
        self.assertEqual(offer[w.DELIVERY_ROW+w.INSTANCE],77)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WG64_PHASE],7)
        applied=offer[w.DELIVERY_ROW:]
        self.tick(self.b,self.cb,1.04,[applied])
        self.assertIn(0,self.b.established)

    def test_gate64_pause_hands_off_to_the_unpaused_peer(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[gate_row(phase=6)])
        offer=self.entry(self.b,self.cb,1.02,gate_row(phase=6))
        self.assertEqual(offer[0],1)
        paused=gate_row(phase=6,paused=1)
        packets=self.send(self.a,self.ca,self.b,self.cb,1.1,[paused])
        self.assertEqual(packets[0]['z'],'01'+'00'*31)
        result,packets=self.tick(self.b,self.cb,1.12,[gate_row(phase=6)])
        self.assertEqual(result['a'][0][0],2)
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+w.RECEIPT],0)
        self.assertEqual(self.b.owners[0],2)
        self.assertEqual(packets[0]['z'],'00'*32)

    def test_gate64_handoff_after_owner_leaves(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[gate_row(phase=4)])
        offer=self.entry(self.b,self.cb,1.02,gate_row(phase=4))
        self.assertEqual(offer[0],1)
        self.cb['players'][1]['online']=False
        # Self delivery confirms ownership without claiming a bootstrap.
        result,packets=self.tick(self.b,self.cb,1.1,[gate_row(phase=4)])
        self.assertEqual(result['a'][0][0],2)
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+w.RECEIPT],0)
        self.assertTrue(packets)
        self.assertEqual(packets[0]['a'][0][w.WG64_PHASE],4)
        self.assertEqual(self.b.owners[0],2)

    def test_gate64_active_camera_retains_ownership_while_paused(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[gate_row(phase=6,busy=1,paused=1)])
        result,_=self.tick(self.b,self.cb,1.1,[gate_row(phase=6)])
        self.assertEqual(self.b.owners[0],1)
        self.assertEqual(result['a'][0][0],1)
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+38],1)

    def test_gate64_hit_winner_gets_native_confirmation(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[gate_row()])
        hit=gate_row(phase=3,timer=60);hit[w.INSTANCE]=9
        result,_=self.tick(self.b,self.cb,1.02,[hit])
        self.assertEqual(self.b.owners[0],2)
        self.assertEqual(result['a'][0][:2],[2,0])
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+w.INSTANCE],9)
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+w.RECEIPT],0)
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+w.WG64_PHASE],3)

    def test_gate64_phase_edge_bypasses_to_the_50ms_floor(self):
        t=w.WorldTransport()
        row=gate_row(phase=3,timer=59)
        _,packets=self.tick(t,self.ca,1,[row]);t.sent(packets,True,1)
        # A steady gate keeps the idle heartbeat.
        _,steady=self.tick(t,self.ca,1.5,[row]);self.assertFalse(steady)
        _,steady=self.tick(t,self.ca,2.1,[row]);self.assertTrue(steady)
        t.sent(steady,True,2.1)
        # A phase boundary is an edge and bypasses to 50 ms.
        moved=gate_row(phase=4,timer=0)
        _,blocked=self.tick(t,self.ca,2.13,[moved]);self.assertFalse(blocked)
        _,edge=self.tick(t,self.ca,2.16,[moved]);self.assertTrue(edge)
        self.assertEqual(edge[0]['a'][0][w.WG64_PHASE],4)
        t.sent(edge,True,2.16)
        # The local busy flag alone is still an edge.
        busy=copy.deepcopy(moved);busy[3]=1
        _,blocked=self.tick(t,self.ca,2.18,[busy]);self.assertFalse(blocked)
        _,edge=self.tick(t,self.ca,2.23,[busy]);self.assertTrue(edge)
        self.assertEqual(edge[0]['b'],'01'+'00'*31)

    def test_gate64_send_failure_retries_on_a_new_sequence(self):
        row=gate_row(phase=5)
        _,packets=self.tick(self.a,self.ca,1,[row])
        self.assertTrue(packets)
        self.a.sent(packets,False,1)
        q=packets[0]['q']
        _,blocked=self.tick(self.a,self.ca,1.01,[row]);self.assertFalse(blocked)
        _,retry=self.tick(self.a,self.ca,1.06,[row])
        self.assertEqual(retry[0]['q'],q+1)

    def test_gate64_schema_rejects_forbidden_values(self):
        base=gate_row(phase=3)
        self.assertTrue(w.row_valid(base))
        forbidden={1:0x324,2:9,3:2,4:3276701,5:-3276801,7:1024,10:0,
                   11:-2,12:0,13:2,14:1,17:3276701,20:1024,23:149,24:2,
                   26:1,37:1,38:2,39:1,47:1}
        for column,value in forbidden.items():
            altered=list(base);altered[column]=value
            self.assertFalse(w.row_valid(altered),(column,value))
        # The completion flag mirrors the final phase exactly.
        self.assertTrue(w.row_valid(gate_row(phase=10,complete=1)))
        self.assertFalse(w.row_valid(gate_row(phase=10,complete=0)))
        self.assertFalse(w.row_valid(gate_row(phase=9,complete=1)))
        self.assertFalse(w.row_valid(gate_row(phase=1,complete=1)))
        self.assertTrue(w.row_valid(gate_row(phase=1,timer=-1,slot=2)))
        self.assertTrue(w.row_valid(gate_row(phase=10,timer=32767,busy=1,paused=1)))


class Gate64CodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        lib=Path(cls.temp.name)/'gate64.dylib'
        subprocess.run(['cc','-shared','-fPIC','-std=c99','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I'+str(ROOT/'include'),
                        str(ROOT/'src/utils/anchor_world_codec.c'),
                        str(ROOT/'src/utils/string_utils.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.Row=ctypes.c_int*w.WORDS
        cls.lib.anchor_world_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def test_gate64_validation_parity(self):
        examples=[gate_row(),
                  gate_row(phase=10,busy=1,paused=1,slot=2,timer=32767),
                  gate_row(phase=3,timer=-1,body=(-3276800,3276700,0),
                           child=(0,0,3276700))]
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for column in range(w.WORDS):
                for value in (-3276801,-32769,-32768,-2,-1,0,1,2,3,9,10,11,12,13,23,
                              24,25,26,37,38,39,47,149,150,151,255,256,1023,1024,
                              32767,32768,3276701,2147483647):
                    altered=list(r);altered[column]=value
                    self.assertEqual(
                        bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                        w.row_valid(altered),(column,value))

    def test_gate64_encoder_rejects_forbidden_rows(self):
        rows=((ctypes.c_int*w.WORDS)*2)()
        out=ctypes.create_string_buffer(w.STATE_BYTES)
        good=gate_row()
        rows[0][:]=good
        self.assertTrue(self.lib.anchor_world_encode(rows,1,(ctypes.c_ubyte*32)(),out,len(out)))
        for mutate in (lambda r:r.__setitem__(w.WG64_COMPLETE,1),
                       lambda r:r.__setitem__(1,0x324),
                       lambda r:r.__setitem__(23,149)):
            bad=gate_row(phase=5);mutate(bad)
            self.assertFalse(self.lib.anchor_world_row_valid(self.Row(*bad)))
            rows[0][:]=bad
            self.assertFalse(self.lib.anchor_world_encode(
                rows,1,(ctypes.c_ubyte*32)(),out,len(out)))


if __name__=='__main__':unittest.main()
