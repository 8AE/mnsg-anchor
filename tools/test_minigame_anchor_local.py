#!/usr/bin/env python3
"""Probe minigame sessions on a disposable loopback Anchor, never public hosts.

Start exact Anchor bf7b43c10b19428ceba54772c7bae3abca44a345 separately and
pass --port. Two same-team clients plus one different-team observer are used.
"""

import argparse
import json
from pathlib import Path
import select
import socket
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "py"))
import anchor_minigame as mg


class Peer:
    def __init__(self, port, room, team, session):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=2)
        self.socket.settimeout(None)
        self.buffer = b""
        self.cid = 0
        self.team, self.session, self.room = team, session, 10
        self.players = {}
        self.engine = mg.MinigameTransport()
        self.received, self.sent = [], []
        self.send({"type": "HANDSHAKE", "clientId": 0, "roomId": room,
                   "clientState": self.state(), "roomState": {}})
        for packet in self.read(0.15):
            if packet.get("type") == "ALL_CLIENT_STATE":
                self.roster(packet["state"])
                for state in packet["state"]:
                    if state.get("self"):
                        self.cid = state["clientId"]
        assert self.cid, "Anchor did not assign an ID"
        self.engine._scope(self.context())

    def context(self):
        return {"cid": self.cid, "session": self.session, "team": self.team,
                "connected": True, "loaded": True, "room": self.room, "players": self.players}

    def state(self):
        return {"clientId": self.cid, "name": f"Minigame probe {self.session}",
                "online": True, "isSaveLoaded": True, "teamId": self.team,
                "interactionSession": self.session, "currentRoomId": self.room,
                "playerEpoch": 1, mg.METADATA_KEY: self.engine.advertisement(self.context())}

    def normalise(self, state):
        return {**state, "roomId": state.get("currentRoomId", -1), "_minigameReceived": time.monotonic()}

    def roster(self, states):
        self.players = {s["clientId"]: self.normalise(s) for s in states}

    def send(self, packet):
        wire = (json.dumps(packet, separators=(",", ":"), allow_nan=False) + "\0").encode()
        if packet.get("type") == mg.PACKET_TYPE:
            assert len(wire) <= mg.PACKET_BYTES
        self.socket.sendall(wire)
        self.sent.append(packet)

    def read(self, seconds):
        result = []
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            ready, _, _ = select.select([self.socket], [], [], max(0, end - time.monotonic()))
            if not ready:
                break
            data = self.socket.recv(65536)
            assert data, "Anchor disconnected a probe"
            self.buffer += data
            while b"\0" in self.buffer:
                raw, self.buffer = self.buffer.split(b"\0", 1)
                packet = json.loads(raw)
                if packet.get("type") != "HEARTBEAT":
                    result.append(packet)
        return result

    def metadata(self):
        state = self.state()
        self.players[self.cid] = self.normalise(state)
        self.send({"type": "UPDATE_CLIENT_STATE", "clientId": self.cid, "state": state})

    def step(self, game, visit, native, now):
        status, packets = self.engine.update(self.context(), game, visit, native, now)
        self.metadata()
        for p in packets:
            self.send(p)
            self.engine.packet_send_result(p, True)
        return status


