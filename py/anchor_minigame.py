"""Bounded, transient minigame sessions and complete native graph snapshots.

Native adapters own the meaning of controller/actor words and resource safety.
This module owns identity, ordering, authority, atomic assembly and transport.
SYNC is latest state to the team (5 Hz running/changed, 1 Hz idle). Goemon/Ebi v2
use 1 Hz complete checkpoints and base-dependent ACTION phase/lifetime cues
at a 50 ms floor. INPUT is direct to the authority; RESULT is an idempotent
transient team edge. None enters Anchor's durable queue or native gameplay FIFO.
"""

import copy
import json

METADATA_KEY = "mg"
PACKET_TYPE = "MNSG_MINIGAME_SYNC"
ACTION_TYPE = "MNSG_MINIGAME_ACTION"
INPUT_TYPE = "MNSG_MINIGAME_INPUT"
RESULT_TYPE = "MNSG_MINIGAME_RESULT"
ROOMS = {1: 0x1e0, 2: 0x1e1, 3: 0x1e2, 4: 0x154}
MAX_INT = 0x7fffffff
PACKET_BYTES = 4096
MAX_GRAPH_BYTES = 32 * 1024
MAX_PARTS = 8
MAX_ACTORS = 64
MAX_WORDS = 32
MAX_ACTOR_WORDS = 40
MAX_INPUTS = 32
MAX_INPUT_MEMBERS = 64
INPUT_BYTES = 512
LEASE = 3.0
ASSEMBLY_TTL = 1.5
INTERVAL = 0.2
FLOOR = 0.05
KEEPALIVE = 1.0
PREPARE_TTL = 30.0


def positive(x):
    return type(x) is int and 0 < x <= MAX_INT


def words(row, minimum=0, maximum=MAX_WORDS):
    return (isinstance(row, list) and minimum <= len(row) <= maximum and
            all(type(x) is int and -0x80000000 <= x <= MAX_INT for x in row))


def token(t):
    return (isinstance(t, list) and len(t) == 4 and
            type(t[0]) is int and t[0] in ROOMS and all(positive(x) for x in t[1:]))


def owner(o):
    return isinstance(o, list) and len(o) == 3 and all(positive(x) for x in o)


def receipt(row):
    return isinstance(row, list) and len(row) == 4 and all(positive(x) for x in row)


def actor_row(row):
    return words(row, 3, MAX_ACTOR_WORDS) and all(x >= 0 for x in row[:3])


def ebi_v2(game, c):
    return game == 2 and len(c) == 13 and c[0] == 2


def goemon_v2(game, c):
    return game == 1 and len(c) == 5 and c[0] == 2


def action_v2(game, c):
    return goemon_v2(game, c) or ebi_v2(game, c)


def _direction(bits):
    # Native velocity words are IEEE float bits, including negative zero.
    return 0 if not bits & 0x7fffffff else -1 if bits & 0x80000000 else 1


def actor_cue(row):
    """Ebi phase/lifetime edges; exclude continuous motion and native clocks."""
    if len(row) not in (32, 40):
        return tuple(row)
    flags = row[12] & ~0x20 if row[0] == 5 else row[12]
    cue = tuple(row[:3]) + (flags,) + tuple(row[i] for i in (13, 17, 18, 20, 21, 22))
    cue += tuple(_direction(row[i]) for i in (14, 15, 16))
    if row[0] == 3:
        cue += ((row[19] >> 24) & 0xff, row[36] if len(row) == 40 else 0)
    elif row[0] == 1:
        cue += tuple(row[i] for i in (23, 24, 25))
    else:
        cue += (row[23],)
    return cue


def ebi_cue(state, terminal, acks):
    c = state["c"]
    return (tuple(c[i] for i in (0, 1, 4, 6, 7, 8, 9, 10, 12)),
            tuple(sorted(actor_cue(row) for row in state["a"])),
            state["running"], terminal, tuple(sorted(acks.items())))


def action_actor_cue(game, row):
    if game == 2:
        return actor_cue(row)
    if row[0] == 1 and len(row) == 31:
        # Schedule and restart edges seed native target simulation. Height,
        # clocks, child transforms and scales advance locally between fulls.
        return tuple(row[i] for i in (0, 1, 2, 3, 4, 5, 7, 8, 10, 18, 19, 29, 30))
    if row[0] == 2 and len(row) == 20:
        # A launch stays stable across subsequent flight pose checkpoints;
        # held pose sequences are bounded native anchors for the carrier.
        return tuple(row[:8]) + (row[15], row[19], row[14] if row[7] else 0)
    return tuple(row)


def action_cue(game, state, terminal, acks):
    if game == 2:
        return ebi_cue(state, terminal, acks)
    return (tuple(state["c"][i] for i in (0, 1, 4)),
            tuple(sorted(action_actor_cue(game, row) for row in state["a"])),
            state["running"], terminal, tuple(sorted(acks.items())))


