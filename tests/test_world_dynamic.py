import copy
import ctypes
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import anchor_world as w
import anchor_world_dynamic as d


def actor(serial=1,kind=2):
    r=[0]*d.WORDS
    r[:12]=[0,0,0,serial,0,0,kind,1,0x82,1,4,0]
    r[24:27]=[1000]*3
    r[32]=163
    r[42]={1:0,2:1,3:3,4:4,5:6}[kind]
    if kind==1:r[8:12]=[0x2bd,0x2bd,0,1]
    if kind==3:r[10]=3
    if kind==4:r[8:12]=[0x85,0x85,0,0]
    if kind==5:r[9:12]=[0x191,0,1]
    if kind in (2,3,4):r[64]=serial-1
    return r


def robot(ordinal=1,parent=3,base_y=0,pause=0,life=None,serial=None):
    r=[0]*d.WORDS
    r[:12]=[0,0,0,ordinal if serial is None else serial,0,
            d.LIVE if life is None else life,d.SHUTTER_ENEMY,
            parent,0xFC,0xFB,0,1]
    r[24:27]=[1000]*3
    r[32]=57
    r[42]=15
    r[45]=base_y
    r[64]=ordinal
    r[66]=pause
    return r


class DynamicTests(unittest.TestCase):
    def setUp(self):
        self.wa=w.WorldTransport();self.wb=w.WorldTransport()
        self.a=d.DynamicTransport(self.wa);self.b=d.DynamicTransport(self.wb)
        self.ca=dict(cid=1,session=10,team='default',connected=True,loaded=True,room=302,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        for t,c in ((self.wa,self.ca),(self.wb,self.cb)):
            t.update(c,302,42,1,[],'00'*32,0)
        for c,other,t in ((self.ca,self.cb,self.wb),(self.cb,self.ca,self.wa)):
            c['players'][other['cid']]=dict(online=True,isSaveLoaded=True,teamId='default',roomId=302,
                interactionSession=other['session'],worldSync=t.advertisement(other))
        self.a.update(self.ca,[],0);self.b.update(self.cb,[],0)
        for tx,c,rx,rc in ((self.wa,self.ca,self.wb,self.cb),(self.wb,self.cb,self.wa,self.ca)):
            _,packets=tx.update(c,302,42,1,[],'00'*32,0)
            for packet in packets:self.assertTrue(rx.receive(rc,packet,0))

    def send(self,t,c,rx,rc,rows,now):
        result,packets=t.update(c,rows,now)
        self.assertTrue(packets)
        for p in reversed(packets):self.assertTrue(rx.receive(rc,p,now+.001))
        t.sent(packets,True,now)
        return result['a'],packets

    def test_birth_late_entry_current_state_and_handoff(self):
        r=actor(kind=5);r[12]=12345;r[31]=42;r[43]=-3200
        state,packets=self.send(self.a,self.ca,self.b,self.cb,[r],1)
        result,_=self.b.update(self.cb,[],1.02)
        self.assertEqual(result['a'][0],state[0])
        self.assertEqual(result['a'][0][:4],[1,10,1,1])
        self.assertEqual(result['a'][0][31],42)
        self.cb['players'][1]['online']=False
        result,_=self.b.update(self.cb,state,1.1)
        self.assertEqual(result['a'][0][4],2)
        self.assertEqual(result['a'][0][:4],state[0][:4])
        self.assertFalse(self.b.receive(self.cb,packets[0],1.11))

    def test_puzzle_reward_slot_survives_takeover_and_collection(self):
        for kind in (2,3):
            self.setUp()
            r=actor(serial=17,kind=kind);r[8]=0x3d0;r[64]=0xff08
            state,_=self.send(self.a,self.ca,self.b,self.cb,[r],1+kind)
            duplicate=list(r);duplicate[3]=99
            result,_=self.b.update(self.cb,[duplicate],1.02+kind)
            matching=[x for x in result['a'] if x[6]==kind]
            self.assertEqual(len(matching),1)
            self.assertEqual(matching[0][:4],state[0][:4])
            claim=copy.deepcopy(matching);claim[0][5]=d.CLAIM
            self.send(self.b,self.cb,self.a,self.ca,claim,1.1+kind)
            committed,_=self.send(self.a,self.ca,self.b,self.cb,state,1.2+kind)
            result,_=self.b.update(self.cb,[duplicate],1.3+kind)
            same=[x for x in result['a'] if x[:4]==committed[0][:4]]
            self.assertEqual(len(same),1)
            self.assertEqual(same[0][5],d.REMOVED)

    def test_scripted_npc_births_deduplicate_across_allocation_orders(self):
        first=actor(17,1);first[8]=0x2bd;first[61:64]=[100,200,300]
        state,_=self.send(self.a,self.ca,self.b,self.cb,[first],1)
        second=list(first);second[3]=99
        result,_=self.b.update(self.cb,[second],1.1)
        self.assertEqual(len(result['a']),1)
        self.assertEqual(result['a'][0][:4],state[0][:4])
        self.assertEqual(result['a'][0][4],1)
        twin=list(second);twin[3]=100;twin[64]=1
        result,_=self.b.update(self.cb,[second,twin],1.2)
        self.assertEqual(len(result['a']),2)

    def test_placed_pickups_use_roster_identity_and_the_same_claim_arbiter(self):
        first=actor(17);first[7]=8;first[64]=0x7fffffff
        state,_=self.send(self.a,self.ca,self.b,self.cb,[first],1)
        second=list(first);second[3]=99
        result,_=self.b.update(self.cb,[second],1.1)
        self.assertEqual(len(result['a']),1)
        self.assertEqual(result['a'][0][:4],[0x7ffffffe,42,303,8])
        self.assertEqual(result['a'][0][4],1)

    def test_claim_round_trip_only_owner_decides_and_one_winner(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[actor()],1)
        claim=copy.deepcopy(state);claim[0][5]=d.CLAIM
        reply,_=self.send(self.b,self.cb,self.a,self.ca,claim,1.1)
        self.assertEqual(reply[0][5],d.LIVE)
        committed,_=self.send(self.a,self.ca,self.b,self.cb,state,1.2)
        self.assertEqual(committed[0][4:6],[2,d.REMOVED])
        late_claim=copy.deepcopy(state);late_claim[0][5]=d.CLAIM
        result,_=self.a.update(self.ca,late_claim,1.3)
        self.assertEqual(result['a'][0][4:6],[2,d.REMOVED])
        result,_=self.b.update(self.cb,claim,1.31)
        self.assertEqual(result['a'][0][4:6],[2,d.REMOVED])

    def test_two_claimants_converge_and_death_cannot_resurrect(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[actor()],1)
        claim=copy.deepcopy(state);claim[0][5]=d.CLAIM
        self.send(self.b,self.cb,self.a,self.ca,claim,1.1)
        committed,_=self.send(self.a,self.ca,self.b,self.cb,claim,1.2)
        self.assertEqual(committed[0][4:6],[1,d.REMOVED])
        result,_=self.b.update(self.cb,state,1.3)
        self.assertEqual(result['a'][0][4:6],[1,d.REMOVED])

    def test_silent_online_arbiter_does_not_allow_a_second_award(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[actor()],1)
        state[0][5]=d.CLAIM
        result,_=self.b.update(self.cb,state,4)
        self.assertEqual(result['a'][0][4:6],[1,d.LIVE])
        self.assertFalse(self.b.dead)
        self.cb['players'][1]['online']=False
        result,_=self.b.update(self.cb,state,4.1)
        self.assertEqual(result['a'][0][4:6],[2,d.REMOVED])

    def test_award_and_native_retirement_wait_for_successful_commit_send(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[actor()],1)
        state[0][5]=d.CLAIM
        result,packets=self.a.update(self.ca,state,1.1)
        self.assertEqual(result['a'][0][5],d.REMOVED)
        self.assertFalse(self.a.native_result(result)['a'])
        self.a.sent(packets,False,1.1)
        self.assertFalse(self.a.native_result(result)['a'])
        result,packets=self.a.update(self.ca,state,1.2)
        self.a.sent(packets,True,1.2)
        self.assertEqual(self.a.native_result(result)['a'][0][5],d.REMOVED)

    def test_multipart_atomic_reordered_duplicate_and_bounds(self):
        rows=[actor(i+1) for i in range(128)]
        _,packets=self.a.update(self.ca,rows,1)
        self.assertEqual(len(packets),11)
        for p in packets[:-1]:self.assertTrue(self.b.receive(self.cb,p,1))
        self.assertFalse(self.b.peers)
        self.assertFalse(self.b.receive(self.cb,packets[0],1))
        self.assertTrue(self.b.receive(self.cb,packets[-1],1))
        self.assertEqual(len(self.b.peers[1]['rows']),128)
        self.assertFalse(self.b.receive(self.cb,packets[-1],1))
        for p in packets:
            self.assertLessEqual(len(json.dumps(p,separators=(',',':')).encode())+1,d.PACKET_BYTES)

    def test_invalid_sender_session_room_and_row(self):
        _,packets=self.a.update(self.ca,[actor()],1)
        for change in ({'clientId':99},{'targetTeamId':'other'}, {'targetClientId':2},
                       {'addToQueue':True},{'m':[1,10,1,302,42]},{'parts':12},
                       {'a':[[1]*d.WORDS]},{'a':[packets[0]['a'][0]]*13}):
            p=copy.deepcopy(packets[0]);p.update(change)
            self.assertFalse(self.b.receive(self.cb,p,1),change)
        p=copy.deepcopy(packets[0]);p['a'][0][42]=14
        self.assertFalse(self.b.receive(self.cb,p,1))
        p=copy.deepcopy(packets[0]);p['a'][0][12]=float('nan')
        self.assertFalse(self.b.receive(self.cb,p,1))

    def test_room_reset_cadence_and_failed_send_floor(self):
        state,packets=self.a.update(self.ca,[actor()],1)
        self.a.sent(packets,False,1)
        self.assertFalse(self.a.update(self.ca,[actor()],1.01)[1])
        packets=self.a.update(self.ca,[actor()],1.06)[1]
        self.assertEqual(packets[0]['q'],2)
        self.a.sent(packets,True,1.06)
        self.assertFalse(self.a.update(self.ca,[actor()],1.2)[1])
        self.wa.update(self.ca,302,42,2,[],'00'*32,2)
        self.a.update(self.ca,[],2)
        self.assertFalse(self.a.dead)
        self.assertFalse(self.a.peers)

    def test_late_entrant_accepts_commit_with_a_different_winner(self):
        placed=actor();placed[64]=0x7fffffff
        state,_=self.send(self.a,self.ca,self.b,self.cb,[placed],1)
        claim=copy.deepcopy(state);claim[0][d.LIFE]=d.CLAIM
        self.send(self.b,self.cb,self.a,self.ca,claim,1.1)
        committed,packets=self.send(self.a,self.ca,self.b,self.cb,state,1.2)
        self.assertEqual(committed[0][d.OWNER],2)
        self.assertEqual(committed[0][d.COMMITTER],1)
        # Forget received hot state to model entering this occupied room late.
        self.b.reset();self.b.update(self.cb,[placed],1.3)
        for p in packets:self.assertTrue(self.b.receive(self.cb,p,1.4))
        result,_=self.b.update(self.cb,[placed],1.5)
        self.assertEqual(result['a'][0][d.LIFE],d.REMOVED)
        self.assertEqual(result['a'][0][d.OWNER],2)

    def test_survivor_forwards_removal_after_original_arbiter_leaves(self):
        placed=actor();placed[64]=0x7fffffff
        state,_=self.send(self.a,self.ca,self.b,self.cb,[placed],1)
        claim=copy.deepcopy(state);claim[0][d.LIFE]=d.CLAIM
        self.send(self.b,self.cb,self.a,self.ca,claim,1.1)
        committed,_=self.send(self.a,self.ca,self.b,self.cb,state,1.2)
        self.b.update(self.cb,claim,1.25)
        self.cb['players'][1]['online']=False
        wc=w.WorldTransport();tc=d.DynamicTransport(wc)
        cc=dict(self.cb,cid=3,session=30,players={
            2:dict(online=True,isSaveLoaded=True,teamId='default',roomId=302,
                   interactionSession=20,worldSync=self.wb.advertisement(self.cb))})
        wc.update(cc,302,42,1,[],'00'*32,1.3)
        tc.update(cc,[placed],1.3)
        _,packets=self.b.update(self.cb,committed,1.5)
        self.assertTrue(packets)
        for packet in packets:self.assertTrue(tc.receive(cc,packet,1.6))
        result,_=tc.update(cc,[placed],1.7)
        self.assertEqual(result['a'][0][d.LIFE],d.REMOVED)
        self.assertEqual(result['a'][0][d.OWNER],2)

    def test_worst_case_packet_size(self):
        r=[hi if abs(hi)>=abs(lo) else lo for lo,hi in d.BOUNDS]
        r[d.LIFE]=d.REMOVED
        # The probe only needs maximum-width scalars. Kind 6 is room scoped, so
        # keep a placed-safe kind; a removal skips every recipe anyway.
        r[d.KIND]=5
        r[74:]=[0]*9
        rows=[]
        for i in range(d.MAX_ACTORS):
            row=list(r);row[d.SERIAL]=i+1;rows.append(row)
        self.assertTrue(all(d.valid(row) for row in rows))
        # Exercise actual chunk/envelope byte sizes with maximum-width scalars.
        self.a.dead={d.key(row):row for row in rows}
        self.a.dead_dirty=set(self.a.dead)
        _,packets=self.a.update(self.ca,rows,1)
        self.assertEqual(len(packets),d.MAX_PARTS)
        self.assertLessEqual(max(len(json.dumps(p,separators=(',',':')).encode())+1
                                 for p in packets),d.PACKET_BYTES)

    def test_replica_motion_is_presence_cadence_but_claim_is_an_edge(self):
        state,_=self.send(self.a,self.ca,self.b,self.cb,[actor()],1)
        self.send(self.b,self.cb,self.a,self.ca,state,1.1)
        moving=copy.deepcopy(state);moving[0][12]+=100
        self.assertFalse(self.b.update(self.cb,moving,1.4)[1])
        moving[0][5]=d.CLAIM
        self.assertTrue(self.b.update(self.cb,moving,1.41)[1])

    def test_speculative_hazard_birth_waits_for_presence_then_retires_locally(self):
        self.wb.peers.clear()
        row=actor(kind=5)
        result,packets=self.b.update(self.cb,[row],.1)
        self.assertFalse(result['a'])
        self.assertTrue(all(not p['a'] for p in packets))
        # Empty parent presence makes the room leader the fallback authority.
        _,placed=self.wa.update(self.ca,302,42,1,[],'00'*32,.2)
        for packet in placed:self.assertTrue(self.wb.receive(self.cb,packet,.2))
        result,packets=self.b.update(self.cb,[row],.3)
        self.assertEqual(result['a'][0][d.OWNER:d.KIND],[0,d.REMOVED])
        self.assertEqual(result['a'][0][d.COMMITTER],0)
        self.assertTrue(all(not p['a'] for p in packets))

    def test_same_parent_loot_deduplicates_and_keeps_multiple_items(self):
        first,second=actor(1),actor(2)
        state,_=self.send(self.a,self.ca,self.b,self.cb,[first,second],1)
        other=copy.deepcopy(first);other[3]=99;other[12]=1234
        result,_=self.b.update(self.cb,[other],1.1)
        self.assertEqual(len(result['a']),2)
        self.assertEqual({tuple(r[:4]) for r in result['a']},{tuple(r[:4]) for r in state})

    def setup_room(self,room):
        # Rebuild both sides in another room. Shutter enemies are room scoped.
        self.wa.reset();self.wb.reset()
        self.a.reset();self.b.reset()
        self.ca['room']=self.cb['room']=room
        self.ca['players']={};self.cb['players']={}
        for t,c in ((self.wa,self.ca),(self.wb,self.cb)):
            t.update(c,room,42,1,[],'00'*32,0)
        for c,other,t in ((self.ca,self.cb,self.wb),(self.cb,self.ca,self.wa)):
            c['players'][other['cid']]=dict(online=True,isSaveLoaded=True,teamId='default',
                roomId=room,interactionSession=other['session'],
                worldSync=t.advertisement(other))
        self.a.update(self.ca,[],0);self.b.update(self.cb,[],0)
        for tx,c,rx,rc in ((self.wa,self.ca,self.wb,self.cb),(self.wb,self.cb,self.wa,self.ca)):
            _,packets=tx.update(c,room,42,1,[],'00'*32,0)
            for packet in packets:self.assertTrue(rx.receive(rc,packet,0))

    def test_shutter_enemy_identity_coalesces_and_separates_cycles(self):
        self.setup_room(0xb2)
        first=robot(1)
        expected=[0x7ffffffc,42,(3<<10)|(0xb2+1),1]
        state,packets=self.a.update(self.ca,[first],1)
        self.assertTrue(packets)
        self.assertEqual(state['a'][0][:4],expected)
        for packet in packets:self.assertTrue(self.b.receive(self.cb,packet,1.001))
        # The other client watches the same birth and coalesces onto that key
        # instead of advertising a second copy of the same enemy.
        local=list(first);local[3]=99
        result,packets=self.b.update(self.cb,[local],1.01)
        self.assertEqual(len(result['a']),1)
        self.assertEqual(result['a'][0][:4],expected)
        self.assertTrue(all(not p['a'] for p in packets))
        # A later emission cycle is a different enemy, not a reused identity.
        result,_=self.a.update(self.ca,[first,robot(2)],1.1)
        self.assertEqual({tuple(r[:4]) for r in result['a']},
                         {tuple(expected),tuple([0x7ffffffc,42,(3<<10)|(0xb2+1),2])})

    def test_shutter_enemy_claim_single_committer_and_retry(self):
        self.setup_room(0xb2)
        state,packets=self.a.update(self.ca,[robot(1)],1)
        self.assertEqual(state['a'][0][d.OWNER],1)
        key=tuple(state['a'][0][:4])
        for packet in packets:self.assertTrue(self.b.receive(self.cb,packet,1.001))
        self.a.sent(packets,True,1)
        # Only the shooter claims, so the arbiter is the single committer.
        claim=copy.deepcopy(state['a'][0]);claim[d.LIFE]=d.CLAIM
        reply,_=self.send(self.b,self.cb,self.a,self.ca,[claim],1.1)
        self.assertEqual(reply[0][d.LIFE],d.LIVE)
        result,packets=self.a.update(self.ca,[robot(1)],1.2)
        committed=result['a'][0]
        self.assertEqual(committed[d.LIFE],d.REMOVED)
        self.assertEqual(committed[d.OWNER],2)
        self.assertEqual(committed[d.COMMITTER],1)
        self.assertEqual(committed[d.LANDED],1)
        # A committed kill only arms the native death once it reached the wire.
        self.assertFalse(self.a.native_result(result)['a'])
        self.a.sent(packets,False,1.2)
        self.assertFalse(self.a.native_result(result)['a'])
        result,packets=self.a.update(self.ca,[robot(1)],1.3)
        self.assertTrue(packets)
        self.a.sent(packets,True,1.3)
        self.assertEqual(self.a.native_result(result)['a'][0][d.LIFE],d.REMOVED)
        self.assertIn(key,self.a.dead)

    def test_shutter_enemy_route_exit_stays_non_lethal(self):
        self.setup_room(0xb2)
        _,packets=self.send(self.a,self.ca,self.b,self.cb,[robot(1)],1)
        gone=robot(1,life=d.REMOVED)
        gone[d.COMMITTER]=1
        result,packets=self.a.update(self.ca,[gone],1.1)
        self.assertEqual(result['a'][0][d.LIFE],d.REMOVED)
        self.assertEqual(result['a'][0][d.LANDED],0)
        self.assertEqual(result['a'][0][d.COMMITTER],1)
        for packet in packets:self.assertTrue(self.b.receive(self.cb,packet,1.101))
        accepted,_=self.b.update(self.cb,[gone],1.11)
        self.assertEqual(accepted['a'][0][d.LIFE],d.REMOVED)
        self.assertEqual(accepted['a'][0][d.LANDED],0)

    def test_shutter_enemy_unbound_route_exit_stamps_committer(self):
        self.setup_room(0xb2)
        self.send(self.a,self.ca,self.b,self.cb,[robot(1)],1)
        # The local capture reports the route exit unbound, with no committer.
        gone=robot(1,life=d.REMOVED)
        self.assertEqual(gone[d.COMMITTER],0)
        result,_=self.b.update(self.cb,[gone],2)
        self.assertEqual(result['a'][0][:4],[0x7ffffffc,42,(3<<10)|(0xb2+1),1])
        self.assertEqual(result['a'][0][d.COMMITTER],2)
        self.assertEqual(result['a'][0][d.LANDED],0)

    def test_shutter_enemy_lower_id_fresh_copy_cannot_rewind_holder(self):
        self.setup_room(0xb2)
        # The shutter belongs to the higher-id client, which publishes first.
        self.wa.owners[2]=self.wb.owners[2]=2
        state,_=self.send(self.b,self.cb,self.a,self.ca,[robot(1)],1)
        self.assertEqual(state[0][d.OWNER],2)
        key=tuple(state[0][:4])
        # The shutter hands off to the lower-id client, which reports the same
        # birth again. The established holder keeps the enemy.
        self.wa.owners[2]=self.wb.owners[2]=1
        state_a,_=self.send(self.a,self.ca,self.b,self.cb,[robot(1)],1.1)
        self.assertEqual(state_a[0][d.OWNER],2)
        self.assertEqual(self.a.owners[key],2)
        result,_=self.b.update(self.cb,[robot(1)],1.12)
        self.assertEqual(result['a'][0][d.OWNER],2)
        self.assertEqual(self.b.owners[key],2)

    def test_extended_drop_identity_is_injective_and_uses_the_new_cid(self):
        placed=w.WorldTransport();placed.signature=42;placed.room=0xb2
        def drop(parent,kind,ordinal):
            r=actor(serial=1,kind=kind)
            r[7]=parent;r[8]=0xFC;r[64]=ordinal
            return r
        # The 16-bit placed window keeps its old key.
        self.assertEqual(d.loot_identity(drop(3,2,7),placed),
                         [0x7ffffffd,42,0xb3,(2*3+0)*65536+7+1])
        # Masking the packed serial was not injective: these two collided.
        first,second=(3,2,200000),(3,3,134464)
        masked=lambda t:(((t[0]-1)*3+t[1]-2)*65536+t[2]+1)&0x7fffffff
        self.assertEqual(masked(first),masked(second))
        self.assertNotEqual(d.drop_identity(drop(*first),placed),
                            d.drop_identity(drop(*second),placed))
        key=d.drop_identity(drop(*first),placed)
        self.assertEqual(key[0],0x7ffffffb)
        self.assertEqual(key[3],200000)
        self.assertNotEqual(key[0],0x7ffffffd)
        self.setup_room(0xb2)
        for ordinal in (65536,0x7fffffff):
            supplied=drop(3,2,ordinal)
            state,_=self.a.update(self.ca,[supplied],1+ordinal/0x7fffffff)
            self.assertEqual(state['a'][0][:4],d.drop_identity(supplied,placed))
            self.assertTrue(d.valid(state['a'][0]))

    def test_shutter_enemy_scope_stale_session_and_isolation(self):
        # Kind 6 belongs to room 0xB2 on both the wire and the local capture.
        r=robot(1)
        self.assertEqual(self.ca['room'],302)
        result,packets=self.a.update(self.ca,[r],1)
        self.assertFalse(result['a']);self.assertFalse(packets)
        _,packets=self.a.update(self.ca,[actor()],1.1)
        self.assertTrue(packets)
        bad=copy.deepcopy(packets[0]);bad['a'][0]=list(r)
        self.assertFalse(self.b.receive(self.cb,bad,1.2))
        self.assertTrue(self.b.receive(self.cb,packets[0],1.2))
        # A new visit cannot inherit the previous room's enemies.
        self.setup_room(0xb2)
        self.send(self.a,self.ca,self.b,self.cb,[robot(1)],1)
        self.assertTrue(self.a.peers or self.a.dead or self.a.owners)
        self.wa.update(self.ca,0xb2,42,2,[],'00'*32,2)
        self.a.update(self.ca,[],2)
        self.assertFalse(self.a.dead);self.assertFalse(self.a.peers)
        self.assertFalse(self.a.owners)


class DynamicCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        root=Path(__file__).resolve().parents[1]
        lib=Path(cls.tmp.name)/'codec.so'
        subprocess.run(['cc','-shared','-fPIC','-I'+str(root/'include'),
            str(root/'src/world/anchor_world_dynamic_codec.c'),str(root/'src/utils/string_utils.c'),
            '-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.lib.anchor_world_dynamic_row_valid.argtypes=[ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()

    def test_c_python_bounds_and_recipe_parity(self):
        for kind in range(1,6):
            r=actor(kind=kind)
            self.assertTrue(d.valid(r))
            self.assertTrue(self.lib.anchor_world_dynamic_row_valid((ctypes.c_int*d.WORDS)(*r)))
            for index,(lo,hi) in enumerate(d.BOUNDS):
                for value in (lo-1,lo,hi,min(hi+1,0x7fffffff)):
                    row=list(r);row[index]=value
                    actual=self.lib.anchor_world_dynamic_row_valid((ctypes.c_int*d.WORDS)(*row))
                    self.assertEqual(bool(actual),d.valid(row),(kind,index,value))

    def test_strict_native_decode_and_roundtrip(self):
        r=actor();r[:4]=[1,10,1,1];r[4]=1
        rows=((ctypes.c_int*d.WORDS)*128)();count=ctypes.c_uint();leader=ctypes.c_uint()
        decode=self.lib.anchor_world_dynamic_decode
        def run(value):return decode(value.encode(),rows,ctypes.byref(count),ctypes.byref(leader))
        good=json.dumps({'a':[r],'l':1})
        self.assertTrue(run(good));self.assertEqual(list(rows[0]),r)
        for bad in (good+'garbage',good[:-3],json.dumps({'a':[r,r],'l':1}),
                    json.dumps({'a':[r],'l':-1}),'{"a":[],"l":01}'):
            self.assertFalse(run(bad),bad)

    def test_native_npc_recipe_parity(self):
        from anchor_world_npc import MODELS
        for phase,model in enumerate(MODELS,1):
            r=actor(kind=1)
            r[8]=0x2c3 if phase==7 else model;r[9]=model;r[74]=phase
            if model==0x2c4:r[78:83]=[2,2,1,9999,2]
            self.assertTrue(d.valid(r))
            for column in range(74,d.WORDS):
                for value in (-32769,-2,0,1,2,3,4,6,8,16,18,24,26,32768):
                    altered=list(r);altered[column]=value
                    actual=self.lib.anchor_world_dynamic_row_valid((ctypes.c_int*d.WORDS)(*altered))
                    self.assertEqual(bool(actual),d.valid(altered),(phase,column,value))

    def test_shutter_enemy_recipe_and_bounds_parity(self):
        examples=[robot(ordinal=o,parent=p,base_y=y)
                  for o in (1,65535,0x7fffffff) for p in (1,256) for y in (0,1)]
        for r in examples:
            self.assertTrue(d.valid(r))
            self.assertTrue(self.lib.anchor_world_dynamic_row_valid((ctypes.c_int*d.WORDS)(*r)))
            for index,(lo,hi) in enumerate(d.BOUNDS):
                for value in (lo-1,lo,hi,min(hi+1,0x7fffffff)):
                    row=list(r);row[index]=value
                    actual=self.lib.anchor_world_dynamic_row_valid((ctypes.c_int*d.WORDS)(*row))
                    self.assertEqual(bool(actual),d.valid(row),(r[7],index,value))
            for column,value in ((8,0x354),(9,0xFC),(10,1),(11,0),(32,56),(43,256),
                                 (40,1),(41,1),(45,2),(64,0),(65,1)):
                row=list(r);row[column]=value
                actual=self.lib.anchor_world_dynamic_row_valid((ctypes.c_int*d.WORDS)(*row))
                self.assertEqual(bool(actual),d.valid(row),(r[7],column,value))


if __name__=='__main__':unittest.main()
