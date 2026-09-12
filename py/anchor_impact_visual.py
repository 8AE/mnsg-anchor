"""Owner-only, atomic native render frames. No durable or gameplay events.

Team route, at most four 6 KiB pages at 8 Hz (192 KiB/s owner ceiling).
Only the latest completed frame and one bounded assembly survive receipt.
"""
import json

from anchor_impact import VERSION, METADATA_KEY, metadata, _u32, _impact_stage
from anchor_boss_transport import positive

PACKET_TYPE = "MNSG_IMPACT_VISUAL"
MAX_ROWS, ROW_WORDS, PAGE_ROWS, PACKET_BYTES = 64, 29, 16, 6144
PERIOD, TTL = 0.125, 0.75


def rows_valid(rows):
    if not isinstance(rows, list) or len(rows) > MAX_ROWS:
        return False
    ids = set()
    for r in rows:
        if (not isinstance(r, list) or len(r) != ROW_WORDS or
                not all(_u32(x) for x in r) or not 1 <= r[0] <= MAX_ROWS or
                r[0] in ids or not 1 <= r[1] <= 121 or r[2] > 38 or
                r[3] > 2 or r[3] and not r[2] or r[5] > 15 or
                r[6] & ~0x3FF01):
            return False
        ids.add(r[0])
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
            self.latest, self.received_at = rows, now
            self.received_sequence, self.assembly = q, None
        return True

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
        return (self.latest if battle.role == 2 and now-self.received_at < TTL else None), []
