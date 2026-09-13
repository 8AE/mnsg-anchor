"""Transient owner audio, team route, <=30 packets/s including loop heartbeats.

Eight native commands per <=1 KiB packet, 32 queued on receipt, .75s lifetime.
Loop state repeats every .25s so a failed write cannot leave a loop running.
No offline queue; scope and authority reuse the Impact visual roster fences.
"""
from collections import deque
import json
from anchor_boss_transport import positive
from anchor_impact import VERSION, _u32, metadata, METADATA_KEY
from anchor_impact_visual import ImpactVisualTransport, authority

PACKET_TYPE = "MNSG_IMPACT_SOUND"
PACKET_BYTES, MAX_COMMANDS, TTL = 1024, 8, .75
LOOPS = (0x122, 0x130, 0x152, 0x23E, 0x240, 0x241)
LOOP_MASK = (1 << len(LOOPS)) - 1
# Literal cue IDs verified in the file_13 native fight paths.
CUES = frozenset((
    0x106, 0x108, 0x10A, 0x10F, 0x113, 0x116, 0x117, 0x118, 0x119, 0x11A,
    0x11B, 0x11E, 0x11F, 0x120, 0x121, 0x122, 0x123, 0x12E, 0x12F, 0x130,
    0x131, 0x14E, 0x150, 0x151, 0x152, 0x153, 0x169, 0x16A, 0x16B, 0x16C,
    0x181, 0x229, 0x22A, 0x22B, 0x22D, 0x22E, 0x230, 0x231, 0x232, 0x233,
    0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23A, 0x23B, 0x23E, 0x240,
    0x241, 0x242, 0x245, 0x246, 0x247, 0x248, 0x24A, 0x24F, 0x26B, 0x274,
    0x276, 0x279, 0x27A, 0x27B, 0x27C, 0x27F, 0x280, 0x281, 0x282, 0x283,
    0x284, 0x286, 0x2A8, 0x34E,
))


TAISAMBA_CUES = frozenset((1,7,8,0x27,0x28,0x4A,0x4C,0x4D,0x60))

def command_valid(command, encounter=0):
    # Verified combat cues, including the six global loop stops.
    return (_u32(command) and command & 0xFFFF != 0 and
            (command & 0xFFFF in CUES or
             encounter == 2 and command & 0xFFFF in TAISAMBA_CUES or
             command & 0xFFFF in tuple(x | 0x8000 for x in LOOPS)) and
            (command >> 16 & 255) < 128)


class ImpactSoundTransport:
    def __init__(self):
        self.reset()

    def reset(self):
        self.guard = ImpactVisualTransport()
        self.queue = deque(maxlen=32)
        self.sequence = self.received_sequence = self.loops = 0
        self.last_wire = self.received_at = -1e9

    def receive(self, ctx, battle, packet, now):
        g = self.guard
        if not g.scope or g.authority != authority(battle) or not g.owner_live(ctx):
            self.queue.clear()
            self.loops = 0
            return False
        cid, session, visit, term, encounter = g.authority
        if (cid == ctx["cid"] or not isinstance(packet, dict) or
                packet.get("type") != PACKET_TYPE or type(packet.get("v")) is not int or packet["v"] != VERSION or
                packet.get("targetTeamId") != ctx["team"] or "targetClientId" in packet or "addToQueue" in packet or
                any(not positive(packet.get(k)) for k in ("clientId", "session", "visit", "term", "q")) or
                (packet["clientId"], packet["session"], packet["visit"], packet["term"]) != (cid, session, visit, term) or
                not isinstance(packet.get("e"), list) or not all(positive(x) for x in packet["e"]) or
                tuple(packet["e"]) != encounter or type(packet.get("s")) is not int or type(packet.get("k")) is not int or
                (packet["s"], packet["k"]) != g.scope[2:4] or not _u32(packet.get("l")) or packet["l"] & ~LOOP_MASK or
                not isinstance(packet.get("r"), list) or len(packet["r"]) > MAX_COMMANDS or
                not all(command_valid(c,packet["k"]) for c in packet["r"]) or packet["q"] <= self.received_sequence):
            return False
        try:
            if len(json.dumps(packet, separators=(",", ":"), allow_nan=False).encode())+1 > PACKET_BYTES:
                return False
        except (ValueError, TypeError, RecursionError):
            return False
        self.received_sequence, self.received_at = packet["q"], now
        self.loops = packet["l"]
        for c in packet["r"]:
            self.queue.append((now, c))
        return True

    def update(self, ctx, battle, ready, stage, boss, visit, sample, now):
        old = (self.guard.scope, self.guard.authority)
        self.guard.update(ctx, battle, ready, stage, boss, visit, None, now)
        if old != (self.guard.scope, self.guard.authority):
            self.queue.clear()
            self.sequence = self.received_sequence = self.loops = 0
            self.last_wire = self.received_at = -1e9
        if not self.guard.scope or not self.guard.owner_live(ctx):
            self.queue.clear()
            self.loops = 0
            return "00000000", []
        packets = []
        # Compact C bridge: eight hex digits for loop state, then <=8 commands.
        valid = (isinstance(sample, str) and 8 <= len(sample) <= 72 and len(sample) % 8 == 0 and
                 all(c in "0123456789abcdefABCDEF" for c in sample))
        words = [int(sample[i:i+8],16) for i in range(0,len(sample),8)] if valid else []
        if battle.role == 1:
            if words and not words[0] & ~LOOP_MASK and all(command_valid(c,boss) for c in words[1:]):
                self.loops = words[0]
                for c in words[1:]: self.queue.append((now,c))
            while self.queue and now-self.queue[0][0] >= TTL: self.queue.popleft()
            peers = any(cid != ctx["cid"] and p.get("online") and p.get("teamId") == ctx["team"] and
                        (m := metadata(p.get(METADATA_KEY))) and m[1] and
                        m[5] == p.get("interactionSession") and m[14:] == [stage,boss]
                        for cid,p in ctx["players"].items())
            if not peers:
                self.queue.clear()
            if peers and now-self.last_wire >= 1/30 and (self.queue or now-self.last_wire >= .25):
                self.sequence += 1
                self.last_wire = now
                commands = [self.queue.popleft()[1] for _ in range(min(MAX_COMMANDS,len(self.queue)))]
                packets = [{"type":PACKET_TYPE, "v":VERSION, "quiet":True,
                    "clientId":ctx["cid"], "targetTeamId":ctx["team"], "session":ctx["session"],
                    "visit":visit, "term":battle.term, "e":list(battle.e), "s":stage, "k":boss,
                    "q":self.sequence, "l":self.loops, "r":commands}]
            if packets and len(json.dumps(packets[0], separators=(",", ":")).encode())+1 > PACKET_BYTES:
                packets = []
            return "00000000", packets
        if now-self.received_at >= TTL:
            self.queue.clear()
            self.loops = 0
        while self.queue and now-self.queue[0][0] >= TTL: self.queue.popleft()
        commands = [self.queue.popleft()[1] for _ in range(min(MAX_COMMANDS,len(self.queue)))]
        return f"{self.loops:08x}" + "".join(f"{c:08x}" for c in commands), []
