"""File68's placed 0x338 fish collectibles (kind 14).

Each fish is a native-placed actor holding a private animation object, so a
network row identifies and arbitrates one fish but never reconstructs it. The
stable identity is the room, the one-based placed roster slot, the packed save
flag the native callback sets, and the fish variant (its colour).

Collection is a shared one-winner claim: the native callback increments the
per-colour counter *before* it sets the placement flag, so the claim must be
won before the local pickup runs or both peers would increment. The transport
reuses the dynamic claim/arbiter/commit machinery and only the winning client
lets its native pickup proceed.

Flag IDs repeat across the room pairs 0x168/0x17E and 0x16B/0x180, so the same
flag can be present in two rooms. Two clients collecting that pair in different
rooms at the same instant is a team-global lease gap this same-room typed claim
does not cover; see the transport tests.

Wire rows never carry the local native instance serial or its apply receipt:
those two words stay on the bridge-facing row, and a replica only counts a row
as established once the native side echoed the token after the real apply.
"""

KIND = 14
ENTITY = 0x338
ROUTE_MARK = 163
ROOT_ORIGIN = 0x7FFFFFEB
# Single-role family: no linked child and no native drop.
BLADE_ORIGIN = None
LOOT_ORIGIN = None
ROLE, FLAG, VARIANT, MODE, RESERVED, INSTANCE, RECEIPT, ESTABLISHED, PRESENT = \
    range(74, 83)
# Shared words this recipe reads. They mirror anchor_world_dynamic.h; the
# module cannot import the transport that imports it.
OWNER, SERIAL, LIFE, PARENT, ENTITY_W, MODEL, CLIP, ANIMATED = \
    4, 3, 5, 7, 8, 9, 10, 11
ROUTE, TALKABLE, DIALOG, PHASE, LANDED = 32, 40, 41, 42, 44
BIRTH_X, BIRTH_Y, BIRTH_Z, ORDINAL, BUSY, COMMITTER = 61, 62, 63, 64, 65, 72
LIVE, CLAIM, REMOVED = range(3)
MAX_TOKEN = 0x7FFFFFFF
# room -> (first one-based slot, packed save flag per slot, variant per slot).
# The two room pairs share one flag/variant list, which is why the same flag can
# appear in two rooms. Mirrors anchor_world_fish_placement exactly.
_PLACEMENTS = {
    0x168: (10, (0xB8, 0xB9, 0xBA, 0xBB, 0xBC), (0, 1, 2, 2, 0)),
    0x16B: (6, (0xAB, 0xAC, 0xAD, 0xAE, 0xAF), (0, 0, 1, 1, 2)),
    0x16E: (11, (0xA7, 0xA8, 0xA9, 0xAA), (0, 0, 0, 1)),
    0x171: (12, (0xB0, 0xB1, 0xB2, 0xB3), (0, 2, 2, 0)),
    0x172: (8, (0xBD, 0xBE, 0xBF, 0xC0, 0xC1), (0, 1, 1, 1, 2)),
    0x17E: (3, (0xB8, 0xB9, 0xBA, 0xBB, 0xBC), (0, 1, 2, 2, 0)),
    0x180: (2, (0xAB, 0xAC, 0xAD, 0xAE, 0xAF), (0, 0, 1, 1, 2)),
}
SCOPE = {room: {first + index: (flags[index], variants[index])
                for index in range(len(flags))}
         for room, (first, flags, variants) in _PLACEMENTS.items()}
# The paired rooms that share a flag list; a cross-room simultaneous collect of
# the same flag is not covered by the same-room typed claim.
PAIRED_ROOMS = ((0x168, 0x17E), (0x16B, 0x180))


def bounds(shared):
    """The shared numeric table with the fish columns overridden."""
    table = list(shared)
    table[6] = (1, KIND)
    table[ROUTE] = (ROUTE_MARK, ROUTE_MARK)
    table[PHASE] = table[ORDINAL] = (0, 0)
    table[CLIP] = table[ANIMATED] = (0, 0)
    table[TALKABLE] = table[DIALOG] = table[BUSY] = (0, 0)
    table[BIRTH_X] = table[BIRTH_Y] = table[BIRTH_Z] = (0, 0)
    table[ROLE] = table[MODE] = table[RESERVED] = (0, 0)
    table[INSTANCE] = table[RECEIPT] = (0, MAX_TOKEN)
    table[ESTABLISHED] = table[PRESENT] = (0, 1)
    # The row is an identity/claim record, not a portable copy of File68's
    # private animation state, so the whole clip..role span is forced clear
    # apart from the route, claim marker and arbiter words.
    for index in range(CLIP, ROLE):
        if index not in (ROUTE, LANDED, COMMITTER):
            table[index] = (0, 0)
    return table


def placement(room, parent):
    """The (packed save flag, variant) for one room and one-based slot."""
    return SCOPE.get(room, {}).get(parent)


def scope(row, room):
    """Exact room, roster slot, flag and variant binding."""
    fixed = placement(room, row[PARENT])
    return fixed is not None and (row[FLAG], row[VARIANT]) == fixed


def valid(row):
    """The typed recipe, mirroring anchor_world_fish_valid exactly."""
    if any(type(v) is not int for v in row):
        return False
    if (row[6] != KIND or row[ENTITY_W] != ENTITY or row[MODEL] != ENTITY or
            row[ROUTE] != ROUTE_MARK or
            row[ORDINAL] or row[PHASE] or row[CLIP] or row[ANIMATED] or
            row[TALKABLE] or row[DIALOG] or row[BUSY] or
            row[BIRTH_X] or row[BIRTH_Y] or row[BIRTH_Z] or
            row[ROLE] or row[MODE] or row[RESERVED] or
            row[INSTANCE] < 0 or row[RECEIPT] < 0 or
            row[ESTABLISHED] not in (0, 1) or row[PRESENT] not in (0, 1) or
            (row[LIFE] == CLAIM and not row[LANDED])):
        return False
    # This is an identity/claim row, not a portable copy of File68's private
    # animation state. Reject accidental pose or collision serialization.
    for index in range(CLIP, ROLE):
        if index not in (ROUTE, LANDED, COMMITTER) and row[index]:
            return False
    # The native helper accepts a row matching any placed fish, so a capture
    # validates before the acting room is known.
    return any(scope(row, room) for room in SCOPE)


def identity(row, placed):
    """Canonical origin for a typed fish row, or None when it is not one.

    The placed slot and the room are packed into one key word so two rooms that
    share a flag still produce distinct keys. The last key word is the fixed
    ordinal marker: fish always carry ordinal 0, and the shared wire invariant
    requires every key word to be non-zero.
    """
    if not valid(row) or not scope(row, placed.room):
        return None
    return [ROOT_ORIGIN, placed.signature,
            (row[PARENT] << 10) | (placed.room + 1), 1]


def family(k):
    """Each fish arbitrates for itself; there is no emitter above it."""
    return tuple(k)


def typed(row, room):
    """True when the row is a well-formed fish row for this room."""
    return valid(row) and scope(row, room)
