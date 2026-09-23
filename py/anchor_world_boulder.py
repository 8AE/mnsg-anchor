"""File34's placed falling boulders (entity 0x3DA) and their race duplicates.

One boulder is one shared actor: it is spawned at an original placement, ages
through four phases and expires. The race multiplier can spawn extra boulders
from the same placement, so the actor key is the placed roster slot plus the
race ordinal; ordinal 0 is the original and 1.. are the duplicates. A duplicate
gets a small random X/Z offset, which is why the *pose* is per-actor while the
*birth* stays the immutable original placement coordinate from the roster.

There is no linked child and no native loot, and expiry is not a kill: a
removal carries no committer and is only trusted from the acting owner. Absent
native copies are deliberately not reconstructed, so a replica waits for a
live capture instead of inventing a task.

Wire rows never carry the local native instance serial or its apply receipt:
those two words stay on the bridge-facing row, and a replica only counts a row
as established once the native side echoed the token after the real apply.
"""

KIND = 13
ENTITY = 0x3DA
# Native phase 3 swaps the model as it hands over to phase 4, so phases 0..3
# carry the plain boulder model and the final phase carries the cracked one.
# Confirmed against the C worker's integration.
FALLING_MODEL = 0x191
PHASE_MAX = 4
AGE_MAX = 150
ORDINAL_MAX = 65535
ROUTE_MARK = 163
ROOT_ORIGIN = 0x7FFFFFEC
# Single-role family: no linked child and no native drop.
BLADE_ORIGIN = None
LOOT_ORIGIN = None
ROLE, RESERVED0, RESERVED1, RESERVED2, RESERVED3, INSTANCE, RECEIPT, \
    ESTABLISHED, PRESENT = range(74, 83)
# Shared words this recipe reads. They mirror anchor_world_dynamic.h; the
# module cannot import the transport that imports it.
SERIAL, LIFE, PARENT, ENTITY_W, MODEL, CLIP, ANIMATED = 3, 5, 7, 8, 9, 10, 11
X, Y, Z, VX, VY, VZ, TIMER, ROUTE, PHASE, TALKABLE, DIALOG = \
    12, 13, 14, 21, 22, 23, 31, 32, 42, 40, 41
ORDINAL, BUSY = 64, 65
BIRTH_X, BIRTH_Y, BIRTH_Z = 61, 62, 63
# The native actor keeps this whole path/route block zero; a boulder never
# walks a patrol route.
PATH_FIRST, PATH_LAST = 33, 39
LIVE, CLAIM, REMOVED = range(3)
MAX_TOKEN = 0x7FFFFFFF
# room -> {one-based placed slot: original placement coordinate}, from the
# production roster (rom sha256
# e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c): eight
# boulders in room 0x13D and twelve in room 0x13F.
SCOPE = {
    0x13D: {10: (9, -65, 161), 11: (47, -95, 222), 12: (21, -25, 80),
            13: (59, 134, -237), 14: (94, 134, -237), 15: (130, 134, -237),
            16: (89, 94, -155), 17: (68, 34, -34)},
    0x13F: {8: (84, -213, 465), 9: (62, -233, 509), 10: (-23, -116, -192),
            11: (11, -96, -152), 12: (117, -183, 403), 18: (39, 66, -94),
            19: (5, 36, -35), 20: (51, 86, -136), 21: (15, 76, -112),
            22: (-60, 236, -626), 23: (-100, 236, -626),
            24: (-142, 236, -626)},
}


def bounds(shared):
    """The shared numeric table with the boulder columns overridden."""
    table = list(shared)
    table[6] = (1, KIND)
    table[TIMER] = (0, AGE_MAX)
    table[ROUTE] = (ROUTE_MARK, ROUTE_MARK)
    table[PHASE] = (0, PHASE_MAX)
    table[ORDINAL] = (0, ORDINAL_MAX)
    table[CLIP] = (0, 0)
    table[ROLE] = (0, 0)
    table[RESERVED0] = table[RESERVED1] = (0, 0)
    table[RESERVED2] = table[RESERVED3] = (0, 0)
    table[INSTANCE] = table[RECEIPT] = (0, MAX_TOKEN)
    table[ESTABLISHED] = table[PRESENT] = (0, 1)
    for index in range(PATH_FIRST, PATH_LAST + 1):
        table[index] = (0, 0)
    return table


def placement(room, parent):
    """The immutable original placement coordinate, or None."""
    return SCOPE.get(room, {}).get(parent)


def scope(row, room):
    """Exact room, roster slot, entity, ordinal bound and immutable birth."""
    original = placement(room, row[PARENT])
    return (original is not None and row[ENTITY_W] == ENTITY and
            0 <= row[ORDINAL] <= ORDINAL_MAX and
            (row[BIRTH_X], row[BIRTH_Y], row[BIRTH_Z]) == original)


def valid(row):
    """The typed recipe, mirroring anchor_world_boulder_valid exactly."""
    if any(type(v) is not int for v in row):
        return False
    if (row[6] != KIND or row[ROLE] or row[RESERVED0] or row[RESERVED1] or
            row[RESERVED2] or row[RESERVED3] or
            row[INSTANCE] < 0 or row[RECEIPT] < 0 or
            row[ESTABLISHED] not in (0, 1) or row[PRESENT] not in (0, 1) or
            not 0 <= row[PHASE] <= PHASE_MAX or
            not 0 <= row[TIMER] <= AGE_MAX or
            row[ROUTE] != ROUTE_MARK or
            row[TALKABLE] or row[DIALOG] or row[BUSY] or
            row[LIFE] == CLAIM or
            any(row[i] for i in range(PATH_FIRST, PATH_LAST + 1))):
        return False
    # Native phase 3 swaps the model as it hands over to phase 4.
    if row[MODEL] != (FALLING_MODEL if row[PHASE] >= PHASE_MAX else ENTITY):
        return False
    # The native phase-3 handover installs clip 0 with the animation rate and
    # loop flag set, so the final phase is the only one that must be animated.
    if row[CLIP] or (row[PHASE] >= PHASE_MAX and not row[ANIMATED]):
        return False
    # The native helper accepts a row bound to either boulder room, so the
    # recipe validates a capture before the acting room is known.
    return scope(row, 0x13D) or scope(row, 0x13F)


def identity(row, placed):
    """Canonical origin for a typed boulder row, or None when it is not one.

    The roster slot and the race ordinal are both part of the key, so two
    boulders spawned from the same placement never coalesce into one actor.
    The shared wire invariant is that every key word is non-zero, while the
    original boulder legitimately carries ordinal 0, so the key stores the
    ordinal biased by one: key word 1 is the original, 2.. are the duplicates.
    """
    if not valid(row) or not scope(row, placed.room):
        return None
    return [ROOT_ORIGIN, placed.signature,
            (row[PARENT] << 10) | (placed.room + 1), row[ORDINAL] + 1]


def family(k):
    """Each boulder is its own family; there is no emitter to arbitrate for."""
    return tuple(k)


def typed(row, room):
    """True when the row is a well-formed boulder row for this room."""
    return valid(row) and scope(row, room)
