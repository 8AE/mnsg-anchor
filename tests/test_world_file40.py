"""File40 static-model mechanisms: native codec parity and owner continuity."""
import json
import unittest
import test_world_spike as support
w=support.w

def mechanism(index=0, top=True, subtype=0, radius=80, step=4, angle=0, yaw=0, paused=0):
    r=[0]*w.WORDS;r[:3]=[index,0x365 if top else 0x366,2]
    r[8]=yaw;r[18]=78 if top else 79;r[19]=angle if top else subtype
    if top:r[20:25]=[subtype,radius,step,-45,-230]
    r[26:30]=[100,100,100,5];r[38]=paused
    return r

class File40CodecTests(unittest.TestCase):
    setUpClass=classmethod(support.SpikeCodecTests.setUpClass.__func__)
    tearDownClass=classmethod(support.SpikeCodecTests.tearDownClass.__func__)

    def test_native_parity_and_all_parameter_families(self):
        rows=[mechanism(subtype=s,radius=r,step=k,angle=1023)
              for s,r,k in ((0,80,4),(0,130,2),(2,160,1),(3,160,1))]
        rows += [mechanism(top=False,subtype=s,yaw=1022) for s in (0,1)]
        for row in rows:
            self.assertTrue(w.row_valid(row));self.assertEqual(w.controller_progress(row),0)
            for col in range(w.WORDS):
                for v in (-2147483648,-32769,-1,0,1,2,3,4,5,78,79,80,100,130,160,1023,1024,32768,2147483647):
                    altered=list(row);altered[col]=v
                    self.assertEqual(w.row_valid(altered),bool(self.lib.anchor_world_row_valid(self.Row(*altered))),
                                     (row[1],row[20:23],col,v))

    def test_no_animation_or_pointer_state(self):
        for top in (True,False):
            for col in (3,7,9,*range(10,18),25,*range(30,38),*range(39,48)):
                r=mechanism(top=top);r[col]=1
                self.assertFalse(w.row_valid(r),(top,col))
            r=mechanism(top=top);r[19]=True;self.assertFalse(w.row_valid(r))

class File40TransportTests(unittest.TestCase):
    def test_late_entry_pause_handoff_and_distinct_mechanisms(self):
        a,b=w.WorldTransport(),w.WorldTransport()
        ca,cb=support.context(1),support.context(2);ca['room']=cb['room']=0x3f
        live=[mechanism(0,angle=700,yaw=444,paused=1),
              mechanism(1,top=False,subtype=1,yaw=900,paused=1)]
        support.tick(a,ca,[],0)
        _,ps=support.tick(b,cb,live,0);b.sent(ps,True,0)
        for c,other,t in ((ca,cb,b),(cb,ca,a)):
            c['players'][other['cid']]=dict(online=True,isSaveLoaded=True,teamId=other['team'],
                roomId=other['room'],interactionSession=other['session'],worldSync=t.advertisement(other))
        _,ps=support.tick(b,cb,live,1)
        for p in ps:self.assertTrue(a.receive(ca,p,1.001))
        b.sent(ps,True,1)
        fresh=[mechanism(0),mechanism(1,top=False,subtype=1)]
        for r in fresh:r[w.INSTANCE]=100+r[0]
        result,ps=support.tick(a,ca,fresh,1.02)
        self.assertEqual(a.owners,{0:2,1:2});self.assertFalse(a.established)
        self.assertTrue(all(not p['a'] for p in ps))
        applied=[]
        for offer in result['a']:
            r=offer[w.DELIVERY_ROW:];self.assertTrue(r[w.RECEIPT]&w.BOOTSTRAP)
            self.assertEqual(r[:38],live[r[0]][:38]);r[38]=0;applied.append(r)
        _,ps=support.tick(a,ca,applied,1.12)
        self.assertEqual(a.owners,{0:1,1:1});self.assertEqual(a.established,{0,1})
        a.sent(ps,True,1.12);ca['players'][2]['online']=False
        # A non-owner leaving does not force another identical hot packet.
        # The next keepalive must retain the established checkpoint.
        _,ps=support.tick(a,ca,applied,1.4)
        self.assertEqual(a.owners,{0:1,1:1});self.assertFalse(ps)
        _,ps=support.tick(a,ca,applied,2.2)
        self.assertEqual([r[:38] for p in ps for r in p['a']],[r[:38] for r in live])

    def test_motion_cadence_and_team_envelope(self):
        t=w.WorldTransport();c=support.context(1);c['room']=0x3f
        _,ps=support.tick(t,c,[mechanism()],1);t.sent(ps,True,1)
        moved=mechanism(angle=16,yaw=16)
        self.assertFalse(support.tick(t,c,[moved],1.06)[1])
        _,ps=support.tick(t,c,[moved],1.21);self.assertTrue(ps)
        for p in ps:
            self.assertEqual(p['clientId'],1);self.assertEqual(p['targetTeamId'],'default')
            self.assertNotIn('addToQueue',p);self.assertNotIn('targetClientId',p)
            self.assertLessEqual(len(json.dumps(p,separators=(',',':')).encode())+1,w.PACKET_BYTES)

if __name__=='__main__':unittest.main()
