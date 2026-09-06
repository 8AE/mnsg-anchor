"""Transient Congo encounter replication. The caller owns the roster lock/I/O.

Only the elected simulator publishes boss state. Full native-word checkpoints
are coalesced at 10 Hz; membership and attack delivery are independent of that
cadence. No packets are added to Anchor's durable save queue.
"""

from collections import deque
import json


VERSION = 1
ROOM = 22
PACKET_TYPE = "MNSG_CONGO"
MAX_STATE_BYTES = 4096
MAX_INT = 0x7fffffff
INTERVAL = 0.1
KEEPALIVE = 1.0
LEASE = 3.0
DISCOVERY = 0.35
RETRY = 0.25
HIT_QUEUE = 8
HITS_PER_FRAME = 32
DAMAGE_AMOUNTS = (1, 2, 3, 4, 8)


def positive(value):
    return type(value) is int and 0 < value <= MAX_INT


def encounter(value):
    return (isinstance(value, (list, tuple)) and len(value) == 3 and
            all(positive(x) for x in value))


def validate_state(value):
    """Exact native codec: two 24-word parts, two scalars, 7-word flames.

    Native code validates the meaning of these unsigned words (including float
    bit patterns); Python never accepts arbitrary object fields or pointers.
    """
    def u32(word):
        return type(word) is int and 0 <= word <= 0xffffffff

    if not isinstance(value, dict) or set(value) != {"r", "p", "s", "t", "f"}:
        return None
    if (any(not isinstance(value[k], list) or len(value[k]) != 24 or
            not all(u32(word) for word in value[k]) for k in ("r", "p")) or
            not u32(value["s"]) or not u32(value["t"]) or
            not isinstance(value["f"], list) or len(value["f"]) > 32 * 7 or
            len(value["f"]) % 7 or not all(u32(word) for word in value["f"])):
        return None
    encoded = json.dumps(value, separators=(",", ":"), allow_nan=False)
    return value if len(encoded.encode("utf-8")) <= MAX_STATE_BYTES else None


def metadata(value):
    """version, ready, visit, paused, revision, session, encounter*3,
    authority term, owner cid, owner session, checkpoint available,
    native capture available (intro actors can receive before they simulate).
    """
    if (not isinstance(value, list) or len(value) != 14 or
            any(type(x) is not int for x in value) or value[0] != VERSION or
            any(value[i] not in (0, 1) for i in (1, 3, 12, 13)) or
            any(not 0 <= x <= MAX_INT for x in value[2:])):
        return None
    if value[1] and not positive(value[2]):
        return None
    if not positive(value[5]):
        return None
    if any(value[6:9]) and not encounter(value[6:9]):
        return None
    if any(value[9:12]) and not all(positive(x) for x in value[9:12]):
        return None
    return list(value)


def merge_metadata(previous, incoming, expected_session=None):
    new = metadata(incoming)
    old = metadata(previous)
    if positive(expected_session):
        if old and old[5] != expected_session:
            old = None
        if new and new[5] != expected_session:
            new = None
    if not new:
        return old
    if old and old[5] == new[5] and old[4] > new[4]:
        return old
    return new


