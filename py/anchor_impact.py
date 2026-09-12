"""Shared giant-robot Impact-battle snapshot and transient coordinator.

All four Impact bosses (Kashiwagi, Thaisamba, Balberra, D'Etoile) share the
dedicated file_13 battle-state block, so a single transport with a room set to
the live Impact stage serves every encounter. The native codec validates the
meaning of every word, including float bit patterns and the overlay callback.
Python never accepts arbitrary object fields or pointers.
"""

from collections import deque
import json
import math
import struct

from anchor_boss_transport import (
    BossTransport,
    metadata as _metadata,
    positive,
)

VERSION = 3
# The transport room is the live Impact stage; ordinary roomId is unrelated in
# title-menu boss rush. Every advertisement and operation also names the boss.
ROOM = 0x0220
PACKET_TYPE = "MNSG_IMPACT"
METADATA_KEY = "mnsgImpact"
MAX_STATE_BYTES = 4096

# Battle boss HP at +0x60, player/mech Ryo ammo at +0x64, mech HP at +0x68,
# combat pause at +0x2C0, per-encounter clock at +0x2C8, plus the root model
# transform/animation mirrored for visible alignment.
ROOT_WORDS = 172
DAMAGE_AMOUNTS = tuple(range(1, 256))

IMPACT_ENCOUNTER_MIN = 1
IMPACT_ENCOUNTER_MAX = 4
IMPACT_STAGE_MIN = 0x021C
IMPACT_STAGE_MAX = 0x0223
# The title-menu "Consecutive Fighting! Large Boss" mode plays all four bosses
# in this dedicated stage.
IMPACT_BOSS_RUSH_STAGE = 0x0260


def _impact_stage(stage):
    return (type(stage) is int and
            ((IMPACT_STAGE_MIN <= stage <= IMPACT_STAGE_MAX) or
             0x0239 <= stage <= 0x023C or
             stage == IMPACT_BOSS_RUSH_STAGE))


def _u32(word):
    return type(word) is int and 0 <= word <= 0xFFFFFFFF


def validate_state(value):
    """Validate the exact integer-only Impact battle checkpoint shape.

    ``k`` is the encounter selector (1..4), ``s`` the native Impact stage and
    ``r`` the flat native snapshot. Float fields cross the bridge as
    IEEE-754 u32 bits and get semantic checks in native C.
    """
    if not isinstance(value, dict) or set(value) != {"k", "s", "r"}:
        return None
    if not _u32(value["k"]) or not _u32(value["s"]):
        return None
    if not IMPACT_ENCOUNTER_MIN <= value["k"] <= IMPACT_ENCOUNTER_MAX:
        return None
    if not _impact_stage(value["s"]):
        return None
    if (not isinstance(value["r"], list) or len(value["r"]) != ROOT_WORDS or
            not all(_u32(word) for word in value["r"])):
        return None
    encoded = json.dumps(value, separators=(",", ":"), allow_nan=False)
    return value if len(encoded.encode("utf-8")) <= MAX_STATE_BYTES else None


def metadata(value):
    if not isinstance(value, list) or len(value) != 16:
        return None
    base = _metadata(value[:14], VERSION)
    if base is None:
        return None
    stage, boss = value[14:]
    if (type(stage) is not int or type(boss) is not int or
            (stage, boss) != (0, 0) and
            (not _impact_stage(stage) or not 1 <= boss <= 4) or
            base[1] and (stage, boss) == (0, 0)):
        return None
    return base + [stage, boss]


def merge_metadata(previous, incoming, expected_session=None):
    new, old = metadata(incoming), metadata(previous)
    if positive(expected_session):
        if old and old[5] != expected_session:
            old = None
        if new and new[5] != expected_session:
            new = None
    if not new or old and old[5] == new[5] and old[4] > new[4]:
        return old
    return new


