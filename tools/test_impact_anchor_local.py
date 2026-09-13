#!/usr/bin/env python3
"""Three real TCP clients against an explicitly supplied loopback Anchor.

Run a disposable Anchor bound to 127.0.0.1 first. This tool refuses other hosts
and has no public-service default. Exercises the production Impact transport.
"""
import argparse
import json
from pathlib import Path
import select
import socket
import sys
import time
from types import SimpleNamespace
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"py"))
import anchor_impact as impact
import anchor_impact_visual as visual
import anchor_impact_sound as sound


class Peer:
    def __init__(self, port, room, team):
        self.socket = socket.create_connection(("127.0.0.1",port),timeout=2)
        self.buffer = b""
        self.cid = 0
        self.team = team
        self.session = 100+len(team)
        self.send({"type":"HANDSHAKE","roomId":room,"clientId":0,
                   "clientState":{"name":"Impact automatic check","online":True,
                                  "teamId":team,"interactionSession":self.session},"roomState":{}})
        for packet in self.read(.3):
            if packet.get("type") == "ALL_CLIENT_STATE":
                for p in packet["state"]:
                    if p.get("self"): self.cid = p["clientId"]
        assert self.cid, "Handshake did not assign an identity"

    def send(self, packet):
        self.socket.sendall(json.dumps(packet,separators=(",", ":"),allow_nan=False).encode()+b"\0")

    def read(self, seconds):
        packets = []
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            ready,_,_ = select.select([self.socket],[],[],max(0,end-time.monotonic()))
            if not ready: break
            data = self.socket.recv(65536)
            assert data, "Unexpected disconnect"
            self.buffer += data
            while b"\0" in self.buffer:
                raw,self.buffer = self.buffer.split(b"\0",1)
                packet = json.loads(raw)
                if packet.get("type") != "HEARTBEAT": packets.append(packet)
        return packets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",type=int,required=True)
    parser.add_argument("--boss",type=int,choices=(1,2),default=1)
    parser.add_argument("--stage",type=lambda s:int(s,0),
                        help="Native stage; defaults to the selected boss's boss-rush stage")
    args = parser.parse_args()
    boss = args.boss
    stage = args.stage if args.stage is not None else 0x25F + boss
    assert impact._impact_stage(stage)
    peers = []
    try:
        room = "impact-automated-"+uuid.uuid4().hex
        for team in ("default","default","isolated"):
            peers.append(Peer(args.port,room,team))
        host,guest,outsider = peers
        players = {}
        for p in peers:
            ad = [impact.VERSION,1,1,0,1,p.session,host.cid,host.session,1,1,host.cid,host.session,1,1,stage,boss]
            state = {"online":True,"teamId":p.team,"interactionSession":p.session,impact.METADATA_KEY:ad}
            players[p.cid] = state
            p.send({"type":"UPDATE_CLIENT_STATE","clientId":p.cid,"state":state})
        for p in peers: p.read(.1)
        def context(p):
            return {"cid":p.cid,"session":p.session,"team":p.team,"connected":True,
                    "loaded":True,"room":stage,"players":players}
        host_battle = SimpleNamespace(role=1,owner=host.cid,owner_session=host.session,owner_visit=1,
                                      term=1,e=(host.cid,host.session,1),paused=False)
        guest_battle = SimpleNamespace(**{**vars(host_battle),"role":2})
        tx,rx = visual.ImpactVisualTransport(),visual.ImpactVisualTransport()
        rx.update(context(guest),guest_battle,1,stage,boss,1,None,10)
        rows = [[i+1,79,16,0,0,9,0,0,0,0,0,512,0,
                 0x3E4CCCCD,0x3E4CCCCD,0x3E4CCCCD,0,0x4A8,0,*([0,0]*5)] for i in range(64)]
        for i, recipe in enumerate([2,7,9,11,12,122,*range(123,131)]):
            rows[i][1] = recipe  # real punch/kick/arm/hook and all chain lengths
            rows[i][5] = 5
        if boss == 2:
            # Actual Taisamba clips, returning weapon and whirlwind recipes.
            for row,recipe in zip(rows[16:],(18,21,22,27,31,35,39,41,44,46,50,53,55,57,58,97,99,100,101,103)):
                row[1],row[17] = recipe,0x4B2
        _,packets = tx.update(context(host),host_battle,1,stage,boss,1,rows,10)
        assert len(packets) == 4
        for packet in packets: host.send(packet)
        incoming = [p for p in guest.read(.2) if p.get("type") == visual.PACKET_TYPE]
        assert len(incoming) == 4, len(incoming)
        for p in incoming: assert rx.receive(context(guest),guest_battle,p,10)
        assert rx.update(context(guest),guest_battle,1,stage,boss,1,None,10)[0] == rows
        assert not [p for p in host.read(.05) if p.get("type") == visual.PACKET_TYPE], "Sender echoed"
        assert not [p for p in outsider.read(.05) if p.get("type") == visual.PACKET_TYPE], "Cross-team leak"
        # Actual cursor/control transport uses the same server route.
        ptx,prx = impact.ImpactPlayerTransport(),impact.ImpactPlayerTransport()
        sample = {"c":[1,0,0,0,0,512,0],"a":[[1,2,0x10,0x10,990,630,host.cid,1],
                  [2,3,110,60,0,0,host.cid,1], [3,2,0x8000,0x8000,990,630,host.cid,1],
                  [4,2,0x4000,0x4000,990,630,host.cid,1]]}
        prx.update(context(host),1,stage,boss,1,{"c":[0]*7,"a":[]},10)
        _,packets = ptx.update(context(guest),1,stage,boss,1,sample,10)
        for p in packets: guest.send(p); ptx.packet_send_result(p,True)
        for p in host.read(.2):
            if p.get("type") == impact.PLAYER_PACKET_TYPE: assert prx.receive(context(host),p,10)
        status,_ = prx.update(context(host),1,stage,boss,1,{"c":[0]*7,"a":[]},10)
        assert len(status["a"]) == 4 and len(status["c"]) == 1
        assert status["a"][1][3:8] == [3,110,60,0,0]
        assert status["a"][0][-2:] == [host.cid,1]
        assert status["a"][0][4:8] == [0x10,0x10,990,630]
        assert [r[5] for r in status["a"][2:]] == [0x8000,0x4000]
        assert not [p for p in outsider.read(.05) if p.get("type") == impact.PLAYER_PACKET_TYPE]
        stx,srx = sound.ImpactSoundTransport(),sound.ImpactSoundTransport()
        srx.update(context(guest),guest_battle,1,stage,boss,1,"",10)
        _,packets = stx.update(context(host),host_battle,1,stage,boss,1,"000000107840022900000240",10)
        for p in packets: host.send(p)
        audio = [p for p in guest.read(.1) if p.get("type") == sound.PACKET_TYPE]
        assert len(audio) == 1 and srx.receive(context(guest),guest_battle,audio[0],10)
        assert not srx.receive(context(guest),guest_battle,audio[0],10)
        assert srx.update(context(guest),guest_battle,1,stage,boss,1,"",10)[0] == "000000107840022900000240"
        assert not [p for p in host.read(.05) if p.get("type") == sound.PACKET_TYPE]
        assert not [p for p in outsider.read(.05) if p.get("type") == sound.PACKET_TYPE]
        _,packets = stx.update(context(host),host_battle,1,stage,boss,1,"0000000000008240",10.1)
        for p in packets: host.send(p)
        audio = [p for p in guest.read(.1) if p.get("type") == sound.PACKET_TYPE]
        assert len(audio) == 1 and srx.receive(context(guest),guest_battle,audio[0],10.1)
        assert srx.update(context(guest),guest_battle,1,stage,boss,1,"",10.1)[0] == "0000000000008240"
        # Elect and deliver an actual root checkpoint through the same sockets.
        engines = [impact.ImpactTransport(),impact.ImpactTransport()]
        root = [0]*impact.ROOT_WORDS
        root[:3] = [1800,80,950]
        if boss == 2:
            root[15] = 129  # returning-weapon launch phase, beyond old phase limit
            root[165+1],root[165+4],root[165+6] = 0x3F800000,0x43480000,1
        checkpoint = {"k":boss,"s":stage,"r":root}
        for engine in engines: engine.set_encounter(stage,boss)
        for tick in range(10):
            now = 20+tick*.1
            for peer,engine in zip(peers[:2],engines):
                players[peer.cid][impact.METADATA_KEY] = engine.advertisement(context(peer))
            for peer,engine in zip(peers[:2],engines):
                _,out = engine.update(context(peer),1,1,0,checkpoint,now)
                for packet in out: peer.send(packet)
            for peer,engine in zip(peers[:2],engines):
                for packet in peer.read(.04):
                    if packet.get("type") == impact.PACKET_TYPE:
                        engine.receive(context(peer),packet,now)
        assert sorted(e.role for e in engines) == [1,2]
        assert all(e.state == checkpoint for e in engines)
        assert not [p for p in outsider.read(.05) if p.get("type") == impact.PACKET_TYPE]
        print(json.dumps({"clients":3,"boss":boss,"stage":stage,"root_words":len(root),"checkpoint":True,"render_pages":4,"render_objects":64,
                          "frame_bytes":sum(len(json.dumps(p,separators=(",", ":")).encode())+1 for p in incoming),
                          "sender_excluded":True,"team_isolated":True,"cursor_and_controls":True,
                          "limbs_and_chain":True,"hook_aim_and_ordered_mashes":True,
                          "audio_start_stop":True,"audio_deduplicated":True}))
    finally:
        for peer in peers: peer.socket.close()


if __name__ == "__main__": main()
