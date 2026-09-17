#!/usr/bin/env python3
"""Bounded production world-transport probe against loopback Anchor only."""
import argparse
import json
from pathlib import Path
import sys
import uuid
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'py'))
from test_impact_anchor_local import Peer
from anchor_world import WorldTransport, PACKET_TYPE, VERSION


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',type=int,required=True);args=p.parse_args()
    peers=[]
    try:
        room='world-automated-'+uuid.uuid4().hex
        peers=[Peer(args.port,room,t) for t in ('default','default','isolated')]
        peers.sort(key=lambda p:(p.team!='default',p.cid))
        host,guest,outsider=peers
        tx,rx=WorldTransport(),WorldTransport()
        players={}
        def ctx(p):return dict(cid=p.cid,session=p.session,team=p.team,room=302,connected=True,loaded=True,players=players)
        rows=[]
        for i in range(256):
            r=[0]*39;r[:3]=[i,0x3e0,2];r[4]=i*100;r[18]=1;r[26:29]=[1000]*3;rows.append(r)
        for t,p in ((tx,host),(rx,guest)):t.update(ctx(p),302,42,1,rows,'00'*32,0)
        for p in peers:
            state=dict(online=True,isSaveLoaded=True,teamId=p.team,currentRoomId=302,interactionSession=p.session,
                       worldSync=[VERSION,p.session,1,302,42])
            players[p.cid]=dict(state,roomId=302)
            p.send(dict(type='UPDATE_CLIENT_STATE',clientId=p.cid,state=state))
        for p in peers:p.read(.1)
        _,packets=tx.update(ctx(host),302,42,1,rows,'01'+'00'*31,1)
        assert len(packets)==11
        for packet in packets:host.send(packet)
        incoming=[p for p in guest.read(.2) if p.get('type')==PACKET_TYPE]
        assert len(incoming)==11,len(incoming)
        for p in reversed(incoming):assert rx.receive(ctx(guest),p,1.1)
        result,_=rx.update(ctx(guest),302,42,1,rows,'00'*32,1.2)
        assert len(result['a'])==256 and result['a'][255][2:]==rows[255]
        assert result['d']=='01'+'00'*31
        assert not any(p.get('type')==PACKET_TYPE for p in host.read(.05))
        assert not any(p.get('type')==PACKET_TYPE for p in outsider.read(.05))
        # A local interaction can temporarily own just its actor.
        rows[4][3]=1;rows[4][4]=12345
        _,packets=rx.update(ctx(guest),302,42,1,rows,'00'*32,1.3)
        for packet in packets:guest.send(packet)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,1.4)
        local=[list(r) for r in rows];local[4][3]=0
        result,_=tx.update(ctx(host),302,42,1,local,'00'*32,1.5)
        assert result['a'][0][0]==guest.cid and result['a'][0][2]==4
        # Peer expiry releases control without converting culling into death.
        result,_=tx.update(ctx(host),302,42,1,local,'00'*32,4)
        assert not result['a'] and result['d']=='01'+'00'*31
        print(json.dumps(dict(ok=True,actors=256,parts=11,max_packet_bytes=max(len(json.dumps(p,separators=(',',':')).encode())+1 for p in incoming),checks=['NUL framing','atomic reordered batch','roster-validated root identity','sender exclusion','team isolation','pickup tombstones','interaction ownership','peer expiry'])))
    finally:
        for p in peers:p.socket.close()

if __name__=='__main__':main()
