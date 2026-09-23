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
