#!/usr/bin/env python3
"""Exercise Control Machine through a disposable Anchor on 127.0.0.1.

Start a local Anchor server first and pass its port explicitly. This creates a
unique private room with two teammates and one different-team observer.
"""

import argparse
import copy
import json
from pathlib import Path
import select
import socket
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "py"))
import anchor_control_machine as cm


def initial_advertisement(session):
    return [cm.VERSION, 0, 0, 0, 1, session, 0, 0, 0, 0, 0, 0, 0, 0]


class Peer:
    def __init__(self, port, room, team, session):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=2)
        self.socket.settimeout(None)
        self.buffer = b""
        self.cid = 0
        self.team = team
        self.session = session
        self.players = {}
        self.engine = cm.ControlMachineTransport()
        self.received = []
        self.sent = []
        self.send({
            "type": "HANDSHAKE", "clientId": 0, "roomId": room,
            "clientState": {
                "name": f"Control Machine local {session}",
                "online": True, "teamId": team, "isSaveLoaded": True,
                "currentRoomId": cm.ROOM, "interactionSession": session,
                "playerEpoch": 7,
                cm.METADATA_KEY: initial_advertisement(session),
            }, "roomState": {},
        })
        for packet in self.read(0.25):
            if packet.get("type") == "ALL_CLIENT_STATE":
                for state in packet.get("state", []):
                    if state.get("self"):
                        self.cid = state["clientId"]
        assert self.cid, "Anchor did not assign a client ID"

    def send(self, packet):
        wire = (json.dumps(packet, separators=(",", ":"), allow_nan=False)
                .encode("utf-8") + b"\0")
        if packet.get("type") == cm.PACKET_TYPE:
            assert len(wire) <= 8 * 1024, len(wire)
        self.socket.sendall(wire)
        self.sent.append(packet)

    def read(self, seconds):
        packets = []
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            ready, _, _ = select.select(
                [self.socket], [], [], max(0, end - time.monotonic())
            )
            if not ready:
                break
            data = self.socket.recv(65536)
            assert data, "Anchor disconnected a local peer"
            self.buffer += data
            while b"\0" in self.buffer:
                raw, self.buffer = self.buffer.split(b"\0", 1)
                packet = json.loads(raw)
                if packet.get("type") != "HEARTBEAT":
                    packets.append(packet)
        return packets

    def context(self):
        return {
            "cid": self.cid, "session": self.session, "team": self.team,
            "connected": True, "loaded": True, "room": cm.ROOM,
            "players": self.players,
        }

    def publish_metadata(self):
        state = {
            "clientId": self.cid,
            "name": f"Control Machine local {self.session}",
            "online": True, "teamId": self.team, "isSaveLoaded": True,
            "currentRoomId": cm.ROOM, "interactionSession": self.session,
            "playerEpoch": 7,
            cm.METADATA_KEY: self.engine.advertisement(self.context()),
        }
        self.players[self.cid] = normalise(state)
        self.send({"type": "UPDATE_CLIENT_STATE", "clientId": self.cid,
                   "state": state})
        self.engine.advertisement_dirty = False


def normalise(state):
    value = dict(state)
    value["roomId"] = state.get("currentRoomId", state.get("roomId", -1))
    return value


def pump(peers, seconds=0.03, now=0.0):
    for peer in peers:
        for packet in peer.read(seconds):
            kind = packet.get("type")
            if kind == "ALL_CLIENT_STATE":
                peer.players = {
                    state["clientId"]: normalise(state)
                    for state in packet.get("state", [])
                    if isinstance(state, dict) and
                    type(state.get("clientId")) is int
                }
            elif kind == "UPDATE_CLIENT_STATE":
                state = packet.get("state", {})
                if (isinstance(state, dict) and
                        type(state.get("clientId")) is int):
                    peer.players[state["clientId"]] = normalise(state)
            elif kind == cm.PACKET_TYPE:
                assert packet["clientId"] != peer.cid, "Sender was echoed"
                assert packet["targetTeamId"] == peer.team, "Cross-team packet"
                assert not packet.get("addToQueue"), "Hot packet was queued"
                assert packet.get("quiet") is True
                assert peer.engine.receive(peer.context(), packet, now)
                peer.received.append(packet)


