#!/usr/bin/env python3
"""File62 container and nested Silver Doll against a loopback Anchor only."""
import argparse
import copy
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'py'))
from test_impact_anchor_local import Peer
import anchor_world as w
import anchor_world_dynamic as d

ROOM = 0x16a


def container(cycle=1, phase=2, pitch=70):
    r = [0] * w.WORDS
    r[:3] = [7, 0x3d6, 11]
    r[4:10] = [-1000, 2100, -12600, pitch, 79, 0]
    r[11:14] = [cycle, phase, 1]
    return r


def doll(y=8000):
    r = [0] * d.WORDS
    r[:12] = [0, 0, 0, 1, 0, 0, 7, 8, 0, 0, 2, 0]
    r[12:15] = [-600, y, -10200]
    r[24:27] = [1000] * 3
    r[32] = 163
    r[42] = 17 if y == 3400 else 16
    r[64] = 1
    return r


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    args = parser.parse_args()
    peers, sizes = [], []
    try:
        lobby = 'doll-native-' + uuid.uuid4().hex
        peers = [Peer(args.port, lobby, team) for team in ('default', 'default', 'isolated')]
        peers.sort(key=lambda p: (p.team != 'default', p.cid))
        low, high, outsider = peers
        players = {}

        def ctx(p):
            return dict(cid=p.cid, session=p.session, team=p.team, room=ROOM,
                        connected=True, loaded=True, players=players)

        wa, wb = w.WorldTransport(), w.WorldTransport()
        a, b = d.DynamicTransport(wa), d.DynamicTransport(wb)
        for world, dynamic, p in ((wa, a, high), (wb, b, low)):
            world.update(ctx(p), ROOM, 42, 1, [], '00' * 32, 0)
            dynamic.update(ctx(p), [], 0)
        for p in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=p.team,
                         currentRoomId=ROOM, interactionSession=p.session,
                         worldSync=[w.VERSION, p.session, 1, ROOM, 42])
            players[p.cid] = dict(state, roomId=ROOM)
            p.send(dict(type='UPDATE_CLIENT_STATE', clientId=p.cid, state=state))
        for p in peers:
            p.read(.1)

        def relay(tx, source, target, rx, packets, now, packet_type):
            assert packets
            for packet in packets:
                assert packet['clientId'] == source.cid and packet['targetTeamId'] == source.team
                assert 'addToQueue' not in packet and 'targetClientId' not in packet
                sizes.append(len(json.dumps(packet, separators=(',', ':')).encode()) + 1)
                source.send(packet)
            tx.sent(packets, True, now)
            incoming = [p for p in target.read(.15) if p.get('type') == packet_type]
            assert len(incoming) == len(packets)
            for p in incoming:
                assert rx.receive(ctx(target), p, now + .01)
            assert not any(p.get('type') == packet_type for p in source.read(.02))
            assert not any(p.get('type') == packet_type for p in outsider.read(.02))
            return incoming

        # The lower-id entrant accepts the already opened container first.
        _, packets = wa.update(ctx(high), ROOM, 42, 1, [container()], '00' * 32, 1)
        relay(wa, high, low, wb, packets, 1, w.PACKET_TYPE)
        entry, packets = wb.update(ctx(low), ROOM, 42, 1,
                                   [container(0, 0, 0)], '00' * 32, 1.1)
        assert wb.owners[7] == high.cid
        assert entry['a'][0][w.DELIVERY_ROW + 12] == 2
        relay(wb, low, high, wa, packets, 1.1, w.PACKET_TYPE)

        # It reconstructs a reward halfway down, never a second full-height drop.
        state, packets = a.update(ctx(high), [doll()], 1.2)
        relay(a, high, low, b, packets, 1.2, d.PACKET_TYPE)
        observed, _ = b.update(ctx(low), [doll(12500)], 1.25)
        assert len(observed['a']) == 1 and observed['a'][0][13] == 8000
        assert observed['a'][0][:4] == [0x7ffffffa, 42, ROOM + 1, 8]
        assert observed['a'][0][4] == high.cid
        claim = copy.deepcopy(observed['a'])
        claim[0][5] = d.CLAIM
        _, packets = b.update(ctx(low), claim, 1.4)
        relay(b, low, high, a, packets, 1.4, d.PACKET_TYPE)
        committed, packets = a.update(ctx(high), state['a'], 1.6)
        assert committed['a'][0][4:6] == [low.cid, d.REMOVED]
        assert not a.native_result(committed)['a']
        a.sent(packets, False, 1.6)
        assert not a.native_result(committed)['a']
        committed, packets = a.update(ctx(high), state['a'], 1.8)
        relay(a, high, low, b, packets, 1.8, d.PACKET_TYPE)
        assert a.native_result(committed)['a'][0][5] == d.REMOVED
        observed, _ = b.update(ctx(low), claim, 1.9)
        assert observed['a'][0][4:6] == [low.cid, d.REMOVED]

        _, packets = wa.update(ctx(high), ROOM, 42, 1, [container(1, 0, 0)], '00' * 32, 2.0)
        relay(wa, high, low, wb, packets, 2.0, w.PACKET_TYPE)
        closed, _ = wb.update(ctx(low), ROOM, 42, 1, [container(0, 0, 0)], '00' * 32, 2.02)
        assert closed['a'][0][w.DELIVERY_ROW+11:w.DELIVERY_ROW+13] == [1, 0]
        reopened = closed['a'][0][w.DELIVERY_ROW:]
        reopened[7] = 0
        reopened[11:13] = [2, 1]
        _, packets = wb.update(ctx(low), ROOM, 42, 1, [reopened], '00' * 32, 2.1)
        assert wb.owners[7] == low.cid
        relay(wb, low, high, wa, packets, 2.1, w.PACKET_TYPE)
        remote, _ = wa.update(ctx(high), ROOM, 42, 1, [container(1, 0, 0)], '00' * 32, 2.12)
        assert wa.owners[7] == low.cid
        assert remote['a'][0][w.DELIVERY_ROW+11:w.DELIVERY_ROW+13] == [2, 1]

        high.socket.close()
        assert any(p.get('type') == 'ALL_CLIENT_STATE' for p in low.read(.2))
        players[high.cid]['online'] = False
        after, _ = b.update(ctx(low), [doll()], 2.2)
        assert len(after['a']) == 1 and after['a'][0][5] == d.REMOVED
        print(json.dumps(dict(ok=True, max_packet_bytes=max(sizes), checks=[
            'container bootstrap and reopening', 'late-entry descent', 'duplicate birth coalescing',
            'collector arbitration', 'failed-send award barrier', 'removal survives owner departure',
            'sender exclusion', 'team isolation', 'bounded team-only transient packets'])))
    finally:
        for p in peers:
            p.socket.close()


if __name__ == '__main__':
    main()
