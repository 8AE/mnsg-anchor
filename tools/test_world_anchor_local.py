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

        # A higher-ID traveller opens the door, then leaves the game room.
        door=[0]*39;door[:3]=[10,0x23c,5];door[12]=256;door[26:29]=[1000]*3;door[29]=4
        for t,p in ((tx,host),(rx,guest)):
            t.update(ctx(p),302,42,2,[door],'00'*32,5)
            state=dict(online=True,isSaveLoaded=True,teamId=p.team,currentRoomId=302,
                       interactionSession=p.session,worldSync=[VERSION,p.session,2,302,42])
            players[p.cid]=dict(state,roomId=302)
            p.send(dict(type='UPDATE_CLIENT_STATE',clientId=p.cid,state=state))
        host.read(.1);guest.read(.1)
        opening=list(door);opening[3]=1;opening[11]=1500;opening[29]=20
        _,packets=rx.update(ctx(guest),302,42,2,[opening],'00'*32,5.2)
        for packet in packets:guest.send(packet)
        rx.sent(packets,True,5.2)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,5.3)
        result,_=tx.update(ctx(host),302,42,2,[door],'00'*32,5.31)
        assert result['a'][0][0]==guest.cid and result['a'][0][2:]==opening
        state=dict(online=True,isSaveLoaded=True,teamId=guest.team,currentRoomId=303,
                   interactionSession=guest.session,worldSync=[VERSION,guest.session,3,303,43])
        guest.send(dict(type='UPDATE_CLIENT_STATE',clientId=guest.cid,state=state))
        departed=[p for p in host.read(.2) if p.get('type')=='UPDATE_CLIENT_STATE'
                  and p.get('clientId')==guest.cid]
        assert len(departed)==1 and departed[0]['state']['currentRoomId']==303
        players[guest.cid]=dict(departed[0]['state'],roomId=303)
        result,_=tx.update(ctx(host),302,42,2,[door],'00'*32,5.4)
        assert not result['a'] and tx.owners[10]==host.cid

        # A new observer receives the current reverse pose, then closed state.
        observer=Peer(args.port,room,'default');peers.append(observer)
        late=WorldTransport();late.update(ctx(observer),302,42,1,[door],'00'*32,5.4)
        state=dict(online=True,isSaveLoaded=True,teamId=observer.team,currentRoomId=302,
                   interactionSession=observer.session,worldSync=[VERSION,observer.session,1,302,42])
        players[observer.cid]=dict(state,roomId=302)
        observer.send(dict(type='UPDATE_CLIENT_STATE',clientId=observer.cid,state=state))
        observer.read(.1);host.read(.1)
        closing=list(door);closing[11]=1200;closing[29]=22
        for now,sample in ((5.5,closing),(5.56,door)):
            _,packets=tx.update(ctx(host),302,42,2,[sample],'00'*32,now)
            assert packets
            for packet in packets:host.send(packet)
            tx.sent(packets,True,now)
            received=[p for p in observer.read(.2) if p.get('type')==PACKET_TYPE]
            assert received
            for packet in received:assert late.receive(ctx(observer),packet,now+.01)
            result,_=late.update(ctx(observer),302,42,1,[door],'00'*32,now+.02)
            assert result['a'][0][0]==host.cid and result['a'][0][2:]==sample
        assert not any(p.get('type')==PACKET_TYPE for p in outsider.read(.05))
        print(json.dumps(dict(ok=True,actors=256,parts=11,max_packet_bytes=max(len(json.dumps(p,separators=(',',':')).encode())+1 for p in incoming),checks=['NUL framing','atomic reordered batch','roster-validated root identity','sender exclusion','team isolation','pickup tombstones','interaction ownership','peer expiry','door traveller departure','door reverse late entry','door closed checkpoint'])))
    finally:
        for p in peers:p.socket.close()

if __name__=='__main__':main()
