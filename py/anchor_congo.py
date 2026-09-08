"""Congo snapshot schema and compatibility wrapper for boss coordination."""

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
ROOM = 22
PACKET_TYPE = "MNSG_CONGO"
METADATA_KEY = "mnsgCongo"
MAX_STATE_BYTES = 4096
DAMAGE_AMOUNTS = (1, 2, 3, 4, 8)


def validate_state(value):
    """Exact native codec: two 24-word parts, two scalars, 7-word flames.

    Native code validates the meaning of these unsigned words, including float
    bit patterns. Python never accepts arbitrary object fields or pointers.
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
    return _metadata(value, VERSION)


def merge_metadata(previous, incoming, expected_session=None):
    return _merge_metadata(previous, incoming, expected_session, VERSION)


class CongoTransport(BossTransport):
    def __init__(self):
        super().__init__(
            packet_type=PACKET_TYPE,
            room=ROOM,
            metadata_key=METADATA_KEY,
            validate_state=validate_state,
            damage_amounts=DAMAGE_AMOUNTS,
            version=VERSION,
            include_wire_version=False,
        )