def goemon_core_keys(rows):
    return {tuple(row[:3]) for row in rows if row[0] == 1}


def compose_rows(previous, changed, removed):
    """Use precisely the published baseline; omitted rows keep their words."""
    replacements = {tuple(row[:3]): row for row in changed}
    removed = {tuple(key) for key in removed}
    rows = []
    for row in previous:
        key = tuple(row[:3])
        if key not in removed:
            rows.append(replacements.pop(key, row))
    rows.extend(replacements.values())
    return copy.deepcopy(rows)


def metadata(m, session=None):
    if (not isinstance(m, dict) or type(m.get("v")) is not int or m["v"] != 1 or
            not positive(m.get("s")) or (positive(session) and m["s"] != session) or
            type(m.get("q")) is not int or not 0 <= m["q"] <= MAX_INT or
            type(m.get("r")) is not int or m["r"] not in (0, 1)):
        return None
    if m.get("t") == []:
        return {"v": 1, "s": m["s"], "q": m["q"], "r": 0, "t": []}
    if (not token(m.get("t")) or not owner(m.get("o")) or
            not positive(m.get("g")) or not positive(m.get("visit")) or
            type(m.get("x")) is not int or m["x"] not in (0, 1, 2)):
        return None
    return {key: copy.deepcopy(m[key]) for key in ("v", "s", "q", "r", "t", "o", "g", "visit", "x")}


def merge_metadata(previous, incoming, session=None):
    old, new = metadata(previous, session), metadata(incoming, session)
    if old and new and old["s"] == new["s"] and old["q"] > new["q"]:
        return old
    return new or old


def native_state(value):
    if not isinstance(value, dict):
        return None
    c, a = value.get("c"), value.get("a")
    if (not words(c, 1) or not isinstance(a, list) or len(a) > MAX_ACTORS or
            any(not actor_row(row) for row in a) or
            len({tuple(row[:3]) for row in a}) != len(a)):
        return None
    for key, default, allowed in (("ready", 1, (0, 1)), ("running", 1, (0, 1)),
                                  ("edge", 0, (0, 1)), ("result", 0, (0, 1, 2))):
        x = value.get(key, default)
        if type(x) is not int or x not in allowed:
            return None
    adopted = value.get("adoptedGeneration", 0)
    if type(adopted) is not int or not 0 <= adopted <= MAX_INT:
        return None
    inputs = value.get("inputs", [])
    if (not isinstance(inputs, list) or len(inputs) > MAX_INPUTS or
            any(not words(row, 2, 8) or not positive(row[0]) or
                not positive(row[1]) for row in inputs)):
        return None
    accepted = value.get("inputAck", [])
    if (not isinstance(accepted, list) or len(accepted) > MAX_INPUTS or
            any(not receipt(row) for row in accepted)):
        return None
    return {"c": copy.deepcopy(c), "a": copy.deepcopy(a), "inputs": copy.deepcopy(inputs),
            "ready": value.get("ready", 1), "running": value.get("running", 1),
            "edge": value.get("edge", 0), "result": value.get("result", 0),
            "inputAck": copy.deepcopy(accepted), "adoptedGeneration": adopted}


def wire_bytes(packet):
    try:
        return len(json.dumps(packet, separators=(",", ":"), allow_nan=False).encode()) + 1
    except (TypeError, ValueError, RecursionError):
        return MAX_GRAPH_BYTES + 1


