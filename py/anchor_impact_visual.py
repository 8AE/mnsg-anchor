"""Owner-only, atomic native render frames. No durable or gameplay events.

Team route, at most four 6 KiB pages at 8 Hz (192 KiB/s owner ceiling).
Six completed frames and one bounded assembly survive receipt. Rendering uses
a short interpolation buffer; authoritative checkpoints and damage stay immediate.
"""
from collections import deque
import json
import struct

from anchor_impact import VERSION, METADATA_KEY, metadata, _u32, _impact_stage
from anchor_boss_transport import positive

PACKET_TYPE = "MNSG_IMPACT_VISUAL"
MAX_ROWS, ROW_WORDS, PAGE_ROWS, PACKET_BYTES = 64, 29, 16, 6144
PERIOD, TTL = 0.125, 0.75
RENDER_DELAY, MAX_BLEND_GAP = 0.15, 0.35
TELEPORT_DISTANCE = 500.0


def _float(word):
    return struct.unpack("!f", struct.pack("!I", word))[0]


def _bits(value):
    return struct.unpack("!I", struct.pack("!f", value))[0]


def blend_row(a, b, fraction):
    # IDs include a native allocation generation. Snap recipe, visibility,
    # material and segment-file transitions rather than blending unrelated poses.
    # Offsets within the same file may be animated texture frames.
    if (a[:4] != b[:4] or a[5:7] != b[5:7] or a[17::2] != b[17::2] or
            sum((_float(b[i]) - _float(a[i])) ** 2 for i in range(7, 10)) >
            TELEPORT_DISTANCE ** 2):
        return list(b)
    if fraction >= 1:
        return list(b)
    if fraction <= 0:
        return list(a)
    out = list(a)
    for i in (7, 8, 9, 13, 14, 15):
        x, y = _float(a[i]), _float(b[i])
        out[i] = _bits(x + (y - x) * fraction)
    for i in (10, 11, 12):
        if a[i] == b[i] or a[i] == 0x8000 or b[i] == 0x8000:
            out[i] = b[i]  # Native billboard sentinel is not an angle.
        else:
            delta = (b[i] - a[i] + 512) % 1024 - 512
            out[i] = round(a[i] + delta * fraction) % 1024
    x, y = _float(a[16]), _float(b[16])
    # Animation loops/clip resets must not run backwards through old frames.
    if 0 <= y - x <= 30:
        out[16] = _bits(x + (y - x) * fraction)
    else:
        out[16] = b[16]
    if a[3]:
        out[4] = sum(round(((a[4] >> shift) & 255) * (1 - fraction) +
                           ((b[4] >> shift) & 255) * fraction) << shift
                     for shift in (0, 8, 16, 24))
    return out


def rows_valid(rows):
    if not isinstance(rows, list) or len(rows) > MAX_ROWS:
        return False
    ids = set()
    for r in rows:
        if (not isinstance(r, list) or len(r) != ROW_WORDS or
                not all(_u32(x) for x in r) or not r[0] or
                (r[0]-1) % MAX_ROWS in ids or not 1 <= r[1] <= 121 or r[2] > 38 or
                r[3] > 2 or r[3] and not r[2] or r[5] > 15 or
                r[6] & ~0x3FF01):
            return False
        ids.add((r[0]-1) % MAX_ROWS)
        for i in range(7, 17):
            if 10 <= i <= 12:
                if r[i] > 65535:
                    return False
            elif r[i] & 0x7FFFFFFF > 0x49742400:
                return False
        for i in range(17, 29, 2):
            if (r[i] > 0x876F or r[i+1] >= 0x800000 or
                    not r[i] and r[i+1]):
                return False
    return True


def authority(battle):
    return (battle.owner, battle.owner_session, battle.owner_visit,
            battle.term, tuple(battle.e or ()))


