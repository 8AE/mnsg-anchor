#!/usr/bin/env python3
"""Crane bootstrap, shared pad inputs and expiry against loopback Anchor only."""
import argparse
import json
from pathlib import Path
import sys
import uuid
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'py'))
import anchor_world as w
from test_impact_anchor_local import Peer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    args = parser.parse_args()
    peers = []
    try:
        lobby = 'crane-automated-' + uuid.uuid4().hex
        peers = [Peer(args.port, lobby, team) for team in ('default', 'default', 'isolated')]
        peers.sort(key=lambda peer: (peer.team != 'default', peer.cid))
        low, high, outsider = peers
        tx, rx = w.WorldTransport(), w.WorldTransport()
        players = {}
        sizes = []
        def ctx(peer):
            return dict(cid=peer.cid, session=peer.session, team=peer.team, room=0x31,
                        connected=True, loaded=True, players=players)
        def row(instance, pad):
            r = [0] * w.WORDS
            r[:3] = [5, w.CRANE_ENTITY, w.CRANE]
            r[10] = 1; r[18] = 6; r[19] = 3; r[22] = 2; r[23] = 10
            r[4:7] = [-9900, -5000, -3800]; r[26:29] = [200] * 3
            r[30] = 1; r[37] = 3; r[34:37] = [-9900, -5000, -3800]
            r[41] = 200; r[w.INSTANCE] = instance; r[w.CRANE_INPUT] = pad
            assert w.row_valid(r)
            return r
        a, b = row(10, 1), row(20, 2)
        # The existing higher-ID simulator establishes before the newcomer arrives.
        rx.update(ctx(high), 0x31, 42, 1, [b], '00'*32, 0)
        for peer in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=peer.team,
                         currentRoomId=0x31, interactionSession=peer.session,
                         worldSync=[w.VERSION, peer.session, 1, 0x31, 42])
            players[peer.cid] = dict(state, roomId=0x31)
            peer.send(dict(type='UPDATE_CLIENT_STATE', clientId=peer.cid, state=state))
        for peer in peers: peer.read(.1)
        tx.update(ctx(low), 0x31, 42, 1, [a], '00'*32, .1)
        def publish(transport, source, target, receiver, local, now):
            result, packets = transport.update(ctx(source), 0x31, 42, 1, [local], '00'*32, now)
            assert packets
            for packet in packets:
                sizes.append(len(json.dumps(packet, separators=(',', ':')).encode())+1)
                for wire in packet['a']:
                    assert not any(wire[i] for i in (w.CRANE_INPUT,w.CRANE_AGGREGATE,w.INSTANCE,w.RECEIPT))
                source.send(packet)
            transport.sent(packets, True, now)
            incoming = [p for p in target.read(.12) if p.get('type') == w.PACKET_TYPE]
            assert len(incoming) == len(packets)
            for packet in incoming: assert receiver.receive(ctx(target), packet, now+.01)
            assert not any(p.get('type') == w.PACKET_TYPE for p in source.read(.02))
            assert not any(p.get('type') == w.PACKET_TYPE for p in outsider.read(.02))
            return result
        publish(rx, high, low, tx, b, 1)
        offered, _ = tx.update(ctx(low), 0x31, 42, 1, [a], '00'*32, 1.02)
        delivery = offered['a'][0]
        assert delivery[0] == high.cid and delivery[2+w.RECEIPT] & w.BOOTSTRAP
        assert delivery[2+w.CRANE_AGGREGATE] == 3 and delivery[2+w.INSTANCE] == 10
        # Simulate the native apply receipt; production C has a separate harness.
        a = delivery[2:]
        result = publish(tx, low, high, rx, a, 1.1)
        assert result['a'][0][0] == low.cid and not result['a'][0][2+w.RECEIPT]
        assert result['a'][0][2+w.CRANE_AGGREGATE] == 3
        result, _ = rx.update(ctx(high), 0x31, 42, 1, [b], '00'*32, 1.12)
        assert result['a'][0][0] == low.cid and result['a'][0][2+w.CRANE_AGGREGATE] == 3
        b = result['a'][0][2:]; b[w.CRANE_INPUT] = 1
        publish(rx, high, low, tx, b, 1.2)
        a[w.CRANE_INPUT] = 0
        result = publish(tx, low, high, rx, a, 1.3)
        assert result['a'][0][2+w.CRANE_AGGREGATE] == 1  # The other player still holds it.
        b[38] = 1
        publish(rx, high, low, tx, b, 1.4)
        result, _ = tx.update(ctx(low), 0x31, 42, 1, [a], '00'*32, 1.42)
        assert result['a'][0][2+w.CRANE_AGGREGATE] == 0
        b[38] = 0
        publish(rx, high, low, tx, b, 1.5)
        result, _ = tx.update(ctx(low), 0x31, 42, 1, [a], '00'*32, 3.1)
        assert result['a'][0][2+w.CRANE_AGGREGATE] == 0
        print(json.dumps(dict(ok=True, max_packet_bytes=max(sizes), checks=[
            'native receipt bootstrap before lower-ID takeover', 'atomic crane/pads/reward',
            'owner input echo', 'different pad aggregation', 'release preserves another holder',
            'pause removes contribution', 'expiry removes contribution',
            'local-only words stripped', 'sender exclusion', 'team isolation'])))
    finally:
        for peer in peers: peer.socket.close()


if __name__ == '__main__':
    main()