class ImpactTransport(BossTransport):
    def __init__(self):
        self.encounter = 1
        super().__init__(
            packet_type=PACKET_TYPE,
            room=ROOM,
            metadata_key=METADATA_KEY,
            validate_state=validate_state,
            damage_amounts=DAMAGE_AMOUNTS,
            version=VERSION,
            include_wire_version=True,
            require_save=False,
            require_room=False,
            state_scope=self._state_scope,
        )

    def _state_scope(self, value):
        return value.get("s") == self.room and value.get("k") == self.encounter

    def reset(self):
        # Client metadata revisions must stay monotonic across boss/stage
        # resets; otherwise a later roster snapshot can resurrect the old boss.
        revision = getattr(self, "meta_revision", 0)
        super().reset()
        self.meta_revision = revision

    def _metadata(self, value):
        return metadata(value)

    def advertisement(self, ctx):
        ready, visit, paused = self.local
        value = [self.version, int(ready), visit, int(paused), 0, ctx["session"],
                 *(self.e or (0, 0, 0)), self.term, self.owner,
                 self.owner_session, int(self.state is not None),
                 int(self.capture_ready), self.room, self.encounter]
        if (not self.meta_value or
                value[:4] + value[5:] != self.meta_value[:4] + self.meta_value[5:]):
            self.meta_revision += 1
            value[4] = self.meta_revision
            self.meta_value = value
            self.advertisement_dirty = True
        return list(self.meta_value)

    def _peer(self, ctx, cid, require_local_room=False):
        m = super()._peer(ctx, cid, require_local_room)
        if m and m[14:] == [self.room, self.encounter]:
            return m
        return None

    def observe(self, ctx):
        current = {}
        for cid, p in ctx["players"].items():
            m = self._metadata(p.get(self.metadata_key))
            if (cid != ctx["cid"] and m and m[1] and p.get("online") and
                    p.get("teamId") == ctx["team"] and
                    p.get("interactionSession") == m[5] and
                    m[14:] == [self.room, self.encounter]):
                current[cid] = (cid, m[5], m[2])
        for cid, identity in self.members.items():
            if current.get(cid) != identity:
                self.unavailable[identity[:2]] = max(
                    self.unavailable.get(identity[:2], 0), identity[2])
        self.members = current

    def _host_ctx(self, ctx, self_visit=None):
        """Present the Impact visit as the hit epoch for the shared core.

        The generic boss core keys hit/ack bookkeeping on ``playerEpoch``, but
        the on-foot player lifecycle is not published during title-menu boss
        rush. Inside an Impact battle the encounter visit already fences every
        re-entry, so map each participant's epoch onto that visit. No ``epoch``
        word ever crosses the wire.
        """
        if self_visit is None:
            self_visit = self.local[1]
        players = {}
        for cid, player in ctx["players"].items():
            if cid == ctx["cid"]:
                visit = self_visit
            else:
                m = self._metadata(player.get(self.metadata_key))
                visit = m[2] if m and m[1] else 0
            entry = dict(player)
            entry["playerEpoch"] = visit
            players[cid] = entry
        if ctx["cid"] not in players:
            players[ctx["cid"]] = {"playerEpoch": self_visit}
        host = dict(ctx)
        host["players"] = players
        return host

    def _packet(self, ctx, op, **fields):
        packet = super()._packet(ctx, op, **fields)
        packet.update(s=self.room, k=self.encounter)
        return packet

    def receive(self, ctx, packet, now, defer=True):
        if (not isinstance(packet, dict) or not positive(packet.get("clientId")) or
                type(packet.get("s")) is not int or
                type(packet.get("k")) is not int or
                packet["s"] != self.room or packet["k"] != self.encounter):
            return False
        return super().receive(self._host_ctx(ctx), packet, now, defer)

    def send_hit(self, ctx, sequence, amount, now, target=0):
        return super().send_hit(
            self._host_ctx(ctx), sequence, amount, now, target)

    def update(self, ctx, ready, visit, paused, supplied, now):
        ready = bool(ready and _impact_stage(self.room) and 1 <= self.encounter <= 4)
        if not isinstance(supplied, dict) or not self._state_scope(supplied):
            supplied = None
        return super().update(self._host_ctx(ctx, visit if ready else 0),
                              ready, visit, paused, supplied, now)

    def set_encounter(self, stage, boss):
        valid = (_impact_stage(stage) and type(boss) is int and 1 <= boss <= 4)
        scope = (stage, boss) if valid else (0, 0)
        if scope != (self.room, self.encounter):
            self.reset()
            self.room, self.encounter = scope

    def set_stage(self, stage):
        """Track the live Impact stage so peer room checks stay exact."""
        if _impact_stage(stage):
            self.set_encounter(stage, self.encounter or 1)


PLAYER_PACKET_TYPE = "MNSG_IMPACT_PLAYER"
PLAYER_PACKET_BYTES = 1024  # includes the trailing NUL
PLAYER_INTERVAL = 0.1
PLAYER_EVENT_INTERVAL = 1 / 30
PLAYER_KEEPALIVE = 0.75
PLAYER_TTL = 1.5
PLAYER_EVENT_TTL = 0.75
PLAYER_MAX_PEERS = 16
PLAYER_EVENT_QUEUE = 32
PLAYER_EVENTS_PER_PACKET = 4
PLAYER_EVENTS_PER_FRAME = 16