class ImpactVisualTransport:
    def __init__(self):
        self.reset()

    def reset(self):
        self.scope = None
        self.authority = None
        self.sequence = self.received_sequence = 0
        self.last_wire = -1e9
        self.history = deque(maxlen=6)
        self.latest = None
        self.received_at = -1e9
        self.assembly = None

    def owner_live(self, ctx):
        if not self.scope or not ctx["connected"] or self.scope[:2] != (ctx["session"], ctx["team"]):
            return False
        cid, session, visit, term, encounter = self.authority
        if cid == ctx["cid"]:
            return session == ctx["session"] and visit == self.scope[4]
        peer = ctx["players"].get(cid, {})
        m = metadata(peer.get(METADATA_KEY))
        return bool(m and peer.get("online") and peer.get("teamId") == ctx["team"] and
                    peer.get("interactionSession") == session == m[5] and
                    m[1] and m[2] == visit and m[14:] == list(self.scope[2:4]))

    def receive(self, ctx, battle, packet, now):
        if self.authority != authority(battle) or not self.owner_live(ctx):
            self.latest = self.assembly = None
            self.history.clear()
            return False
        cid, session, visit, term, encounter = self.authority
        if (cid == ctx["cid"] or not isinstance(packet, dict) or
                packet.get("type") != PACKET_TYPE or
                type(packet.get("v")) is not int or packet["v"] != VERSION or
                packet.get("targetTeamId") != ctx["team"] or
                "targetClientId" in packet or "addToQueue" in packet or
                any(not positive(packet.get(k)) for k in ("clientId", "session", "visit", "term", "q")) or
                (packet["clientId"], packet["session"], packet["visit"], packet["term"]) != (cid, session, visit, term) or
                not isinstance(packet.get("e"), list) or
                not all(positive(x) for x in packet["e"]) or tuple(packet["e"]) != encounter or
                type(packet.get("s")) is not int or type(packet.get("k")) is not int or
                (packet["s"], packet["k"]) != self.scope[2:4] or
                type(packet.get("n")) is not int or not 1 <= packet["n"] <= 4 or
                type(packet.get("p")) is not int or not 0 <= packet["p"] < packet["n"] or
                not rows_valid(packet.get("r")) or len(packet["r"]) > PAGE_ROWS or
                packet["p"] < packet["n"]-1 and len(packet["r"]) != PAGE_ROWS):
            return False
        try:
            if len(json.dumps(packet, separators=(",", ":"), allow_nan=False).encode())+1 > PACKET_BYTES:
                return False
        except (ValueError, TypeError, RecursionError):
            return False
        q = packet["q"]
        if q <= self.received_sequence:
            return False
        if self.assembly and (q < self.assembly[0] or q == self.assembly[0] and
                              (packet["n"] != self.assembly[1] or now-self.assembly[3] >= TTL)):
            return False
        if not self.assembly or q > self.assembly[0]:
            self.assembly = (q, packet["n"], {}, now)
        pages = self.assembly[2]
        if packet["p"] in pages:
            return False
        pages[packet["p"]] = [list(row) for row in packet["r"]]
        if len(pages) == packet["n"]:
            rows = [row for i in range(packet["n"]) for row in pages[i]]
            if not rows_valid(rows):
                self.received_sequence = q
                self.assembly = None
                return False
            if now - self.received_at > MAX_BLEND_GAP:
                self.history.clear()
            self.latest, self.received_at = rows, now
            self.history.append((now, rows))
            self.received_sequence, self.assembly = q, None
        return True

    def render(self, now):
        if not self.history or not self.latest:
            return self.latest
        target = now - RENDER_DELAY
        left = right = self.history[0]
        for sample in self.history:
            right = sample
            if sample[0] >= target:
                break
            left = sample
        span = right[0] - left[0]
        fraction = min(1.0, max(0.0, (target - left[0]) / span)) if span > 0 else 1.0
        before = {r[0]: r for r in left[1]}
        after = {r[0]: r for r in right[1]}
        # Birth/death are immediate. A new generation never inherits a dead
        # object's interpolation history, even when it reuses the same slot.
        return [blend_row(before[r[0]], after[r[0]], fraction)
                if r[0] in before and r[0] in after else list(r)
                for r in self.latest]

    def update(self, ctx, battle, ready, stage, boss, visit, rows, now):
        active = (ready and ctx["connected"] and positive(ctx["cid"]) and
                  positive(ctx["session"]) and positive(visit) and _impact_stage(stage) and
                  type(boss) is int and 1 <= boss <= 4 and battle.role in (1, 2) and
                  not battle.paused and positive(battle.owner) and positive(battle.term))
        scope = (ctx["session"], ctx["team"], stage, boss, visit) if active else None
        auth = authority(battle) if active else None
        if scope != self.scope or auth != self.authority:
            self.reset()
            self.scope, self.authority = scope, auth
        if not active or not self.owner_live(ctx):
            self.latest = self.assembly = None
            self.history.clear()
            return None, []
        peers = any(cid != ctx["cid"] and p.get("online") and p.get("teamId") == ctx["team"] and
                    (m := metadata(p.get(METADATA_KEY))) and m[1] and
                    m[5] == p.get("interactionSession") and m[14:] == [stage,boss]
                    for cid,p in ctx["players"].items())
        if battle.role == 1 and peers and rows_valid(rows) and now-self.last_wire >= PERIOD:
            self.sequence += 1
            self.last_wire = now  # Hard ceiling includes failed writes; next frame retries latest state.
            pages = max(1, (len(rows)+PAGE_ROWS-1)//PAGE_ROWS)
            packets = [{"type": PACKET_TYPE, "v": VERSION, "quiet": True,
                        "clientId": ctx["cid"], "targetTeamId": ctx["team"],
                        "session": ctx["session"], "visit": visit,
                        "term": battle.term, "e": list(battle.e), "s": stage, "k": boss,
                        "q": self.sequence, "p": p, "n": pages,
                        "r": rows[p*PAGE_ROWS:(p+1)*PAGE_ROWS]} for p in range(pages)]
            if all(len(json.dumps(p, separators=(",", ":")).encode())+1 <= PACKET_BYTES for p in packets):
                return None, packets
        return (self.render(now) if battle.role == 2 and now-self.received_at < TTL else None), []
