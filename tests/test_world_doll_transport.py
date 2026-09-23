"""File62 doll container (world kind 11) and the nested Doll (dynamic kind 7).

The container row carries one cycle/phase/pitch checkpoint per room; the Doll
row carries the typed falling/rest checkpoint with a canonical per-container
origin. The transport owns validation, presence, receipt and the claim lease;
the native adapters own the presentation, the spawned actor and the descent.
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
import anchor_world_dynamic as d


DOLL_ROOM_A, DOLL_ROOM_B = 0x16a, 0x182
DOLL_PARENT_A, DOLL_PARENT_B = 8, 3


def container(index=0,phase=0,pitch=0,cycle=0,spawned=0,paused=0):
    r=[0]*w.WORDS
    r[0]=index;r[1]=w.DOLL_ENTITY;r[2]=w.DOLL
    r[7]=pitch;r[w.DOLL_CYCLE]=cycle;r[w.DOLL_PHASE]=phase
    r[w.DOLL_SPAWNED]=spawned;r[38]=paused
    return r


def doll(y=12500,phase=16,parent=DOLL_PARENT_A,life=d.LIVE,serial=1,index=0):
    r=[0]*d.WORDS
    r[:12]=[0,0,0,serial,0,life,d.DOLL,parent,0,0,2,0]
    r[0]=index
    r[12]=-600;r[13]=y;r[14]=-10200
    r[24:27]=[100]*3
    r[32]=163;r[42]=phase;r[64]=1
    return r


class DollContainerTests(unittest.TestCase):
    def setUp(self):
        self.a=w.WorldTransport();self.b=w.WorldTransport()
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,
                     room=DOLL_ROOM_A,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        self.tick(self.a,self.ca,0)
        self.tick(self.b,self.cb,0)
        self.advertise()

    def tick(self,t,c,now,rows=None,dead='00'*32,visit=1,signature=42):
        return t.update(c,c['room'],signature,visit,
                        rows if rows is not None else [container()],dead,now)

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

    def test_doll_container_poses_and_rejections(self):
        for phase,pitch in ((0,0),(1,0),(1,64),(2,70),(3,70),(3,54),(3,38),(3,22),(3,6)):
            row=container(phase=phase,pitch=pitch,cycle=0 if phase==0 else 3,
                          spawned=1 if phase==2 else 0)
            self.assertTrue(w.row_valid(row),(phase,pitch))
        # A zero cycle only ever means idle, but a finished idle container keeps
        # its live cycle, so close -> idle -> reopen is a valid progression.
        self.assertTrue(w.row_valid(container(phase=0,cycle=1)))
        self.assertTrue(w.row_valid(container(phase=0,cycle=2)))
        for row in (container(phase=3,pitch=6,cycle=1),container(phase=0,cycle=1),
                    container(phase=1,pitch=8,cycle=2)):
            self.assertTrue(w.row_valid(row),row[:14])
        forbidden=[container(phase=0,pitch=8),
                   container(phase=1,pitch=9,cycle=1),container(phase=1,pitch=65,cycle=1),
                   container(phase=1,cycle=0),container(phase=2,pitch=64,cycle=1),
                   container(phase=3,pitch=64,cycle=1),container(phase=4,cycle=1),
                   container(phase=1,pitch=8,cycle=1,spawned=2),
                   container(phase=1,pitch=8,cycle=1000001)]
        for row in forbidden:
            self.assertFalse(w.row_valid(row),row[:14])
        # The busy word is a pinned zero and the payload stays inside bounds.
        busy=list(container(phase=1,pitch=8,cycle=1));busy[3]=1
        self.assertFalse(w.row_valid(busy))
        low=list(container(phase=1,pitch=8,cycle=1));low[4]=-3276801
        self.assertFalse(w.row_valid(low))

    def test_doll_container_progress_and_wire(self):
        self.assertLess(w.controller_progress(container(phase=0,cycle=1)),
                        w.controller_progress(container(phase=1,pitch=8,cycle=2)))
        self.assertLess(w.controller_progress(container(phase=1,pitch=8,cycle=1)),
                        w.controller_progress(container(phase=1,pitch=8,cycle=2)))
        self.assertEqual(w.controller_progress(container(phase=0,cycle=2)),11)
        self.assertEqual(w.controller_progress(container(phase=1,pitch=8,cycle=2)),8)
        self.assertLess(w.controller_progress(container(phase=3,pitch=6,cycle=1)),
                        w.controller_progress(container(phase=0,cycle=2)))
        row=container(phase=1,pitch=16,cycle=2)
        packets=self.send(self.a,self.ca,self.b,self.cb,1,[row])
        wire=packets[0]['a'][0]
        self.assertEqual(wire[w.DOLL_PHASE],1)
        self.assertEqual(wire[w.DOLL_CYCLE],2)
        self.assertEqual(wire[w.INSTANCE],0)
        self.assertEqual(wire[w.RECEIPT],0)
        self.assertEqual(packets[0]['u'],[])
        self.assertLessEqual(len(json.dumps(packets[0],separators=(',',':')).encode())+1,
                             w.PACKET_BYTES)

    def test_doll_container_self_echo_reaches_the_local_simulator(self):
        # The echo waits until the owner is no longer waiting for members, so
        # let the peer publish its presence first.
        self.send(self.b,self.cb,self.a,self.ca,1,[container(phase=1,pitch=16,cycle=4)])
        row=container(phase=1,pitch=24,cycle=5)
        result,_=self.tick(self.a,self.ca,1.2,[row])
        echo=result['a'][0]
        self.assertEqual(echo[0],self.ca['cid'])
        self.assertEqual(echo[1],0)
        self.assertEqual(echo[w.DELIVERY_ROW+w.DOLL_PHASE],1)
        self.assertEqual(echo[w.DELIVERY_ROW+w.DOLL_CYCLE],5)
        self.assertEqual(echo[w.DELIVERY_ROW+w.RECEIPT],0)

    def test_doll_container_reopening_and_stale_peer_bootstrap(self):
        first=container(phase=3,pitch=6,cycle=4)
        self.send(self.a,self.ca,self.b,self.cb,1,[first])
        reopened=container(phase=1,pitch=8,cycle=5)
        _,packets=self.tick(self.a,self.ca,1.06,[reopened])
        self.assertTrue(packets)
        self.assertEqual(self.a.owners[0],self.ca['cid'])
        # A peer arriving with the previous cycle takes the newer checkpoint.
        self.b.reset()
        self.tick(self.b,self.cb,1.1,[]);self.advertise()
        self.send(self.a,self.ca,self.b,self.cb,1.12,[reopened])
        stale=container(phase=3,pitch=6,cycle=4);stale[w.INSTANCE]=9
        entry,_=self.tick(self.b,self.cb,1.14,[stale])
        applied=[r for r in entry['a'] if r[0]==self.ca['cid']][0]
        self.assertEqual(applied[w.DELIVERY_ROW+w.DOLL_CYCLE],5)
        self.assertTrue(applied[w.DELIVERY_ROW+w.RECEIPT] & w.BOOTSTRAP)

    def test_doll_container_room_scope(self):
        self.ca['room']=self.cb['room']=0x12e
        self.a.reset();self.b.reset()
        self.tick(self.a,self.ca,0,[]);self.tick(self.b,self.cb,0,[]);self.advertise()
        result,packets=self.tick(self.a,self.ca,1,[container(phase=1,pitch=8,cycle=1)])
        self.assertFalse(result['a']);self.assertFalse(packets)
        # Both File62 rooms accept the container.
        for room in (DOLL_ROOM_A,DOLL_ROOM_B):
            t=w.WorldTransport();c=dict(self.ca,room=room)
            t.update(c,room,42,1,[],'00'*32,0)
            _,packets=t.update(c,room,42,1,[container(phase=1,pitch=8,cycle=1)],
                               '00'*32,1)
            self.assertTrue(packets,room)


class DollDynamicTests(unittest.TestCase):
    def setUp(self):
        self.wa=w.WorldTransport();self.wb=w.WorldTransport()
        self.a=d.DynamicTransport(self.wa);self.b=d.DynamicTransport(self.wb)
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,
                     room=DOLL_ROOM_A,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        for t,c in ((self.wa,self.ca),(self.wb,self.cb)):
            t.update(c,DOLL_ROOM_A,42,1,[],'00'*32,0)
        for c,other,t in ((self.ca,self.cb,self.wb),(self.cb,self.ca,self.wa)):
            c['players'][other['cid']]=dict(online=True,isSaveLoaded=True,teamId='default',
                roomId=DOLL_ROOM_A,interactionSession=other['session'],
                worldSync=t.advertisement(other))
        self.a.update(self.ca,[],0);self.b.update(self.cb,[],0)
        for tx,c,rx,rc in ((self.wa,self.ca,self.wb,self.cb),(self.wb,self.cb,self.wa,self.ca)):
            _,packets=tx.update(c,DOLL_ROOM_A,42,1,[],'00'*32,0)
            for packet in packets:self.assertTrue(rx.receive(rc,packet,0))

    def send(self,t,c,rx,rc,rows,now):
        result,packets=t.update(c,rows,now)
        self.assertTrue(packets)
        for packet in reversed(packets):self.assertTrue(rx.receive(rc,packet,now+.001))
        t.sent(packets,True,now)
        return result['a'],packets

    def test_doll_row_contract_and_claim_parity(self):
        self.assertTrue(d.valid(doll()))
        self.assertTrue(d.valid(doll(y=3500)))
        self.assertTrue(d.valid(doll(y=3400,phase=17)))
        self.assertTrue(d.valid(doll(life=d.CLAIM)))
        for bad in (doll(y=3499),doll(y=3600,phase=17),doll(y=3400),
                    doll(parent=0),doll(parent=257),doll(serial=0),
                    doll(y=12500,phase=17),doll(y=3400,phase=16)):
            self.assertFalse(d.valid(bad),bad[7:15])
        for column,value in ((12,-601),(32,162),(40,1),(41,1),(65,1),(21,1),(74,1)):
            broken=doll();broken[column]=value
            self.assertFalse(d.valid(broken),(column,value))

    def test_doll_birth_coalesces_and_descent_never_rewinds(self):
        expected=[d.DOLL_ORIGIN,42,DOLL_ROOM_A+1,DOLL_PARENT_A]
        state,_=self.send(self.a,self.ca,self.b,self.cb,[doll(y=12500)],1)
        self.assertEqual(state[0][:4],expected)
        # The same container birth on the other client coalesces onto that key.
        result,_=self.b.update(self.cb,[doll(y=12500,serial=99)],1.02)
        self.assertEqual({tuple(r[:4]) for r in result['a']},{tuple(expected)})
        # A deeper observer cannot be rewound by the fresh owner row.
        self.send(self.b,self.cb,self.a,self.ca,[doll(y=9000,serial=7)],1.04)
        observed,_=self.a.update(self.ca,[doll(y=12500)],1.06)
        self.assertEqual(observed['a'][0][13],9000)
        self.assertEqual(observed['a'][0][42],16)
        # A settled observer forces the matching resting phase, so the row the
        # transport hands back stays valid.
        # Replicas advertise at 1 Hz, so the next report lands a second later.
        self.send(self.b,self.cb,self.a,self.ca,[doll(y=3400,phase=17,serial=8)],2.2)
        settled,_=self.a.update(self.ca,[doll(y=12500)],2.22)
        self.assertEqual(settled['a'][0][13],3400)
        self.assertEqual(settled['a'][0][42],17)
        self.assertTrue(d.valid(list(settled['a'][0])))

    def test_doll_claim_commits_once_and_retries_until_sent(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[doll(y=8000)],1)
        claim=copy.deepcopy(state[0]);claim[d.LIFE]=d.CLAIM
        self.send(self.b,self.cb,self.a,self.ca,[claim],1.1)
        # Decide the kill first: the award is withheld until the commit reaches
        # the wire, and a failed send keeps the retry alive.
        committed,packets=self.a.update(self.ca,state,1.2)
        self.assertEqual(sum(r[d.LIFE]==d.REMOVED for r in committed['a']),1)
        self.assertFalse(self.a.native_result(committed)['a'])
        self.a.sent(packets,False,1.2)
        self.assertFalse(self.a.native_result(committed)['a'])
        again,packets=self.a.update(self.ca,state,1.3)
        self.a.sent(packets,True,1.3)
        self.assertTrue(self.a.native_result(again)['a'])
        for packet in packets:self.assertTrue(self.b.receive(self.cb,packet,1.31))
        late,_=self.b.update(self.cb,[claim],1.4)
        self.assertEqual(sum(r[d.LIFE]==d.REMOVED for r in late['a']),1)

    def test_doll_late_entry_paused_owner_and_departure(self):
        key=[d.DOLL_ORIGIN,42,DOLL_ROOM_A+1,DOLL_PARENT_A]
        self.send(self.a,self.ca,self.b,self.cb,[doll(y=7000)],1)
        # A late entrant forgets the received hot state, then receives the
        # owner checkpoint again before reporting its own fresh copy.
        self.b.reset();self.b.update(self.cb,[],1.1)
        self.send(self.a,self.ca,self.b,self.cb,[doll(y=7000)],2.4)
        entry,_=self.b.update(self.cb,[doll(y=7000,serial=5)],2.42)
        applied=[r for r in entry['a'] if r[:4]==key][0]
        self.assertEqual(applied[:4],[d.DOLL_ORIGIN,42,DOLL_ROOM_A+1,DOLL_PARENT_A])
        self.assertEqual(applied[13],7000)
        # Pause does not release the award lease. A silent but still eligible
        # owner cannot be replaced by a second arbiter during a transient stall.
        paused=doll(y=7000);paused[66]=1
        self.send(self.a,self.ca,self.b,self.cb,[paused],3.6)
        result,_=self.b.update(self.cb,[doll(y=6000,serial=6)],3.62)
        self.assertEqual(result['a'][0][d.OWNER],self.ca['cid'])
        self.assertEqual(result['a'][0][66],1)
        # Owner departure keeps the survivor authoritative.
        self.cb['players'][1]['online']=False
        result,_=self.b.update(self.cb,[doll(y=6000,serial=6)],3.64)
        self.assertEqual(result['a'][0][d.OWNER],self.cb['cid'])
        self.assertEqual(result['a'][0][66],0)

    def test_simultaneous_claims_commit_one_winner(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[doll(y=5000)],1)
        claim=copy.deepcopy(state[0]);claim[d.LIFE]=d.CLAIM
        self.send(self.b,self.cb,self.a,self.ca,[claim],1.2)
        result,packets=self.a.update(self.ca,[claim],1.4)
        self.assertEqual(len(result['a']),1)
        self.assertEqual(result['a'][0][d.OWNER:d.LIFE+1],[1,d.REMOVED])
        self.assertFalse(self.a.native_result(result)['a'])
        self.a.sent(packets,True,1.4)
        for packet in packets:self.assertTrue(self.b.receive(self.cb,packet,1.41))
        observer,_=self.b.update(self.cb,[claim],1.6)
        self.assertEqual(observer['a'][0][d.OWNER:d.LIFE+1],[1,d.REMOVED])

    def test_doll_room_and_parent_binding(self):
        self.assertTrue(d.valid(doll(parent=DOLL_PARENT_A)))
        self.assertTrue(d.valid(doll(parent=DOLL_PARENT_B)))
        # A Doll for the other room's container is rejected on update.
        result,packets=self.a.update(self.ca,[doll(parent=DOLL_PARENT_B)],1)
        self.assertFalse(result['a']);self.assertFalse(packets)
        # The same rule holds on the wire.
        _,packets=self.a.update(self.ca,[doll(parent=DOLL_PARENT_A)],1.06)
        good=packets[0]
        smuggled=copy.deepcopy(good);smuggled['a'][0]=list(doll(parent=DOLL_PARENT_B))
        self.assertFalse(self.b.receive(self.cb,smuggled,1.07))
        self.assertTrue(self.b.receive(self.cb,good,1.08))
        # A bound Doll key must be the canonical container origin: a forged
        # signature, visit, serial or a mixed-zero prefix never becomes state.
        forged=copy.deepcopy(packets[0])
        for prefix in ([d.DOLL_ORIGIN,41,DOLL_ROOM_A+1,DOLL_PARENT_A],
                       [d.DOLL_ORIGIN,42,0x12e,DOLL_PARENT_A],
                       [d.DOLL_ORIGIN,42,DOLL_ROOM_A+1,DOLL_PARENT_B],
                       [0,42,DOLL_ROOM_A+1,DOLL_PARENT_A],
                       [d.DOLL_ORIGIN,42,0,DOLL_PARENT_A]):
            bad=copy.deepcopy(forged);bad['q']+=1;bad['a'][0][:4]=list(prefix)
            self.assertFalse(self.b.receive(self.cb,bad,1.09),prefix)
        # The local capture cannot publish a bound Doll under a foreign origin.
        bound=doll();bound[:4]=[d.DOLL_ORIGIN,41,DOLL_ROOM_A+1,DOLL_PARENT_A]
        result,packets=self.a.update(self.ca,[bound],2.4)
        self.assertFalse(result['a']);self.assertFalse(packets)

    def test_doll_team_and_session_rejection(self):
        _,packets=self.a.update(self.ca,[doll()],1)
        packet=packets[0]
        self.assertFalse(self.b.receive(self.cb,dict(packet,targetTeamId='other'),1))
        self.cb['players'][1]['interactionSession']=99
        self.assertFalse(self.b.receive(self.cb,packet,1))
        self.cb['players'][1]['interactionSession']=10
        self.assertTrue(self.b.receive(self.cb,packet,1))


class DollCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        world=Path(cls.temp.name)/'world.dylib'
        dyn=Path(cls.temp.name)/'dynamic.dylib'
        for lib,src in ((world,'src/world/anchor_world_codec.c'),
                        (dyn,'src/world/anchor_world_dynamic_codec.c')):
            subprocess.run(['cc','-shared','-fPIC','-std=c99','-Wall','-Wextra','-Werror',
                            '-Wno-misleading-indentation','-I'+str(ROOT/'include'),
                            str(ROOT/src),str(ROOT/'src/utils/string_utils.c'),
                            '-o',str(lib)],check=True)
        cls.wlib=ctypes.CDLL(str(world));cls.dlib=ctypes.CDLL(str(dyn))
        cls.WRow=ctypes.c_int*w.WORDS;cls.DRow=ctypes.c_int*d.WORDS
        cls.wlib.anchor_world_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]
        cls.dlib.anchor_world_dynamic_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def test_doll_container_validation_parity(self):
        examples=[container(phase=0),container(phase=1,pitch=32,cycle=2),
                  container(phase=2,pitch=70,cycle=2,spawned=1),
                  container(phase=3,pitch=54,cycle=2,paused=1)]
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.wlib.anchor_world_row_valid(self.WRow(*r)))
            for column in range(w.WORDS):
                for value in (-3276801,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,16,22,38,54,
                              64,65,70,71,1023,1024,1000000,1000001,3276701,2147483647):
                    altered=list(r);altered[column]=value
                    self.assertEqual(
                        bool(self.wlib.anchor_world_row_valid(self.WRow(*altered))),
                        w.row_valid(altered),(column,value))

    def test_doll_dynamic_validation_parity(self):
        examples=[doll(),doll(y=3500),doll(y=3400,phase=17),doll(life=d.CLAIM),
                  doll(parent=DOLL_PARENT_B),doll(life=d.REMOVED)]
        for r in examples:
            self.assertTrue(d.valid(r),r[:15])
            self.assertTrue(self.dlib.anchor_world_dynamic_row_valid(self.DRow(*r)))
            for column in range(d.WORDS):
                for value in (-2147483648,-10201,-600,-1,0,1,2,3,4,5,6,7,8,16,17,18,40,
                              41,64,65,74,163,164,255,256,257,3400,3499,3500,12500,12501,
                              32767,2147483647):
                    altered=list(r);altered[column]=value
                    self.assertEqual(
                        bool(self.dlib.anchor_world_dynamic_row_valid(self.DRow(*altered))),
                        d.valid(altered),(column,value))


if __name__=='__main__':unittest.main()
