#!/usr/bin/env python3
"""Three bounded TCP clients against an explicitly supplied loopback Anchor."""
import argparse
import copy
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "py"))
from test_impact_anchor_local import Peer
from anchor_world import WorldTransport, VERSION, PACKET_TYPE as PLACED_PACKET
from anchor_world_dynamic import DynamicTransport, PACKET_TYPE, WORDS, REMOVED, CLAIM


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()
    peers = []
    try:
        room = "world-children-" + uuid.uuid4().hex
        for team in ("default", "default", "isolated"):
            peers.append(Peer(args.port, room, team))
        host, guest, outsider = peers
        players = {}

        def ctx(p):
            return dict(cid=p.cid, session=p.session, team=p.team, room=302,
                        connected=True, loaded=True, players=players)

        tx_world, rx_world = WorldTransport(), WorldTransport()
        tx, rx = DynamicTransport(tx_world), DynamicTransport(rx_world)
        for placed, dynamic, p in ((tx_world, tx, host), (rx_world, rx, guest)):
            placed.update(ctx(p), 302, 42, 1, [], "00" * 32, 0)
            dynamic.update(ctx(p), [], 0)
        for p in peers:
            state = dict(online=True, isSaveLoaded=True, teamId=p.team,
                         currentRoomId=302, interactionSession=p.session,
                         worldSync=[VERSION, p.session, 1, 302, 42])
            players[p.cid] = dict(state, roomId=302)
            p.send(dict(type="UPDATE_CLIENT_STATE", clientId=p.cid, state=state))
        for p in peers:
            p.read(.1)

        for placed, p in ((tx_world, host), (rx_world, guest)):
            _, initial = placed.update(ctx(p),302,42,1,[],"00"*32,.5)
            for packet in initial:
                p.send(packet)
            placed.sent(initial,True,.5)
        for placed, p in ((tx_world, host), (rx_world, guest)):
            for packet in p.read(.1):
                if packet.get("type")==PLACED_PACKET:
                    assert placed.receive(ctx(p),packet,.6)

        rows = []
        for i in range(128):
            row = [0] * WORDS
            row[:12] = [0, 0, 0, i + 1, 0, 0, 5, 1, 0x19a, 0x191, 0, 1]
            row[12:15] = [i * 100, 4000, -500]
            row[24:27] = [1000] * 3
            row[31:33] = [42, 163]
            row[42:44] = [6, -2250]
            rows.append(row)
        # One dropped coin among already-moving hazards.
        rows[0][6] = 2
        rows[0][9:12] = [1, 4, 0]
        rows[0][42] = 1

        state, packets = tx.update(ctx(host), rows, 1)
        assert len(packets) == 11
        for p in packets:
            host.send(p)
        tx.sent(packets, True, 1)
        incoming = [p for p in guest.read(.2) if p.get("type") == PACKET_TYPE]
        assert len(incoming) == 11
        for p in reversed(incoming[:-1]):
            assert rx.receive(ctx(guest), p, 1.1)
        assert not rx.peers  # incomplete batches never reconstruct half a set
        assert rx.receive(ctx(guest), incoming[-1], 1.1)
        assert not rx.receive(ctx(guest), incoming[-1], 1.1)
        result, _ = rx.update(ctx(guest), [], 1.12)
        assert result["a"] == state["a"]
        assert result["a"][4][31] == 42 and result["a"][4][43] == -2250
        assert not any(p.get("type") == PACKET_TYPE for p in host.read(.05))
        assert not any(p.get("type") == PACKET_TYPE for p in outsider.read(.05))

        claims = copy.deepcopy(result["a"])
        coin_index=next(i for i,r in enumerate(claims) if r[6]==2)
        claims[coin_index][5] = CLAIM
        result, packets = rx.update(ctx(guest), claims, 1.3)
        assert result["a"][coin_index][5] == 0
        for p in packets:
            guest.send(p)
        rx.sent(packets, True, 1.3)
        for p in host.read(.2):
            if p.get("type") == PACKET_TYPE:
                assert tx.receive(ctx(host), p, 1.4)
        committed, packets = tx.update(ctx(host), state["a"], 1.5)
        assert committed["a"][coin_index][4:6] == [guest.cid, REMOVED]
        assert not any(r[5] == REMOVED for r in tx.native_result(committed)["a"])
        for p in packets:
            host.send(p)
        tx.sent(packets, True, 1.5)
        assert tx.native_result(committed)["a"][coin_index][5] == REMOVED
        for p in guest.read(.2):
            if p.get("type") == PACKET_TYPE:
                assert rx.receive(ctx(guest), p, 1.6)
        result, _ = rx.update(ctx(guest), claims, 1.7)
        assert result["a"][coin_index][4:6] == [guest.cid, REMOVED]

        # Close the actual origin socket, then consume its roster departure.
        host.socket.close()
        departed = guest.read(.3)
        assert any(p.get("type") == "ALL_CLIENT_STATE" for p in departed)
        players[host.cid]["online"] = False
        live = [r for r in result["a"] if r[5] != REMOVED]
        result, _ = rx.update(ctx(guest), live, 2.1)
        assert len([r for r in result["a"] if r[5] != REMOVED]) == 127
        assert all(r[4] == guest.cid for r in result["a"] if r[5] != REMOVED)
        assert all(r[0] == host.cid for r in result["a"] if r[5] != REMOVED)
        print(json.dumps(dict(ok=True, actors=128, parts=11,
            max_packet_bytes=max(len(json.dumps(p, separators=(",", ":")).encode()) + 1 for p in incoming),
            checks=["atomic late-entry current state", "duplicate rejection",
                    "sender exclusion", "team isolation", "pickup claim/commit",
                    "native award waits for commit send", "origin disconnect/handoff"])))
    finally:
        for p in peers:
            p.socket.close()


if __name__ == "__main__":
    main()
