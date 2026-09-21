#!/usr/bin/env python3
"""Bounded production world-transport probe against loopback Anchor only."""
import argparse
import json
from pathlib import Path
import sys
import uuid
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'py'))
from test_impact_anchor_local import Peer
from anchor_world import WorldTransport, PACKET_TYPE, VERSION, WORDS, INSTANCE, RECEIPT, BOOTSTRAP


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
            r=[0]*WORDS;r[:3]=[i,0x3e0,2];r[4]=i*100;r[18]=1;r[26:29]=[1000]*3;rows.append(r)
        rows[255][1:3]=[0x2c4,1]
        rows[255][39:48]=[13,-200,100,60,24,-2,1,500,2]
        rows[249][1:3]=[0x3d0,2];rows[249][10]=2;rows[249][17]=120
        rows[249][18]=73;rows[249][23]=5;rows[249][29]=17;rows[249][30]=6
        rows[250][1:3]=[0x228,2];rows[250][18]=62
        rows[250][19:24]=[120,-50,80,20,1]
        rows[251][1:3]=[0x1fe,2];rows[251][10]=2;rows[251][18]=68
        rows[251][19:25]=[120,-50,80,20,1,3];rows[251][17]=256
        rows[251][31:34]=[1696,0,1]
        rows[251][40:48]=[12750,6000,0x200,-176,-100,-300,125,-50]
        for index,entity,kind,phase in ((252,0x324,2,55),(253,0x326,2,59),(254,0x226,6,5)):
            rows[index][1:4]=[entity,kind,1]
            rows[index][18]=phase;rows[index][21]=1
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
        assert len(result['a'])==256 and result['a'][255][2:2+INSTANCE]==rows[255][:INSTANCE]
        assert result['a'][250][2:2+INSTANCE]==rows[250][:INSTANCE] and result['a'][251][2:2+INSTANCE]==rows[251][:INSTANCE]
        assert result['a'][249][2:2+INSTANCE]==rows[249][:INSTANCE]
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
        door=[0]*WORDS;door[:3]=[10,0x23c,5];door[12]=256;door[26:29]=[1000]*3;door[29]=4
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
        assert result['a'][0][0]==guest.cid and result['a'][0][2:2+INSTANCE]==opening[:INSTANCE]
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
            assert result['a'][0][0]==host.cid and result['a'][0][2:2+INSTANCE]==sample[:INSTANCE]
        assert not any(p.get('type')==PACKET_TYPE for p in outsider.read(.05))

        # A higher-ID player presses a switch, and the older observer
        # restores it. Presence still reports pause after row ownership moves.
        switch=[0]*WORDS;switch[:3]=[11,0x226,6];switch[18]=1;switch[26:29]=[150]*3
        for t,p,visit in ((tx,host,3),(late,observer,2)):
            t.update(ctx(p),302,42,visit,[switch],'00'*32,6)
            state=dict(online=True,isSaveLoaded=True,teamId=p.team,currentRoomId=302,
                       interactionSession=p.session,worldSync=[VERSION,p.session,visit,302,42])
            players[p.cid]=dict(state,roomId=302)
            p.send(dict(type='UPDATE_CLIENT_STATE',clientId=p.cid,state=state))
        host.read(.1);observer.read(.1)
        _,packets=tx.update(ctx(host),302,42,3,[switch],'00'*32,6.1)
        for packet in packets:host.send(packet)
        tx.sent(packets,True,6.1)
        for packet in observer.read(.2):
            if packet.get('type')==PACKET_TYPE:assert late.receive(ctx(observer),packet,6.11)
        pressed=list(switch);pressed[3]=1;pressed[18]=5;pressed[21]=pressed[22]=1;pressed[29]=2
        _,packets=late.update(ctx(observer),302,42,2,[pressed],'00'*32,6.2)
        assert packets and packets[0]['a']==[pressed]
        for packet in packets:observer.send(packet)
        late.sent(packets,True,6.2)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,6.21)
        result,_=tx.update(ctx(host),302,42,3,[switch],'00'*32,6.22)
        assert result['a'][0][0]==observer.cid and result['a'][0][2:2+INSTANCE]==pressed[:INSTANCE]
        _,packets=tx.update(ctx(host),302,42,3,[pressed],'00'*32,6.3)
        for packet in packets:host.send(packet)
        tx.sent(packets,True,6.3)
        for packet in observer.read(.2):
            if packet.get('type')==PACKET_TYPE:assert late.receive(ctx(observer),packet,6.31)
        paused=list(pressed);paused[38]=1
        _,packets=tx.update(ctx(host),302,42,3,[paused],'00'*32,6.4)
        assert packets and not packets[0]['a'] and packets[0]['z']=='0008'+'00'*30
        for packet in packets:host.send(packet)
        tx.sent(packets,True,6.4)
        for packet in observer.read(.2):
            if packet.get('type')==PACKET_TYPE:assert late.receive(ctx(observer),packet,6.41)
        result,_=late.update(ctx(observer),302,42,2,[pressed],'00'*32,6.42)
        assert not result['a'] and late.owners[11]==observer.cid
        moving=[0]*WORDS;moving[:4]=[12,0x326,2,1];moving[18]=57;moving[21]=1;moving[26:29]=[1000]*3
        _,packets=late.update(ctx(observer),302,42,2,[moving],'00'*32,6.6)
        for packet in packets:observer.send(packet)
        late.sent(packets,True,6.6)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,6.61)
        loaded=list(moving);loaded[18]=59;loaded[24]=1
        result,_=tx.update(ctx(host),302,42,3,[loaded],'00'*32,6.62)
        assert result['a'][0][0]==observer.cid and result['a'][0][2:2+INSTANCE]==moving[:INSTANCE]
        # A fire hit wins just this parent; its child/tint scalars survive TCP.
        fire=list(rows[251]);fire[0]=13;fire[3]=1;fire[18]=67;fire[24]=1
        fire[17]=40;fire[25]=10
        _,packets=late.update(ctx(observer),302,42,2,[fire],'00'*32,6.8)
        for packet in packets:observer.send(packet)
        late.sent(packets,True,6.8)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,6.81)
        local=list(fire);local[3]=0;local[18]=65;local[24]=local[17]=local[25]=0
        local[40]=local[41]=0
        result,_=tx.update(ctx(host),302,42,3,[local],'00'*32,6.82)
        assert result['a'][0][0]==observer.cid and result['a'][0][2:2+INSTANCE]==fire[:INSTANCE]
        gated=list(rows[250]);gated[0]=14;gated[18]=61;gated[23]=0;gated[24]=7
        _,packets=late.update(ctx(observer),302,42,2,[gated],'00'*32,7)
        for packet in packets:observer.send(packet)
        late.sent(packets,True,7)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,7.01)
        waiting=list(gated);waiting[18]=60
        result,_=tx.update(ctx(host),302,42,3,[waiting],'00'*32,7.02)
        assert result['a'][0][0]==observer.cid and result['a'][0][2:2+INSTANCE]==gated[:INSTANCE]
        # The lower-ID client enters an already occupied cyclic-platform room.
        platform=[0]*WORDS;platform[:3]=[15,0x3e0,2];platform[18]=2
        platform[4]=43210;platform[7]=384;platform[26:29]=[1000]*3
        late.update(ctx(observer),302,42,2,[platform],'00'*32,8)
        fresh=list(platform);fresh[4]=fresh[7]=0;fresh[INSTANCE]=55
        tx.update(ctx(host),302,42,4,[fresh],'00'*32,8.1)
        state=dict(online=True,isSaveLoaded=True,teamId=host.team,currentRoomId=302,
                   interactionSession=host.session,worldSync=[VERSION,host.session,4,302,42])
        players[host.cid]=dict(state,roomId=302)
        host.send(dict(type='UPDATE_CLIENT_STATE',clientId=host.cid,state=state))
        observer.read(.1)
        _,packets=late.update(ctx(observer),302,42,2,[platform],'00'*32,8.2)
        assert packets and packets[0]['a']==[platform]
        for packet in packets:observer.send(packet)
        late.sent(packets,True,8.2)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,8.21)
        result,packets=tx.update(ctx(host),302,42,4,[fresh],'00'*32,8.22)
        assert tx.owners[15]==observer.cid and all(not p['a'] for p in packets)
        applied=result['a'][0][2:]
        assert applied[4]==43210 and applied[INSTANCE]==55 and applied[RECEIPT]&BOOTSTRAP
        # The native harness independently verifies when this receipt is set.
        result,packets=tx.update(ctx(host),302,42,4,[applied],'00'*32,8.3)
        assert tx.owners[15]==host.cid and not result['a'] and packets
        assert packets[0]['a'][0][INSTANCE:]==[0,0]
        for packet in packets:host.send(packet)
        tx.sent(packets,True,8.3)
        for packet in observer.read(.2):
            if packet.get('type')==PACKET_TYPE:assert late.receive(ctx(observer),packet,8.31)
        result,_=late.update(ctx(observer),302,42,2,[platform],'00'*32,8.32)
        assert result['a'][0][0]==host.cid and result['a'][0][6]==43210
        # A completed puzzle survives a delayed participant's newer retry.
        puzzle=[0]*WORDS;puzzle[:3]=[16,0x3d0,2];puzzle[10]=2
        puzzle[17:19]=[120,73];puzzle[26:29]=[1000]*3;puzzle[29:31]=[17,6]
        _,packets=late.update(ctx(observer),302,42,2,[puzzle],'00'*32,9)
        assert packets and packets[0]['a']==[puzzle]
        for packet in packets:observer.send(packet)
        late.sent(packets,True,9)
        for packet in host.read(.2):
            if packet.get('type')==PACKET_TYPE:assert tx.receive(ctx(host),packet,9.01)
        retry=list(puzzle);retry[10]=retry[17]=retry[30]=0
        retry[18]=69;retry[23]=8388607;retry[29]=31;retry[INSTANCE]=88
        result,_=tx.update(ctx(host),302,42,4,[retry],'00'*32,9.02)
        applied=result['a'][0][2:]
        assert result['a'][0][0]==observer.cid and applied[18]==73 and applied[17]==120
        assert applied[INSTANCE]==88 and applied[RECEIPT]&BOOTSTRAP
        result,packets=tx.update(ctx(host),302,42,4,[applied],'00'*32,9.1)
        assert not result['a'] and tx.owners[16]==host.cid
        # Native completion of the slot leaves 119; an old 120 cannot rewind it.
        applied[17]=119
        _,packets=tx.update(ctx(host),302,42,4,[applied],'00'*32,9.3)
        assert packets and packets[0]['a'][0][17]==119
        for packet in packets:host.send(packet)
        tx.sent(packets,True,9.3)
        for packet in observer.read(.2):
            if packet.get('type')==PACKET_TYPE:assert late.receive(ctx(observer),packet,9.31)
        result,_=late.update(ctx(observer),302,42,2,[puzzle],'00'*32,9.32)
        assert result['a'][0][0]==host.cid and result['a'][0][19]==119
        assert not any(p.get('type')==PACKET_TYPE for p in outsider.read(.05))
        print(json.dumps(dict(ok=True,actors=256,parts=11,max_packet_bytes=max(len(json.dumps(p,separators=(',',':')).encode())+1 for p in incoming),checks=['NUL framing','atomic reordered batch','roster-validated root identity','sender exclusion','team isolation','pickup tombstones','interaction ownership','peer expiry','door traveller departure','door reverse late entry','door closed checkpoint','native NPC continuation checkpoint','physics puzzle reward checkpoint','completed puzzle outranks delayed retry','reward timer handoff without rewind','switch and mechanism checkpoints','switch activation overrides older observer','pause presence after ownership handoff','save-aware constructor yields to moving checkpoint','linked platform child and tint checkpoint','local fire ownership','gate-open checkpoint before model initialization','lower-ID occupied-room bootstrap','native receipt required before takeover','local instance and receipt stripped from wire'])))
    finally:
        for p in peers:p.socket.close()

if __name__=='__main__':main()