def step(peers, now, source, ready):
    status = {}
    for peer in peers:
        state = source if peer is peers[0] else source if ready.get(peer.cid) else None
        active = 1 if peer is peers[0] or ready.get(peer.cid) else 0
        value, outgoing = peer.engine.update(
            peer.context(), active, 1 if peer.team == "blue" else 0,
            0 if active else 1, state, now,
        )
        status[peer.cid] = value
        if peer.engine.advertisement_dirty:
            peer.publish_metadata()
        for packet in outgoing:
            peer.send(packet)
            peer.engine.packet_send_result(packet, True)
    pump(peers, now=now)
    return status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()
    assert 1 <= args.port <= 65535
    room = "cm-local-" + uuid.uuid4().hex
    peers = []
    root = [0] * cm.ROOT_WORDS
    root[0:3] = [2, 11, 5]
    root[15:18] = [0x3e4ccccd] * 3  # native child scale 0.2
    root[19:22] = [0, 5, 256]  # initial status, HP mirror, no flash
    root[22] = 0xff000000
    root[23] = 1  # first authoritative command
    sample = {"fight": cm.VERSION, "r": root,
              "p": [[1, 10, 4, 0x3f800000, 0, 0, 0, 0, 0]]}
    assert cm.validate_state(sample) is sample
    try:
        owner = Peer(args.port, room, "blue", 101)
        peers.append(owner)
        owner.publish_metadata()
        for now in (10.0, 10.4):
            step(peers, now, sample, {})
        assert owner.engine.role == 1
        outsider = Peer(args.port, room, "red", 303)
        peers.append(outsider)
        visitor = Peer(args.port, room, "blue", 202)
        peers.append(visitor)
        for peer in peers:
            peer.publish_metadata()
        pump(peers, 0.06)
        status = {}
        for index in range(12):
            status = step(peers, 10.5 + index * 0.12, sample, {})
            if status[visitor.cid]["preview"] == sample:
                break
        assert status[visitor.cid]["preview"] == sample, "No late-fight preview"
        assert status[visitor.cid]["role"] == 0
        assert visitor.engine.e is None
        assert not visitor.engine.send_hit(visitor.context(), 1, 1, 12)
        assert not outsider.received
        for index in range(4):
            status = step(peers, 12 + index * 0.12, sample, {visitor.cid: True})
            if status[visitor.cid]["role"] == 2:
                break
        assert status[visitor.cid]["role"] == 2
        assert status[visitor.cid]["owner"] == owner.cid
        assert visitor.engine.send_hit(visitor.context(), 1, 1, 12.6)
        delivered = []
        for index in range(12):
            status = step(peers, 12.7 + index * 0.12, sample,
                          {visitor.cid: True})
            delivered += status[owner.cid]["hits"]
            if delivered and not visitor.engine.outgoing:
                break
        assert delivered == [[visitor.cid, visitor.session, 7, 1, 1]], delivered
        assert not visitor.engine.outgoing, "Hit acknowledgment did not arrive"

        command_two = copy.deepcopy(sample)
        command_two["r"][0] = 3
        command_two["r"][23] = 2
        step(peers, 15.0, command_two, {visitor.cid: True})
        status = step(peers, 15.12, command_two, {visitor.cid: True})
        assert status[visitor.cid]["state"] == command_two

        command_three = copy.deepcopy(command_two)
        command_three["r"][0] = 4
        command_three["r"][23] = 3
        step(peers, 15.3, command_three, {visitor.cid: True})
        status = step(peers, 15.42, command_three, {visitor.cid: True})
        assert status[visitor.cid]["state"] == command_three
        received_serials = [packet["d"]["r"][23]
                            for packet in visitor.received
                            if packet.get("op") == "s"]
        assert 2 in received_serials and 3 in received_serials

        same_command_hazards = copy.deepcopy(command_three)
        same_command_hazards["p"].append(
            [2, 20, 80, 0x40000000, 0, 0, 0, 0, 0])
        step(peers, 15.6, same_command_hazards, {visitor.cid: True})
        status = step(peers, 15.72, same_command_hazards,
                      {visitor.cid: True})
        assert status[visitor.cid]["state"] == same_command_hazards
        assert any(packet.get("op") == "s" and
                   packet["d"]["r"][23] == 3 and
                   packet["d"]["p"] == same_command_hazards["p"]
                   for packet in visitor.received)
        assert not outsider.received, "Other team saw encounter traffic"
        assert not [packet for packet in owner.received
                    if packet.get("clientId") == owner.cid]
        print(json.dumps({
            "clients": 3, "private_room": room, "checkpoint": True,
            "late_preview": True, "direct_hit": True, "ack": True,
            "command_serials": [2, 3], "same_command_projectiles": True,
            "sender_excluded": True, "team_isolated": True,
            "max_wire_bytes": max(
                len(json.dumps(packet, separators=(",", ":")).encode()) + 1
                for peer in peers for packet in peer.sent
                if packet.get("type") == cm.PACKET_TYPE),
        }))
    finally:
        for peer in peers:
            peer.socket.close()


if __name__ == "__main__":
    main()
