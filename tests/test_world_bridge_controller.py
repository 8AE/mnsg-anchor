"""File51 Super Pass bridge: kind 9 schema, latch propagation, authority, ack.

Room 0x15E root entity 0x240. The transport owns presence, the temp-latch
aggregate, bootstrap/ack, handoff and the schema; the native adapter owns the
pointers, dialogue, route scripts and the blocker's actual deletion.
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


def bridge_row(index=0,flags=0,fresh=0,gate=1,frame=0,animation=0,blocker=0,
               guard0=1,guard1=1,pc0=40,pc1=39,paused=0,temp=0):
    r=[0]*w.WORDS
    r[0]=index;r[1]=w.BRIDGE_ENTITY;r[2]=w.BRIDGE
    r[w.WB_FLAGS]=flags;r[w.WB_FRESH]=fresh;r[w.WB_GATE_PHASE]=gate
    r[w.WB_GATE_FRAME]=frame;r[w.WB_GATE_ANIMATION]=animation
    r[w.WB_BLOCKER_REMOVED]=blocker
    r[w.WB_GUARD_0]=guard0;r[w.WB_GUARD_1]=guard1
    r[w.WB_GUARD_0+13]=pc0;r[w.WB_GUARD_1+13]=pc1
    r[w.WB_PAUSED]=paused
    r[w.WB_INPUT]=temp
    return r


class BridgeTransportTests(unittest.TestCase):
    def setUp(self):
        self.a=w.WorldTransport();self.b=w.WorldTransport()
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,
                     room=w.BRIDGE_ROOM,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        self.tick(self.a,self.ca,0)
        self.tick(self.b,self.cb,0)
        self.advertise()

    def tick(self,t,c,now,rows=None,dead='00'*32,visit=1,signature=42):
        return t.update(c,c['room'],signature,visit,
                        rows if rows is not None else [bridge_row()],dead,now)

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

    def entry(self,transport,context,now,row):
        result,_=self.tick(transport,context,now,[row])
        self.assertEqual(len(result['a']),1)
        return result['a'][0]

    def test_bridge_latch_aggregates_and_echoes_without_wire_leak(self):
        a_row=bridge_row(temp=1);b_row=bridge_row(temp=2)
        packets=self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        wire=packets[0]['a'][0]
        self.assertEqual(wire[w.WB_INPUT],0)
        self.assertEqual(wire[w.WB_AGGREGATE],0)
        self.assertEqual(packets[0]['u'],[[0,1]])
        offer=self.entry(self.b,self.cb,1.02,b_row)
        self.assertEqual(offer[0],1)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WB_INPUT],2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WB_AGGREGATE],3)
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        echo=self.entry(self.a,self.ca,1.06,a_row)
        self.assertEqual(echo[0],1)
        self.assertEqual(echo[w.DELIVERY_ROW+w.WB_AGGREGATE],3)
        self.assertEqual(echo[w.DELIVERY_ROW+w.RECEIPT],0)
        self.assertEqual(echo[w.DELIVERY_ROW+w.WB_INPUT],1)

    def test_bridge_paused_peer_still_contributes_its_latch(self):
        a_row=bridge_row(temp=1);paused=bridge_row(temp=2,paused=1)
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        packets=self.send(self.b,self.cb,self.a,self.ca,1.1,[paused])
        self.assertEqual(packets[0]['u'],[[0,2]])
        self.assertEqual(packets[0]['z'],'01'+'00'*31)
        echo=self.entry(self.a,self.ca,1.12,a_row)
        self.assertEqual(echo[w.DELIVERY_ROW+w.WB_AGGREGATE],3)

    def test_bridge_late_entry_bootstraps_with_current_aggregate(self):
        a_row,b_row=bridge_row(temp=1),bridge_row(temp=2)
        self.b.reset()
        self.tick(self.b,self.cb,.5,[]);self.advertise()
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        fresh=bridge_row(temp=2);fresh[w.INSTANCE]=77
        result,packets=self.tick(self.b,self.cb,1.02,[fresh])
        self.assertNotIn(0,self.b.established)
        self.assertTrue(all(not p['a'] for p in packets))
        offer=result['a'][0]
        self.assertTrue(offer[w.DELIVERY_ROW+w.RECEIPT] & w.BOOTSTRAP)
        self.assertEqual(offer[w.DELIVERY_ROW+w.INSTANCE],77)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WB_INPUT],2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.WB_AGGREGATE],3)
        applied=offer[w.DELIVERY_ROW:]
        self.tick(self.b,self.cb,1.04,[applied])
        self.assertIn(0,self.b.established)

    def test_bridge_established_route_outranks_fresh_save1_constructor(self):
        live=bridge_row(flags=0,guard0=2,guard1=2)
        fresh=bridge_row(flags=3,fresh=1,gate=3,guard0=3,guard1=3)
        self.assertGreater(w.controller_progress(fresh),w.controller_progress(live))
        self.b.reset()
        self.tick(self.b,self.cb,.5,[]);self.advertise()
        packets=self.send(self.a,self.ca,self.b,self.cb,1,[live])
        # The live route is established: its owner advertises the receipt claim
        # while the fresh client has not applied any receipt yet.
        self.assertTrue(w.bit(bytes.fromhex(packets[0]['h']),0))
        self.assertIn(0,self.a.established)
        self.assertNotIn(0,self.b.established)
        result,_=self.tick(self.b,self.cb,1.02,[fresh])
        self.assertEqual(self.b.owners[0],1)
        self.assertEqual(result['a'][0][0],1)

    def test_bridge_handoff_after_owner_leaves(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[bridge_row(temp=1)])
        offer=self.entry(self.b,self.cb,1.02,bridge_row(temp=2))
        self.assertEqual(offer[0],1)
        self.cb['players'][1]['online']=False
        echo=self.entry(self.b,self.cb,1.1,bridge_row(temp=2))
        self.assertEqual(echo[0],2)
        self.assertEqual(self.b.owners[0],2)

    def test_bridge_send_failure_retries_on_the_edge_floor(self):
        a_row=bridge_row(temp=3)
        _,packets=self.tick(self.a,self.ca,1,[a_row])
        self.assertTrue(packets)
        self.a.sent(packets,False,1)
        q=packets[0]['q']
        _,blocked=self.tick(self.a,self.ca,1.01,[a_row])
        self.assertFalse(blocked)
        _,retry=self.tick(self.a,self.ca,1.06,[a_row])
        self.assertEqual(retry[0]['q'],q+1)
        self.assertEqual(retry[0]['u'],[[0,3]])

    def test_bridge_flags_guard_and_blocker_edges_use_the_existing_floor(self):
        t=w.WorldTransport()
        row=bridge_row()
        _,packets=self.tick(t,self.ca,1,[row]);t.sent(packets,True,1)
        # Idle heartbeat, then an edge inside the 50 ms floor.
        _,steady=self.tick(t,self.ca,2.1,[row]);self.assertTrue(steady)
        t.sent(steady,True,2.1)
        moved=bridge_row(flags=1,gate=2,guard0=2)
        _,blocked=self.tick(t,self.ca,2.13,[moved]);self.assertFalse(blocked)
        _,edge=self.tick(t,self.ca,2.16,[moved]);self.assertTrue(edge)
        t.sent(edge,True,2.16)
        removed=copy.deepcopy(moved);removed[w.WB_BLOCKER_REMOVED]=1
        removed[w.WB_GUARD_1+13]=41
        _,blocked=self.tick(t,self.ca,2.18,[removed]);self.assertFalse(blocked)
        _,edge=self.tick(t,self.ca,2.23,[removed]);self.assertTrue(edge)

    def test_bridge_wire_and_schema_rejections(self):
        a_row=bridge_row(temp=1)
        packets=self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        base=packets[0]
        for column in (w.WB_INPUT,w.WB_AGGREGATE):
            bad=copy.deepcopy(base);bad['a'][0][column]=1
            self.assertFalse(self.b.receive(self.cb,bad,1.01),column)
        for value in (None,[[0,4]],[[0,1],[0,1]],[[256,1]],'m',[[0,True]]):
            bad=copy.deepcopy(base);bad['u']=value
            self.assertFalse(self.b.receive(self.cb,bad,1.01),value)
        missing=copy.deepcopy(base);missing.pop('u')
        self.assertFalse(self.b.receive(self.cb,missing,1.01))
        outsider=copy.deepcopy(base);outsider['targetTeamId']='other'
        self.assertFalse(self.b.receive(self.cb,outsider,1.01))
        # Every rejection above is a validation failure, not a stale sequence:
        # the same owner is still accepted on the next sequence number.
        later=self.send(self.a,self.ca,self.b,self.cb,1.2,[bridge_row(temp=3)])
        self.assertEqual(later[0]['q'],base['q']+1)

    def test_bridge_row_schema_rejects_forbidden_values(self):
        base=bridge_row()
        self.assertTrue(w.row_valid(base))
        forbidden={1:0x241,2:7,3:2,4:4,5:2,6:0,7:1000001,8:0x100000,9:2,
                   10:0,14:262144,15:3,17:0x80000,22:8,23:256,
                   24:0,28:262144,29:3,31:0x80000,36:8,37:256,
                   38:2,39:100001,44:-100001,45:1,46:4,47:4}
        for column,value in forbidden.items():
            altered=list(base);altered[column]=value
            self.assertFalse(w.row_valid(altered),(column,value))
        zero=list(base);zero[w.WB_GUARD_0]=4
        self.assertFalse(w.row_valid(zero))
        self.assertTrue(w.row_valid(bridge_row(guard0=11,guard1=15)))

    def test_bridge_is_scoped_to_its_room(self):
        def plain(index=0):
            r=[0]*w.WORDS
            r[0]=index;r[1]=0x3e0;r[2]=2;r[18]=1;r[26:29]=[1000]*3
            return r
        self.ca['room']=self.cb['room']=0x12e
        self.a.reset();self.b.reset()
        self.tick(self.a,self.ca,0,[]);self.tick(self.b,self.cb,0,[]);self.advertise()
        result,packets=self.tick(self.a,self.ca,1,[bridge_row()])
        self.assertFalse(result['a']);self.assertFalse(packets)
        # A kind 9 row smuggled into another room never becomes bridge state.
        packets=self.send(self.a,self.ca,self.b,self.cb,1.1,[plain()])
        smuggled=copy.deepcopy(packets[0]);smuggled['a'][0]=list(bridge_row())
        self.assertFalse(self.b.receive(self.cb,smuggled,1.11))
        fresh=self.tick(self.a,self.ca,1.2,[plain(index=1)])[1]
        self.assertTrue(self.b.receive(self.cb,fresh[0],1.21))

    def test_bridge_latch_change_is_an_edge_even_when_paused(self):
        t=w.WorldTransport()
        row=bridge_row(temp=0)
        _,packets=self.tick(t,self.ca,1,[row]);t.sent(packets,True,1)
        _,steady=self.tick(t,self.ca,2.1,[row]);self.assertTrue(steady)
        t.sent(steady,True,2.1)
        paused=bridge_row(temp=1,paused=1)
        _,blocked=self.tick(t,self.ca,2.13,[paused]);self.assertFalse(blocked)
        _,edge=self.tick(t,self.ca,2.16,[paused]);self.assertTrue(edge)
        self.assertEqual(edge[0]['u'],[[0,1]])


class BridgeCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        lib=Path(cls.temp.name)/'bridge.dylib'
        subprocess.run(['cc','-shared','-fPIC','-std=c99','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I'+str(ROOT/'include'),
                        str(ROOT/'src/world/anchor_world_codec.c'),
                        str(ROOT/'src/utils/string_utils.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.Row=ctypes.c_int*w.WORDS
        cls.lib.anchor_world_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def test_bridge_validation_parity(self):
        examples=[bridge_row(),
                  bridge_row(flags=3,fresh=1,gate=3,guard0=11,guard1=15),
                  bridge_row(gate=2,guard0=2,guard1=3,blocker=1,paused=1,temp=3)]
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for column in range(w.WORDS):
                for value in (-3276801,-100001,-32769,-32768,-1,0,1,2,3,4,7,8,14,15,16,
                              255,256,16383,16384,262143,262144,0x7ffff,0x80000,
                              0xfffff,0x100000,1000000,1000001,2147483647):
                    altered=list(r);altered[column]=value
                    self.assertEqual(
                        bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                        w.row_valid(altered),(column,value))

    def test_bridge_encoder_rejects_forbidden_rows(self):
        rows=((ctypes.c_int*w.WORDS)*4)()
        out=ctypes.create_string_buffer(w.STATE_BYTES)
        self.assertTrue(self.lib.anchor_world_encode(rows,0,(ctypes.c_ubyte*32)(),out,len(out)))
        for mutate in (lambda r:r.__setitem__(45,1),
                       lambda r:r.__setitem__(w.WB_GUARD_1,0),
                       lambda r:r.__setitem__(1,0x241)):
            bad=bridge_row();mutate(bad)
            self.assertFalse(self.lib.anchor_world_row_valid(self.Row(*bad)))
            self.assertFalse(self.lib.anchor_world_encode(
                (self.Row*1)(self.Row(*bad)),1,(ctypes.c_ubyte*32)(),out,len(out)))


if __name__=='__main__':unittest.main()
