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
from anchor_world_npc import MODELS as NPC_MODELS


def row(index=0,entity=0x3e0,kind=2):
    r=[0]*w.WORDS
    r[:3]=[index,entity,kind]
    r[18]=1 if kind==2 else 0
    r[26:29]=[1000]*3
    return r


def switch_row(phase=1,mode=0):
    r=row(0,0x226,6)
    r[18:23]=[phase,mode,0x194 if mode else 0,int(phase>=4),int(phase>=3)]
    r[3]=int(phase>1)
    return r


def linked_row(fire=False, phase=None, state=0):
    r=row(0,0x1fe if fire else 0x228,2)
    r[18]=phase if phase is not None else (65 if fire else 62)
    r[19:24]=[120,-50,80,20,int(r[18]>=(65 if fire else 62))]
    if fire and r[23]:
        r[10]=2;r[24]=state;r[33]=1;r[42]=0x240
        r[43:48]=[-176,-100,-300,125,-50]
        if 1 <= state <= 4:
            r[17]=256 if state==3 else 40;r[40:42]=[12750,6000]
        if r[18]==67:r[25]=10
    return r


def physics_row(phase=69, timer=0, attempt=0):
    r=row(0,0x3d0,2)
    r[18]=phase;r[17]=timer;r[23]=attempt
    r[10]=1 if phase==71 else 2 if 72<=phase<=74 else 0
    r[30]=5 if phase==70 else 6 if 71<=phase<=74 else 0
    r[29]=(31,27,1,11,15,27,17)[r[30]]
    return r


def crane_row(index=0,mask=0,phase=1,reward=1,pause=0,power=0,timer=0):
    r=[0]*w.WORDS
    r[0]=index;r[1]=w.CRANE_ENTITY;r[2]=w.CRANE;r[10]=1
    r[17]=timer;r[18]=phase
    r[19]=1;r[20]=0;r[21]=0;r[22]=1;r[23]=0;r[24]=0
    r[30]=power
    r[32]=int(reward==13);r[37]=reward;r[38]=pause
    r[44]=2 if reward==13 else 0
    r[46]=mask
    return r


