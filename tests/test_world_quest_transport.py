import copy
import json
import unittest
import anchor_world as world
import anchor_world_quest as quest


def row(role=1,part=0,serial=1,instance=1,receipt=0,life=0):
    r=[0]*quest.WORDS
    r[0]=quest.ABI;r[1]=1;r[2]=role;r[5]=serial;r[6]=life
    r[8]=5;r[9]=1;r[10]=0x316;r[11]=quest.ROLE_MODEL[role]
    r[12]=quest.role_recipe(role,part)[1]
    r[13]=part;r[23:26]=[100]*3;r[62:64]=[instance,receipt]
    return r


def dragon_row(part=0,instance=1,ready=1):
    r=[0]*quest.WORDS
    r[0]=quest.ABI;r[1]=2;r[2]=7 if part else 6
    r[5]=part+1;r[8]=5;r[9]=1;r[10]=0x1b4 if part else 0x1b0
    r[11]=0x1b0;r[12]=quest.role_recipe(r[2],part)[1]
    r[13]=part;r[23:26]=[100]*3;r[62]=instance
    if not part:
        r[14]=1;r[quest.DRAGON_D4]=0x180
        r[quest.DRAGON_READY]=ready
    return r


class QuestTransportTests(unittest.TestCase):
    def setUp(self):
        self.wa,self.wb=world.WorldTransport(),world.WorldTransport()
        self.a,self.b=quest.QuestTransport(self.wa),quest.QuestTransport(self.wb)
        self.ca=dict(cid=1,session=10,team='default',connected=True,
                     loaded=True,room=0x153,players={})
        self.cb=dict(self.ca,cid=2,session=20,players={})
        for t,c in ((self.wa,self.ca),(self.wb,self.cb)):
            t.update(c,0x153,42,1,[],'00'*32,0)
        for c,other,t in ((self.ca,self.cb,self.wb),(self.cb,self.ca,self.wa)):
            c['players'][other['cid']]=dict(online=True,isSaveLoaded=True,
                teamId='default',roomId=0x153,interactionSession=other['session'],
                worldSync=t.advertisement(other))
        self.a.update(self.ca,[],[],0)
        self.b.update(self.cb,[],[],0)
        self.send(self.a,self.ca,self.b,self.cb,[],.1)
        self.send(self.b,self.cb,self.a,self.ca,[],.1)

    def send(self,tx,c,rx,rc,rows,now,status=None):
        result,packets=tx.update(c,rows,status or [],now)
        for p in packets:
            self.assertTrue(rx.receive(rc,p,now+.001))
        tx.sent(packets,True,now)
        return result,packets

    def dragon_room(self):
        for transport,ctx,other in ((self.wa,self.ca,self.cb),
                                    (self.wb,self.cb,self.ca)):
            ctx['room']=0x155
            transport.update(ctx,0x155,42,2,[],'00'*32,0)
            other['players'][ctx['cid']]['roomId']=0x155
            other['players'][ctx['cid']]['worldSync']=transport.advertisement(ctx)
        self.a.update(self.ca,[],[],0)
        self.b.update(self.cb,[],[],0)

    def test_graph_authority_and_late_proxy_receipt(self):
        root=row();child=row(role=2,serial=2)
        host,packets=self.send(self.a,self.ca,self.b,self.cb,[root,child],1)
        self.assertTrue(host['ready']);self.assertEqual(len(host['a']),2)
        self.assertTrue(all(r[quest.OWNER]==1 for r in host['a']))
        self.assertTrue(all(not r[quest.INSTANCE] and not r[quest.RECEIPT]
                            for p in packets for r in p['a']))
        guest,_=self.b.update(self.cb,[],[],1.01)
        self.assertEqual({r[quest.ROLE] for r in guest['a']},{1,2})
        self.assertTrue(all(not r[quest.INSTANCE] and r[quest.RECEIPT]
                            for r in guest['a']))
        created=list(guest['a'][0]);created[quest.INSTANCE]=72
        created[quest.RECEIPT]=guest['a'][0][quest.RECEIPT]
        next_result,_=self.b.update(self.cb,[],[created],1.02)
        matching=next(r for r in next_result['a'] if r[quest.ROLE]==created[quest.ROLE])
        self.assertEqual(matching[quest.INSTANCE],72)
        self.assertEqual(matching[quest.RECEIPT],created[quest.RECEIPT])

    def test_local_original_does_not_masquerade_as_proxy(self):
        self.send(self.a,self.ca,self.b,self.cb,[row()],1)
        local_original=row(instance=99)
        offered,_=self.b.update(self.cb,[local_original],[],1.01)
        self.assertEqual(len(offered['a']),1)
        self.assertEqual(offered['a'][0][quest.OWNER],1)
        self.assertEqual(offered['a'][0][quest.INSTANCE],0)
        self.assertGreater(offered['a'][0][quest.RECEIPT],0)

    def test_source_and_proxy_status_require_distinct_local_instances(self):
        missing=row(instance=0)
        refused,packets=self.a.update(self.ca,[missing],[],1)
        self.assertFalse(refused['ready']);self.assertFalse(packets)
        forged=row(receipt=5)
        refused,packets=self.a.update(self.ca,[forged],[],1)
        self.assertFalse(refused['ready']);self.assertFalse(packets)
        refused,packets=self.a.update(self.ca,[],[missing],1)
        self.assertFalse(refused['ready']);self.assertFalse(packets)

    def test_simultaneous_family_birth_converges_on_same_owner(self):
        first,packet_a=self.a.update(self.ca,[row()],[],1)
        second,packet_b=self.b.update(self.cb,[row(instance=2)],[],1)
        self.assertEqual(first['a'][0][quest.OWNER],1)
        self.assertEqual(second['a'][0][quest.OWNER],2)
        self.assertTrue(all(self.b.receive(self.cb,p,1.01) for p in packet_a))
        self.assertTrue(all(self.a.receive(self.ca,p,1.01) for p in packet_b))
        converged_a,_=self.a.update(self.ca,[row()],[],1.02)
        converged_b,_=self.b.update(self.cb,[row(instance=2)],[],1.02)
        self.assertEqual(converged_a['a'][0][quest.OWNER],1)
        self.assertEqual(converged_b['a'][0][quest.OWNER],1)

    def test_live_graph_outranks_retired_lower_client(self):
        retired=row(life=quest.REMOVED)
        live=row(instance=2)
        self.send(self.a,self.ca,self.b,self.cb,[retired],1)
        self.send(self.b,self.cb,self.a,self.ca,[live],1.1)
        result_a,_=self.a.update(self.ca,[retired],[],1.11)
        result_b,_=self.b.update(self.cb,[live],[],1.11)
        self.assertEqual(result_a['a'][0][quest.OWNER],2)
        self.assertEqual(result_b['a'][0][quest.OWNER],2)
        self.assertEqual(result_a['a'][0][quest.LIFE],quest.LIVE)

    def test_full_snapshot_and_owner_departure(self):
        self.send(self.a,self.ca,self.b,self.cb,[row(),row(role=2,serial=2)],1)
        guest,_=self.send(self.b,self.cb,self.a,self.ca,[row()],1.1)
        self.assertEqual({r[quest.ROLE] for r in guest['a']},{1,2})
        self.ca['players'][2]['online']=False
        self.cb['players'][1]['online']=False
        after,_=self.b.update(self.cb,[row()],[],2)
        self.assertEqual({r[quest.ROLE] for r in after['a']},{1})
        self.assertEqual(after['a'][0][quest.OWNER],2)

    def test_sender_scope_replay_and_pointer_words_rejected(self):
        _,packets=self.a.update(self.ca,[row()],[],1)
        self.assertTrue(packets)
        packet=packets[0]
        bad=copy.deepcopy(packet);bad['a'][0][31]=0x80201234
        self.assertFalse(self.b.receive(self.cb,bad,1.01))
        bad=copy.deepcopy(packet);bad['targetTeamId']='isolated'
        self.assertFalse(self.b.receive(self.cb,bad,1.01))
        bad=copy.deepcopy(packet);bad['a'][0][62]=5
        self.assertFalse(self.b.receive(self.cb,bad,1.01))
        self.assertTrue(self.b.receive(self.cb,packet,1.01))
        self.assertFalse(self.b.receive(self.cb,packet,1.02))

    def test_announced_member_without_world_snapshot_holds_apply(self):
        third=dict(online=True,isSaveLoaded=True,teamId='default',roomId=0x153,
                   interactionSession=30)
        self.ca['players'][3]=third
        offered,_=self.a.update(self.ca,[row()],[],1)
        self.assertFalse(offered['ready'])
        self.assertFalse(offered['a'])

    def test_role_recipe_rejects_unusable_peer_rows(self):
        original=row(role=2)
        self.assertTrue(quest.valid(original,0x153))
        for index,value in ((10,0x315),(11,0x315),(12,0),
                            (13,1),(14,256),(26,1),(31,0x80201234),
                            (44,1)):
            bad=list(original);bad[index]=value
            self.assertFalse(quest.valid(bad,0x153),(index,value))
        staged=[0]*quest.WORDS
        staged[0]=quest.ABI;staged[1]=4;staged[2]=12
        staged[5]=1;staged[8]=4;staged[9]=1;staged[10]=0
        staged[11]=0x35b;staged[13]=19;staged[23:26]=[100]*3
        self.assertTrue(quest.valid(staged,0xc1))
        staged[13]=22
        self.assertFalse(quest.valid(staged,0xc1))

    def test_gmc_child_model_and_clip_recipes(self):
        models74=([0x36a,0x36a,0x351,0x318,0x352,0x353]+[0xfb]*8+
                  [0x367]*3+[0x359,0x35a,0x35b,0x367,0x367])
        self.assertEqual(len(models74),22)
        for part,model in enumerate(models74):
            self.assertEqual(quest.role_recipe(12,part)[0],model)
            self.assertEqual(quest.role_recipe(12,part)[1],
                             {16:1,20:3,21:2}.get(part,0))
            self.assertEqual(quest.role_recipe(12,part)[2],
                             {0:10,1:11,2:5,3:3,4:2,5:2}.get(part,0))
            r=[0]*quest.WORDS
            r[0]=quest.ABI;r[1]=4;r[2]=12;r[5]=part+1
            r[8]=4;r[9]=1;r[10]=0;r[11]=model
            r[12]=quest.role_recipe(12,part)[1];r[13]=part
            r[23:26]=[100]*3;r[26]=quest.role_recipe(12,part)[2]
            self.assertTrue(quest.valid(r,0xc1),part)
            wrong=list(r);wrong[10]=0x35c
            self.assertFalse(quest.valid(wrong,0xc1),part)
            wrong=list(r);wrong[12]=(r[12]+1)%8
            self.assertFalse(quest.valid(wrong,0xc1),part)
            wrong=list(r);wrong[26]+=1
            self.assertFalse(quest.valid(wrong,0xc1),part)
        for part,model,clip in ((0,0x2da,5),(1,0x32c,6),(2,0x2d6,2)):
            self.assertEqual(quest.role_recipe(14,part),(model,0,clip))
            r=[0]*quest.WORDS
            r[0]=quest.ABI;r[1]=5;r[2]=14;r[5]=1
            r[8]=5;r[9]=1;r[10]=0x35d;r[11]=model;r[13]=part
            r[23:26]=[100]*3;r[27]=64;r[29]=1
            self.assertTrue(quest.valid(r,0xc1))
            for index,value in ((11,0x367),(12,1),(26,clip+1)):
                bad=list(r);bad[index]=value
                self.assertFalse(quest.valid(bad,0xc1),(part,index))
        self.assertIsNone(quest.role_recipe(14,3))

    def test_koryuta_parts_keep_native_child_entity(self):
        for part in range(1,12):
            r=[0]*quest.WORDS
            r[0]=quest.ABI;r[1]=2;r[2]=7;r[5]=part
            r[8]=5;r[9]=1;r[10]=0x1b4;r[11]=0x1b0
            r[12]=quest.role_recipe(7,part)[1];r[13]=part
            r[23:26]=[100]*3
            self.assertTrue(quest.valid(r,0x155),part)
            wrong=list(r);wrong[10]=0x1b0
            self.assertFalse(quest.valid(wrong,0x155),part)

    def test_dragon_election_requires_complete_native_graph(self):
        self.dragon_room()
        a_rows=[dragon_row(p,instance=p+1) for p in range(12)]
        b_rows=[dragon_row(p,instance=p+101) for p in range(12)]
        b_rows[0][quest.DRAGON_READY]=0
        self.send(self.a,self.ca,self.b,self.cb,a_rows[:11],1)
        self.send(self.b,self.cb,self.a,self.ca,b_rows,1.1)
        pending,_=self.a.update(self.ca,a_rows[:11],[],1.11)
        self.assertEqual(pending['a'],[])
        b_rows[0][quest.DRAGON_READY]=1
        self.send(self.b,self.cb,self.a,self.ca,b_rows,1.31)
        pending,_=self.a.update(self.ca,a_rows[:11],[],1.32)
        self.assertEqual(len(pending['a']),12)
        self.assertTrue(all(r[quest.OWNER]==2 for r in pending['a']))
        self.assertEqual(pending['a'][-1][quest.INSTANCE],0)
        self.assertTrue(all(r[quest.RECEIPT]>0 for r in pending['a']))
        self.send(self.a,self.ca,self.b,self.cb,a_rows,1.41)
        converged,_=self.b.update(self.cb,b_rows,[],1.42)
        self.assertEqual(len(converged['a']),12)
        self.assertTrue(all(r[quest.OWNER]==2 for r in converged['a']))
        self.assertEqual({r[quest.INSTANCE] for r in converged['a']},
                         {p+101 for p in range(12)})
        self.ca['players'][2]['online']=False
        self.cb['players'][1]['online']=False
        departed,_=self.b.update(self.cb,b_rows,[],2)
        self.assertEqual(len(departed['a']),12)
        self.assertTrue(all(r[quest.OWNER]==2 for r in departed['a']))

    def test_dragon_simultaneous_ready_converges_and_stays(self):
        self.dragon_room()
        self.send(self.a,self.ca,self.b,self.cb,[],.2)
        self.send(self.b,self.cb,self.a,self.ca,[],.2)
        a_rows=[dragon_row(p,instance=p+1) for p in range(12)]
        b_rows=[dragon_row(p,instance=p+101) for p in range(12)]
        first_a,packets_a=self.a.update(self.ca,a_rows,[],1)
        first_b,packets_b=self.b.update(self.cb,b_rows,[],1)
        self.assertEqual({r[quest.OWNER] for r in first_a['a']},{1})
        self.assertEqual({r[quest.OWNER] for r in first_b['a']},{2})
        self.assertTrue(packets_a and packets_b)
        for packet in packets_a:
            self.assertTrue(self.b.receive(self.cb,packet,1.001))
        for packet in packets_b:
            self.assertTrue(self.a.receive(self.ca,packet,1.001))
        self.a.sent(packets_a,True,1)
        self.b.sent(packets_b,True,1)
        for now in (1.01,1.31,1.61):
            result_a,packets_a=self.a.update(self.ca,a_rows,[],now)
            result_b,packets_b=self.b.update(self.cb,b_rows,[],now)
            self.assertEqual({r[quest.OWNER] for r in result_a['a']},{1})
            self.assertEqual({r[quest.OWNER] for r in result_b['a']},{1})
            for packet in packets_a:
                self.assertTrue(self.b.receive(self.cb,packet,now+.001))
            for packet in packets_b:
                self.assertTrue(self.a.receive(self.ca,packet,now+.001))
            self.a.sent(packets_a,True,now)
            self.b.sent(packets_b,True,now)

    def test_dragon_crossed_claims_recover_deterministically(self):
        self.dragon_room()
        self.send(self.a,self.ca,self.b,self.cb,[],.2)
        self.send(self.b,self.cb,self.a,self.ca,[],.2)
        a_rows=[dragon_row(p,instance=p+1) for p in range(12)]
        b_rows=[dragon_row(p,instance=p+101) for p in range(12)]
        self.send(self.a,self.ca,self.b,self.cb,a_rows,1)
        self.send(self.b,self.cb,self.a,self.ca,b_rows,1)
        family=quest.family_key(a_rows[0])
        self.a.owners[family]=2
        self.b.owners[family]=1
        self.a.peers[2]['rows'][quest.key(b_rows[0])][quest.OWNER]=1
        self.b.peers[1]['rows'][quest.key(a_rows[0])][quest.OWNER]=2
        for now in (1.01,1.31):
            a,packets_a=self.a.update(self.ca,a_rows,[],now)
            b,packets_b=self.b.update(self.cb,b_rows,[],now)
            self.assertEqual({r[quest.OWNER] for r in a['a']},{1})
            self.assertEqual({r[quest.OWNER] for r in b['a']},{1})
            for packet in packets_a:
                self.assertTrue(self.b.receive(self.cb,packet,now+.001))
            for packet in packets_b:
                self.assertTrue(self.a.receive(self.ca,packet,now+.001))
            self.a.sent(packets_a,True,now)
            self.b.sent(packets_b,True,now)

    def test_dragon_peer_snapshot_expiry_makes_reply_unready(self):
        self.dragon_room()
        self.send(self.a,self.ca,self.b,self.cb,[],.2)
        self.send(self.b,self.cb,self.a,self.ca,[],.2)
        local=[dragon_row(p,instance=p+1) for p in range(12)]
        remote=[dragon_row(p,instance=p+101) for p in range(12)]
        self.send(self.b,self.cb,self.a,self.ca,remote,1)
        ready,_=self.a.update(self.ca,local,[],1.01)
        self.assertTrue(ready['ready'])
        expired,_=self.a.update(self.ca,local,[],1.01+world.TTL+.01)
        self.assertFalse(expired['ready'])
        self.assertEqual(expired['a'],[])

    def test_dragon_lower_cid_late_and_owner_failover(self):
        self.dragon_room()
        early=[dragon_row(p,instance=100+p) for p in range(12)]
        late=[dragon_row(p,instance=1+p,ready=0) for p in range(12)]
        self.send(self.b,self.cb,self.a,self.ca,early,1)
        first,_=self.a.update(self.ca,late,[],1.01)
        self.assertEqual({r[quest.OWNER] for r in first['a']},{2})
        self.send(self.a,self.ca,self.b,self.cb,late,1.1)
        self.send(self.b,self.cb,self.a,self.ca,early,1.11)
        late[0][quest.DRAGON_READY]=1
        self.send(self.a,self.ca,self.b,self.cb,late,1.31)
        a,_=self.a.update(self.ca,late,[],1.32)
        b,_=self.b.update(self.cb,early,[],1.32)
        self.assertEqual({r[quest.OWNER] for r in a['a']},{2})
        self.assertEqual({r[quest.OWNER] for r in b['a']},{2})
        self.ca['players'][2]['online']=False
        self.cb['players'][1]['online']=False
        failed,_=self.a.update(self.ca,late,[],2)
        self.assertEqual({r[quest.OWNER] for r in failed['a']},{1})

    def test_dragon_lower_cid_early_and_room_reset(self):
        self.dragon_room()
        early=[dragon_row(p,instance=1+p) for p in range(12)]
        late=[dragon_row(p,instance=100+p) for p in range(12)]
        self.send(self.a,self.ca,self.b,self.cb,early,1)
        self.send(self.b,self.cb,self.a,self.ca,late,1.1)
        a,_=self.a.update(self.ca,early,[],1.11)
        b,_=self.b.update(self.cb,late,[],1.11)
        self.assertEqual({r[quest.OWNER] for r in a['a']},{1})
        self.assertEqual({r[quest.OWNER] for r in b['a']},{1})
        self.wa.update(self.ca,0x153,42,3,[],'00'*32,2)
        self.a.update(self.ca,[],[],2)
        self.assertFalse(self.a.owners)

    def test_dragon_cycle_words_and_packet_validation(self):
        for transport,ctx in ((self.wa,self.ca),(self.wb,self.cb)):
            ctx['room']=0x155
            transport.update(ctx,0x155,42,2,[],'00'*32,0)
        self.cb['players'][1]['roomId']=0x155
        self.cb['players'][1]['worldSync']=self.wa.advertisement(self.ca)
        source=dragon_row()
        source[quest.DRAGON_D8]=65535
        source[quest.DRAGON_DA]=-32768
        source[quest.DRAGON_DC]=32767
        source[quest.DRAGON_DE]=-1
        self.assertTrue(quest.valid(source,0x155))
        for index,value in ((14,6),(quest.DRAGON_D4,65536),
                            (quest.DRAGON_D8,-1),
                            (quest.DRAGON_DA,-32769),
                            (quest.DRAGON_DC,32768),
                            (quest.DRAGON_READY,2)):
            bad=list(source);bad[index]=value
            self.assertFalse(quest.valid(bad,0x155),(index,value))
        part=dragon_row(1)
        part[quest.DRAGON_D4]=1
        self.assertFalse(quest.valid(part,0x155))
        part=dragon_row(1)
        part[quest.DRAGON_READY]=1
        self.assertFalse(quest.valid(part,0x155))
        _,packets=self.a.update(self.ca,[source],[],1)
        self.assertEqual(len(packets),1)
        bad=copy.deepcopy(packets[0]);bad['a'][0][quest.DRAGON_DC]=32768
        self.assertFalse(self.b.receive(self.cb,bad,1.01))
        bad=copy.deepcopy(packets[0]);bad['a'][0][quest.DRAGON_READY]=2
        self.assertFalse(self.b.receive(self.cb,bad,1.01))

    def test_full_gmc_staged_graph_stays_within_packet_cap(self):
        placed=world.WorldTransport();transport=quest.QuestTransport(placed)
        ctx=dict(cid=7,session=70,team='default',connected=True,
                 loaded=True,room=0xc1,players={})
        placed.update(ctx,0xc1,99,1,[],'00'*32,0)
        staged=[]
        for part in range(22):
            r=[0]*quest.WORDS
            r[0]=quest.ABI;r[1]=4;r[2]=12;r[5]=part+1
            r[8]=4;r[9]=1;r[10]=0;r[11]=quest.role_recipe(12,part)[0]
            r[12]=quest.role_recipe(12,part)[1]
            r[13]=part;r[23:26]=[100]*3;r[62]=part+1
            staged.append(r)
        for part in range(3):
            r=[0]*quest.WORDS
            r[0]=quest.ABI;r[1]=5;r[2]=14;r[5]=part+1
            r[8]=5;r[9]=1;r[10]=0x35d;r[11]=quest.role_recipe(14,part)[0]
            r[13]=part;r[23:26]=[100]*3;r[62]=part+23
            staged.append(r)
        result,packets=transport.update(ctx,staged,[],1)
        self.assertTrue(result['ready'])
        self.assertEqual(len(result['a']),25)
        self.assertEqual(len(packets),4)
        self.assertLessEqual(len(packets),quest.MAX_PARTS)
        self.assertTrue(all(len(json.dumps(p,separators=(',',':')).encode())+1
                            <=quest.PACKET_BYTES for p in packets))
        other=world.WorldTransport();receiver=quest.QuestTransport(other)
        peer=dict(ctx,cid=8,session=80,players={})
        other.update(peer,0xc1,99,1,[],'00'*32,0)
        peer['players'][7]=dict(online=True,isSaveLoaded=True,teamId='default',
            roomId=0xc1,interactionSession=70,
            worldSync=other.advertisement(ctx))
        receiver.update(peer,[],[],0)
        for p in packets[:-1]:
            self.assertTrue(receiver.receive(peer,p,1.01))
        self.assertFalse(receiver.peers)
        self.assertTrue(receiver.receive(peer,packets[-1],1.01))
        self.assertEqual(len(receiver.peers[7]['rows']),25)

    def test_failed_multipart_send_retries_complete_graph(self):
        placed=world.WorldTransport();transport=quest.QuestTransport(placed)
        ctx=dict(cid=7,session=70,team='default',connected=True,
                 loaded=True,room=0xc1,players={})
        placed.update(ctx,0xc1,99,1,[],'00'*32,0)
        staged=[]
        for ordinal in range(1,33):
            r=[0]*quest.WORDS
            r[0]=quest.ABI;r[1]=4;r[2]=12;r[5]=ordinal
            r[8]=4;r[9]=ordinal;r[10]=0;r[11]=quest.role_recipe(12,0)[0]
            r[23:26]=[100]*3;r[62]=ordinal
            staged.append(r)
        result,packets=transport.update(ctx,staged,[],1)
        self.assertTrue(result['ready'])
        self.assertEqual(len(packets),4)
        transport.sent(packets,False,1)
        self.assertEqual(transport.last,())
        _,retry=transport.update(ctx,staged,[],1.06)
        self.assertEqual(len(retry),4)
        self.assertGreater(retry[0]['q'],packets[0]['q'])
        transport.sent(retry,True,1.06)
        self.assertEqual(len(transport.last),32)
        extra=list(staged[0]);extra[9]=33;extra[5]=33
        refused,no_packets=transport.update(ctx,staged+[extra],[],1.3)
        self.assertFalse(refused['ready'])
        self.assertFalse(no_packets)
        self.assertEqual(len(transport.last),32)


if __name__=='__main__':
    unittest.main()
