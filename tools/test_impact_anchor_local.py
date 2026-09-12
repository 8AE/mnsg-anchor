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
    args = parser.parse_args()
    peers = []
    try:
        room = "impact-automated-"+uuid.uuid4().hex
        for team in ("default","default","isolated"):
            peers.append(Peer(args.port,room,team))
        host,guest,outsider = peers
        players = {}
        for p in peers:
            ad = [impact.VERSION,1,1,0,1,p.session,host.cid,host.session,1,1,host.cid,host.session,1,1,0x260,1]
            state = {"online":True,"teamId":p.team,"interactionSession":p.session,impact.METADATA_KEY:ad}
            players[p.cid] = state
            p.send({"type":"UPDATE_CLIENT_STATE","clientId":p.cid,"state":state})
        for p in peers: p.read(.1)
        def context(p):
            return {"cid":p.cid,"session":p.session,"team":p.team,"connected":True,"players":players}
        host_battle = SimpleNamespace(role=1,owner=host.cid,owner_session=host.session,owner_visit=1,
                                      term=1,e=(host.cid,host.session,1),paused=False)
        guest_battle = SimpleNamespace(**{**vars(host_battle),"role":2})
        tx,rx = visual.ImpactVisualTransport(),visual.ImpactVisualTransport()
        rx.update(context(guest),guest_battle,1,0x260,1,1,None,10)
        rows = [[i+1,79,16,0,0,9,0,0,0,0,0,512,0,
                 0x3E4CCCCD,0x3E4CCCCD,0x3E4CCCCD,0,0x4A8,0,*([0,0]*5)] for i in range(64)]
        _,packets = tx.update(context(host),host_battle,1,0x260,1,1,rows,10)
        assert len(packets) == 4
        for packet in packets: host.send(packet)
        incoming = [p for p in guest.read(.2) if p.get("type") == visual.PACKET_TYPE]
        assert len(incoming) == 4, len(incoming)
        for p in incoming: assert rx.receive(context(guest),guest_battle,p,10)
        assert rx.update(context(guest),guest_battle,1,0x260,1,1,None,10)[0] == rows
        assert not [p for p in host.read(.05) if p.get("type") == visual.PACKET_TYPE], "Sender echoed"
        assert not [p for p in outsider.read(.05) if p.get("type") == visual.PACKET_TYPE], "Cross-team leak"
        # Actual cursor/control transport uses the same server route.
        ptx,prx = impact.ImpactPlayerTransport(),impact.ImpactPlayerTransport()
        sample = {"c":[1,0,0,0,0,512,0],"a":[[1,2,0x2000,0x2000,0,512,host.cid,1]]}
        prx.update(context(host),1,0x260,1,1,{"c":[0]*7,"a":[]},10)
        _,packets = ptx.update(context(guest),1,0x260,1,1,sample,10)
        for p in packets: guest.send(p); ptx.packet_send_result(p,True)
        for p in host.read(.2):
            if p.get("type") == impact.PLAYER_PACKET_TYPE: assert prx.receive(context(host),p,10)
        status,_ = prx.update(context(host),1,0x260,1,1,{"c":[0]*7,"a":[]},10)
        assert len(status["a"]) == len(status["c"]) == 1
        assert status["a"][0][-2:] == [host.cid,1]
        assert not [p for p in outsider.read(.05) if p.get("type") == impact.PLAYER_PACKET_TYPE]
        print(json.dumps({"clients":3,"render_pages":4,"render_objects":64,
                          "frame_bytes":sum(len(json.dumps(p,separators=(",", ":")).encode())+1 for p in incoming),
                          "sender_excluded":True,"team_isolated":True,"cursor_and_controls":True}))
    finally:
        for peer in peers: peer.socket.close()


if __name__ == "__main__": main()
