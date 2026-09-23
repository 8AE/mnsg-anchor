#!/usr/bin/env python3
"""Shutter robot overlap, kill commit and handoff against loopback Anchor."""
import argparse
import copy
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'py'))
import anchor_world as w
import anchor_world_dynamic as d
from test_impact_anchor_local import Peer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    args = parser.parse_args()
    peers = []
    try:
        lobby = 'shutter-automated-' + uuid.uuid4().hex
        peers = [Peer(args.port, lobby, t) for t in ('default', 'default', 'isolated')]
        peers.sort(key=lambda p: (p.team != 'default', p.cid))
        host, guest, outsider = peers
        players, sizes = {}, []
        def ctx(peer):
            return dict(cid=peer.cid, session=peer.session, team=peer.team, room=0xb2,
                        connected=True, loaded=True, players=players)
        worlds = [w.WorldTransport(), w.WorldTransport()]
        transports = [d.DynamicTransport(world) for world in worlds]
        shutter = [0]*w.WORDS
        shutter[:3] = [0, 0x354, w.SHUTTER]
        shutter[10] = 14; shutter[17:21] = [29, 3, 1, 2]
        shutter[26:29] = [100]*3; shutter[w.INSTANCE] = 1
        for world, transport, peer in zip(worlds, transports, (host, guest)):
            world.update(ctx(peer), 0xb2, 42, 1, [shutter], '00'*32, 0)
            transport.update(ctx(peer), [], 0)
        for peer in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=peer.team,
                         currentRoomId=0xb2, interactionSession=peer.session,
                         worldSync=[w.VERSION, peer.session, 1, 0xb2, 42])
            players[peer.cid] = dict(state, roomId=0xb2)
            peer.send(dict(type='UPDATE_CLIENT_STATE', clientId=peer.cid, state=state))
        for peer in peers: peer.read(.1)
        for world, peer in zip(worlds, (host, guest)):
            _, packets = world.update(ctx(peer), 0xb2, 42, 1, [shutter], '00'*32, .5)
            for packet in packets: peer.send(packet)
            world.sent(packets, True, .5)
        for world, peer in zip(worlds, (host, guest)):
            for packet in peer.read(.1):
                if packet.get('type') == w.PACKET_TYPE:
                    assert world.receive(ctx(peer), packet, .6)
        outsider.read(.1)
        rows = []
        for ordinal in (1, 2):
            row = [0]*d.WORDS
            row[:12] = [0, 0, 0, ordinal, 0, 0, 6, 1, 0xfc, 0xfb, 0, 1]
            row[12:15] = [ordinal*5000, -1500, 14000]
            row[24:27] = [100]*3
            row[32] = 57; row[35] = 12; row[42] = 15; row[64] = ordinal
            assert d.valid(row)
            rows.append(row)
        tx, rx = transports
        def deliver(transport, source, target, receiver, packets, now):
            assert packets
            for packet in packets:
                sizes.append(len(json.dumps(packet, separators=(',', ':')).encode())+1)
                source.send(packet)
            transport.sent(packets, True, now)
            incoming = [p for p in target.read(.12) if p.get('type') == d.PACKET_TYPE]
            assert len(incoming) == len(packets)
            for packet in incoming: assert receiver.receive(ctx(target), packet, now+.01)
            assert not any(p.get('type') == d.PACKET_TYPE for p in source.read(.02))
            assert not any(p.get('type') == d.PACKET_TYPE for p in outsider.read(.02))
        state, packets = tx.update(ctx(host), rows, 1)
        assert len(state['a']) == 2 and len({tuple(r[:4]) for r in state['a']}) == 2
        deliver(tx, host, guest, rx, packets, 1)
        observed, _ = rx.update(ctx(guest), [], 1.02)
        assert observed['a'] == state['a']
        claims = copy.deepcopy(observed['a'])
        claims[0][d.LIFE] = d.CLAIM; claims[0][44] = 1
        _, packets = rx.update(ctx(guest), claims, 1.1)
        deliver(rx, guest, host, tx, packets, 1.1)
        committed, packets = tx.update(ctx(host), state['a'], 1.2)
        dead = next(r for r in committed['a'] if r[d.LIFE] == d.REMOVED)
        assert dead[d.OWNER] == guest.cid and dead[d.COMMITTER] == host.cid and dead[44] == 1
        tx.sent(packets, False, 1.2)
        assert not any(r[d.LIFE] == d.REMOVED for r in tx.native_result(committed)['a'])
        committed, packets = tx.update(ctx(host), state['a'], 1.3)
        deliver(tx, host, guest, rx, packets, 1.3)
        assert any(r[d.LIFE] == d.REMOVED for r in tx.native_result(committed)['a'])
        observed, _ = rx.update(ctx(guest), claims, 1.32)
        assert sum(r[d.LIFE] == d.REMOVED for r in observed['a']) == 1
        host.socket.close()
        assert any(p.get('type') == 'ALL_CLIENT_STATE' for p in guest.read(.3))
        players[host.cid]['online'] = False
        live = [r for r in observed['a'] if r[d.LIFE] != d.REMOVED]
        handed, _ = rx.update(ctx(guest), live, 1.7)
        survivor = next(r for r in handed['a'] if r[d.LIFE] != d.REMOVED)
        assert survivor[:4] == live[0][:4] and survivor[d.OWNER] == guest.cid
        ended = list(survivor)
        ended[d.LIFE] = d.REMOVED; ended[d.COMMITTER] = guest.cid; ended[44] = 0
        removed, _ = rx.update(ctx(guest), [ended], 1.8)
        assert next(r for r in removed['a'] if r[:4] == ended[:4])[44] == 0
        print(json.dumps(dict(ok=True, max_packet_bytes=max(sizes), checks=[
            'overlapping emission identities', 'current-state late entry',
            'replica lethal claim', 'one native death committer', 'failed-send retry',
            'owner disconnect handoff', 'route-end removal without loot',
            'sender exclusion', 'team isolation'])))
    finally:
        for peer in peers: peer.socket.close()


if __name__ == '__main__':
    main()
