#!/usr/bin/env python3
"""File51 Super Pass bridge: latch aggregate, bootstrap, ack and handoff.

Runs against a loopback Anchor only:

    python3 tools/test_world_bridge_anchor_local.py --port 20000
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

ROOM = w.BRIDGE_ROOM


def bridge(flags=0, fresh=0, gate=1, guard0=1, guard1=1, blocker=0, paused=0,
           temp=0, index=0):
    r = [0] * w.WORDS
    r[0] = index
    r[1] = w.BRIDGE_ENTITY
    r[2] = w.BRIDGE
    r[w.WB_FLAGS] = flags
    r[w.WB_FRESH] = fresh
    r[w.WB_GATE_PHASE] = gate
    r[w.WB_BLOCKER_REMOVED] = blocker
    r[w.WB_GUARD_0] = guard0
    r[w.WB_GUARD_1] = guard1
    r[w.WB_GUARD_0 + 13] = 0
    r[w.WB_GUARD_1 + 13] = 0
    r[w.WB_PAUSED] = paused
    r[w.WB_INPUT] = temp
    return r


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    args = parser.parse_args()
    peers = []
    sizes = []
    try:
        lobby = 'bridge-automated-' + uuid.uuid4().hex
        peers = [Peer(args.port, lobby, t) for t in ('default', 'default', 'isolated')]
        peers.sort(key=lambda p: (p.team != 'default', p.cid))
        host, guest, outsider = peers
        players = {}

        def ctx(peer):
            return dict(cid=peer.cid, session=peer.session, team=peer.team, room=ROOM,
                        connected=True, loaded=True, players=players)

        worlds = [w.WorldTransport(), w.WorldTransport()]
        pairs = list(zip(worlds, (host, guest)))
        row = bridge(temp=1)
        assert w.row_valid(row), 'bridge row must validate'
        for world, peer in pairs:
            world.update(ctx(peer), ROOM, 42, 1, [row], '00' * 32, 0)
        for peer in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=peer.team,
                         currentRoomId=ROOM, interactionSession=peer.session,
                         worldSync=[w.VERSION, peer.session, 1, ROOM, 42])
            players[peer.cid] = dict(state, roomId=ROOM)
            peer.send(dict(type='UPDATE_CLIENT_STATE', clientId=peer.cid, state=state))
        for peer in peers:
            peer.read(.1)

        # Roster presence so both sides agree on the room and the placed root.
        for world, peer in pairs:
            _, packets = world.update(ctx(peer), ROOM, 42, 1, [row], '00' * 32, .5)
            for packet in packets:
                peer.send(packet)
            world.sent(packets, True, .5)
        for world, peer in pairs:
            for packet in peer.read(.12):
                if packet.get('type') == w.PACKET_TYPE:
                    assert world.receive(ctx(peer), packet, .6)
        outsider.read(.1)

        host_world, guest_world = worlds
        host_ctx, guest_ctx = ctx(host), ctx(guest)

        def deliver(transport, source, target, receiver, packets, now):
            assert packets, 'expected a bridge packet'
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

        # The owner publishes with its own latch; the wire keeps 46/47 clear.
        host_row = bridge(temp=1)
        state, packets = host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 1)
        assert len(state['a']) == 1
        assert packets[0]['a'][0][w.WB_INPUT] == 0
        assert packets[0]['a'][0][w.WB_AGGREGATE] == 0
        assert packets[0]['u'] == [[0, 1]]
        sizes.append(len(json.dumps(packets[0], separators=(',', ':')).encode()) + 1)
        deliver(host_world, host, guest, guest_world, packets, 1)

        # The replica applies the owner row with its own latch and the OR.
        guest_row = bridge(temp=2)
        state, packets = guest_world.update(guest_ctx, ROOM, 42, 1, [guest_row], '00' * 32, 1.02)
        offer = state['a'][0]
        assert offer[0] == host.cid
        assert offer[w.DELIVERY_ROW + w.WB_INPUT] == 2
        assert offer[w.DELIVERY_ROW + w.WB_AGGREGATE] == 3
        deliver(guest_world, guest, host, host_world, packets, 1.04)

        # The owner echo returns the aggregate with a zero receipt.
        echo_state, _ = host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 1.06)
        echo = echo_state['a'][0]
        assert echo[0] == host.cid
        assert echo[w.DELIVERY_ROW + w.WB_INPUT] == 1
        assert echo[w.DELIVERY_ROW + w.WB_AGGREGATE] == 3
        assert echo[w.DELIVERY_ROW + w.RECEIPT] == 0

        # A paused bridge row still propagates its latch.
        paused = bridge(temp=2, paused=1)
        _, packets = guest_world.update(guest_ctx, ROOM, 42, 1, [paused], '00' * 32, 1.1)
        assert packets[0]['u'] == [[0, 2]]
        assert packets[0]['z'] == '01' + '00' * 31
        deliver(guest_world, guest, host, host_world, packets, 1.1)
        echo_state, _ = host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 1.12)
        assert echo_state['a'][0][w.DELIVERY_ROW + w.WB_AGGREGATE] == 3

        # A late entrant bootstraps with the current state and the live latches.
        guest_world.reset()
        guest_world.update(guest_ctx, ROOM, 42, 1, [], '00' * 32, 1.14)
        host_row = bridge(flags=1, guard0=2, guard1=2, temp=1)
        _, packets = host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 1.21)
        deliver(host_world, host, guest, guest_world, packets, 1.21)
        fresh = bridge(temp=2)
        fresh[w.INSTANCE] = 77
        entry, _ = guest_world.update(guest_ctx, ROOM, 42, 1, [fresh], '00' * 32, 1.22)
        offer = entry['a'][0]
        assert offer[w.DELIVERY_ROW + w.RECEIPT] & w.BOOTSTRAP
        assert offer[w.DELIVERY_ROW + w.INSTANCE] == 77
        assert offer[w.DELIVERY_ROW + w.WB_AGGREGATE] == 3
        guest_world.update(guest_ctx, ROOM, 42, 1, [offer[w.DELIVERY_ROW:]], '00' * 32, 1.24)
        assert 0 in guest_world.established

        # Established native receipt outranks a fresh save-1 constructor pose.
        live = bridge(flags=1, guard0=2, guard1=2, temp=1)
        live[w.WB_GUARD_0 + 13] = 8
        fresh_pose = bridge(flags=3, fresh=1, gate=3, guard0=3, guard1=3)
        assert w.controller_progress(fresh_pose) > w.controller_progress(live)
        _, packets = host_world.update(host_ctx, ROOM, 42, 1, [live], '00' * 32, 1.3)
        deliver(host_world, host, guest, guest_world, packets, 1.3)
        handed, _ = guest_world.update(guest_ctx, ROOM, 42, 1, [fresh_pose], '00' * 32, 1.32)
        assert handed['a'][0][0] == host.cid, 'fresh pose outranked an established route'

        # A failed send retries on a new sequence, never reusing the old one.
        _, packets = host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 2.5)
        assert packets
        stale = packets[0]['q']
        host_world.sent(packets, False, 2.5)
        assert not host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 2.51)[1]
        retry = host_world.update(host_ctx, ROOM, 42, 1, [host_row], '00' * 32, 2.56)[1]
        assert retry[0]['q'] == stale + 1

        # Owner departure hands the root to the survivor.
        host.socket.close()
        assert any(p.get('type') == 'ALL_CLIENT_STATE' for p in guest.read(.3))
        players[host.cid]['online'] = False
        survivor, _ = guest_world.update(guest_ctx, ROOM, 42, 1, [bridge(temp=2)],
                                         '00' * 32, 3.0)
        assert survivor['a'][0][0] == guest.cid

        print(json.dumps(dict(ok=True, max_packet_bytes=max(sizes), checks=[
            'kind9 schema accepted on the wire', 'wire keeps 46/47 clear',
            'latch aggregate on the replica', 'owner echo carries the aggregate',
            'paused latch still propagates', 'late entry bootstrap with live latches',
            'established receipt outranks a fresh save-1 pose',
            'failed send retries on a new sequence', 'owner disconnect handoff',
            'sender exclusion', 'team isolation'])))
    finally:
        for peer in peers:
            try:
                peer.socket.close()
            except OSError:
                pass


if __name__ == '__main__':
    main()