def shutter_row(index=0,phase=1,timer=0,ordinal=0,emitter=1,pause=0,playback=0):
    r=row(index,w.SHUTTER_ENTITY,w.SHUTTER)
    r[10]=14
    r[17]=timer;r[18]=phase
    r[19]=emitter;r[20]=ordinal
    r[29]=playback;r[38]=pause
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
        self.ca['players'][2]=dict(online=True,isSaveLoaded=True,teamId='default',roomId=self.ca['room'],
            interactionSession=20,worldSync=self.b.advertisement(self.cb))
        self.cb['players'][1]=dict(online=True,isSaveLoaded=True,teamId='default',roomId=self.cb['room'],
            interactionSession=10,worldSync=self.a.advertisement(self.ca))

    def send(self,t,c,receiver,rc,now,rows=None,dead='00'*32,visit=1):
        status,packets=self.tick(t,c,now,rows,dead,visit=visit)
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

    def test_physics_completion_retry_and_reward_progress(self):
        r=physics_row(73,120)
        self.send(self.b,self.cb,self.a,self.ca,1,[r])
        result,_=self.tick(self.a,self.ca,1.02,[physics_row(73,140)])
        self.assertEqual(result['a'][0][0],2)
        self.assertEqual(result['a'][0][19],120)
        # A broken attempt outranks an idle newcomer, including when culled.
        broken=physics_row(75);broken[38]=1
        self.send(self.b,self.cb,self.a,self.ca,1.1,[broken])
        result,_=self.tick(self.a,self.ca,1.12,[physics_row()])
        self.assertEqual(result['a'][0][20],75)
        # Native proximity reload starts a new attempt; no permanent tombstone.
        retry=physics_row(attempt=1)
        result,packets=self.tick(self.a,self.ca,1.2,[retry])
        self.assertFalse(result['a']);self.assertEqual(self.a.owners[0],1)
        self.assertTrue(packets)
        self.assertGreater(w.controller_progress(retry),w.controller_progress(broken))

    def test_physics_grab_handoff_and_completion_beats_carry(self):
        self.send(self.a,self.ca,self.b,self.cb,1,[physics_row()])
        carried=physics_row();carried[3]=1;carried[22]=16;carried[21]=5
        carried[30]=2;carried[29]=1
        result,packets=self.tick(self.b,self.cb,1.02,[carried])
        self.assertFalse(result['a']);self.assertEqual(self.b.owners[0],2)
        self.assertTrue(packets)
        # A completion checkpoint cannot be reset by an older in-flight grab.
        self.send(self.a,self.ca,self.b,self.cb,1.1,[physics_row(70)])
        result,_=self.tick(self.b,self.cb,1.12,[carried])
        self.assertEqual(result['a'][0][0],1)
        self.assertEqual(result['a'][0][20],70)
        # Even a newer retry cannot undo a completed puzzle in this visit.
        retry=physics_row(attempt=8388607)
        result,_=self.tick(self.b,self.cb,1.14,[retry])
        self.assertEqual(result['a'][0][20],70)

    def test_physics_zero_health_pause_hands_off_after_native_receipt(self):
        self.b.reset()
        fresh=physics_row();fresh[w.INSTANCE]=22
        self.tick(self.b,self.cb,.5,[fresh]);self.advertise()
        stopped=physics_row();stopped[38]=1
        self.send(self.a,self.ca,self.b,self.cb,1,[stopped])
        result,_=self.tick(self.b,self.cb,1.02,[fresh])
        self.assertEqual(result['a'][0][0],1)
        self.assertTrue(result['a'][0][-1] & w.BOOTSTRAP)
        applied=result['a'][0][2:];applied[38]=0
        result,_=self.tick(self.b,self.cb,1.04,[applied])
        self.assertFalse(result['a'])
        self.assertEqual(self.b.owners[0],2)

    def test_dialogue_temporary_owner_and_pause_handoff(self):
        r=row(0,0x2c1,1);r[3]=1
        self.send(self.b,self.cb,self.a,self.ca,1,[r])
        result,_=self.tick(self.a,self.ca,1.02,[row(0,0x2c1,1)])
        self.assertEqual(result['a'][0][0],2)
        self.b.reset();self.tick(self.b,self.cb,2);self.advertise()
        r=row();r[38]=1
        self.send(self.a,self.ca,self.b,self.cb,3,[r])
        result,_=self.tick(self.b,self.cb,3.02)
        self.assertTrue(result['a'][0][-1] & w.BOOTSTRAP)
        applied=result['a'][0][2:];applied[38]=0
        result,_=self.tick(self.b,self.cb,3.04,[applied])
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
                 {'z':None},{'z':'80'+'00'*31},
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

    def test_door_departure_handoff_and_reverse_checkpoint(self):
        opening=row(entity=0x23c,kind=5)
        opening[3]=1;opening[11]=1500;opening[12]=256;opening[29]=20
        self.send(self.a,self.ca,self.b,self.cb,1,[opening])
        local=row(entity=0x23c,kind=5)
        result,_=self.tick(self.b,self.cb,1.02,[local])
        self.assertEqual(result['a'][0][0],1)
        self.assertEqual(result['a'][0][2:2+w.INSTANCE],opening[:w.INSTANCE])
        # Room departure immediately drops the stale opening checkpoint.
        self.cb['players'][1]['roomId']=0x12f
        result,_=self.tick(self.b,self.cb,1.03,[local])
        self.assertEqual(result['a'],[])
        self.assertEqual(self.b.owners[0],2)
        closing=copy.deepcopy(opening)
        closing[3]=0;closing[11]=1200;closing[29]=22
        self.send(self.b,self.cb,self.a,self.ca,1.1,[closing])
        result,_=self.tick(self.a,self.ca,1.12,rows=[])
        # Receivers without a placed actor defer binding until native load.
        self.assertEqual(result['a'],[])
        # A traveller's native input takes priority over passive closing.
        result,_=self.tick(self.a,self.ca,1.13,[opening])
        self.assertEqual(self.a.owners[0],1)

    def test_door_close_and_stop_edges_bypass_steady_interval(self):
        r=row(entity=0x23c,kind=5);r[11]=1500;r[29]=20
        _,packets=self.tick(self.a,self.ca,1,[r])
        self.a.sent(packets,True,1)
        r[29]=22
        _,blocked=self.tick(self.a,self.ca,1.02,[r])
        self.assertFalse(blocked)
        _,closing=self.tick(self.a,self.ca,1.06,[r])
        self.assertTrue(closing)
        self.a.sent(closing,True,1.06)
        r[11]=1400
        _,motion=self.tick(self.a,self.ca,1.12,[r])
        self.assertFalse(motion)
        r[11]=0;r[29]=4
        _,closed=self.tick(self.a,self.ca,1.12,[r])
        self.assertTrue(closed)

    def test_old_world_version_is_ineligible(self):
        peer=self.cb['players'][1]
        for version in range(w.VERSION):
            peer['worldSync'][0]=version
            self.assertIsNone(self.b._peer(self.cb,1))

    def test_npc_continuation_edge_and_late_checkpoint(self):
        r=row(0,0x2c4,1);r[39:48]=[8,-20,30,60,2,2,0,50,1]
        self.send(self.a,self.ca,self.b,self.cb,1,[r])
        r[39]=9
        _,packets=self.tick(self.a,self.ca,1.02,[r]);self.assertFalse(packets)
        self.send(self.a,self.ca,self.b,self.cb,1.06,[r])
        state,_=self.tick(self.b,self.cb,1.08,[row(0,0x2c4,1)])
        self.assertEqual(state['a'][0][2:2+w.INSTANCE],r[:w.INSTANCE])
        r[4]=100
        _,packets=self.tick(self.a,self.ca,1.12,[r]);self.assertFalse(packets)

    def crane_setup(self,a_mask=0,b_mask=0,t=0.):
        self.ca['room']=self.cb['room']=0x31
        self.a.reset();self.b.reset()
        self.crane_a=crane_row(mask=a_mask);self.crane_b=crane_row(mask=b_mask)
        self.tick(self.a,self.ca,t,[self.crane_a])
        self.tick(self.b,self.cb,t,[self.crane_b])
        self.advertise()
        return self.crane_a,self.crane_b

    def crane_entry(self,transport,context,now,row_value):
        result,_=self.tick(transport,context,now,[row_value])
        self.assertEqual(len(result['a']),1)
        return result['a'][0]

    def test_crane_pad_inputs_aggregate_on_owner_and_replica(self):
        a_row,b_row=self.crane_setup(a_mask=1,b_mask=2)
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        result,_=self.tick(self.b,self.cb,1.02,[b_row])
        offer=result['a'][0]
        self.assertEqual(offer[0],1)
        self.assertEqual(offer[w.DELIVERY_ROW+w.CRANE_INPUT],2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        self.assertEqual(offer[w.DELIVERY_ROW+w.INSTANCE],b_row[w.INSTANCE])
        # The owner also gets its own row back with the shared aggregate.
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        echo=self.crane_entry(self.a,self.ca,1.06,a_row)
        self.assertEqual(echo[0],1)
        self.assertEqual(echo[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        self.assertEqual(echo[w.DELIVERY_ROW+w.RECEIPT],0)
        self.assertEqual(echo[w.DELIVERY_ROW+w.INSTANCE],a_row[w.INSTANCE])

    def test_crane_same_pad_press_is_one_bit_and_release_keeps_holder(self):
        a_row,b_row=self.crane_setup(a_mask=1,b_mask=1)
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        result,_=self.tick(self.b,self.cb,1.02,[b_row])
        # Both players holding pad0 is one bit, not a doubled mask.
        self.assertEqual(result['a'][0][w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.06,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)
        # B lets go while A still holds: the aggregate does not clear.
        self.send(self.b,self.cb,self.a,self.ca,1.1,[crane_row(mask=0)])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.12,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)
        # A lets go too: nothing is held any more.
        released=crane_row(mask=0)
        self.assertEqual(self.crane_entry(self.a,self.ca,1.14,released)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],0)

    def test_crane_release_and_pause_drop_only_that_contribution(self):
        a_row,b_row=self.crane_setup(a_mask=1,b_mask=2)
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.06,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        # B lets go of pad1 while A keeps pad0 held.
        self.send(self.b,self.cb,self.a,self.ca,1.1,[crane_row(mask=0)])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.12,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)
        # A paused peer publication carries no pad input at all.
        paused_peer=crane_row(mask=2,pause=1)
        self.send(self.b,self.cb,self.a,self.ca,1.2,[paused_peer])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.22,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)
        # A paused replica keeps the remote press and its own local word.
        offer=self.crane_entry(self.b,self.cb,1.24,paused_peer)
        self.assertEqual(offer[0],1)
        self.assertEqual(offer[w.DELIVERY_ROW+w.CRANE_INPUT],2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)

    def test_crane_peer_expiry_removes_input(self):
        a_row,b_row=self.crane_setup(a_mask=1,b_mask=2)
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.06,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        self.assertEqual(self.crane_entry(self.a,self.ca,2.0,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        self.assertEqual(self.crane_entry(self.a,self.ca,3.0,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)

    def test_crane_room_change_and_disconnect_remove_input(self):
        a_row,b_row=self.crane_setup(a_mask=1,b_mask=2)
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        self.assertEqual(self.crane_entry(self.a,self.ca,1.06,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        self.ca['players'][2]['roomId']=0x32
        self.assertEqual(self.crane_entry(self.a,self.ca,1.08,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)
        # Re-entering publishes a fresh contribution on the input edge.
        self.ca['players'][2]['roomId']=0x31
        self.send(self.b,self.cb,self.a,self.ca,2.2,[crane_row(mask=3)])
        self.assertEqual(self.crane_entry(self.a,self.ca,2.22,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        self.ca['players'][2]['online']=False
        self.assertEqual(self.crane_entry(self.a,self.ca,2.24,a_row)[w.DELIVERY_ROW+w.CRANE_AGGREGATE],1)

    def test_crane_bootstrap_offer_keeps_local_input_and_aggregate(self):
        a_row,b_row=self.crane_setup(a_mask=1,b_mask=2)
        self.b.reset()
        self.tick(self.b,self.cb,.5,[]);self.advertise()
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        fresh=crane_row(mask=2);fresh[w.INSTANCE]=77
        result,_=self.tick(self.b,self.cb,1.02,[fresh])
        self.assertNotIn(0,self.b.established)
        offer=[entry for entry in result['a'] if entry[0]==1][0]
        self.assertTrue(offer[w.DELIVERY_ROW+w.RECEIPT] & w.BOOTSTRAP)
        self.assertEqual(offer[w.DELIVERY_ROW+w.INSTANCE],77)
        self.assertEqual(offer[w.DELIVERY_ROW+w.CRANE_INPUT],2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.CRANE_AGGREGATE],3)
        # The receipt acknowledges the offer without touching the input words.
        applied=offer[w.DELIVERY_ROW:]
        self.tick(self.b,self.cb,1.04,[applied])
        self.assertIn(0,self.b.established)

    def test_crane_input_is_local_to_room_0x31(self):
        self.ca['room']=self.cb['room']=0x12e
        r=crane_row(mask=3)
        self.a.reset();self.b.reset()
        self.tick(self.a,self.ca,0,[r]);self.tick(self.b,self.cb,0,[r]);self.advertise()
        _,packets=self.tick(self.a,self.ca,1,[r])
        self.assertEqual(packets[0]['u'],[])
        bad=copy.deepcopy(packets[0]);bad['u']=[[0,1]]
        self.assertFalse(self.b.receive(self.cb,bad,1.01))

    def test_crane_wire_strips_local_and_aggregate_words(self):
        a_row,_=self.crane_setup(a_mask=3)
        _,packets=self.tick(self.a,self.ca,1,[a_row])
        wire=packets[0]['a'][0]
        self.assertEqual(wire[w.CRANE_INPUT],0)
        self.assertEqual(wire[w.CRANE_AGGREGATE],0)
        self.assertEqual(packets[0]['u'],[[0,3]])
        for column in (w.CRANE_INPUT,w.CRANE_AGGREGATE):
            bad=copy.deepcopy(packets[0]);bad['a'][0][column]=1
            self.assertFalse(self.b.receive(self.cb,bad,1.01))

    def test_crane_input_edge_uses_existing_floor(self):
        a_row,_=self.crane_setup(a_mask=0)
        _,packets=self.tick(self.a,self.ca,1,[a_row]);self.a.sent(packets,True,1)
        pressed=crane_row(mask=1)
        _,blocked=self.tick(self.a,self.ca,1.02,[pressed]);self.assertFalse(blocked)
        _,edge=self.tick(self.a,self.ca,1.06,[pressed]);self.assertTrue(edge)
        self.assertEqual(edge[0]['u'],[[0,1]])
        self.assertEqual(edge[0]['a'][0][w.CRANE_INPUT],0)
        self.assertEqual(edge[0]['a'][0][w.CRANE_AGGREGATE],0)

    def test_crane_input_field_rejects_malformed_and_mismatched_parts(self):
        self.ca['room']=self.cb['room']=0x31
        self.a.reset();self.b.reset()
        rows=[row(i) for i in range(30)];rows[3]=crane_row(index=3,mask=1)
        self.tick(self.a,self.ca,0,rows);self.tick(self.b,self.cb,0,rows);self.advertise()
        _,packets=self.tick(self.a,self.ca,1,rows)
        self.assertEqual(len(packets),2)
        self.assertEqual(packets[0]['u'],[[3,1]])
        self.assertEqual(packets[1]['u'],[[3,1]])
        for value in (None,[[3,4]],[[3,-1]],[[3,1],[3,1]],[[3]],[[3,1,2]],'m',[[3,True]],[[256,1]],[[3,1],[9,0]]):
            bad=copy.deepcopy(packets[0]);bad['u']=value
            self.assertFalse(self.b.receive(self.cb,bad,1.01))
        missing=copy.deepcopy(packets[0]);missing.pop('u')
        self.assertFalse(self.b.receive(self.cb,missing,1.01))
        self.assertFalse(self.b.peers);self.assertFalse(self.b.pending)
        # Mismatched chunks are rejected atomically and leave no pending part.
        self.assertTrue(self.b.receive(self.cb,packets[0],1.02))
        mismatched=copy.deepcopy(packets[1]);mismatched['u']=[[3,2]]
        self.assertFalse(self.b.receive(self.cb,mismatched,1.03))
        self.assertNotIn(1,self.b.peers)
        self.assertTrue(self.b.receive(self.cb,packets[1],1.04))
        self.assertEqual(self.b.peers[1]['u'],[[3,1]])
        self.assertEqual(len(self.b.peers[1]['rows']),30)

    def test_crane_batch_keeps_existing_packet_budget(self):
        self.ca['room']=self.cb['room']=0x31
        self.a.reset()
        rows=[row(i) for i in range(256)];rows[7]=crane_row(index=7,mask=3)
        _,packets=self.tick(self.a,self.ca,1,rows)
        self.assertEqual(len(packets),w.MAX_PARTS)
        self.assertTrue(all(p['u']==[[7,3]] for p in packets))
        self.assertTrue(all(len(p['a'])<=w.ROWS_PER_PACKET for p in packets))
        self.assertTrue(all(len(json.dumps(p,separators=(',',':')).encode())+1<=w.PACKET_BYTES for p in packets))

    def shutter_setup(self,a_row=None,b_row=None,t=0.):
        self.ca['room']=self.cb['room']=w.SHUTTER_ROOM
        self.a.reset();self.b.reset()
        self.shutter_a=shutter_row() if a_row is None else a_row
        self.shutter_b=shutter_row() if b_row is None else b_row
        self.tick(self.a,self.ca,t,[self.shutter_a])
        self.tick(self.b,self.cb,t,[self.shutter_b])
        self.advertise()
        return self.shutter_a,self.shutter_b

    def test_shutter_late_entrant_bootstraps_before_publishing(self):
        a_row,b_row=self.shutter_setup()
        self.b.reset()
        self.tick(self.b,self.cb,.5,[]);self.advertise()
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        fresh=shutter_row();fresh[w.INSTANCE]=77
        result,packets=self.tick(self.b,self.cb,1.02,[fresh])
        self.assertNotIn(0,self.b.established)
        offer=[entry for entry in result['a'] if entry[0]==1][0]
        self.assertTrue(offer[w.DELIVERY_ROW+w.RECEIPT] & w.BOOTSTRAP)
        self.assertEqual(offer[w.DELIVERY_ROW+w.INSTANCE],77)
        self.assertEqual(offer[w.DELIVERY_ROW+1],w.SHUTTER_ENTITY)
        self.assertTrue(all(not p['a'] for p in packets))
        # Only the applied receipt promotes the newcomer to a publisher.
        applied=offer[w.DELIVERY_ROW:]
        self.tick(self.b,self.cb,1.04,[applied])
        self.assertIn(0,self.b.established)

    def test_shutter_emission_ordinal_beats_stale_peer(self):
        a_row,b_row=self.shutter_setup(a_row=shutter_row(ordinal=5),b_row=shutter_row(ordinal=9))
        self.assertLess(w.controller_progress(a_row),w.controller_progress(b_row))
        # The lower-ID client publishes first, then cedes to the newer ordinal.
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        result,packets=self.tick(self.b,self.cb,1.02,[b_row])
        self.assertFalse(result['a'])
        self.assertEqual(self.b.owners[0],2)
        self.assertTrue(packets)
        self.assertEqual(packets[0]['a'][0][20],9)
        self.send(self.b,self.cb,self.a,self.ca,1.04,[b_row])
        result,_=self.tick(self.a,self.ca,1.06,[a_row])
        self.assertEqual(self.a.owners[0],2)
        offer=[entry for entry in result['a'] if entry[0]==2][0]
        self.assertEqual(offer[w.DELIVERY_ROW+20],9)

    def test_shutter_pause_hands_off_to_unpaused_peer(self):
        a_row,b_row=self.shutter_setup(a_row=shutter_row(ordinal=4),b_row=shutter_row(ordinal=4))
        self.send(self.a,self.ca,self.b,self.cb,1,[a_row])
        result,_=self.tick(self.b,self.cb,1.02,[b_row])
        self.assertEqual(result['a'][0][0],1)
        # The incumbent pauses; the unpaused peer takes the checkpoint over.
        paused=list(a_row);paused[38]=1
        a_packets=self.send(self.a,self.ca,self.b,self.cb,1.1,[paused])
        self.assertEqual(a_packets[0]['z'],'01'+'00'*31)
        result,packets=self.tick(self.b,self.cb,1.12,[b_row])
        self.assertFalse(result['a'])
        self.assertEqual(self.b.owners[0],2)
        self.assertEqual(packets[0]['a'][0][38],0)
        self.assertEqual(packets[0]['z'],'00'*32)

    def test_shutter_forbidden_row_bounds_and_room(self):
        base=shutter_row()
        self.assertTrue(w.row_valid(base))
        forbidden={1:0x162,2:0,3:1,10:13,14:1,15:-1,16:2,17:-2,18:0,19:2,20:-1,
                   21:1,25:1,29:4,30:1,37:1,38:2,39:1,47:1}
        for column,value in forbidden.items():
            altered=list(base);altered[column]=value
            self.assertFalse(w.row_valid(altered),(column,value))
        self.assertTrue(w.row_valid(shutter_row(timer=-1,ordinal=0x7fffffff)))
        self.assertFalse(w.row_valid(shutter_row(timer=91)))
        self.assertFalse(w.row_valid(shutter_row(ordinal=0x80000000)))
        # Kind 8 is only legal in room 0xB2, on both sides of the wire.
        self.ca['room']=self.cb['room']=0x12e
        self.a.reset();self.b.reset()
        self.tick(self.a,self.ca,0,[]);self.tick(self.b,self.cb,0,[]);self.advertise()
        shr=shutter_row()
        result,packets=self.tick(self.a,self.ca,1,[shr])
        self.assertFalse(result['a']);self.assertFalse(packets)
        _,packets=self.tick(self.a,self.ca,2.2,[row()])
        self.assertTrue(packets)
        bad=copy.deepcopy(packets[0]);bad['a'][0]=list(shr)
        self.assertFalse(self.b.receive(self.cb,bad,2.21))
        self.assertTrue(self.b.receive(self.cb,packets[0],2.21))

    def test_shutter_phase_and_emission_edges_use_existing_floor(self):
        self.ca['room']=self.cb['room']=w.SHUTTER_ROOM
        t=w.WorldTransport()
        r=shutter_row(phase=1,ordinal=3)
        _,packets=self.tick(t,self.ca,1,[r]);self.assertTrue(packets);t.sent(packets,True,1)
        # A steady shutter checkpoint keeps the normal idle heartbeat.
        _,packets=self.tick(t,self.ca,1.5,[r]);self.assertFalse(packets)
        _,packets=self.tick(t,self.ca,2.1,[r]);self.assertTrue(packets)
        t.sent(packets,True,2.1)
        # Either the phase or the emission ordinal bypasses to the 50 ms floor.
        r[18]=2
        _,packets=self.tick(t,self.ca,2.13,[r]);self.assertFalse(packets)
        _,packets=self.tick(t,self.ca,2.16,[r]);self.assertTrue(packets)
        self.assertEqual(packets[0]['a'][0][18],2)
        t.sent(packets,True,2.16)
        r[20]=9
        _,packets=self.tick(t,self.ca,2.18,[r]);self.assertFalse(packets)
        _,packets=self.tick(t,self.ca,2.23,[r]);self.assertTrue(packets)
        self.assertEqual(packets[0]['a'][0][20],9)

    def test_switch_hit_progress_late_entry_and_handoff(self):
        idle=switch_row()
        self.send(self.a,self.ca,self.b,self.cb,1,[idle])
        hit=list(idle);hit[3]=1
        self.send(self.b,self.cb,self.a,self.ca,1.1,[hit])
        result,_=self.tick(self.a,self.ca,1.12,[idle])
        self.assertEqual(result['a'][0][0],2)
        # The higher-ID hitter advances while the former owner still has
        # an earlier busy phase. Progress wins this delayed round trip.
        older=switch_row(2)
        self.send(self.a,self.ca,self.b,self.cb,1.2,[older])
        pressed=switch_row(5)
        result,packets=self.tick(self.b,self.cb,1.22,[pressed])
        self.assertFalse(result['a'])
        self.assertTrue(packets)
        self.send(self.b,self.cb,self.a,self.ca,1.23,[pressed])
        result,_=self.tick(self.a,self.ca,1.24,[older])
        self.assertEqual(result['a'][0][2:2+w.INSTANCE],pressed[:w.INSTANCE])
        # An idle lower-ID re-entrant restores the completed phase.
        self.a.reset();self.tick(self.a,self.ca,1.25,[idle],visit=2);self.advertise()
        self.send(self.b,self.cb,self.a,self.ca,2.3,[pressed])
        result,_=self.tick(self.a,self.ca,2.31,[idle],visit=2)
        self.assertEqual(result['a'][0][2:2+w.INSTANCE],pressed[:w.INSTANCE])
        # Equal progress can hand off to an unpaused peer.
        self.send(self.a,self.ca,self.b,self.cb,2.4,[pressed],visit=2)
        paused=list(pressed);paused[38]=1
        packets=self.send(self.a,self.ca,self.b,self.cb,2.5,[paused],visit=2)
        self.assertFalse(packets[0]['a'])
        self.assertEqual(packets[0]['z'],'01'+'00'*31)
        result,_=self.tick(self.b,self.cb,2.51,[pressed])
        self.assertFalse(result['a'])

    def test_switch_and_mechanism_phase_edges_use_existing_floor(self):
        for entity,kind,start,end in ((0x226,6,1,5),(0x324,2,53,55),(0x326,2,56,59)):
            t=w.WorldTransport()
            r=switch_row() if kind==6 else row(0,entity,kind)
            r[18]=start
            _,packets=self.tick(t,self.ca,1,[r]);t.sent(packets,True,1)
            r[18]=end;r[3]=1;r[21]=1
            _,packets=self.tick(t,self.ca,1.02,[r]);self.assertFalse(packets)
            _,packets=self.tick(t,self.ca,1.06,[r]);self.assertTrue(packets)
            t.sent(packets,True,1.06)
            r[4]=100
            _,packets=self.tick(t,self.ca,1.12,[r]);self.assertFalse(packets)

    def test_save_aware_late_constructor_cannot_finish_remote_motion(self):
        moving=row(0,0x326,2);moving[18]=57;moving[3]=moving[21]=1
        loaded=list(moving);loaded[18]=59;loaded[24]=1
        idle=list(moving);idle[18]=56;idle[3]=idle[21]=0
        self.assertGreater(w.controller_progress(loaded),w.controller_progress(idle))
        self.send(self.b,self.cb,self.a,self.ca,1,[moving])
        result,_=self.tick(self.a,self.ca,1.02,[loaded])
        self.assertEqual(result['a'][0][2:2+w.INSTANCE],moving[:w.INSTANCE])
        complete=list(loaded);complete[24]=0
        result,packets=self.tick(self.a,self.ca,1.1,[complete])
        self.assertFalse(result['a'])
        self.assertEqual(packets[0]['a'],[complete])

    def test_linked_activation_handoff_and_cyclic_phase(self):
        self.assertLess(w.controller_progress(linked_row(False,60)),
                        w.controller_progress(linked_row(False,61)))
        self.assertLess(w.controller_progress(linked_row(False,61)),
                        w.controller_progress(linked_row(False,62)))
        for fire in (False,True):
            self.setUp()
            active=linked_row(fire);active[4]=12345
            self.send(self.b,self.cb,self.a,self.ca,1,[active])
            waiting=linked_row(fire,64 if fire else 60)
            result,_=self.tick(self.a,self.ca,1.02,[waiting])
            self.assertEqual(result['a'][0][2:2+w.INSTANCE],active[:w.INSTANCE])
            opposite=list(active);opposite[18]+=1
            self.assertEqual(w.controller_progress(active),w.controller_progress(opposite))
            # Culling retains activation, but releases an old interaction lease.
            active[38]=1;active[3]=0
            self.send(self.b,self.cb,self.a,self.ca,1.3,[active])
            result,packets=self.tick(self.a,self.ca,1.4,[opposite])
            self.assertFalse(result['a']);self.assertEqual(packets[0]['a'],[opposite])

    def test_fire_hit_and_phase_edges(self):
        idle=linked_row(True)
        hit=linked_row(True,67,1);hit[3]=1
        self.send(self.b,self.cb,self.a,self.ca,1,[hit])
        result,_=self.tick(self.a,self.ca,1.02,[idle])
        self.assertEqual(result['a'][0][2:2+w.INSTANCE],hit[:w.INSTANCE])
        t=w.WorldTransport()
        _,packets=self.tick(t,self.ca,1,[hit]);t.sent(packets,True,1)
        hit[18]=68;hit[25]=0
        _,packets=self.tick(t,self.ca,1.02,[hit]);self.assertFalse(packets)
        _,packets=self.tick(t,self.ca,1.06,[hit]);self.assertTrue(packets)

    def entrant(self,paused=False):
        live=row();live[4]=77700;live[38]=int(paused)
        self.a.reset()
        fresh=row();fresh[w.INSTANCE]=100
        self.tick(self.a,self.ca,.5,[fresh]);self.advertise()
        return live,fresh

    def test_lower_id_entrant_cannot_publish_before_native_application(self):
        live,fresh=self.entrant()
        fresh[3]=1  # Being loaded under a rider cannot reset the incumbent.
        packets=self.send(self.a,self.ca,self.b,self.cb,.6,[fresh])
        self.assertFalse(w.bit(bytes.fromhex(packets[0]['h']),0))
        result,_=self.tick(self.b,self.cb,.62,[live])
        self.assertFalse(result['a']);self.assertEqual(self.b.owners[0],2)
        self.send(self.b,self.cb,self.a,self.ca,.7,[live])
        offered,packets=self.tick(self.a,self.ca,.72,[fresh])
        receipt=offered['a'][0][2:]
        self.assertEqual(receipt[4],77700)
        self.assertEqual(receipt[w.INSTANCE],100)
        self.assertTrue(receipt[w.RECEIPT] & w.BOOTSTRAP)
        self.assertEqual(self.a.owners[0],2)
        self.assertTrue(all(not p['a'] for p in packets))
        repeated,_=self.tick(self.a,self.ca,.73,[fresh])
        self.assertEqual(repeated['a'][0][-1],receipt[w.RECEIPT])
        receipt[3]=1
        result,packets=self.tick(self.a,self.ca,.8,[receipt])
        self.assertFalse(result['a']);self.assertEqual(self.a.owners[0],1)
        self.assertEqual(packets[0]['a'][0][4],77700)
        self.assertEqual(packets[0]['a'][0][w.INSTANCE:],[0,0])
        self.assertTrue(w.bit(bytes.fromhex(packets[0]['h']),0))

    def test_paused_incumbent_bootstraps_before_handoff(self):
        live,fresh=self.entrant(paused=True)
        self.send(self.b,self.cb,self.a,self.ca,1,[live])
        offered,_=self.tick(self.a,self.ca,1.02,[fresh])
        self.assertEqual(self.a.owners[0],2)
        applied=offered['a'][0][2:];applied[38]=0
        result,_=self.tick(self.a,self.ca,1.04,[applied])
        self.assertFalse(result['a']);self.assertEqual(self.a.owners[0],1)

    def test_wrong_instance_old_scope_and_failed_apply_do_not_acknowledge(self):
        live,fresh=self.entrant()
        self.send(self.b,self.cb,self.a,self.ca,1,[live])
        offered,_=self.tick(self.a,self.ca,1.02,[fresh])
        old=offered['a'][0][2:]
        replaced=list(old);replaced[w.INSTANCE]+=1
        offered,_=self.tick(self.a,self.ca,1.04,[replaced])
        self.assertEqual(self.a.owners[0],2)
        self.assertNotEqual(offered['a'][0][-1],old[w.RECEIPT])
        applied=offered['a'][0][2:]
        self.tick(self.a,self.ca,1.06,[applied])
        self.assertEqual(self.a.owners[0],1)
        applied[w.RECEIPT]=w.APPLY_FAILED
        offered,_=self.tick(self.a,self.ca,1.08,[applied])
        self.assertEqual(self.a.owners[0],2)
        self.assertTrue(offered['a'][0][-1] & w.BOOTSTRAP)
        latest=offered['a'][0][2:]
        self.a.reset();self.tick(self.a,self.ca,1.1,[latest],visit=2);self.advertise()
        self.send(self.b,self.cb,self.a,self.ca,1.3,[live])
        offered,_=self.tick(self.a,self.ca,1.32,[latest],visit=2)
        self.assertEqual(self.a.owners[0],2)
        self.assertNotEqual(offered['a'][0][-1],latest[w.RECEIPT])

    def test_newer_checkpoint_invalidates_unapplied_offer(self):
        live,fresh=self.entrant()
        self.send(self.b,self.cb,self.a,self.ca,1,[live])
        old,_=self.tick(self.a,self.ca,1.02,[fresh])
        live[4]=88800
        self.send(self.b,self.cb,self.a,self.ca,1.3,[live])
        new,_=self.tick(self.a,self.ca,1.32,[old['a'][0][2:]])
        self.assertEqual(self.a.owners[0],2)
        self.assertNotEqual(old['a'][0][-1],new['a'][0][-1])
        self.assertEqual(new['a'][0][6],88800)

    def test_former_owner_presence_does_not_offer_old_cached_pose(self):
        live,fresh=self.entrant()
        packets=self.send(self.b,self.cb,self.a,self.ca,1,[live])
        presence_only=copy.deepcopy(packets[0]);presence_only['q']+=1;presence_only['a']=[]
        self.assertTrue(self.a.receive(self.ca,presence_only,1.2))
        result,_=self.tick(self.a,self.ca,1.22,[fresh])
        self.assertFalse(result['a'])
        self.assertEqual(self.a.owners[0],2)
        self.assertNotIn(0,self.a.established)
        # A new published checkpoint completes the offer again.
        live[4]=99900
        self.b.sequence=presence_only['q']
        self.send(self.b,self.cb,self.a,self.ca,1.4,[live])
        result,_=self.tick(self.a,self.ca,1.42,[fresh])
        self.assertEqual(result['a'][0][6],99900)

    def test_bootstrap_envelope_is_atomic_and_private_receipts_stay_local(self):
        _,packets=self.tick(self.a,self.ca,1,[row(i) for i in range(256)])
        first=packets[0]
        for field,value in [('h','ff'*32),('h','bad')]:
            bad=copy.deepcopy(first);bad[field]=value
            if value=='ff'*32:bad['p']='01'+'00'*31
            self.assertFalse(self.b.receive(self.cb,bad,1.01))
        for column in (w.INSTANCE,w.RECEIPT):
            bad=copy.deepcopy(first);bad['a'][0][column]=123
            self.assertFalse(self.b.receive(self.cb,bad,1.01))
        self.assertTrue(self.b.receive(self.cb,first,1.01))
        second=copy.deepcopy(packets[1]);second['h']='00'*32
        self.assertFalse(self.b.receive(self.cb,second,1.02))
        for packet in packets[1:]:self.assertTrue(self.b.receive(self.cb,packet,1.03))
        self.assertEqual(len(self.b.peers[1]['published']),256)

    def test_unknown_member_bootstrap_wait_is_bounded_by_liveness(self):
        self.a.reset()
        unknown=self.ca['players'][2];unknown.pop('worldSync')
        _,packets=self.tick(self.a,self.ca,1)
        self.assertEqual(packets[0]['h'],'00'*32)
        _,packets=self.tick(self.a,self.ca,2.6)
        self.assertEqual(packets[0]['h'],'01'+'00'*31)

    def test_handshake_roster_must_arrive_before_world_authority(self):
        self.ca['worldRosterReady']=False
        result,packets=self.tick(self.a,self.ca,1)
        self.assertFalse(result['a']);self.assertFalse(packets);self.assertIsNone(self.a.scope)


class CodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        lib=Path(cls.temp.name)/'world.dylib'
        subprocess.run(['cc','-shared','-fPIC','-std=c99','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I'+str(ROOT/'include'),
                        str(ROOT/'src/world/anchor_world_codec.c'),
                        str(ROOT/'src/utils/string_utils.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.Row=ctypes.c_int*w.WORDS;cls.StatusRow=ctypes.c_int*(w.WORDS+2)
        cls.lib.anchor_world_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def decode(self,value):
        rows=(self.StatusRow*256)();count=ctypes.c_uint();dead=(ctypes.c_ubyte*32)()
        ok=self.lib.anchor_world_decode(value.encode(),rows,ctypes.byref(count),dead)
        return ok,rows,count.value,bytes(dead)

    def test_counterweight_codec_parity_and_atomic_round_trip(self):
        cw=w.counterweight
        for index,pose in cw.POSES.items():
            base=[0]*w.WORDS
            base[:3]=[index,cw.ENTITY,cw.KIND]
            base[4:10]=pose;base[10:16]=[base[5]]*6;base[16]=63
            self.assertTrue(w.row_valid(base))
            good=json.dumps({'a':[[2,0]+base],'d':'00'*32})
            ok,result,count,_=self.decode(good)
            self.assertTrue(ok);self.assertEqual(count,1)
            self.assertEqual(list(result[0])[2:],base)
            for col in range(w.WORDS):
                for value in (-3276801,-18001,-18000,-8000,-1,0,1,2,3,4,8,13,
                              14,15,16,31,32,63,64,256,512,800,1600,2400,0x7fffffff):
                    altered=list(base);altered[col]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(index,col,value))

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

    def test_npc_callback_route_and_scalar_validation_parity(self):
        for phase in range(1,23):
            model=NPC_MODELS[phase-1]
            entity=0x2c3 if phase==7 else model
            r=row(0,entity,1);r[39]=phase
            if model==0x2c4:r[43:48]=[2,2,1,9999,2]
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for column in range(39,w.WORDS):
                for value in (-32769,-2,0,1,2,3,4,6,8,16,18,24,26,32768):
                    altered=list(r);altered[column]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(phase,column,value))

    def test_switch_and_mechanism_validation_parity(self):
        examples=[switch_row(p,m) for p in range(1,6) for m in (0,1)]
        for entity,first,last in ((0x324,53,55),(0x326,56,59)):
            for phase in range(first,last+1):
                r=row(0,entity,2);r[18]=phase;r[3]=int(phase>first)
                examples.append(r)
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for col in (1,2,3,10,17,18,19,20,21,22,23,24,25,29,39):
                for value in (-1,0,1,2,5,7,11,53,55,56,59,799,800,2047,2048):
                    altered=list(r);altered[col]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(r[1],r[18],col,value))

    def test_linked_platform_validation_parity(self):
        examples=[linked_row(False,p) for p in range(60,64)]
        examples += [linked_row(True,64),linked_row(True,65),linked_row(True,66,5),
                     linked_row(True,67,1)]
        examples += [linked_row(True,68,s) for s in range(1,5)]
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for col in range(w.WORDS):
                for value in (-32769,-1,0,1,2,3,5,10,40,60,64,65,67,68,255,
                              256,512,1024,2047,2048,32768):
                    altered=list(r);altered[col]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(r[1],r[18],col,value))

    def test_native_rejects_malformed_rows_and_trailing_data(self):
        good=json.dumps({'a':[[1,0]+row()],'d':'00'*32})
        for data in [good+'x',good[:-1],good.replace('"d"','"bad"'),
                     json.dumps({'a':[[1,0]+row()]*2,'d':'00'*32}),
                     json.dumps({'a':[[1,0]+[256]+row()[1:]],'d':'00'*32})]:
            self.assertFalse(self.decode(data)[0])
        for n in [-(1<<31)-1,1<<31,True,float('nan')]:
            r=row();r[4]=n
            self.assertFalse(self.decode(json.dumps({'a':[[1,0]+r],'d':'00'*32}))[0])

    def test_physics_validation_parity(self):
        for phase in range(69,76):
            r=physics_row(phase,200 if phase==73 else 0,8388607)
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for col in range(w.WORDS):
                for value in (-32769,-1,0,1,5,6,16,17,31,69,75,200,201,256,32768,8388607,8388608):
                    altered=list(r);altered[col]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(phase,col,value))

    def test_crane_validation_parity(self):
        examples=[crane_row(mask=m) for m in range(4)]
        examples+=[crane_row(phase=p,reward=r) for r in (1,5,11,13) for p in (1,10,19)]
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for col in range(w.WORDS):
                for value in (-32769,-1,0,1,2,5,6,7,10,11,12,13,19,20,60,61,255,
                              256,1023,1024,19407,19408,32767,32768,2147483647):
                    altered=list(r);altered[col]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(r[18],r[37],col,value))

    def test_shutter_validation_parity(self):
        examples=[shutter_row(phase=p,ordinal=o) for p in (1,4) for o in (0,65535)]
        examples+=[shutter_row(timer=t,pause=1,playback=3) for t in (-1,90)]
        for r in examples:
            self.assertTrue(w.row_valid(r))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*r)))
            for col in range(w.WORDS):
                for value in (-32769,-1,0,1,2,3,4,5,7,8,13,14,15,90,91,255,256,
                              852,1023,1024,65535,65536,2147483647):
                    altered=list(r);altered[col]=value
                    self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     w.row_valid(altered),(r[18],r[20],col,value))

    def test_native_python_validation_agree(self):
        self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*row())))
        for column in range(w.WORDS):
            for value in [-2147483648,-32769,-1,0,1,163,256,1024,32768,2147483647]:
                r=row();r[column]=value
                self.assertEqual(bool(self.lib.anchor_world_row_valid(self.Row(*r))),w.row_valid(r),(column,value))


if __name__=='__main__':unittest.main()
