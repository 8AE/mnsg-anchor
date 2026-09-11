"""Shared giant-robot Impact-battle snapshot and transient coordinator.

All four Impact bosses (Kashiwagi, Thaisamba, Balberra, D'Etoile) share the
dedicated file_13 battle-state block, so a single transport with a room set to
the live Impact stage serves every encounter. The native codec validates the
meaning of every word, including float bit patterns and the overlay callback.
Python never accepts arbitrary object fields or pointers.
"""

import json

from anchor_boss_transport import (
    BossTransport,
    merge_metadata as _merge_metadata,
    metadata as _metadata,
)

VERSION = 1
# The transport room is the live Impact stage (0x0220..0x0223); the native
# bridge sets it each frame before update().
ROOM = 0x0220
PACKET_TYPE = "MNSG_IMPACT"
METADATA_KEY = "mnsgImpact"
MAX_STATE_BYTES = 4096

# Battle boss HP at +0x60, player/mech Ryo ammo at +0x64, mech HP at +0x68,
# combat pause at +0x2C0 and the per-encounter clock at +0x2C8.
ROOT_WORDS = 5
DAMAGE_AMOUNTS = tuple(range(1, 256))

IMPACT_ENCOUNTER_MIN = 1
IMPACT_ENCOUNTER_MAX = 4
IMPACT_STAGE_MIN = 0x021C
IMPACT_STAGE_MAX = 0x023C
# The title-menu "Consecutive Fighting! Large Boss" mode plays all four bosses
# in this dedicated stage.
IMPACT_BOSS_RUSH_STAGE = 0x0260


def _impact_stage(stage):
    return (type(stage) is int and
            ((IMPACT_STAGE_MIN <= stage <= IMPACT_STAGE_MAX) or
             stage == IMPACT_BOSS_RUSH_STAGE))


def _u32(word):
    return type(word) is int and 0 <= word <= 0xFFFFFFFF


def validate_state(value):
    """Validate the exact integer-only Impact battle checkpoint shape.

    ``k`` is the encounter selector (1..4), ``s`` the native Impact stage and
    ``r`` the flat 21-word native snapshot. Float fields cross the bridge as
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
    return _metadata(value, VERSION)


def merge_metadata(previous, incoming, expected_session=None):
    return _merge_metadata(previous, incoming, expected_session, VERSION)


class ImpactTransport(BossTransport):
    def __init__(self):
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
        """Only accept checkpoints from this live Impact stage."""
        return value.get("s") == self.room

    def set_stage(self, stage):
        """Track the live Impact stage so peer room checks stay exact."""
        if _impact_stage(stage):
            self.room = stage
