import copy
import ctypes
import subprocess
import tempfile
import unittest
from pathlib import Path
from tests import test_world_random_transport as random_tests
import anchor_world_dynamic as d
import anchor_world_bomb as b
import anchor_world as w

ROOT=Path(__file__).resolve().parents[1]


def actor(role=0,parent=8,room=0x65,instance=1):
    r=[0]*d.WORDS
    r[3]=role+1;r[6:12]=[b.KIND,parent,b.ENTITY,b.CHILD_MODEL if role else b.ENTITY,8 if 1<=role<=4 else 0,0]
    r[24:27]=[200]*3;r[32]=163;r[42]=b.IDLE if not role else b.RING if role<=4 else b.PARTICLE
    r[47:50]=[30,60,0];r[61:64]=b.POSES[room][parent];r[64]=role
    r[b.ROLE]=role;r[b.ALPHA]=255;r[b.INSTANCE]=instance;r[b.PRESENT]=1
    if role and role<=4:
        r[b.VARIANT]=b.VARIANTS[role]
        if role in (2,4):
            r[50]=90;r[53]=6;r[67]=17
    return r


class BombTransportTests(unittest.TestCase):
    send=random_tests.RandomTransportTests.send

    def setUp(self):
        self.wa,self.wb=w.WorldTransport(),w.WorldTransport()
        self.a,self.b=d.DynamicTransport(self.wa),d.DynamicTransport(self.wb)
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,room=0x65,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        for t,c in ((self.wa,self.ca),(self.wb,self.cb)):
            t.update(c,0x65,42,1,[],'00'*32,0)
        for c,other,t in ((self.ca,self.cb,self.wb),(self.cb,self.ca,self.wa)):
            c['players'][other['cid']]=dict(online=True,isSaveLoaded=True,teamId='default',roomId=0x65,
                interactionSession=other['session'],worldSync=t.advertisement(other))
        self.a.update(self.ca,[],0);self.b.update(self.cb,[],0)
        self.send(self.a,self.ca,self.b,self.cb,[],.1)
        self.send(self.b,self.cb,self.a,self.ca,[],.1)

    def test_all_twelve_children_have_distinct_typed_identity(self):
        rows=[actor(role) for role in range(13)]
        result,packets=self.send(self.a,self.ca,self.b,self.cb,rows,1)
        self.assertEqual(len(result['a']),13)
        self.assertEqual(len({tuple(r[:4]) for r in result['a']}),13)
        self.assertEqual(len(packets),2)
        late,_=self.b.update(self.cb,[],1.01)
        self.assertEqual({r[b.ROLE] for r in late['a']},set(range(13)))
        self.assertEqual([r[b.ROLE] for r in late['a'] if r[53]],[2,4])
        for p in packets:
            self.assertNotIn('addToQueue',p)
            self.assertTrue(all(not r[b.INSTANCE] and not r[b.RECEIPT] for r in p['a']))

    def test_remote_proximity_arms_owner_and_commit_waits_for_send(self):
        root=actor()
        state,_=self.send(self.a,self.ca,self.b,self.cb,[root],1)
        root[b.RECEIPT]=state['a'][0][b.RECEIPT]
        self.send(self.a,self.ca,self.b,self.cb,[root],1.1)
        remote=actor();remote[45]=1
        self.send(self.b,self.cb,self.a,self.ca,[remote],1.2)
        state,_=self.a.update(self.ca,[root],1.3)
        self.assertEqual(state['a'][0][45],1)
        remote[d.LIFE]=d.CLAIM;remote[b.CAUSE]=2;remote[42]=b.EXPLODE
        self.send(self.b,self.cb,self.a,self.ca,[remote],1.4)
        state,packets=self.a.update(self.ca,[root],1.5)
        self.assertEqual(state['a'][0][d.LIFE],d.REMOVED)
        self.assertEqual(state['a'][0][b.CAUSE],2)
        self.assertEqual(state['a'][0][d.COMMITTER],1)
        self.assertFalse(self.a.native_result(state)['a'])
        self.a.sent(packets,True,1.5)
        self.assertEqual(self.a.native_result(state)['a'][0][d.LANDED],1)

    def test_scope_variant_and_collision_forgery_rejected(self):
        for col,value in ((7,7),(61,4001),(b.VARIANT,1),(b.ROLE,13)):
            bad=actor();bad[col]=value
            result,packets=self.a.update(self.ca,[bad],1)
            self.assertFalse(result['a']);self.assertFalse(packets)
        for role in (1,3,5,12):
            bad=actor(role);bad[53]=6
            self.assertFalse(d.valid(bad))
        _,packets=self.a.update(self.ca,[actor()],1)
        bad=copy.deepcopy(packets[0]);bad['a'][0][0]=0x7ffffff7
        self.assertFalse(self.b.receive(self.cb,bad,1.1))

    def test_codec_parity(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'codec.so'
            subprocess.run(['cc','-shared','-fPIC','-std=c99','-I'+str(ROOT/'include'),
                str(ROOT/'src/utils/anchor_world_dynamic_codec.c'),str(ROOT/'src/utils/string_utils.c'),
                '-o',str(path)],check=True)
            lib=ctypes.CDLL(str(path));Row=ctypes.c_int*d.WORDS
            lib.anchor_world_dynamic_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]
            for role in (0,1,2,3,4,5,12):
                base=actor(role)
                self.assertTrue(d.valid(base))
                for col in range(d.WORDS):
                    for value in (-32769,-1,0,1,2,3,4,8,12,13,17,29,30,60,90,176,255,256,32768,0x7fffffff):
                        probe=list(base);probe[col]=value
                        self.assertEqual(bool(lib.anchor_world_dynamic_row_valid(Row(*probe))),
                            d.valid(probe),(role,col,value))


if __name__=='__main__':
    unittest.main()