class MinigameTransport:
    """Caller holds its roster lock; socket writes happen after update returns."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.scope = None
        self.t = None
        self.o = None
        self.g = 0
        self.visit = 0
        self.game = 0
        self.prepared = False
        self.prepare_room = -1
        self.prepared_at = -1e9
        self.local_ready = False
        self.takeover_pending = False
        self.takeover_graph = None
        self.graph = None
        self.received = -1e9
        self.missing_since = None
        self.last_wire = -1e9
        self.last_full_wire = -1e9
        self.last_attempt = -1e9
        self.published = None
        self.tick = 0
        # Metadata ordering spans gameplay visits inside one connection. Room
        # exit/reset must not make the empty record older than the active one.
        self.q = getattr(self, "q", 0)
        self.meta_value = None
        self.advertisement_dirty = True
        self.cache = {}
        self.pending = {}
        self.acks = {}
        self.ack_cursor = 0
        self.input_pending = {}
        self.input_sent = set()
        self.input_blocked = {}
        self.input_seen = {}
        self.incoming = []
        self.result_seen = {}
        self.result_pending = None
        self.result_sent = 0
        self.terminal = 0
        self.last_input_wire = -1e9
        self.last_result_wire = -1e9
        self.last_input_attempt = -1e9
        self.last_result_attempt = -1e9
        self.send_commits = {}

    def _scope(self, ctx):
        scope = (ctx["cid"], ctx["session"], ctx["team"])
        if self.scope != scope:
            self.reset()
            self.scope = scope

    def advertisement(self, ctx):
        self._scope(ctx)
        value = {"v": 1, "s": ctx["session"], "q": self.q,
                 "r": int(self.local_ready), "t": list(self.t or [])}
        if self.t:
            value.update(o=list(self.o), g=self.g, visit=self.visit or 1,
                         x=self.terminal or self.result_pending or self.result_sent or 0)
        baseline = dict(value)
        baseline.pop("q")
        if baseline != self.meta_value:
            self.q = min(self.q + 1, MAX_INT)
            self.meta_value = baseline
            self.advertisement_dirty = True
        value["q"] = self.q
        return value

    def _peer(self, ctx, cid, session, game):
        p = ctx["players"].get(cid)
        return p if (p and p.get("online") and p.get("isSaveLoaded") and
                     p.get("teamId") == ctx["team"] and
                     p.get("interactionSession") == session and
                     (game != 4 or p.get("currentCharacter", p.get("character")) in (3, "3", "Yae")) and
                     p.get("roomId", p.get("currentRoomId")) == ROOMS[game]) else None

    def _members(self, ctx, t):
        members = {}
        for cid, p in ctx["players"].items():
            m = metadata(p.get(METADATA_KEY), p.get("interactionSession"))
            if (m and m["t"] == list(t) and m["r"] and not m["x"] and
                    self._peer(ctx, cid, m["s"], t[0])):
                members[cid] = (m["s"], m["visit"])
        if (self.t == tuple(t) and self.local_ready and not self.terminal and
                not self.result_pending and not self.result_sent and
                ctx["room"] == ROOMS[t[0]]):
            members[ctx["cid"]] = (ctx["session"], self.visit)
        return members

    def _status_members(self, ctx):
        if not self.t or self.terminal or self.result_pending or self.result_sent:
            return []
        eligible = []
        for cid, (session, visit) in self._members(ctx, self.t).items():
            if not positive(cid):
                continue
            if cid == ctx["cid"]:
                if (self.o != (ctx["cid"], ctx["session"], self.visit) and
                        (not self.graph or self.graph["o"] != list(self.o) or self.graph["g"] != self.g)):
                    continue
                character = ctx.get("character", ctx["players"].get(cid, {}).get("currentCharacter"))
            else:
                peer = ctx["players"][cid]
                m = metadata(peer.get(METADATA_KEY), session)
                if not m or m["o"] != list(self.o) or m["g"] != self.g:
                    continue
                character = peer.get("currentCharacter", peer.get("character"))
            if self.game == 2 and character not in (1, "1", "Ebisumaru"):
                continue
            if self.game == 4 and character not in (3, "3", "Yae"):
                continue
            eligible.append(cid)
        return sorted(eligible)[:MAX_INPUT_MEMBERS]

    def prepare_join(self, ctx, game, cid, session, visit, now):
        self._scope(ctx)
        if (type(game) is not int or game not in ROOMS or
                any(type(x) is not int or not 0 <= x <= MAX_INT for x in (cid, session, visit))):
            return False
        if not ctx["connected"] or not ctx["loaded"]:
            return cid == 0
        choices = []
        for peer_id, p in ctx["players"].items():
            if cid and cid != peer_id or peer_id == ctx["cid"]:
                continue
            m = metadata(p.get(METADATA_KEY), p.get("interactionSession"))
            if (not m or not m["t"] or m["t"][0] != game or not m["r"] or m["x"] or
                    not self._peer(ctx, peer_id, m["s"], game) or
                    session and m["s"] != session or visit and m["t"][3] != visit):
                continue
            graph = self.cache.get(tuple(m["t"]))
            if graph and (graph["g"] != m["g"] or graph["o"] != m["o"]):
                graph = None
            fresh = graph and now - graph["received"] <= LEASE
            stamp = p.get("_minigameReceived", -1e9)
            if not fresh and now - stamp > LEASE:
                continue
            choices.append((tuple(m["t"]), m, graph))
        if not choices:
            if cid:
                return False
            # Debug teleport with no active compatible session starts locally.
            cache, scope = self.cache, self.scope
            self.reset()
            self.cache, self.scope = cache, scope
            self.game, self.prepared = game, True
            self.prepare_room, self.prepared_at = ctx["room"], now
            return True
        t, m, graph = min(choices, key=lambda x: x[0])
        if (self.prepared and self.t == t and self.visit == 0 and
                self.g == m["g"] and self.o == tuple(m["o"])):
            self.prepared_at = now
            return True
        self.t, self.o, self.g = t, tuple(m["o"]), m["g"]
        self.game, self.visit, self.prepared = game, 0, True
        self.prepare_room, self.prepared_at = ctx["room"], now
        self.local_ready = False
        self.takeover_pending = False
        self.takeover_graph = None
        self.acks.clear()
        self.input_pending.clear()
        self.input_sent.clear()
        self.input_blocked.clear()
        self.input_seen.clear()
        self.incoming.clear()
        self.last_input_attempt = self.last_result_attempt = -1e9
        self.terminal = 0
        self.result_sent = 0
        self.result_pending = None
        self.graph = copy.deepcopy(graph) if graph and now - graph["received"] <= LEASE else None
        self.received = graph["received"] if self.graph else now
        self.last_wire, self.published = -1e9, None
        self.advertisement_dirty = True
        return True

    def room_changed(self, ctx, room):
        cancel_active = self.t and room != ROOMS[self.t[0]] and self.visit
        cancel_prepared = (self.prepared and not self.visit and
                           room not in (self.prepare_room, ROOMS[self.game]))
        if cancel_active or cancel_prepared:
            self.reset()
            self.scope = (ctx["cid"], ctx["session"], ctx["team"])

    def _prune(self, now):
        for key, item in tuple(self.pending.items()):
            if now - item["received"] > ASSEMBLY_TTL:
                self.pending.pop(key, None)
        for t, graph in tuple(self.cache.items()):
            if now - graph["received"] > 2 * LEASE:
                self.cache.pop(t, None)
        while len(self.cache) > 8:
            self.cache.pop(min(self.cache, key=lambda t: self.cache[t]["received"]))

    def _input_member(self, ctx, key, t):
        cid, session, visit = key
        if key == (ctx["cid"], ctx["session"], self.visit) and self.t == t:
            return ctx["room"] == ROOMS[t[0]]
        p = self._peer(ctx, cid, session, t[0])
        m = metadata(p.get(METADATA_KEY), session) if p else None
        return bool(m and m["t"] == list(t) and m["visit"] == visit)

    def _prune_inputs(self, ctx):
        if not self.t:
            return
        for key in set(self.input_seen) | set(self.acks) | set(self.input_blocked):
            if not self._input_member(ctx, key, self.t):
                self.input_seen.pop(key, None)
                self.acks.pop(key, None)
                self.input_blocked.pop(key, None)
        self.incoming = [row for row in self.incoming if self._input_member(ctx, tuple(row[:3]), self.t)]

    def _authorized(self, ctx, t, o, g, now):
        p = self._peer(ctx, o[0], o[1], t[0])
        m = metadata(p.get(METADATA_KEY), o[1]) if p else None
        if (not m or m["t"] != list(t) or m["visit"] != o[2] or not m["r"] or
                m["g"] > g):
            return False
        previous = self.cache.get(tuple(t))
        if not previous:
            return m["g"] == g and m["o"] == list(o)
        if g < previous["g"]:
            return False
        if g == previous["g"]:
            return list(o) == previous["o"]
        if g != previous["g"] + 1:
            return False
        # Even a roster departure has a grace period. Never let a fresh local
        # spawn elect itself before it holds the previous complete graph.
        if now - previous["received"] < LEASE:
            return False
        members = self._members(ctx, t)
        old = previous["o"]
        members.pop(old[0], None)
        return bool(members) and o[0] == min(members) and members[o[0]] == tuple(o[1:])

    def receive(self, ctx, packet, now):
        self._scope(ctx)
        self._prune(now)
        self._prune_inputs(ctx)
        if (not ctx["connected"] or not ctx["loaded"] or not isinstance(packet, dict) or
                packet.get("type") not in (PACKET_TYPE, ACTION_TYPE, INPUT_TYPE, RESULT_TYPE) or
                "addToQueue" in packet or not positive(packet.get("clientId")) or
                packet["clientId"] == ctx["cid"] or not token(packet.get("t")) or
                not owner(packet.get("o")) or not positive(packet.get("g"))):
            return False
        t, o, g = tuple(packet["t"]), tuple(packet["o"]), packet["g"]
        if self.t == t and (g < self.g or g == self.g and o != self.o):
            return False
        kind, cid = packet["type"], packet["clientId"]
        if kind == INPUT_TYPE:
            return self._receive_input(ctx, packet, t, o, g)
        if ("targetClientId" in packet or packet.get("targetTeamId") != ctx["team"] or
                not self._authorized(ctx, t, o, g, now)):
            return False
        if kind == RESULT_TYPE:
            return self._receive_result(ctx, packet, t, o, g)
        if (cid != o[0] or not positive(packet.get("k")) or
                type(packet.get("p")) is not int or type(packet.get("n")) is not int or
                not 1 <= packet["n"] <= MAX_PARTS or not 0 <= packet["p"] < packet["n"] or
                not words(packet.get("c"), 1) or
                type(packet.get("r")) is not int or packet["r"] not in (0, 1) or
                type(packet.get("x")) is not int or packet["x"] not in (0, 1, 2) or
                not isinstance(packet.get("a"), list) or len(packet["a"]) > MAX_ACTORS or
                any(not actor_row(row) for row in packet["a"]) or
                not isinstance(packet.get("h"), list) or len(packet["h"]) > MAX_INPUTS or
                any(not receipt(row) for row in packet["h"]) or wire_bytes(packet) > PACKET_BYTES):
            return False
        previous = self.cache.get(t)
        if previous and (g < previous["g"] or g == previous["g"] and packet["k"] <= previous["k"]):
            return False
        action = kind == ACTION_TYPE
        if action and (not action_v2(t[0], packet["c"]) or not positive(packet.get("b")) or
                packet["b"] >= packet["k"] or not previous or previous["g"] != g or
                previous["o"] != list(o) or previous["k"] != packet["b"] or
                not action_v2(t[0], previous["c"]) or
                not isinstance(packet.get("z"), list) or len(packet["z"]) > MAX_ACTORS or
                any(not words(row, 3, 3) or any(x < 0 for x in row) for row in packet["z"]) or
                len({tuple(row) for row in packet["z"]}) != len(packet["z"])):
            return False
        if not action and ("b" in packet or "z" in packet):
            return False
        key = (t, o, g, packet["k"])
        header = {k: copy.deepcopy(packet[k]) for k in ("t", "o", "g", "k", "n", "c", "r", "x", "h")}
        header["type"] = kind
        if action:
            header.update(b=packet["b"], z=copy.deepcopy(packet["z"]))
        item = self.pending.get(key)
        if item and item["header"] != header:
            self.pending.pop(key, None)
            return False
        if not item:
            # Two incomplete ticks per session and a fixed connection-wide cap.
            siblings = [k for k in self.pending if k[0] == t]
            if len(siblings) >= 2:
                self.pending.pop(min(siblings, key=lambda k: (k[2], k[3])))
            if len(self.pending) >= 16:
                self.pending.pop(min(self.pending, key=lambda k: self.pending[k]["received"]))
            item = self.pending[key] = {"header": header, "parts": {}, "received": now, "bytes": {}}
        part = packet["p"]
        if part in item["parts"]:
            return item["parts"][part] == packet["a"]
        item["parts"][part] = copy.deepcopy(packet["a"])
        item["bytes"][part] = wire_bytes(packet)
        if sum(item["bytes"].values()) > MAX_GRAPH_BYTES:
            self.pending.pop(key, None)
            return False
        if len(item["parts"]) != packet["n"]:
            return True
        rows = [row for p in range(packet["n"]) for row in item["parts"][p]]
        self.pending.pop(key, None)
        if len(rows) > MAX_ACTORS or len({tuple(row[:3]) for row in rows}) != len(rows):
            return False
        if action:
            old_keys = {tuple(row[:3]) for row in previous["a"]}
            removed = {tuple(row) for row in header["z"]}
            if not removed <= old_keys or removed & {tuple(row[:3]) for row in rows}:
                return False
            rows = compose_rows(previous["a"], rows, header["z"])
            if (len(rows) > MAX_ACTORS or goemon_v2(t[0], header["c"]) and
                    goemon_core_keys(rows) != goemon_core_keys(previous["a"])):
                return False
        history = dict(previous.get("ackHistory", {})) if previous else {}
        for ack in header["h"]:
            key = tuple(ack[:3])
            if self._input_member(ctx, key, t):
                history[key] = max(history.get(key, 0), ack[3])
        history = {key: seq for key, seq in history.items() if self._input_member(ctx, key, t)}
        while len(history) > MAX_INPUT_MEMBERS:
            history.pop(next(iter(history)))
        graph = {**header, "a": rows, "received": now, "ackHistory": history,
                 "fullTick": previous.get("fullTick", previous["k"]) if action else header["k"]}
        self.cache[t] = graph
        if self.t == t and (g > self.g or g == self.g and o == self.o):
            if o != self.o or g != self.g:
                self.takeover_pending = False
                self.takeover_graph = None
            self.o, self.g, self.graph, self.received = o, g, copy.deepcopy(graph), now
            self.terminal = graph["x"]
            for ack in graph["h"]:
                if ack[:3] == [ctx["cid"], ctx["session"], self.visit]:
                    key = tuple(ack[:3])
                    self.input_seen[key] = max(self.input_seen.get(key, 0), ack[3])
                    for seq in tuple(self.input_pending):
                        if seq <= ack[3]:
                            self.input_pending.pop(seq, None)
                            self.input_sent.discard(seq)
        return True

    def _receive_input(self, ctx, p, t, o, g):
        if (p.get("targetClientId") != ctx["cid"] or "targetTeamId" in p or
                ctx["room"] != ROOMS[t[0]] or
                self.t != t or self.o != o or self.g != g or o[0] != ctx["cid"] or self.terminal or
                not positive(p.get("s")) or not positive(p.get("visit")) or
                not words(p.get("i"), 2, 8) or not positive(p["i"][0]) or not positive(p["i"][1]) or
                wire_bytes(p) > INPUT_BYTES):
            return False
        peer = self._peer(ctx, p["clientId"], p["s"], t[0])
        m = metadata(peer.get(METADATA_KEY), p["s"]) if peer else None
        if not m or m["t"] != list(t) or m["visit"] != p["visit"] or not m["r"] or m["x"]:
            return False
        key = (p["clientId"], p["s"], p["visit"])
        seq = p["i"][0]
        if seq > self.input_blocked.get(key, MAX_INT):
            return False
        if seq <= self.input_seen.get(key, 0):
            return False
        if len(self.incoming) >= MAX_INPUTS or len(self.input_seen) >= MAX_INPUT_MEMBERS and key not in self.input_seen:
            if key in self.input_blocked or len(self.input_blocked) < MAX_INPUT_MEMBERS:
                self.input_blocked[key] = min(self.input_blocked.get(key, seq), seq)
            return False
        self.input_seen[key] = seq
        self.incoming.append([*key, *p["i"]])
        if seq == self.input_blocked.get(key):
            self.input_blocked.pop(key, None)
        return True

    def _receive_result(self, ctx, p, t, o, g):
        if (not positive(p.get("s")) or not positive(p.get("visit")) or
                not positive(p.get("seq")) or type(p.get("x")) is not int or
                p["x"] not in (1, 2) or wire_bytes(p) > INPUT_BYTES):
            return False
        peer = self._peer(ctx, p["clientId"], p["s"], t[0])
        m = metadata(peer.get(METADATA_KEY), p["s"]) if peer else None
        if not m or m["t"] != list(t) or m["visit"] != p["visit"]:
            return False
        key = (t, p["clientId"], p["s"], p["visit"])
        if p["seq"] <= self.result_seen.get(key, 0):
            return False
        if len(self.result_seen) >= 64:
            self.result_seen.pop(next(iter(self.result_seen)))
        self.result_seen[key] = p["seq"]
        # A joiner's personal failure does not end the authority's game.
        if self.t == t and p["clientId"] == o[0]:
            self.terminal = p["x"]
        return True

    def _base(self, ctx, kind):
        return {"type": kind, "clientId": ctx["cid"], "quiet": True,
                "t": list(self.t), "o": list(self.o), "g": self.g}

    def _chunks(self, ctx, state, tick, action=False):
        keys = sorted(self.acks)
        start = self.ack_cursor % len(keys) if keys else 0
        candidates = (keys[start:] + keys[:start])[:MAX_INPUTS]

        def pack(count):
            base = {**self._base(ctx, ACTION_TYPE if action else PACKET_TYPE), "targetTeamId": ctx["team"],
                    "k": tick, "c": state["c"], "r": state["running"], "x": self.terminal,
                    "h": [[*key, self.acks[key]] for key in candidates[:count]], "n": MAX_PARTS, "p": 0}
            selected = state["a"]
            if action:
                prior = {tuple(row[:3]): row for row in self.graph["a"]}
                selected = [row for row in state["a"]
                            if tuple(row[:3]) not in prior or
                            action_actor_cue(self.game, row) != action_actor_cue(self.game, prior[tuple(row[:3])])]
                current = {tuple(row[:3]) for row in state["a"]}
                base.update(b=self.graph["k"], z=[list(key) for key in prior if key not in current])
            parts, rows = [], []
            for row in selected:
                if wire_bytes({**base, "a": rows + [row]}) > PACKET_BYTES:
                    if not rows:
                        return []
                    parts.append(rows)
                    rows = []
                rows.append(row)
            parts.append(rows)
            if len(parts) > MAX_PARTS:
                return []
            packets = [{**base, "a": rows, "p": p, "n": len(parts)} for p, rows in enumerate(parts)]
            return packets if (all(wire_bytes(p) <= PACKET_BYTES for p in packets) and
                               sum(wire_bytes(p) for p in packets) <= MAX_GRAPH_BYTES) else []

        packets = pack(len(candidates))
        if packets or not candidates:
            return packets
        # Receipt headers repeat across atomic chunks. Reduce only the rotating
        # ACK window so the complete native graph always retains its budget.
        low, high, best = 1, len(candidates) - 1, []
        while low <= high:
            count = (low + high) // 2
            trial = pack(count)
            if trial:
                best, low = trial, count + 1
            else:
                high = count - 1
        return best

    def update(self, ctx, game, visit, supplied, now):
        self._scope(ctx)
        self._prune(now)
        self._prune_inputs(ctx)
        if self.prepared and not self.visit and (now - self.prepared_at > PREPARE_TTL or
                ctx["room"] not in (self.prepare_room, ROOMS[self.game])):
            self.reset()
            self.scope = (ctx["cid"], ctx["session"], ctx["team"])
        packets = []
        state = native_state(supplied)
        if (not ctx["connected"] or not ctx["loaded"] or type(game) is not int or
                game not in ROOMS or not positive(visit) or ctx["room"] != ROOMS[game]):
            if self.visit or not ctx["connected"] or not ctx["loaded"]:
                self.reset()
                self.scope = (ctx["cid"], ctx["session"], ctx["team"])
            return {"ready": 0, "owner": 0, "isOwner": 0, "token": [], "generation": 0}, []
        if (self.visit and (self.visit != visit or self.game != game) or
                self.t and self.t[0] != game):
            self.reset()
            self.scope = (ctx["cid"], ctx["session"], ctx["team"])
        self.game, self.visit = game, visit
        takeover = False
        if not self.t:
            self.t = (game, ctx["cid"], ctx["session"], visit)
            self.o, self.g = self.t[1:], 1
            self.received = now
        is_owner = self.o[0] == ctx["cid"] and self.o[1:] == (ctx["session"], visit)
        # A follower needs a complete, recent graph before it becomes eligible
        # for takeover; a locally initialized native root alone is insufficient.
        self.local_ready = bool(state and state["ready"] and (is_owner or self.graph))
        if (not is_owner and self.graph and not self.terminal and
                not self.result_pending and not self.result_sent and
                not (state and state["result"])):
            present = self._peer(ctx, self.o[0], self.o[1], game)
            if game == 4 and present:
                advertised = metadata(present.get(METADATA_KEY), self.o[1])
                if (not advertised or not advertised["r"] or
                        advertised["t"] != list(self.t) or advertised["x"]):
                    present = None
            stale = now - self.received >= LEASE
            if not present or stale:
                if self.missing_since is None:
                    self.missing_since = now if present else self.received
                members = self._members(ctx, self.t)
                members.pop(self.o[0], None)
                if (now - self.missing_since >= LEASE and members and
                        ctx["cid"] == min(members) and self.local_ready):
                    self.o, self.g = (ctx["cid"], ctx["session"], visit), self.g + 1
                    is_owner = True
                    takeover = True
                    self.takeover_pending = True
                    self.takeover_graph = copy.deepcopy(self.graph)
                    self.tick = 0
                    self.published, self.last_wire = None, -1e9
                    # First takeover publication is exactly the last complete
                    # graph, never the fresh local controller's startup state.
                    state = {**state, "c": copy.deepcopy(self.graph["c"]),
                             "a": copy.deepcopy(self.graph["a"]), "running": self.graph["r"]}
                    self.acks = dict(self.graph.get("ackHistory", {}))
                    for r in self.graph["h"]:
                        key = tuple(r[:3])
                        if self._input_member(ctx, key, self.t):
                            self.acks[key] = max(self.acks.get(key, 0), r[3])
                    self.input_seen = dict(self.acks)
            else:
                self.missing_since = None
        if self.takeover_pending:
            if state and state["adoptedGeneration"] == self.g:
                self.takeover_pending = False
                self.takeover_graph = None
                takeover = False
            else:
                takeover = True
                if state:
                    state = {**state, "c": copy.deepcopy(self.takeover_graph["c"]),
                             "a": copy.deepcopy(self.takeover_graph["a"]),
                             "running": self.takeover_graph["r"]}
        if state and state["result"] and not self.result_sent and not self.result_pending:
            self.result_pending = state["result"]
        if self.result_pending and now - self.last_result_attempt >= INTERVAL:
            p = {**self._base(ctx, RESULT_TYPE), "targetTeamId": ctx["team"],
                 "s": ctx["session"], "visit": visit, "seq": 1, "x": self.result_pending}
            if wire_bytes(p) <= INPUT_BYTES:
                packets.append(p)
                self.send_commits[id(p)] = ("result", now)
                self.last_result_attempt = now
        if state:
            if is_owner and self.local_ready and not takeover:
                for ack in state["inputAck"]:
                    key = tuple(ack[:3])
                    if self._input_member(ctx, key, self.t) and ack[3] <= self.input_seen.get(key, 0):
                        self.acks[key] = max(self.acks.get(key, 0), ack[3])
                self.incoming = [row for row in self.incoming
                                 if row[3] > self.acks.get(tuple(row[:3]), 0)]
            for row in sorted(state["inputs"], key=lambda r: r[0]):
                if row[0] > self.input_seen.get((ctx["cid"], ctx["session"], visit), 0):
                    if is_owner:
                        key = (ctx["cid"], ctx["session"], visit)
                        if len(self.incoming) < MAX_INPUTS and (key in self.input_seen or len(self.input_seen) < MAX_INPUT_MEMBERS):
                            self.input_seen[key] = row[0]
                            self.incoming.append([*key, *row])
                    elif len(self.input_pending) < MAX_INPUTS:
                        self.input_pending.setdefault(row[0], row)
        if not is_owner and now - self.last_input_attempt >= FLOOR and self.input_pending and self.local_ready:
            unsent = self.input_pending.keys() - self.input_sent
            row = self.input_pending[min(unsent or self.input_pending.keys())]
            p = {**self._base(ctx, INPUT_TYPE), "targetClientId": self.o[0],
                 "s": ctx["session"], "visit": visit, "i": row}
            if wire_bytes(p) <= INPUT_BYTES:
                packets.append(p)
                self.send_commits[id(p)] = ("input", (now, row[0]))
                self.last_input_attempt = now
        if is_owner and self.local_ready and state:
            if self.result_pending:
                self.terminal = self.result_pending
            semantic = action_v2(game, state["c"])
            fingerprint = (action_cue(game, state, self.terminal, self.acks) if semantic else
                           (state["c"], state["a"], state["running"], self.terminal, sorted(self.acks.items())))
            full = (not semantic or not self.graph or not action_v2(game, self.graph["c"]) or self.graph["g"] != self.g or
                    self.graph["o"] != list(self.o) or now - self.last_full_wire + 1e-9 >= KEEPALIVE or
                    goemon_v2(game, state["c"]) and
                    goemon_core_keys(state["a"]) != goemon_core_keys(self.graph["a"]))
            action = semantic and not full
            interval = (FLOOR if action else KEEPALIVE if semantic else
                        FLOOR if state["edge"] else INTERVAL if state["running"] or fingerprint != self.published else KEEPALIVE)
            changed = not action or fingerprint != self.published or state["edge"]
            due = semantic and full or now - self.last_wire + 1e-9 >= interval
            if changed and due and now - self.last_attempt + 1e-9 >= FLOOR:
                snapshots = self._chunks(ctx, state, self.tick + 1, action)
                if action and not snapshots:
                    # A large lifetime replacement may repeat too many
                    # tombstones per chunk. A complete checkpoint is smaller
                    # and restores the baseline under the same edge floor.
                    action = False
                    snapshots = self._chunks(ctx, state, self.tick + 1)
                if snapshots:
                    committed_rows = (compose_rows(self.graph["a"],
                        [row for packet in snapshots for row in packet["a"]], snapshots[0]["z"])
                        if action else copy.deepcopy(state["a"]))
                    commit = {"remaining": len(snapshots), "ok": True, "now": now,
                              "tick": self.tick + 1, "fingerprint": copy.deepcopy(fingerprint),
                              "full": not action,
                              "ack_cursor": (self.ack_cursor + len(snapshots[0]["h"])) % max(1, len(self.acks)),
                              "graph": {**snapshots[0], "a": committed_rows, "received": now}}
                    commit["graph"]["ackHistory"] = dict(self.acks)
                    commit["graph"]["fullTick"] = (self.graph.get("fullTick", self.graph["k"])
                                                  if action else self.tick + 1)
                    for p in snapshots:
                        self.send_commits[id(p)] = ("sync", commit)
                    packets.extend(snapshots)
                    self.last_attempt = now
        self.advertisement(ctx)
        graph = self.takeover_graph if takeover else self.graph
        checkpoint = self.graph if is_owner else graph
        fresh = is_owner or graph and now - self.received <= LEASE
        result = {"ready": int(bool(self.local_ready and fresh)), "owner": self.o[0],
                  "isOwner": int(is_owner), "token": list(self.t), "generation": self.g,
                  "tick": self.tick if is_owner else graph["k"] if graph else 0, "result": self.terminal,
                  "fullTick": (checkpoint or {}).get("fullTick", 0),
                  "takeover": int(takeover),
                  "localAck": (self.acks.get((ctx["cid"], ctx["session"], visit), 0) if is_owner else
                               (graph or {}).get("ackHistory", {}).get((ctx["cid"], ctx["session"], visit), 0)),
                  "members": self._status_members(ctx),
                  "inputs": []}
        if is_owner and self.local_ready and not takeover:
            # A deferred operation must precede later operations from that
            # participant, otherwise a cumulative ACK could discard it.
            delivered = set()
            for row in self.incoming:
                key = tuple(row[:3])
                if key not in delivered:
                    result["inputs"].append(row[:])
                    delivered.add(key)
        if graph and (not is_owner or takeover):
            result.update(c=copy.deepcopy(graph["c"]), a=copy.deepcopy(graph["a"]),
                          running=graph["r"])
        return result, packets

    def packet_send_result(self, packet, sent):
        commit = self.send_commits.pop(id(packet), None)
        if not commit:
            return
        kind, data = commit
        if kind == "sync":
            data["remaining"] -= 1
            data["ok"] &= bool(sent)
            if not data["remaining"] and data["ok"]:
                self.last_wire, self.tick, self.published = data["now"], data["tick"], data["fingerprint"]
                if data["full"]:
                    self.last_full_wire = data["now"]
                self.ack_cursor = data["ack_cursor"]
                self.graph, self.received = data["graph"], data["now"]
                self.cache[self.t] = copy.deepcopy(self.graph)
        elif sent and kind == "input":
            self.last_input_wire = data[0]
            self.input_sent.add(data[1])
        elif sent and kind == "result":
            self.last_result_wire = data
            self.result_sent = self.result_pending
            self.result_pending = 0
