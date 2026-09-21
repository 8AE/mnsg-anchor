#!/usr/bin/env python3
"""Mechanism checkpoint handoff over three real TCP clients on loopback Anchor.

Native application receipts are simulated; this is transport integration, not
an in-game animation or collision check. No public-server target is supported.
"""
import argparse
import json
from pathlib import Path
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'py'))
sys.path.insert(0, str(ROOT/'tests'))
import anchor_world as w
from test_world_spike import spike
from test_world_rope import rope
from test_world_file40 import mechanism
from test_impact_anchor_local import Peer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--family', choices=('spike','rope','top','rotor'), default='spike')
    args = parser.parse_args()
    peers = []
    try:
        room = 'world-mechanism-automated-' + uuid.uuid4().hex
        for team in ('default','default','isolated'):
            peers.append(Peer(args.port, room, team))
        entrant, incumbent, outsider = peers
        assert entrant.cid < incumbent.cid
        a, b = w.WorldTransport(), w.WorldTransport()
        players, sizes = {}, []
        stage = 0x3f if args.family in ('top','rotor') else 0x41 if args.family == 'rope' else 0x32
        def ctx(peer):
            return dict(cid=peer.cid, session=peer.session, team=peer.team,
                        room=stage, connected=True, loaded=True, players=players)
        def tick(transport, peer, rows, now):
            return transport.update(ctx(peer), stage, 42, 1, rows, '00'*32, now)
        live = ([rope(0, angle=65530, paused=1),rope(1, angle=32768, paused=1)]
                if args.family == 'rope' else
                [spike(0, phase=4, frame=150, paused=1), spike(1, timer=70, paused=1)])
        if args.family in ('top','rotor'):
            live=[mechanism(i,top=args.family=='top',angle=256+i*444,yaw=1022-i*300,paused=1)
                  for i in range(2)]
        tick(a, entrant, [], 0)
        _, packets = tick(b, incumbent, live, 0)
        b.sent(packets, True, 0)
        assert b.established == {0,1}
        for peer in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=peer.team,
                         currentRoomId=stage, interactionSession=peer.session,
                         worldSync=[w.VERSION,peer.session,1,stage,42])
            players[peer.cid] = dict(state, roomId=stage)
            peer.send(dict(type='UPDATE_CLIENT_STATE', clientId=peer.cid, state=state))
        for peer in peers: peer.read(.1)

        def deliver(transport, source, target, receiver, packets, now):
            assert packets
            for packet in packets:
                size = len(json.dumps(packet, separators=(',',':')).encode())+1
                sizes.append(size)
                assert size <= w.PACKET_BYTES
                assert packet['clientId'] == source.cid
                assert packet['targetTeamId'] == source.team and packet['quiet']
                assert 'addToQueue' not in packet and 'targetClientId' not in packet
                source.send(packet)
            transport.sent(packets, True, now)
            incoming = [p for p in target.read(.12) if p.get('type') == w.PACKET_TYPE]
            assert len(incoming) == len(packets)
            for packet in incoming: assert receiver.receive(ctx(target), packet, now+.001)
            assert not any(p.get('type') == w.PACKET_TYPE for p in source.read(.02))
            assert not any(p.get('type') == w.PACKET_TYPE for p in outsider.read(.02))

        _, packets = tick(b, incumbent, live, 1)
        deliver(b, incumbent, entrant, a, packets, 1)
        fresh = [rope(0),rope(1)] if args.family == 'rope' else [spike(0), spike(1)]
        if args.family in ('top','rotor'):
            fresh=[mechanism(i,top=args.family=='top') for i in range(2)]
        for row in fresh: row[w.INSTANCE] = 70 + row[0]
        result, packets = tick(a, entrant, fresh, 1.02)
        assert a.owners == {0:incumbent.cid, 1:incumbent.cid}
        assert not a.established and all(not p['a'] for p in packets)
        assert len(result['a']) == 2
        applied = []
        for offer in result['a']:
            row = list(offer[w.DELIVERY_ROW:])
            assert row[w.RECEIPT] & w.BOOTSTRAP
            assert row[:38] == live[row[0]][:38]
            row[38] = 0  # This local observer is unpaused.
            applied.append(row)
        _, packets = tick(a, entrant, applied, 1.12)
        assert a.established == {0,1}
        assert a.owners == {0:entrant.cid,1:entrant.cid}
        deliver(a, entrant, incumbent, b, packets, 1.12)
        # A duplicate remains a duplicate after real NUL framing and relay.
        entrant.send(packets[0])
        repeated = [p for p in incumbent.read(.1) if p.get('type') == w.PACKET_TYPE]
        assert len(repeated) == 1 and not b.receive(ctx(incumbent), repeated[0], 1.14)
        result, _ = tick(b, incumbent, live, 1.16)
        assert b.owners == {0:entrant.cid,1:entrant.cid}
        current = [list(offer[w.DELIVERY_ROW:]) for offer in result['a']]
        assert len(current) == 2
        for row in current:
            assert row[:38] == live[row[0]][:38]
            row[38] = 0
        # Apply the offered state before the former owner leaves.
        tick(b, incumbent, current, 1.18)
        entrant.socket.close()
        assert any(p.get('type') == 'ALL_CLIENT_STATE' for p in incumbent.read(.2))
        players[entrant.cid]['online'] = False
        _, packets = tick(b, incumbent, current, 1.4)
        assert b.owners == {0:incumbent.cid,1:incumbent.cid}
        assert packets
        for packet in packets:
            for row in packet['a']: assert row[:38] == live[row[0]][:38]
            incumbent.send(packet)
        b.sent(packets, True, 1.4)
        assert not any(p.get('type') == w.PACKET_TYPE for p in outsider.read(.1))
        print(json.dumps(dict(ok=True, family=args.family, max_packet_bytes=max(sizes), checks=[
            'lower-ID late entry waits for applied receipt', 'independent placed identities',
            ('full 16-bit rotation preserved' if args.family == 'rope' else
             'static-model phase and yaw preserved' if args.family in ('top','rotor') else
             'paused reverse frame and timer preserved'), 'unpaused owner handoff',
            'owner disconnect continuity', 'duplicate rejection', 'sender exclusion',
            'team isolation', 'bounded transient team packets'],
            native_receipts='simulated; no in-game renderer/contact execution')))
    finally:
        for peer in peers: peer.socket.close()


if __name__ == '__main__':
    main()
