"""Tsurami checkpoint schema and transient boss/reflection coordination."""

import json

from anchor_boss_transport import (
    BossTransport,
    DISCOVERY,
    HIT_QUEUE,
    HITS_PER_FRAME,
    INTERVAL,
    KEEPALIVE,
    LEASE,
    MAX_INT,
    RETRY,
    encounter,
    merge_metadata as _merge_metadata,
    metadata as _metadata,
    positive,
)


VERSION = 1
ROOM = 0x71
PACKET_TYPE = "MNSG_TSURAMI"
METADATA_KEY = "mnsgTsurami"
MAX_STATE_BYTES = 7680
MAX_STATUS_BYTES = 12288
ROOT_WORDS = 35
VISUAL_WORDS = 4
MAX_PROJECTILES = 32
PROJECTILE_WORDS = 20
DAMAGE_AMOUNTS = (1, 2, 3, 4, 8)
# Full current-state hazards are larger than Congo/Dharumanyo records. Rotate
# acknowledgment history in eight-row slices to retain the 8 KiB wire ceiling.
ACK_ROWS = 8


def _u32(word):
    return type(word) is int and 0 <= word <= 0xffffffff


def validate_state(value):
    """Validate the exact integer-only native checkpoint shape.

    ``r`` contains 35 root words; ``v`` contains four linked visual words.
    ``s`` is the projectile generation and ``t`` the encounter tick. ``p``
    contains at most 32 flat 20-word current-state hazard records. Native C
    validates phases, flags, animation and float bit patterns before use.
    """
    if not isinstance(value, dict) or set(value) != {"r", "v", "s", "t", "p"}:
        return None
    if (not isinstance(value["r"], list) or len(value["r"]) != ROOT_WORDS or
            not all(_u32(word) for word in value["r"]) or
            not isinstance(value["v"], list) or len(value["v"]) != VISUAL_WORDS or
            not all(_u32(word) for word in value["v"]) or
            not _u32(value["s"]) or not _u32(value["t"]) or
            not isinstance(value["p"], list) or
            len(value["p"]) > MAX_PROJECTILES * PROJECTILE_WORDS or
            len(value["p"]) % PROJECTILE_WORDS or
            not all(_u32(word) for word in value["p"])):
        return None
    encoded = json.dumps(value, separators=(",", ":"), allow_nan=False)
    return value if len(encoded.encode("utf-8")) <= MAX_STATE_BYTES else None


def metadata(value):
    return _metadata(value, VERSION)


def merge_metadata(previous, incoming, expected_session=None):
    return _merge_metadata(previous, incoming, expected_session, VERSION)


class TsuramiTransport(BossTransport):
    def __init__(self):
        super().__init__(
            packet_type=PACKET_TYPE,
            room=ROOM,
            metadata_key=METADATA_KEY,
            validate_state=validate_state,
            damage_amounts=DAMAGE_AMOUNTS,
            version=VERSION,
            include_wire_version=True,
            hit_target=True,
            ack_rows=ACK_ROWS,
        )
