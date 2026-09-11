"""Reusable transient boss-encounter coordination for Anchor clients.

The transport owns only network-safe election, checkpoint, pause, and hit
delivery state. Each boss supplies its room, packet/metadata names, strict
snapshot validator, and accepted native damage values.
"""

from collections import deque
import json


MAX_INT = 0x7fffffff
INTERVAL = 0.1
KEEPALIVE = 1.0
LEASE = 3.0
DISCOVERY = 0.35
RETRY = 0.25
HIT_QUEUE = 8
HITS_PER_FRAME = 32
ACK_ROWS = 64


def positive(value):
    return type(value) is int and 0 < value <= MAX_INT


def encounter(value):
    return (isinstance(value, (list, tuple)) and len(value) == 3 and
            all(positive(x) for x in value))


def metadata(value, version=1):
    """Validate the compact per-client encounter advertisement."""
    if (not isinstance(value, list) or len(value) != 14 or
            any(type(x) is not int for x in value) or value[0] != version or
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


def merge_metadata(previous, incoming, expected_session=None, version=1):
    new = metadata(incoming, version)
    old = metadata(previous, version)
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


class BossTransport:
    """One independent transient encounter coordinator.

    Socket I/O and the roster lock remain owned by the caller. Checkpoints are
    team-routed and never durable; hits are direct to the elected authority.
    """

    def __init__(self, *, packet_type, room, metadata_key, validate_state,
                 damage_amounts, version=1, include_wire_version=False,
                 hit_target=False, require_save=True, require_room=True,
                 state_scope=None,
                 interval=INTERVAL, keepalive=KEEPALIVE, lease=LEASE,
                 discovery=DISCOVERY, retry=RETRY, hit_queue=HIT_QUEUE,
                 hits_per_frame=HITS_PER_FRAME, ack_rows=ACK_ROWS):
        self.packet_type = packet_type
        self.room = room
        self.metadata_key = metadata_key
        self.validate_state = validate_state
        self.damage_amounts = tuple(damage_amounts)
        self.version = version
        self.include_wire_version = include_wire_version
        self.hit_target = hit_target
        # The Impact battle can be entered from the title menu (boss rush) with
        # no save loaded, so its transport must not require one. Its stage is
        # also not reported through the ordinary room metadata, so it scopes by
        # the checkpoint's own stage/encounter instead of roomId.
        self.require_save = require_save
        self.require_room = require_room
        # Optional predicate on a validated state value; used to reject a
        # checkpoint that belongs to a different sub-scope (Impact stage).
        self.state_scope = state_scope
        self.interval = interval
        self.keepalive = keepalive
        self.lease = lease
        self.discovery = discovery
        self.retry = retry
        self.hit_queue = hit_queue
        self.hits_per_frame = hits_per_frame
        self.ack_rows = ack_rows
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
        self.force_generation = 0
        self.published = None
        self.cache = {}
        self.unavailable = {}
        self.failed_authorities = {}
        self.retired = {}
        self.pending_packets = {}
        self.members = {}
        self.acks = {}
        self.ack_pending = set()
        self.ack_cursor = None
        self.ack_revision = 0
        self.incoming = {}
        self.delivered = []
        self.outgoing = deque()
        self.hit_cursor = 0
        self.last_peer_hit = {}
        self.meta_revision = 0
        self.meta_value = None
        self.advertisement_dirty = True
        self.takeover_frame = False
        self.send_commits = {}

    def _metadata(self, value):
        return metadata(value, self.version)

    def _scope(self, ctx):
        scope = (ctx["session"], ctx["team"])
        if self.scope != scope:
            self.reset()
            self.scope = scope

    def advertisement(self, ctx):
        ready, visit, paused = self.local
        value = [self.version, int(ready), visit, int(paused), 0, ctx["session"],
                 *(self.e or (0, 0, 0)), self.term, self.owner,
                 self.owner_session, int(self.state is not None), int(self.capture_ready)]
        if (not self.meta_value or
                value[:4] + value[5:] != self.meta_value[:4] + self.meta_value[5:]):
            self.meta_revision += 1
            value[4] = self.meta_revision
            self.meta_value = value
            self.advertisement_dirty = True
        return list(self.meta_value)

    def _peer(self, ctx, cid, require_local_room=False):
        p = ctx["players"].get(cid)
        if (not p or not p.get("online", False) or
                (self.require_save and not p.get("isSaveLoaded", False))):
            return None
        m = self._metadata(p.get(self.metadata_key))
        if (not m or not m[1] or p.get("teamId") != ctx["team"] or
                (self.require_room and p.get("roomId") != self.room) or
                p.get("interactionSession") != m[5] or
                m[2] <= self.unavailable.get((cid, m[5]), 0)):
            return None
        if require_local_room and (
                not self.local[0] or
                (self.require_room and ctx["room"] != self.room)):
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
            m = self._metadata(p.get(self.metadata_key))
            if not m:
                continue
            if (p.get("online", False) and
                    (not self.require_save or p.get("isSaveLoaded", False)) and
                    p.get("teamId") == ctx["team"] and
                    (not self.require_room or p.get("roomId") == self.room) and
                    m[1]):
                current[cid] = (cid, m[5], m[2])
        for cid, identity in self.members.items():
            if current.get(cid) != identity:
                key = identity[:2]
                self.unavailable[key] = max(
                    self.unavailable.get(key, 0), identity[2])
        self.members = current

    def _is_retired(self, e):
        return e[2] <= self.retired.get(e[:2], 0)

    def _retire(self, e):
        self.retired[e[:2]] = max(self.retired.get(e[:2], 0), e[2])

    def _failed(self, authority):
        return authority[3] <= self.failed_authorities.get(authority[:3], 0)

    def _prune(self, ctx, now):
        eligible = self._eligible(ctx)
        sessions = {(cid, p.get("interactionSession"))
                    for cid, p in ctx["players"].items()}
        visits = {(cid, m[5], m[2]) for cid, m in eligible.items()}
        self.unavailable = {key: visit for key, visit in self.unavailable.items()
                            if key in sessions}
        self.failed_authorities = {
            key: term for key, term in self.failed_authorities.items()
            if key in visits
        }
        creator_sessions = sessions | ({self.e[:2]} if self.e else set())
        self.retired = {key: visit for key, visit in self.retired.items()
                        if key in creator_sessions}
        claims = {tuple(m[6:9]) for m in eligible.values()}
        for e, checkpoint in list(self.cache.items()):
            if (e != self.e and e not in claims and
                    now - checkpoint["received"] >= self.lease):
                self.cache.pop(e, None)
                self._retire(e)

        def current_hit(key):
            cid, session, epoch = key
            p = ctx["players"].get(cid, {})
            return (cid in eligible and p.get("interactionSession") == session and
                    p.get("playerEpoch") == epoch)

        self.acks = {key: seq for key, seq in self.acks.items()
                     if current_hit(key)}
        self.ack_pending.intersection_update(self.acks)
        for checkpoint in self.cache.values():
            checkpoint["acks"] = {
                key: seq for key, seq in checkpoint["acks"].items()
                if current_hit(key)
            }
        self.incoming = {cid: pending for cid, pending in self.incoming.items()
                         if cid in eligible and pending}
        self.last_peer_hit = {
            cid: stamp for cid, stamp in self.last_peer_hit.items()
            if cid in eligible
        }
        self.pending_packets = {
            cid: value for cid, value in self.pending_packets.items()
            if cid in ctx["players"] and now - value[1] < self.lease
        }

    def _packet(self, ctx, op, **fields):
        target_client_id = fields.pop("targetClientId", None)
        packet = dict(fields)
        packet.update({
            "type": self.packet_type,
            "clientId": ctx["cid"],
            "targetTeamId": ctx["team"],
            "session": ctx["session"],
            "op": op,
            "e": list(self.e or (0, 0, 0)),
            "term": self.term,
            "visit": self.local[1],
            "quiet": True,
        })
        if self.include_wire_version:
            packet["v"] = self.version
        if target_client_id is not None:
            packet["targetClientId"] = target_client_id
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
        self.ack_pending.clear()
        self.ack_cursor = None
        self.ack_revision += 1
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
                "visit": self.owner_visit, "state": self.state,
                "paused": self.paused, "acks": dict(self.acks),
                "received": now}

    def receive(self, ctx, packet, now, defer=True):
        self._scope(ctx)
        cid = packet.get("clientId")
        if (packet.get("type") != self.packet_type or
                (self.include_wire_version and
                 (type(packet.get("v")) is not int or
                  packet["v"] != self.version)) or
                not positive(cid) or cid == ctx["cid"] or
                packet.get("targetTeamId") != ctx["team"] or
                not positive(packet.get("session")) or
                packet.get("targetClientId", ctx["cid"]) != ctx["cid"]):
            return False
        source = self._peer(ctx, cid)
        if not source:
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
                self.force_generation += 1
                # A requester may have joined after earlier acknowledgments.
                # Replay them in bounded slices alongside the requested state.
                self.ack_pending.update(self.acks)
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
            if (not isinstance(h, list) or
                    len(h) != (4 if self.hit_target else 3) or
                    not all(positive(x) for x in h[:3]) or
                    h[2] not in self.damage_amounts or
                    (self.hit_target and
                     (type(h[3]) is not int or not 0 <= h[3] <= MAX_INT))):
                return False
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
        value = self.validate_state(packet.get("d"))
        rows = packet.get("a", [])
        if (value is None or
                (self.state_scope is not None and
                 not self.state_scope(value)) or
                not isinstance(rows, list) or len(rows) > self.ack_rows):
            return False
        acks = dict(previous["acks"]) if previous else {}
        for row in rows:
            if (not isinstance(row, list) or len(row) != 4 or
                    not all(positive(x) for x in row)):
                return False
            if row[0] not in ctx["players"]:
                continue
            key = tuple(row[:3])
            acks[key] = max(acks.get(key, 0), row[3])
        checkpoint = {"e": e, "term": term, "owner": cid,
                      "session": packet["session"], "q": packet["q"],
                      "visit": packet["visit"], "state": value,
                      "paused": bool(packet["p"]), "acks": acks,
                      "received": now}
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
        if (len(pending) >= self.hit_queue or
                now - self.last_peer_hit.get(hit[0], -1e9) <
                self.interval - 1e-9):
            return False
        pending.append(hit)
        self.last_peer_hit[hit[0]] = now
        return True

    def send_hit(self, ctx, sequence, amount, now, target=0):
        if (self.role not in (1, 2) or self.state is None or
                not self.local[0] or self.local[2] or not self.e or
                not positive(sequence) or type(amount) is not int or
                amount not in self.damage_amounts or
                type(target) is not int or
                not 0 <= target <= (MAX_INT if self.hit_target else 0)):
            return False
        epoch = ctx["players"].get(ctx["cid"], {}).get("playerEpoch", 0)
        if not positive(epoch):
            return False
        hit = [ctx["cid"], ctx["session"], epoch, sequence, amount]
        if self.hit_target:
            hit.append(target)
        if any(h[:4] == hit[:4] for h in self.outgoing):
            return True
        if self.acks.get(tuple(hit[:3]), 0) >= sequence:
            return True
        if (len(self.outgoing) >= self.hit_queue or
                now - self.last_hit_added < self.interval - 1e-9):
            return False
        if (self.outgoing and tuple(self.outgoing[-1][:3]) == tuple(hit[:3]) and
                sequence <= self.outgoing[-1][3]):
            return False
        self.outgoing.append(hit)
        self.last_hit_added = now
        return True

    def _choose(self, ctx, now):
        eligible = self._eligible(ctx)
        valid_cache = []
        for e, checkpoint in list(self.cache.items()):
            m = eligible.get(checkpoint["owner"])
            live_owner = (m and m[5] == checkpoint["session"] and
                          m[2] == checkpoint["visit"] and
                          now - checkpoint["received"] < self.lease)
            replica = any(tuple(member[6:9]) == e and member[12]
                          for member in eligible.values())
            if not self._is_retired(e) and (live_owner or replica):
                valid_cache.append(checkpoint)
        if self.e is None and valid_cache:
            self._adopt(min(valid_cache, key=lambda c: c["e"]), now)
            self.takeover_frame = self.owner == ctx["cid"]
        if self.e is not None:
            incumbent = eligible.get(self.owner)
            authority = (self.owner, self.owner_session,
                         self.owner_visit, self.term)
            live = (incumbent and incumbent[5] == self.owner_session and
                    incumbent[2] == self.owner_visit and
                    not self._failed(authority) and
                    (self.owner == ctx["cid"] or
                     now - self.received < self.lease))
            if live:
                self.role = 1 if self.owner == ctx["cid"] else 2
                return
            if incumbent:
                self.failed_authorities[authority[:3]] = max(
                    self.failed_authorities.get(authority[:3], 0), self.term)
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
                    self.ack_pending.update(self.acks)
                    self.takeover_frame = True
                    self.incoming.clear()
                    self.delivered.clear()
                    self.revision += 1
                    return
            self.role = 0
            return
        claims = [(tuple(m[6:9]), m[9], m[10], m[11])
                  for m in eligible.values()
                  if encounter(m[6:9]) and positive(m[9])]
        if claims:
            e, term, owner, session = min(
                claims, key=lambda c: (c[0], -c[1], c[2]))
            if owner not in eligible:
                replicas = [cid for cid, m in eligible.items()
                            if tuple(m[6:9]) == e and m[12]]
                if replicas:
                    owner = min(replicas)
                    session = eligible[owner][5]
            if not self._is_retired(e) and owner in eligible:
                self.e, self.term = e, term
                self.owner, self.owner_session = owner, session
                self.owner_visit = eligible[owner][2]
                self.received = now
                self.role = 0
                return
        candidates = [cid for cid, m in eligible.items() if m[12] or m[13]]
        if (self.capture_started is not None and
                now - self.capture_started >= self.discovery and
                now - self.started >= self.discovery and candidates and
                min(candidates) == ctx["cid"]):
            self.e = (ctx["cid"], ctx["session"], self.local[1])
            self.owner, self.owner_session = ctx["cid"], ctx["session"]
            self.owner_visit = self.local[1]
            self.term, self.sequence, self.role = 1, 0, 1
            self.received = now
            self.revision += 1

    def update(self, ctx, ready, visit, paused, supplied, now):
        self._scope(ctx)
        ready = bool(ready and positive(visit) and ctx["connected"] and
                     ctx["loaded"] and
                     (not self.require_room or ctx["room"] == self.room) and
                     positive(ctx["cid"]))
        previous_ready, previous_visit, _ = self.local
        if ready != previous_ready or (ready and visit != previous_visit):
            old_e = self.e
            if old_e and not any(
                    cid != ctx["cid"] and (m := self._peer(ctx, cid)) and
                    tuple(m[6:9]) == old_e for cid in ctx["players"]):
                self._retire(old_e)
                self.cache.pop(old_e, None)
            self.e = None
            self.term = self.owner = self.owner_session = 0
            self.owner_visit = self.role = self.sequence = 0
            self.state = None
            self.acks.clear()
            self.ack_pending.clear()
            self.ack_cursor = None
            self.ack_revision += 1
            self.incoming.clear()
            self.delivered.clear()
            self.outgoing.clear()
            self.published = None
            self.started = now
            self.capture_started = None
            self.last_request = -1e9
            if ready:
                # A participant may return after the owner spent time alone
                # and stopped streaming. An old replica is not evidence that
                # this live owner lost its lease: request a fresh checkpoint
                # from its current advertisement before adopting/electing.
                self.cache = {
                    e: checkpoint for e, checkpoint in self.cache.items()
                    if now - checkpoint["received"] < self.lease
                }
        self.local = (ready, int(visit) if ready else 0,
                      bool(paused) if ready else False)
        valid_state = self.validate_state(supplied)
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
            if (now - arrived >= self.lease or
                    self.receive(ctx, packet, now, False)):
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
                    key = tuple(h[:3])
                    if h[3] > self.acks.get(key, 0):
                        self.acks[key] = h[3]
                        self.ack_pending.add(key)
                        self.ack_revision += 1
                self.delivered.clear()
                self.state = valid_state
                self._ack_outgoing()
            peers = [cid for cid in self._eligible(ctx) if cid != ctx["cid"]]
            if self.state is not None:
                self.cache[self.e] = self._checkpoint(now)
                signature = json.dumps(
                    [self.state, self.paused, self.ack_revision],
                    separators=(",", ":"))
                if peers and now - self.last_wire >= self.interval - 1e-9:
                    if (signature != self.published or self.force_snapshot or
                            self.ack_pending):
                        ack_keys = sorted(self.ack_pending)
                        if ack_keys and self.ack_cursor is not None:
                            split = next(
                                (index for index, key in enumerate(ack_keys)
                                 if key > self.ack_cursor),
                                len(ack_keys),
                            )
                            ack_keys = ack_keys[split:] + ack_keys[:split]
                        ack_keys = ack_keys[:self.ack_rows]
                        ack_values = [(key, self.acks[key]) for key in ack_keys
                                      if key in self.acks]
                        self.sequence += 1
                        packet = self._packet(
                            ctx, "s", q=self.sequence, p=int(self.paused),
                            d=self.state,
                            a=[[*key, seq] for key, seq in ack_values])
                        out.append(packet)
                        self.send_commits[id(packet)] = {
                            "kind": "s", "encounter": self.e,
                            "term": self.term, "owner": self.owner,
                            "signature": signature, "force": self.force_generation,
                            "acks": ack_values, "when": now,
                        }
                    elif now - self.last_wire >= self.keepalive - 1e-9:
                        packet = self._packet(
                            ctx, "k", q=self.sequence, p=int(self.paused))
                        out.append(packet)
                        self.send_commits[id(packet)] = {
                            "kind": "k", "encounter": self.e,
                            "term": self.term, "owner": self.owner,
                            "when": now,
                        }
            if self.outgoing:
                self._queue_hit(self.outgoing[0], now)
        elif (ready and self.owner and
              (self.state is None or self.force_snapshot) and
              now - self.last_request >= self.keepalive - 1e-9):
            packet = self._packet(ctx, "r", targetClientId=self.owner)
            out.append(packet)
            self.send_commits[id(packet)] = {
                "kind": "r", "encounter": self.e,
                "term": self.term, "owner": self.owner, "when": now,
            }
        if (self.role == 2 and self.outgoing and
                now - self.last_hit_wire >= self.retry - 1e-9):
            h = self.outgoing[0]
            packet = self._packet(
                ctx, "h", targetClientId=self.owner, h=h[2:])
            out.append(packet)
            self.send_commits[id(packet)] = {
                "kind": "h", "encounter": self.e,
                "term": self.term, "owner": self.owner, "when": now,
            }
        hits = []
        if (self.role == 1 and not self.paused and valid_state is not None and
                not self.delivered and not self.takeover_frame):
            peers = sorted(cid for cid, pending in self.incoming.items()
                           if pending)
            if peers:
                offset = self.hit_cursor % len(peers)
                for cid in (peers[offset:] + peers[:offset])[:self.hits_per_frame]:
                    hit = self.incoming[cid].popleft()
                    peer = ctx["players"].get(cid, {})
                    eligible = ((cid == ctx["cid"] and self.local[0]) or
                                self._peer(ctx, cid, True))
                    if (eligible and
                            peer.get("interactionSession") == hit[1] and
                            peer.get("playerEpoch") == hit[2] and
                            self.acks.get(tuple(hit[:3]), 0) < hit[3]):
                        hits.append(hit)
                self.hit_cursor += len(hits)
                self.delivered = hits
        self.takeover_frame = False
        self.advertisement(ctx)
        status = {
            "role": self.role if self.state is not None or self.role == 1 else 0,
            "encounter": list(self.e or (0, 0, 0)),
            "term": self.term,
            "owner": self.owner,
            "revision": self.revision,
            "state": self.state,
            "paused": int(self.paused),
            "hits": hits,
        }
        return status, out

    def packet_send_result(self, packet, sent):
        """Commit retry/cadence state only after the caller writes the frame."""
        commit = self.send_commits.pop(id(packet), None)
        if not sent or not commit:
            return
        if (commit["encounter"] != self.e or commit["term"] != self.term or
                commit["owner"] != self.owner):
            return
        kind = commit["kind"]
        if kind in ("s", "k"):
            self.last_wire = max(self.last_wire, commit["when"])
        elif kind == "r":
            self.last_request = max(self.last_request, commit["when"])
        elif kind == "h":
            self.last_hit_wire = max(self.last_hit_wire, commit["when"])
        if kind != "s":
            return
        self.published = commit["signature"]
        if self.force_generation == commit["force"]:
            self.force_snapshot = False
        if commit["acks"]:
            self.ack_cursor = commit["acks"][-1][0]
        for key, sequence in commit["acks"]:
            if self.acks.get(key, 0) == sequence:
                self.ack_pending.discard(key)
        self.cache[self.e] = self._checkpoint(commit["when"])
