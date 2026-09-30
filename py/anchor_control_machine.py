"""Control Machine encounter coordination for the Koryuta boss room."""

import json

from anchor_boss_transport import (
    BossTransport,
    encounter,
    merge_metadata as _merge_metadata,
    metadata as _metadata,
    positive,
)


VERSION = 2
ROOM = 0x155
PACKET_TYPE = "MNSG_CONTROL_MACHINE"
METADATA_KEY = "mnsgControlMachine"
MAX_STATE_BYTES = 4096
ROOT_WORDS = 24
MAX_PROJECTILES = 32
PROJECTILE_WORDS = 9
DAMAGE_AMOUNTS = (1,)


def validate_state(value):
    """Validate the native integer checkpoint before boss or intro playback.

    ``r`` contains the typed callback, timer, HP, movement, animation and
    owned flash alpha (word 21, 256 for no owned flash), raw child color
    (word 22), and a positive command serial (word 23). Native C validates
    callback/status pairs and float bit patterns.
    ``p`` contains at most 32 current projectile rows with nine words each.
    """
    if not isinstance(value, dict) or set(value) != {"fight", "r", "p"}:
        return None
    if (type(value["fight"]) is not int or value["fight"] != VERSION or
            not isinstance(value["r"], list) or
            len(value["r"]) != ROOT_WORDS or
            any(type(word) is not int or not 0 <= word <= 0xffffffff
                for word in value["r"]) or
            value["r"][0] not in range(1, 10) or
            value["r"][21] > 256 or
            value["r"][23] == 0 or
            not isinstance(value["p"], list) or
            len(value["p"]) > MAX_PROJECTILES or
            any(not isinstance(row, list) or
                len(row) != PROJECTILE_WORDS or
                any(type(word) is not int or not 0 <= word <= 0xffffffff
                    for word in row)
                for row in value["p"])):
        return None
    encoded = json.dumps(value, separators=(",", ":"), allow_nan=False)
    return value if len(encoded.encode("utf-8")) <= MAX_STATE_BYTES else None


def metadata(value):
    return _metadata(value, VERSION)


def merge_metadata(previous, incoming, expected_session=None):
    return _merge_metadata(previous, incoming, expected_session, VERSION)


class ControlMachineTransport(BossTransport):
    def __init__(self):
        super().__init__(
            packet_type=PACKET_TYPE,
            room=ROOM,
            metadata_key=METADATA_KEY,
            validate_state=validate_state,
            damage_amounts=DAMAGE_AMOUNTS,
            version=VERSION,
            include_wire_version=True,
        )

    def reset(self):
        self.observer_scope = None
        self.observing_visit = 0
        super().reset()

    def advertisement(self, ctx):
        if not self.observing_visit:
            return super().advertisement(ctx)
        # A bound intro-stage player needs the owner's checkpoint but must
        # remain ineligible for authority, hit submission, and takeover. Only
        # its wire advertisement says present; internal local.ready stays false.
        local = self.local
        self.local = (True, self.observing_visit, True)
        try:
            return super().advertisement(ctx)
        finally:
            self.local = local

    def _incumbent(self, ctx, now):
        claims = []
        fresh = []
        for cid in sorted(ctx["players"]):
            m = self._peer(ctx, cid)
            if (m and m[10] == cid and m[11] == m[5] and
                    positive(m[9]) and m[12] and encounter(m[6:9])):
                e = tuple(m[6:9])
                claims.append((e, -m[9], cid))
                checkpoint = self.cache.get(e)
                if (checkpoint and checkpoint["owner"] == cid and
                        checkpoint["session"] == m[5] and
                        checkpoint["visit"] == m[2] and
                        checkpoint["term"] == m[9] and
                        now - checkpoint["received"] < self.lease and
                        not self._is_retired(e)):
                    fresh.append((e, -m[9], cid, checkpoint))
        if fresh:
            _, _, cid, checkpoint = min(fresh, key=lambda row: row[:3])
            return cid, checkpoint
        if claims:
            return min(claims)[2], None
        return 0, None

    def update(self, ctx, ready, visit, paused, supplied, now):
        self._scope(ctx)
        observing = (not ready and positive(visit) and ctx["connected"] and
                     ctx["loaded"] and ctx["room"] == self.room and
                     positive(ctx["cid"]))
        scope = (ctx["session"], ctx["team"], ctx["room"], visit) if observing else None
        if self.observer_scope and scope != self.observer_scope and not ready:
            # A previous intro visit must never supply a new visit's skip cue.
            self.cache.clear()
            self.pending_packets.clear()
            self.last_request = -1e9
        self.observer_scope = scope
        self.observing_visit = visit if observing else 0
        status, packets = super().update(ctx, ready, visit, paused, supplied, now)
        status["preview"] = None
        if observing:
            owner, checkpoint = self._incumbent(ctx, now)
            # The native intro may briefly wait for a checkpoint from this
            # advertised fight owner. A roster claim alone never authorizes
            # skipping dialogue or applying combat state.
            status["owner"] = owner
            if checkpoint is not None:
                status["preview"] = checkpoint["state"]
            elif owner and now - self.last_request >= self.keepalive - 1e-9:
                packet = self._packet(ctx, "r", targetClientId=owner)
                packet["visit"] = visit
                packets.append(packet)
                self.send_commits[id(packet)] = {
                    "kind": "r", "encounter": self.e, "term": self.term,
                    "owner": self.owner, "when": now,
                }
        return status, packets
