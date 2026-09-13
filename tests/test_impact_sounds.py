import copy
import json
import unittest
import anchor_impact_sound as sound
from test_impact_visual import battle
from test_impact_players import context

class ImpactSoundTests(unittest.TestCase):
    def setUp(self):
        self.a,self.b = context(1),context(2)
        self.host,self.guest = battle(1),battle(2)
        self.tx,self.rx = sound.ImpactSoundTransport(),sound.ImpactSoundTransport()
        self.rx.update(self.b,self.guest,1,0x260,1,1,"",10)

    def send(self, sample="000000107840022900000240", now=10):
        return self.tx.update(self.a,self.host,1,0x260,1,1,sample,now)[1]

    def view(self,now=10):
        return self.rx.update(self.b,self.guest,1,0x260,1,1,"",now)[0]

    def test_once_only_commands_loop_heartbeat_and_stop(self):
        p=self.send()[0]
        self.assertTrue(self.rx.receive(self.b,self.guest,p,10))
        self.assertFalse(self.rx.receive(self.b,self.guest,p,10))
        self.assertEqual(self.view(),"000000107840022900000240")
        self.assertEqual(self.view(),"00000010")
        p=self.send("0000000000008240",10.05)[0]
        self.assertTrue(self.rx.receive(self.b,self.guest,p,10.05))
        self.assertEqual(self.view(10.05),"0000000000008240")
        self.send("0000001000000240",10.10)  # simulate failed socket write
        p=self.send("00000010",10.36)[0]
        self.assertTrue(self.rx.receive(self.b,self.guest,p,10.36))
        self.assertEqual(self.view(10.36),"00000010")
        self.assertEqual(self.view(11.2),"00000000")

    def test_route_identity_order_and_payload_validation(self):
        p=self.send()[0]
        for k,v in [("clientId",3),("session",101),("visit",2),("term",2),("q",True),
                    ("v",4),("targetClientId",2),("targetTeamId","elsewhere"),("addToQueue",False),
                    ("s",0x261),("k",2),("e",[1,100,2]),("l",64),("r",[0x27]),
                    ("r",[0x826d]),("r",[0x7ff]),("r",[0x00800229]),("r",[0x229]*9),("extra","x"*1024)]:
            with self.subTest(k=k,v=v):
                self.assertFalse(self.rx.receive(self.b,self.guest,{**p,k:v},10))
        self.assertTrue(self.rx.receive(self.b,self.guest,p,10))
        self.guest.term=2
        self.assertEqual(self.view(),"00000000")
        self.assertFalse(self.rx.receive(self.b,self.guest,p,10))

    def test_scope_reset_and_no_delayed_reentry_sound(self):
        self.rx.receive(self.b,self.guest,self.send()[0],10)
        self.b["players"][1]["online"]=False
        self.assertEqual(self.view(),"00000000")
        self.b["players"][1]["online"]=True
        self.guest.paused=True
        self.assertEqual(self.view(),"00000000")
        self.guest.paused=False
        self.assertEqual(self.view(),"00000000")
        self.assertEqual(len(self.rx.queue),0)

    def test_bounded_rate_size_queue_and_no_solo_traffic(self):
        packets=[]
        for tick in range(300):
            packets += self.send("00000000"+"78400229"*8,10+tick/300)
        self.assertLessEqual(len(packets),30)
        self.assertLessEqual(len(self.tx.queue),32)
        for p in packets:
            self.assertLessEqual(len(json.dumps(p,separators=(",", ":")).encode())+1,1024)
            self.rx.receive(self.b,self.guest,p,10)
        self.assertLessEqual(len(self.rx.queue),32)
        self.assertLessEqual(len(self.view()),72)
        self.a["players"]={1:self.a["players"][1]}
        self.assertEqual(self.send(now=12),[])

if __name__ == "__main__": unittest.main()
