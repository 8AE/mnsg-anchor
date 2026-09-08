"""Dharumanyo snapshot schema and transient encounter coordinator."""

import json

from anchor_boss_transport import (
    ACK_ROWS,
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
ROOM = 0x49
PACKET_TYPE = "MNSG_DHARUMANYO"
METADATA_KEY = "mnsgDharumanyo"
MAX_STATE_BYTES = 4096
ROOT_WORDS = 29
CARRIER_WORDS = 9
MAX_PROJECTILES = 16
PROJECTILE_WORDS = 11
DAMAGE_AMOUNTS = (1,)


def _u32(word):
    return type(word) is int and 0 <= word <= 0xffffffff


def validate_state(value):
    """Validate the exact integer-only native checkpoint shape.

    ``r`` contains 29 root words, ``c`` contains nine linked-carrier words,
    ``s`` is the projectile generation, ``t`` is the encounter tick, and ``p``
    is up to sixteen flat 11-word travelling-projectile records. Float fields
    cross the bridge as IEEE-754 u32 bits and get semantic checks in native C.
    """
    if not isinstance(value, dict) or set(value) != {"r", "c", "s", "t", "p"}:
        return None
    if (not isinstance(value["r"], list) or len(value["r"]) != ROOT_WORDS or
            not all(_u32(word) for word in value["r"]) or
            not isinstance(value["c"], list) or
            len(value["c"]) != CARRIER_WORDS or
            not all(_u32(word) for word in value["c"]) or
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


class DharumanyoTransport(BossTransport):
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