def pump(peers, now):
    for peer in peers:
        for p in peer.read(0.08):
            kind = p.get("type")
            if kind == "ALL_CLIENT_STATE":
                peer.roster(p["state"])
            elif kind == "UPDATE_CLIENT_STATE":
                peer.players[p["clientId"]] = peer.normalise(p["state"])
            elif kind in (mg.PACKET_TYPE, mg.INPUT_TYPE, mg.RESULT_TYPE):
                assert p["clientId"] != peer.cid, "Anchor echoed the sender"
                if kind == mg.INPUT_TYPE:
                    assert p["targetClientId"] == peer.cid
                else:
                    assert p["targetTeamId"] == peer.team, "Cross-team packet"
                assert "addToQueue" not in p, "Transient minigame packet was queued"
                assert peer.engine.receive(peer.context(), p, now), (kind, p)
                peer.received.append(p)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()
    assert 1 <= args.port <= 65535
    peers = []
    try:
        room = "minigame-local-" + uuid.uuid4().hex
        host = Peer(args.port, room, "blue", 101)
        peers.append(host)
        guest = Peer(args.port, room, "blue", 202)
        peers.append(guest)
        observer = Peer(args.port, room, "red", 303)
        peers.append(observer)
        pump(peers, time.monotonic())
        for game, room_id in mg.ROOMS.items():
            now = time.monotonic()
            host.room = room_id
            native = {"ready": 1, "running": 1, "c": [4, 900 - game, 7, 2],
                      "a": [[1, i, 1] + [-2147483648] * 29 for i in range(mg.MAX_ACTORS)]}
            status = host.step(game, game, native, now)
            assert status["isOwner"] == 1
            pump(peers, now)
            assert guest.room == 10, "Join preparation was tested only inside the room"
            assert guest.engine.prepare_join(guest.context(), game, host.cid, 101, game, now)
            guest.room = room_id
            local = {**native, "c": [0, 0, 0, 0]}
            joined = guest.step(game, game + 10, local, now + 0.1)
            assert joined["ready"] == 1 and not joined["isOwner"]
            assert joined["c"] == native["c"] and joined["a"] == native["a"]
            pump(peers, now + 0.1)
            guest.step(game, game + 10, {**local, "inputs": [[1, 2, 42]]}, now + 0.2)
            pump(peers, now + 0.2)
            owner = host.step(game, game, native, now + 0.3)
            assert owner["inputs"] == [[guest.cid, 202, game + 10, 1, 2, 42]]
            pump(peers, now + 0.3)
            host.step(game, game, {**native, "inputAck": [[guest.cid, 202, game + 10, 1]]}, now + 0.5)
            pump(peers, now + 0.5)
            assert not guest.engine.input_pending, "Owner snapshot did not acknowledge input"
            host.step(game, game, {**native, "result": 1}, now + 0.6)
            pump(peers, now + 0.6)
            assert guest.engine.terminal == 1
            guest.room = host.room = 10
            guest.engine.room_changed(guest.context(), 10)
            host.engine.room_changed(host.context(), 10)
            guest.metadata()
            host.metadata()
            pump(peers, now + 0.7)
        # Origin departure: only the explicitly joined ready client may resume
        # the complete graph after the lease, retaining the original token.
        now = time.monotonic()
        host.room = mg.ROOMS[1]
        native["c"] = [4, 555, 8, 2]
        host.step(1, 4, native, now)
        pump(peers, now)
        assert guest.engine.prepare_join(guest.context(), 1, host.cid, 101, 4, now)
        guest.room = mg.ROOMS[1]
        guest.step(1, 14, native, now + 0.1)
        pump(peers, now + 0.1)
        token = list(host.engine.t)
        host.socket.close()
        pump([guest, observer], now + 0.2)
        takeover = guest.step(1, 14, {**native, "c": [0, 0, 0, 0]}, now + 3.2)
        assert takeover["isOwner"] and takeover["takeover"]
        assert takeover["token"] == token and takeover["generation"] == 2
        assert takeover["tick"] == 0 and takeover["c"] == native["c"]
        resumed = guest.step(1, 14, {**native, "adoptedGeneration": 2}, now + 3.4)
        assert resumed["tick"] == 1 and resumed["takeover"] == 0
        pump([guest, observer], now + 3.4)
        assert observer.received == [], "Other team received minigame packets"
        print(json.dumps({"passed": True, "games": 3, "clients": 3,
              "anchorRevision": "bf7b43c10b19428ceba54772c7bae3abca44a345",
              "guestPackets": len(guest.received), "otherTeamPackets": 0,
              "handoff": True, "actorCapacity": mg.MAX_ACTORS,
              "maxSyncBytes": max(mg.wire_bytes(p) for p in host.sent if p.get("type") == mg.PACKET_TYPE)},
             separators=(",", ":")))
    finally:
        for peer in peers:
            peer.socket.close()


if __name__ == "__main__":
    main()