class CongoTransport:
    def __init__(self):
        self.reset()

    def reset(self):
        self.scope = None
        self.local = (False, 0, False)
        self.started = 0.0
        self.capture_started = None
        self.capture_ready = False
        self.e = None
        self.term = 0
        self.owner = 0
        self.owner_session = 0
        self.owner_visit = 0
        self.role = 0
        self.state = None
        self.paused = False
        self.revision = 0
        self.sequence = 0
        self.received = 0.0
        self.last_wire = -1e9
        self.last_request = -1e9
        self.last_hit_wire = -1e9
        self.last_hit_added = -1e9
        self.force_snapshot = False
        self.published = None
        self.cache = {}
        self.unavailable = {}  # (cid, session) -> last departed native visit
        self.failed_authorities = {}  # (cid, session, visit) -> failed term
        self.retired = {}  # creator (cid, session) -> last ended encounter visit
        self.pending_packets = {}
        self.members = {}
        self.acks = {}
        self.incoming = {}
        self.delivered = []
        self.outgoing = deque()
        self.hit_cursor = 0
        self.last_peer_hit = {}
        self.meta_revision = 0
        self.meta_value = None
        self.advertisement_dirty = True
        self.takeover_frame = False

    def _scope(self, ctx):
        scope = (ctx["session"], ctx["team"])
        if self.scope != scope:
            self.reset()
            self.scope = scope

    def advertisement(self, ctx):
        ready, visit, paused = self.local
        value = [VERSION, int(ready), visit, int(paused), 0, ctx["session"],
                 *(self.e or (0, 0, 0)), self.term, self.owner,
                 self.owner_session, int(self.state is not None), int(self.capture_ready)]
        if not self.meta_value or value[:4] + value[5:] != self.meta_value[:4] + self.meta_value[5:]:
            self.meta_revision += 1
            value[4] = self.meta_revision
            self.meta_value = value
            self.advertisement_dirty = True
        return list(self.meta_value)

    def _peer(self, ctx, cid, require_local_room=False):
        p = ctx["players"].get(cid)
        if not p or not p.get("online", False) or not p.get("isSaveLoaded", False):
            return None
        m = metadata(p.get("mnsgCongo"))
        if (not m or not m[1] or p.get("teamId") != ctx["team"] or
                p.get("roomId") != ROOM or p.get("interactionSession") != m[5] or
                m[2] <= self.unavailable.get((cid, m[5]), 0)):
            return None
        if require_local_room and (not self.local[0] or ctx["room"] != ROOM):
            return None
        return m

    def _eligible(self, ctx):
        result = {cid: m for cid in ctx["players"]
                  if (m := self._peer(ctx, cid)) is not None}
        if self.local[0]:
            result[ctx["cid"]] = self.advertisement(ctx)
        return result

    def observe(self, ctx):
        """Remember actual departure edges even if a peer returns before poll."""
        current = {}
        for cid, p in ctx["players"].items():
            if cid == ctx["cid"]:
                continue
            m = metadata(p.get("mnsgCongo"))
            if not m:
                continue
            if (p.get("online", False) and p.get("isSaveLoaded", False) and
                    p.get("teamId") == ctx["team"] and p.get("roomId") == ROOM and m[1]):
                current[cid] = (cid, m[5], m[2])
        for cid, identity in self.members.items():
            if current.get(cid) != identity:
                key = identity[:2]
                self.unavailable[key] = max(self.unavailable.get(key, 0), identity[2])
        self.members = current

    def _is_retired(self, e):
        return e[2] <= self.retired.get(e[:2], 0)

    def _retire(self, e):
        self.retired[e[:2]] = max(self.retired.get(e[:2], 0), e[2])

    def _failed(self, authority):
        return authority[3] <= self.failed_authorities.get(authority[:3], 0)

    def _prune(self, ctx, now):
        eligible = self._eligible(ctx)
        sessions = {(cid, p.get("interactionSession")) for cid, p in ctx["players"].items()}
        visits = {(cid, m[5], m[2]) for cid, m in eligible.items()}
        self.unavailable = {key: visit for key, visit in self.unavailable.items() if key in sessions}
        self.failed_authorities = {key: term for key, term in self.failed_authorities.items() if key in visits}
        creator_sessions = sessions | ({self.e[:2]} if self.e else set())
        self.retired = {key: visit for key, visit in self.retired.items() if key in creator_sessions}
        claims = {tuple(m[6:9]) for m in eligible.values()}
        for e, checkpoint in list(self.cache.items()):
            if e != self.e and e not in claims and now - checkpoint["received"] >= LEASE:
                self.cache.pop(e, None)
                self._retire(e)
        def current_hit(key):
            cid, session, epoch = key
            p = ctx["players"].get(cid, {})
            return (cid in eligible and p.get("interactionSession") == session and
                    p.get("playerEpoch") == epoch)
        self.acks = {key: seq for key, seq in self.acks.items() if current_hit(key)}
        for checkpoint in self.cache.values():
            checkpoint["acks"] = {key: seq for key, seq in checkpoint["acks"].items() if current_hit(key)}
        self.incoming = {cid: pending for cid, pending in self.incoming.items() if cid in eligible and pending}
        self.last_peer_hit = {cid: stamp for cid, stamp in self.last_peer_hit.items() if cid in eligible}
        self.pending_packets = {cid: value for cid, value in self.pending_packets.items()
                                if cid in ctx["players"] and now - value[1] < LEASE}

    def _packet(self, ctx, op, **fields):
        packet = {"type": PACKET_TYPE, "clientId": ctx["cid"],
                  "targetTeamId": ctx["team"], "session": ctx["session"],
                  "op": op, "e": list(self.e or (0, 0, 0)),
                  "term": self.term, "visit": self.local[1], "quiet": True}
        packet.update(fields)
        return packet

    def _adopt(self, checkpoint, now):
        self.e = checkpoint["e"]
        self.term = checkpoint["term"]
        self.owner = checkpoint["owner"]
        self.owner_session = checkpoint["session"]
        self.owner_visit = checkpoint["visit"]
        self.sequence = checkpoint["q"]
        self.state = checkpoint["state"]
        self.paused = checkpoint["paused"]
        self.received = checkpoint["received"]
        self.acks = dict(checkpoint["acks"])
        self.revision += 1
        self.incoming.clear()
        self.delivered.clear()
        self.published = None
        self.last_request = -1e9
        self._ack_outgoing()

    def _ack_outgoing(self):
        while self.outgoing:
            h = self.outgoing[0]
            if self.acks.get(tuple(h[:3]), 0) < h[3]:
                break
            self.outgoing.popleft()
            self.last_hit_wire = -1e9

    def _checkpoint(self, now):
        return {"e": self.e, "term": self.term, "owner": self.owner,
                "session": self.owner_session, "q": self.sequence,
                "visit": self.owner_visit,
                "state": self.state, "paused": self.paused,
                "acks": dict(self.acks), "received": now}

    def receive(self, ctx, packet, now, defer=True):
        self._scope(ctx)
        cid = packet.get("clientId")
        if (not positive(cid) or cid == ctx["cid"] or
                packet.get("targetTeamId") != ctx["team"] or
                not positive(packet.get("session")) or
                packet.get("targetClientId", ctx["cid"]) != ctx["cid"]):
            return False
        source = self._peer(ctx, cid)
        if not source:
            # One coalesced checkpoint per known member covers metadata races.
            p = ctx["players"].get(cid, {})
            if (defer and p and p.get("teamId", "") in ("", ctx["team"]) and
                    packet.get("op") == "s"):
                self.pending_packets[cid] = (packet, now)
            return False
        if source[5] != packet["session"] or source[2] != packet.get("visit"):
            return False
        op = packet.get("op")
        if op == "r":
            if self.role == 1 and self.local[0] and cid != self.owner:
                self.force_snapshot = True
                return True
            return False
        e = packet.get("e")
        term = packet.get("term")
        if not encounter(e) or self._is_retired(tuple(e)) or not positive(term):
            return False
        e = tuple(e)
        if op == "h":
            if (self.role != 1 or e != self.e or term != self.term or
                    not self._peer(ctx, cid, True)):
                return False
            h = packet.get("h")
            if (not isinstance(h, list) or len(h) != 3 or
                    not all(positive(x) for x in h) or h[2] not in DAMAGE_AMOUNTS):
                return False
            # The sender's native player epoch fences attacks from old lives.
            if ctx["players"][cid].get("playerEpoch") != h[0]:
                return False
            return self._queue_hit([cid, packet["session"], *h], now)
        if op not in ("s", "k") or not positive(packet.get("q")):
            return False
        if packet.get("p") not in (0, 1) or type(packet.get("p")) is not int:
            return False
        previous = self.cache.get(e)
        if previous:
            if (term < previous["term"] or
                    (term == previous["term"] and cid > previous["owner"])):
                return False
            if term == previous["term"] and cid == previous["owner"]:
                if previous["session"] != packet["session"]:
                    return False
                if op == "s" and packet["q"] <= previous["q"]:
                    return False
        if op == "k":
            if (not previous or previous["owner"] != cid or
                    previous["term"] != term or packet["q"] != previous["q"]):
                return False
            previous["received"] = now
            if self.e == e and self.owner == cid and self.term == term:
                self.received = now
            return True
        value = validate_state(packet.get("d"))
        rows = packet.get("a", [])
        if value is None or not isinstance(rows, list):
            return False
        acks = {}
        for row in rows:
            if (not isinstance(row, list) or len(row) != 4 or
                    not all(positive(x) for x in row)):
                return False
            if row[0] not in ctx["players"]:
                continue
            acks[tuple(row[:3])] = row[3]
        checkpoint = {"e": e, "term": term, "owner": cid,
                      "session": packet["session"], "q": packet["q"],
                      "visit": packet["visit"],
                      "state": value, "paused": bool(packet["p"]),
                      "acks": acks, "received": now}
        # At most one pending encounter from each sender plus the local active
        # encounter; malformed or delayed claims cannot accumulate state blobs.
        for old_e, old in list(self.cache.items()):
            if old_e != e and old_e != self.e and old["owner"] == cid:
                self.cache.pop(old_e, None)
        self.cache[e] = checkpoint
        if self.local[0] and (self.e == e or self.e is None or e < self.e):
            self._adopt(checkpoint, now)
            self.role = 2
        return True

    def _queue_hit(self, hit, now):
        key = tuple(hit[:3])
        if self.acks.get(key, 0) >= hit[3]:
            return True
        if any(h[:4] == hit[:4] for h in self.delivered):
            return True
        pending = self.incoming.setdefault(hit[0], deque())
        if any(h[:4] == hit[:4] for h in pending):
            return True
        if (len(pending) >= HIT_QUEUE or
                now - self.last_peer_hit.get(hit[0], -1e9) < INTERVAL - 1e-9):
            return False
        pending.append(hit)
        self.last_peer_hit[hit[0]] = now
        return True

    def send_hit(self, ctx, sequence, amount, now):
        if (self.role not in (1, 2) or self.state is None or not self.local[0] or self.local[2] or not self.e or
                not positive(sequence) or type(amount) is not int or amount not in DAMAGE_AMOUNTS):
            return False
        epoch = ctx["players"].get(ctx["cid"], {}).get("playerEpoch", 0)
        if not positive(epoch):
            return False
        hit = [ctx["cid"], ctx["session"], epoch, sequence, amount]
        if any(h[:4] == hit[:4] for h in self.outgoing):
            return True
        if self.acks.get(tuple(hit[:3]), 0) >= sequence:
            return True
        if (len(self.outgoing) >= HIT_QUEUE or
                now - self.last_hit_added < INTERVAL - 1e-9):
            return False
        if self.outgoing and tuple(self.outgoing[-1][:3]) == tuple(hit[:3]) and sequence <= self.outgoing[-1][3]:
            return False
        self.outgoing.append(hit)
        self.last_hit_added = now
        return True

    def _choose(self, ctx, now):
        eligible = self._eligible(ctx)
        valid_cache = []
        for e, checkpoint in list(self.cache.items()):
            m = eligible.get(checkpoint["owner"])
            live_owner = (m and m[5] == checkpoint["session"] and m[2] == checkpoint["visit"] and
                          now - checkpoint["received"] < LEASE)
            replica = any(tuple(member[6:9]) == e and member[12] for member in eligible.values())
            if not self._is_retired(e) and (live_owner or replica):
                valid_cache.append(checkpoint)
        if self.e is None and valid_cache:
            self._adopt(min(valid_cache, key=lambda c: c["e"]), now)
            self.takeover_frame = self.owner == ctx["cid"]
        if self.e is not None:
            incumbent = eligible.get(self.owner)
            authority = (self.owner, self.owner_session, self.owner_visit, self.term)
            live = (incumbent and incumbent[5] == self.owner_session and
                    incumbent[2] == self.owner_visit and
                    not self._failed(authority) and
                    (self.owner == ctx["cid"] or now - self.received < LEASE))
            if live:
                self.role = 1 if self.owner == ctx["cid"] else 2
                return
            if incumbent:
                # Expiring a lease fences that authority term, not the peer.
                # The old owner must still receive its successor's checkpoint
                # so a two-client encounter converges when the stall ends.
                self.failed_authorities[authority[:3]] = max(self.failed_authorities.get(authority[:3], 0), self.term)
            if self.state is not None:
                candidates = [cid for cid, m in eligible.items()
                              if tuple(m[6:9]) == self.e and m[12] and
                              not self._failed((cid, m[5], m[2], self.term))]
                if candidates and min(candidates) == ctx["cid"]:
                    self.owner = ctx["cid"]
                    self.owner_session = ctx["session"]
                    self.owner_visit = self.local[1]
                    self.term += 1
                    self.sequence = 0
                    self.role = 1
                    self.received = now
                    self.published = None
                    self.takeover_frame = True
                    self.incoming.clear()
                    self.delivered.clear()
                    self.revision += 1
                    return
            self.role = 0
            return
        claims = [(tuple(m[6:9]), m[9], m[10], m[11])
                  for m in eligible.values() if encounter(m[6:9]) and positive(m[9])]
        if claims:
            e, term, owner, session = min(claims, key=lambda c: (c[0], -c[1], c[2]))
            if owner not in eligible:
                replicas = [cid for cid, m in eligible.items() if tuple(m[6:9]) == e and m[12]]
                if replicas:
                    owner = min(replicas)
                    session = eligible[owner][5]
            if not self._is_retired(e) and owner in eligible:
                self.e, self.term, self.owner, self.owner_session = e, term, owner, session
                self.owner_visit = eligible[owner][2]
                self.received = now
                self.role = 0  # Native follower cannot run before a checkpoint.
                return
        candidates = [cid for cid, m in eligible.items() if m[12] or m[13]]
        if (self.capture_started is not None and now - self.capture_started >= DISCOVERY and
                now - self.started >= DISCOVERY and candidates and min(candidates) == ctx["cid"]):
            self.e = (ctx["cid"], ctx["session"], self.local[1])
            self.owner, self.owner_session = ctx["cid"], ctx["session"]
            self.owner_visit = self.local[1]
            self.term, self.sequence, self.role = 1, 0, 1
            self.received = now
            self.revision += 1

    def update(self, ctx, ready, visit, paused, supplied, now):
        self._scope(ctx)
        ready = bool(ready and positive(visit) and ctx["connected"] and
                     ctx["loaded"] and ctx["room"] == ROOM and positive(ctx["cid"]))
        previous_ready, previous_visit, _ = self.local
        if ready != previous_ready or (ready and visit != previous_visit):
            old_e = self.e
            if old_e and not any(
                    cid != ctx["cid"] and (m := self._peer(ctx, cid)) and tuple(m[6:9]) == old_e
                    for cid in ctx["players"]):
                self._retire(old_e)
                self.cache.pop(old_e, None)
            self.e = None
            self.term = self.owner = self.owner_session = self.owner_visit = self.role = self.sequence = 0
            self.state = None
            self.acks.clear()
            self.incoming.clear()
            self.delivered.clear()
            self.outgoing.clear()
            self.published = None
            self.started = now
            self.capture_started = None
            self.last_request = -1e9
        self.local = (ready, int(visit) if ready else 0, bool(paused) if ready else False)
        valid_state = validate_state(supplied)
        self.capture_ready = bool(ready and valid_state is not None)
        if self.capture_ready and self.capture_started is None:
            self.capture_started = now
        elif not self.capture_ready and self.e is None:
            self.capture_started = None
        local_epoch = ctx["players"].get(ctx["cid"], {}).get("playerEpoch", 0)
        self.outgoing = deque(h for h in self.outgoing if h[2] == local_epoch)
        self.advertisement(ctx)
        self._prune(ctx, now)
        for cid, (packet, arrived) in list(self.pending_packets.items()):
            if now - arrived >= LEASE or self.receive(ctx, packet, now, False):
                self.pending_packets.pop(cid, None)
        out = []
        if ready:
            self._choose(ctx, now)
        else:
            self.role = 0
        if self.role == 1:
            self.paused = bool(paused)
            if valid_state is not None and not self.takeover_frame:
                for h in self.delivered:
                    self.acks[tuple(h[:3])] = h[3]
                self.delivered.clear()
                self.state = valid_state
                self._ack_outgoing()
            peers = [cid for cid in self._eligible(ctx) if cid != ctx["cid"]]
            if self.state is not None:
                # Keep the solo checkpoint current without sending a stream.
                self.cache[self.e] = self._checkpoint(now)
                signature = json.dumps([self.state, self.paused, sorted(self.acks.items())], separators=(",", ":"))
                if peers and now - self.last_wire >= INTERVAL - 1e-9:
                    if signature != self.published or self.force_snapshot:
                        self.sequence += 1
                        out.append(self._packet(ctx, "s", q=self.sequence, p=int(self.paused),
                                                d=self.state, a=[[*key, seq] for key, seq in sorted(self.acks.items())]))
                        self.published = signature
                        self.force_snapshot = False
                        self.last_wire = now
                        self.cache[self.e] = self._checkpoint(now)
                    elif now - self.last_wire >= KEEPALIVE - 1e-9:
                        out.append(self._packet(ctx, "k", q=self.sequence, p=int(self.paused)))
                        self.last_wire = now
            if self.outgoing:
                self._queue_hit(self.outgoing[0], now)
        elif ready and self.owner and (self.state is None or self.force_snapshot) and now - self.last_request >= KEEPALIVE - 1e-9:
            out.append(self._packet(ctx, "r", targetClientId=self.owner))
            self.last_request = now
        if self.role == 2 and self.outgoing and now - self.last_hit_wire >= RETRY - 1e-9:
            h = self.outgoing[0]
            out.append(self._packet(ctx, "h", targetClientId=self.owner, h=h[2:]))
            self.last_hit_wire = now
        hits = []
        if self.role == 1 and not self.paused and valid_state is not None and not self.delivered and not self.takeover_frame:
            peers = sorted(cid for cid, pending in self.incoming.items() if pending)
            if peers:
                offset = self.hit_cursor % len(peers)
                for cid in (peers[offset:] + peers[:offset])[:HITS_PER_FRAME]:
                    hit = self.incoming[cid].popleft()
                    peer = ctx["players"].get(cid, {})
                    eligible = ((cid == ctx["cid"] and self.local[0]) or self._peer(ctx, cid, True))
                    if (eligible and peer.get("interactionSession") == hit[1] and
                            peer.get("playerEpoch") == hit[2] and
                            self.acks.get(tuple(hit[:3]), 0) < hit[3]):
                        hits.append(hit)
                self.hit_cursor += len(hits)
                self.delivered = hits
        self.takeover_frame = False
        self.advertisement(ctx)
        status = {"role": self.role if self.state is not None or self.role == 1 else 0,
                  "encounter": list(self.e or (0, 0, 0)), "term": self.term,
                  "owner": self.owner, "revision": self.revision,
                  "state": self.state, "paused": int(self.paused), "hits": hits}
        return status, out
