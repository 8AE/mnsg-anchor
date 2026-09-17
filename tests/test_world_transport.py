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


def row(index=0,entity=0x3e0,kind=2):
    r=[0]*w.WORDS
    r[:3]=[index,entity,kind]
    r[18]=1 if kind==2 else 0
    r[26:29]=[1000]*3
    return r


class WorldTests(unittest.TestCase):
    def setUp(self):
        self.a=w.WorldTransport();self.b=w.WorldTransport()
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,room=0x12e,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        self.tick(self.a,self.ca,0)
        self.tick(self.b,self.cb,0)
        self.advertise()

    def tick(self,t,c,now,rows=None,dead='00'*32,visit=1,signature=42):
        return t.update(c,c['room'],signature,visit,rows if rows is not None else [row()],dead,now)

    def advertise(self):
        self.ca['players'][2]=dict(online=True,isSaveLoaded=True,teamId='default',roomId=0x12e,
            interactionSession=20,worldSync=self.b.advertisement(self.cb))
        self.cb['players'][1]=dict(online=True,isSaveLoaded=True,teamId='default',roomId=0x12e,
            interactionSession=10,worldSync=self.a.advertisement(self.ca))

    def send(self,t,c,receiver,rc,now,rows=None,dead='00'*32):
        status,packets=self.tick(t,c,now,rows,dead)
        for p in packets:self.assertTrue(receiver.receive(rc,p,now+.01))
        t.sent(packets,True,now)
        return packets

    def test_snapshot_and_per_instance_authority(self):
        r=row();r[4]=13000
        self.send(self.a,self.ca,self.b,self.cb,1,[r])
        result,_=self.tick(self.b,self.cb,1.02,[row(),row(4)])
        self.assertEqual(len(result['a']),1)
        self.assertEqual(result['a'][0][0],1)
        self.assertEqual(result['a'][0][6],13000)
        # An actor absent on the other client keeps running locally.
        self.assertEqual(result['a'][0][2],0)

    def test_dialogue_temporary_owner_and_pause_handoff(self):
        r=row(0,0x2c1,1);r[3]=1
        self.send(self.b,self.cb,self.a,self.ca,1,[r])
        result,_=self.tick(self.a,self.ca,1.02,[row(0,0x2c1,1)])
        self.assertEqual(result['a'][0][0],2)
        self.b.reset();self.tick(self.b,self.cb,2);self.advertise()
        r=row();r[38]=1
        self.send(self.a,self.ca,self.b,self.cb,3,[r])
        result,_=self.tick(self.b,self.cb,3.02)
        self.assertFalse(result['a'])

    def test_culling_not_destruction_and_expiry(self):
        self.send(self.a,self.ca,self.b,self.cb,1)
        result,_=self.tick(self.b,self.cb,3)
        self.assertFalse(result['a'])
        self.assertEqual(result['d'],'00'*32)

    def test_pickup_tombstones_late_entry_and_empty_room_reset(self):
        dead='01'+'00'*31
        self.send(self.a,self.ca,self.b,self.cb,1,dead=dead)
        result,_=self.tick(self.b,self.cb,1.1)
        self.assertEqual(result['d'],dead)
        result,_=self.tick(self.b,self.cb,2,visit=2)
        self.assertEqual(result['d'],'00'*32)

    def test_batches_atomic_reorder_duplicate_and_bounds(self):
        rows=[row(i) for i in range(256)]
        _,packets=self.tick(self.a,self.ca,1,rows)
        self.assertEqual(len(packets),11)
        for packet in reversed(packets[1:]):self.assertTrue(self.b.receive(self.cb,packet,1.01))
        self.assertNotIn(1,self.b.peers)
        self.assertTrue(self.b.receive(self.cb,packets[0],1.02))
        self.assertEqual(len(self.b.peers[1]['rows']),256)
        self.assertFalse(self.b.receive(self.cb,packets[0],1.03))
        self.assertTrue(all(len(json.dumps(p,separators=(',',':')).encode())+1<=w.PACKET_BYTES for p in packets))

    def test_invalid_packets_do_not_poison_cache(self):
        _,packets=self.tick(self.a,self.ca,1)
        base=packets[0]
        changes=[{'clientId':True},{'clientId':99},{'targetTeamId':'other'},
                 {'targetClientId':2},{'addToQueue':True},{'q':True},
                 {'q':0},{'parts':12},{'part':1},{'p':'0'},{'d':'x'*64},
                 {'m':[w.VERSION,10,2,0x12e,42]},{'a':[row(),row()]},
                 {'a':[[0]*39]},{'a':[[False]+row()[1:]]}]
        for change in changes:
            with self.subTest(change=change):
                self.b.reset();self.tick(self.b,self.cb,0)
                self.assertFalse(self.b.receive(self.cb,dict(base,**change),1))
                self.assertFalse(self.b.peers)

    def test_cross_room_session_layout_save_and_offline_rejected(self):
        _,packets=self.tick(self.a,self.ca,1)
        peer=self.cb['players'][1]
        for key,value in [('roomId',0x12d),('interactionSession',11),('teamId','other'),
                          ('isSaveLoaded',False),('online',False),('worldSync',[w.VERSION,10,1,0x12e,99])]:
            old=peer[key];peer[key]=value
            self.assertFalse(self.b.receive(self.cb,packets[0],1));peer[key]=old

    def test_failed_batch_retries_new_sequence(self):
        _,p=self.tick(self.a,self.ca,1)
        self.a.sent(p,False,1)
        _,blocked=self.tick(self.a,self.ca,1.01)
        self.assertFalse(blocked)
        _,retry=self.tick(self.a,self.ca,1.06)
        self.assertEqual(retry[0]['q'],p[0]['q']+1)
        self.a.sent(retry,True,1.06)
        _,none=self.tick(self.a,self.ca,1.05)
        self.assertFalse(none)

    def test_steady_cadence_and_edge_bypass(self):
        sent=[]
        for frame in range(301):
            _,packets=self.tick(self.a,self.ca,frame/30)
            if packets:
                sent.append(frame/30);self.a.sent(packets,True,frame/30)
        self.assertLessEqual(len(sent),51)
        self.assertTrue(all(b-a>=.2-1e-8 for a,b in zip(sent,sent[1:])))
        _,packets=self.tick(self.a,self.ca,sent[-1]+.051,[row(5)])
        self.assertTrue(packets)

    def test_metadata_monotonic_and_non_integer_rejected(self):
        old=[w.VERSION,10,5,302,42]
        self.assertEqual(w.merge_metadata(old,[w.VERSION,10,4,302,42],10),old)
        self.assertIsNone(w.metadata([True,10,1,302,42]))
        self.assertIsNone(w.metadata([w.VERSION,11,1,302,42],10))

    def test_identity_mismatch_never_applied(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[row(entity=0x2c1,kind=1)])
        result,_=self.tick(self.b,self.cb,1.02)
        self.assertFalse(result['a'])


class CodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        lib=Path(cls.temp.name)/'world.dylib'
        subprocess.run(['cc','-shared','-fPIC','-std=c99','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I'+str(ROOT/'include'),
                        str(ROOT/'src/utils/anchor_world_codec.c'),
                        str(ROOT/'src/utils/string_utils.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.Row=ctypes.c_int*39;cls.StatusRow=ctypes.c_int*41
        cls.lib.anchor_world_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def decode(self,value):
        rows=(self.StatusRow*256)();count=ctypes.c_uint();dead=(ctypes.c_ubyte*32)()
        ok=self.lib.anchor_world_decode(value.encode(),rows,ctypes.byref(count),dead)
        return ok,rows,count.value,bytes(dead)

    def test_cross_language_round_trip(self):
        rows=(self.Row*256)(*[self.Row(*row(i)) for i in range(256)])
        out=ctypes.create_string_buffer(w.STATE_BYTES);dead=(ctypes.c_ubyte*32)(1)
        self.assertTrue(self.lib.anchor_world_encode(rows,256,dead,out,len(out)))
        data=json.loads(out.value)
        self.assertEqual(data['a'],[row(i) for i in range(256)])
        status=json.dumps({'a':[[1,0]+r for r in data['a']],'d':data['d']})
        ok,result,count,bits=self.decode(status)
        self.assertTrue(ok);self.assertEqual(count,256);self.assertEqual(bits[0],1)
        self.assertEqual(list(result[255])[2:],row(255))

    def test_native_rejects_malformed_rows_and_trailing_data(self):
        good=json.dumps({'a':[[1,0]+row()],'d':'00'*32})
        for data in [good+'x',good[:-1],good.replace('"d"','"bad"'),
                     json.dumps({'a':[[1,0]+row()]*2,'d':'00'*32}),
                     json.dumps({'a':[[1,0]+[256]+row()[1:]],'d':'00'*32})]:
            self.assertFalse(self.decode(data)[0])
        for n in [-(1<<31)-1,1<<31,True,float('nan')]:
            r=row();r[4]=n
            self.assertFalse(self.decode(json.dumps({'a':[[1,0]+r],'d':'00'*32}))[0])

    def test_native_python_validation_agree(self):
        self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*row())))
        for column in range(39):
            for value in [-2147483648,-32769,-1,0,1,163,256,1024,32768,2147483647]:
                r=row();r[column]=value
                self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*r))),w.row_valid(r),(column,value))


if __name__=='__main__':unittest.main()
