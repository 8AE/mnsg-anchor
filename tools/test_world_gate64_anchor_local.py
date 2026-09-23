#!/usr/bin/env python3
"""Gate64 phase authority, bootstrap and handoff against loopback Anchor only.

Runs against a local Anchor server:

    python3 tools/test_world_gate64_anchor_local.py --port 20000
"""
import argparse
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'py'))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import anchor_world as w
from test_impact_anchor_local import Peer

ROOM = w.GATE64_ROOM


def gate(index=0, phase=1, timer=0, busy=0, slot=1, paused=0, instance=0,
         receipt=0, body=(-43100, 0, 2900), child=(0, 0, 0)):
    r = [0] * w.WORDS
    r[0] = index
    r[1] = w.GATE64_ENTITY
    r[2] = w.GATE64
    r[3] = busy
    r[4:7] = list(body)
    r[10] = slot
    r[w.WG64_TIMER] = timer
    r[w.WG64_PHASE] = phase
    r[17:20] = list(child)
    r[23] = 150
    r[w.WG64_COMPLETE] = 1 if phase == 10 else 0
    r[38] = paused
    r[w.INSTANCE] = instance
    r[w.RECEIPT] = receipt
    return r


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    args = parser.parse_args()
    peers = []
    sizes = []
    try:
        lobby = 'gate64-automated-' + uuid.uuid4().hex
        peers = [Peer(args.port, lobby, team) for team in ('default', 'default', 'isolated')]
        peers.sort(key=lambda peer: (peer.team != 'default', peer.cid))
        low, high, outsider = peers
        tx, rx = w.WorldTransport(), w.WorldTransport()
        players = {}

        def ctx(peer):
            return dict(cid=peer.cid, session=peer.session, team=peer.team, room=ROOM,
                        connected=True, loaded=True, players=players)

        live, done = gate(phase=4, busy=1), gate(phase=10)
        assert w.row_valid(live) and w.row_valid(done), 'gate rows must validate'
        rx.update(ctx(high), ROOM, 42, 1, [live], '00' * 32, 0)
        tx.update(ctx(low), ROOM, 42, 1, [], '00' * 32, 0)
        for peer in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=peer.team,
                         currentRoomId=ROOM, interactionSession=peer.session,
                         worldSync=[w.VERSION, peer.session, 1, ROOM, 42])
            players[peer.cid] = dict(state, roomId=ROOM)
            peer.send(dict(type='UPDATE_CLIENT_STATE', clientId=peer.cid, state=state))
        for peer in peers:
            peer.read(.1)

        def deliver(transport, source, target, receiver, packets, now):
            assert packets, 'expected a gate packet'
            for packet in packets:
                sizes.append(len(json.dumps(packet, separators=(',', ':')).encode()) + 1)
                source.send(packet)
            transport.sent(packets, True, now)
            incoming = [p for p in target.read(.12) if p.get('type') == w.PACKET_TYPE]
            assert len(incoming) == len(packets), 'packet loss on loopback'
            for packet in incoming:
                assert receiver.receive(ctx(target), packet, now + .01)
            assert not any(p.get('type') == w.PACKET_TYPE for p in source.read(.02))
            assert not any(p.get('type') == w.PACKET_TYPE for p in outsider.read(.02))
            return incoming

        state, packets = rx.update(ctx(high), ROOM, 42, 1, [live], '00' * 32, 1)
        assert packets[0]['u'] == [], 'the gate room carries no input list'
        assert packets[0]['a'][0][w.WG64_PHASE] == 4
        assert packets[0]['a'][0][w.INSTANCE] == 0
        assert packets[0]['a'][0][w.RECEIPT] == 0
        deliver(rx, high, low, tx, packets, 1)

        # An active native camera keeps ownership during pause. Its observer
        # must hold the checkpoint rather than start a competing continuation.
        state, packets = rx.update(ctx(high), ROOM, 42, 1,
                                   [gate(phase=4, busy=1, paused=1)], '00' * 32, 1.10)
        deliver(rx, high, low, tx, packets, 1.10)
        observed, _ = tx.update(ctx(low), ROOM, 42, 1, [gate(phase=4)], '00' * 32, 1.12)
        assert tx.owners[0] == high.cid
        assert observed['a'][0][w.DELIVERY_ROW + 38] == 1

        # The completed phase outranks the running one even from the higher id.
        state, packets = rx.update(ctx(high), ROOM, 42, 1, [done], '00' * 32, 1.20)
        deliver(rx, high, low, tx, packets, 1.20)
        observed, _ = tx.update(ctx(low), ROOM, 42, 1, [gate(phase=4)], '00' * 32, 1.22)
        assert tx.owners[0] == high.cid, 'completed gate did not win authority'
        offer = observed['a'][0]
        assert offer[0] == high.cid and offer[w.DELIVERY_ROW + w.WG64_PHASE] == 10
        assert offer[w.DELIVERY_ROW + w.WG64_COMPLETE] == 1

        # A late entrant bootstraps with the live phase, then acknowledges.
        tx.reset()
        tx.update(ctx(low), ROOM, 42, 1, [], '00' * 32, 1.30)
        # The unchanged completed row uses the normal one-second refresh.
        state, packets = rx.update(ctx(high), ROOM, 42, 1, [done], '00' * 32, 3.20)
        deliver(rx, high, low, tx, packets, 3.20)
        fresh = gate(phase=10, instance=77)
        entry, _ = tx.update(ctx(low), ROOM, 42, 1, [fresh], '00' * 32, 3.30)
        assert 0 not in tx.established
        offer = entry['a'][0]
        assert offer[w.DELIVERY_ROW + w.RECEIPT] & w.BOOTSTRAP
        assert offer[w.DELIVERY_ROW + w.INSTANCE] == 77
        assert offer[w.DELIVERY_ROW + w.WG64_PHASE] == 10
        applied = offer[w.DELIVERY_ROW:]
        tx.update(ctx(low), ROOM, 42, 1, [applied], '00' * 32, 3.40)
        assert 0 in tx.established

        # A paused owner hands the gate to the unpaused peer.
        state, packets = rx.update(ctx(high), ROOM, 42, 1, [gate(phase=10, paused=1)],
                                   '00' * 32, 3.50)
        assert packets[0]['z'] == '01' + '00' * 31
        deliver(rx, high, low, tx, packets, 3.50)
        # Native captures keep their task incarnation after acknowledging it.
        handed, packets = tx.update(ctx(low), ROOM, 42, 1, [applied], '00' * 32, 3.60)
        assert handed['a'][0][0] == low.cid and packets
        assert handed['a'][0][w.DELIVERY_ROW+w.RECEIPT] == 0
        assert tx.owners[0] == low.cid

        # Owner departure keeps the survivor authoritative.
        high.socket.close()
        assert any(p.get('type') == 'ALL_CLIENT_STATE' for p in low.read(.3))
        players[high.cid] = dict(players[high.cid], online=False)
        _, packets = tx.update(ctx(low), ROOM, 42, 1, [applied], '00' * 32, 5.0)
        assert tx.owners[0] == low.cid
        if packets:
            assert packets[0]['a'][0][w.WG64_PHASE] == 10

        # A kind 10 row never becomes state in another room.
        stray_world = w.WorldTransport()
        stray_ctx = dict(ctx(outsider), room=0x12e)
        stray_world.update(stray_ctx, 0x12e, 42, 1, [], '00' * 32, 5.1)
        stray, stray_packets = stray_world.update(stray_ctx, 0x12e, 42, 1,
                                                  [done], '00' * 32, 5.2)
        assert not stray['a'] and not stray_packets

        print(json.dumps(dict(ok=True, max_packet_bytes=max(sizes), checks=[
            'kind10 schema on the wire', 'no input channel in the gate room',
            'completed phase wins authority', 'late entry bootstrap receipt',
            'phase-only progress', 'paused camera owner retains authority',
            'paused camera-free owner hands off',
            'owner disconnect handoff', 'room scope', 'sender exclusion',
            'team isolation'])))
    finally:
        for peer in peers:
            try:
                peer.socket.close()
            except OSError:
                pass


if __name__ == '__main__':
    main()