def _aim(value):
    """Only finite native IEEE-754 vectors; C owns semantic world bounds."""
    return (isinstance(value, list) and len(value) == 6 and
            all(_u32(x) and math.isfinite(struct.unpack(
                "!f", struct.pack("!I", x))[0]) for x in value))


def _cursor(value):
    return (isinstance(value, list) and len(value) == 7 and
            type(value[0]) is int and value[0] in (0, 1) and
            all(_u32(x) and (x & 0x7F800000) != 0x7F800000 for x in value[1:4]) and
            all(type(x) is int and 0 <= x <= 0xFFFF for x in value[4:]))


def _attack(value):
    if (not isinstance(value, list) or len(value) != 8 or
            not positive(value[0]) or type(value[1]) is not int):
        return False
    if value[1] == 1:  # successful native Ryo shot, presentation only
        return _aim(value[2:])
    if value[1] == 2:  # mech input: held, pressed, cursor RX/RY, owner, term
        return (all(type(x) is int and 0 <= x <= 0xFFFF for x in value[2:6]) and
                all(positive(x) for x in value[6:]))
    return False


class ImpactPlayerTransport:
    """Transient per-participant cursor poses, shot visuals and mech input.

    Aim is a latest-value cache, not a FIFO. Attacks are accepted once per
    sender session/visit and drained with a hard per-frame bound. Mech input
    is additionally fenced to its intended owner and term in native C. All clocks used
    for expiry are local receive clocks; sender clocks only order that sender.
    """

    def __init__(self):
        self.reset()

    def reset(self):
        self.scope = None
        self.visit = 0
        self.cursor = [0] * 7
        self.sequence = 0
        self.accepted = 0
        self.last_wire = -1e9
        self.published = None
        self.published_peers = ()
        self.pending = deque()
        self.remote = {}
        self.incoming = deque()
        self.commits = {}
        self.members = {}
        self.retired = {}

    def _peers(self, ctx):
        if self.scope is None:
            return {}
        _, _, stage, boss = self.scope
        peers = {}
        for cid, p in sorted(ctx["players"].items()):
            m = metadata(p.get(METADATA_KEY))
            if (positive(cid) and cid != ctx["cid"] and p.get("online") and
                    p.get("teamId") == ctx["team"] and m and m[1] and
                    m[14:] == [stage, boss] and
                    p.get("interactionSession") == m[5] and
                    m[2] > self.retired.get((cid, m[5]), 0)):
                peers[cid] = (m[5], m[2])
                if len(peers) == PLAYER_MAX_PEERS:
                    break
        return peers

    def observe(self, ctx):
        """Retire departure edges even if a peer returns before C polls."""
        peers = self._peers(ctx)
        for cid, identity in self.members.items():
            if peers.get(cid) != identity:
                key = (cid, identity[0])
                self.retired[key] = max(self.retired.get(key, 0), identity[1])
        self.members = peers
        current = {(cid, p.get("interactionSession"))
                   for cid, p in ctx["players"].items()}
        self.retired = {key: visit for key, visit in self.retired.items() if key in current}
        return peers

    def _prune(self, ctx, now):
        peers = self.observe(ctx)
        # Keep sequence tombstones while membership stays valid even when an
        # aim sample expires. A delayed duplicate must never resurrect it.
        self.remote = {cid: item for cid, item in self.remote.items()
                       if peers.get(cid) == item["identity"]}
        self.incoming = deque(
            (cid, identity, row, arrived)
            for cid, identity, row, arrived in self.incoming
            if peers.get(cid) == identity and now - arrived < PLAYER_EVENT_TTL)
        self.pending = deque((row, born) for row, born in self.pending
                             if now - born < PLAYER_EVENT_TTL)
        return peers

    def receive(self, ctx, packet, now):
        if (self.scope is None or not ctx["connected"] or
                self.scope[:2] != (ctx["session"], ctx["team"]) or
                not isinstance(packet, dict) or
                packet.get("type") != PLAYER_PACKET_TYPE or
                type(packet.get("v")) is not int or packet["v"] != VERSION or
                not positive(packet.get("clientId")) or
                packet.get("targetTeamId") != ctx["team"] or
                "targetClientId" in packet or "addToQueue" in packet or
                not positive(packet.get("session")) or
                not positive(packet.get("visit")) or
                not positive(packet.get("q")) or
                type(packet.get("t")) is not int or not 0 <= packet["t"] < 2**53 or
                type(packet.get("s")) is not int or
                type(packet.get("k")) is not int or
                (packet["s"], packet["k"]) != self.scope[2:4] or
                not _cursor(packet.get("c")) or
                not isinstance(packet.get("a"), list) or
                len(packet["a"]) > PLAYER_EVENTS_PER_PACKET or
                not all(_attack(row) for row in packet["a"]) or
                any(packet["a"][i][0] >= packet["a"][i + 1][0]
                    for i in range(len(packet["a"]) - 1))):
            return False
        # The dispatch framing limit also checks this; retain it for direct
        # callers/tests and to reject oversized extension fields.
        try:
            if len(json.dumps(packet, separators=(",", ":"),
                              allow_nan=False).encode()) + 1 > PLAYER_PACKET_BYTES:
                return False
        except (ValueError, TypeError, RecursionError):
            return False
        peers = self._prune(ctx, now)
        cid = packet["clientId"]
        identity = (packet["session"], packet["visit"])
        if peers.get(cid) != identity:
            return False
        previous = self.remote.get(cid)
        if previous and (packet["q"] <= previous["q"] or
                         packet["t"] < previous["t"]):
            return False
        last_event = previous["event"] if previous else 0
        for row in packet["a"]:
            if row[0] <= last_event:
                continue
            last_event = row[0]
            if len(self.incoming) < PLAYER_EVENT_QUEUE:
                self.incoming.append((cid, identity, list(row), now))
        self.remote[cid] = {"identity": identity, "q": packet["q"],
                            "t": packet["t"], "c": list(packet["c"]),
                            "event": last_event, "received": now}
        return True

    def update(self, ctx, ready, stage, boss, visit, sample, now):
        active = bool(ready and ctx["connected"] and positive(ctx["cid"]) and
                      positive(ctx["session"]) and positive(visit) and
                      _impact_stage(stage) and
                      type(boss) is int and 1 <= boss <= 4)
        scope = (ctx["session"], ctx["team"], stage, boss) if active else None
        if scope != self.scope or active and visit != self.visit:
            self.reset()
            self.scope = scope
            self.visit = visit if active else 0
        if not active:
            return {"accepted": 0, "c": [], "a": []}, []
        peers = self._prune(ctx, now)
        if (isinstance(sample, dict) and set(sample) == {"c", "a"} and
                _cursor(sample["c"]) and isinstance(sample["a"], list) and
                len(sample["a"]) <= PLAYER_EVENTS_PER_FRAME and
                all(_attack(row) for row in sample["a"]) and
                all(sample["a"][i][0] < sample["a"][i + 1][0]
                    for i in range(len(sample["a"]) - 1))):
            self.cursor = list(sample["c"])
            for row in sample["a"]:
                if row[0] <= self.accepted:
                    continue
                if len(self.pending) >= PLAYER_EVENT_QUEUE:
                    break
                self.pending.append((list(row), now))
                self.accepted = row[0]
        signature = tuple(self.cursor)
        identities = tuple(sorted(peers.items()))
        visibility_edge = self.published is None or signature[0] != self.published[0]
        changed = signature != self.published or identities != self.published_peers
        elapsed = now - self.last_wire
        send = peers and (
            visibility_edge or
            self.pending and elapsed >= PLAYER_EVENT_INTERVAL - 1e-9 or
            changed and elapsed >= PLAYER_INTERVAL - 1e-9 or
            elapsed >= PLAYER_KEEPALIVE - 1e-9)
        packets = []
        if send:
            self.sequence += 1
            events = [row for row, _ in list(self.pending)[:PLAYER_EVENTS_PER_PACKET]]
            packet = {"type": PLAYER_PACKET_TYPE, "v": VERSION,
                      "clientId": ctx["cid"], "targetTeamId": ctx["team"],
                      "session": ctx["session"], "s": stage, "k": boss,
                      "visit": visit,
                      "q": self.sequence, "t": int(now * 1000),
                      "c": list(self.cursor), "a": events, "quiet": True}
            if len(json.dumps(packet, separators=(",", ":")).encode()) + 1 <= PLAYER_PACKET_BYTES:
                packets.append(packet)
                self.commits[id(packet)] = ((scope, visit), signature, identities, now,
                                            events[-1][0] if events else 0)
        cursors = [[cid, item["identity"][0], item["q"], *item["c"]]
                   for cid, item in sorted(self.remote.items())
                   if now - item["received"] < PLAYER_TTL]
        events = []
        for _ in range(min(len(self.incoming), PLAYER_EVENTS_PER_FRAME)):
            cid, identity, row, _ = self.incoming.popleft()
            events.append([cid, identity[0], *row])
        return {"accepted": self.accepted, "c": cursors, "a": events}, packets

    def packet_send_result(self, packet, sent):
        commit = self.commits.pop(id(packet), None)
        if not sent or not commit or commit[0] != (self.scope, self.visit):
            return
        _, self.published, self.published_peers, self.last_wire, event = commit
        while self.pending and self.pending[0][0][0] <= event:
            self.pending.popleft()
