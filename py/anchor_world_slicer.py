"""Rooms 0xAB/0xAC slicer emitters and their flying blades (entity 0x19D).

A root sits in a fixed per-room/per-parent recipe and emits blades; a blade
flies out, lands and expires. Both roles are portable scalars: a root's ORDINAL
is a mutable emission counter, a blade's is an immutable serial. The recipe is
checked before the tombstone early return, so a removal still carries the typed
identity of the actor it retired.

A dropped item keeps its normal item kind and is typed by the slicer entity
plus a BASE_Y marker, so it gets its own origin without widening the optional
union. Wire rows never carry the local native instance serial or its apply
receipt: those two words stay on the bridge-facing row, and a replica only
counts a row as established once the native side echoed the token after the
real apply.
"""
KIND = 8
ENTITY = 0x19d
ROOT, BLADE = 0, 1
INITIAL_WAIT, BIRTH, REPEAT_WAIT, FLIGHT = 18, 19, 20, 21
ROLE, SUBTYPE, SPEED, REPEAT, INITIAL, INSTANCE, RECEIPT, ESTABLISHED, PRESENT = range(74, 83)
ROOT_ORIGIN = 0x7ffffff9
BLADE_ORIGIN = 0x7ffffff8
LOOT_ORIGIN = 0x7ffffff7
ROOT_DROP, BLADE_DROP = 2, 3
MAX_TOKEN = 0x7fffffff
# A dropped item keeps the normal item kind, so it is the slicer entity plus
# the BASE_Y marker that types it rather than the optional union.
DROPS = (2, 3, 4)

# The slicer's own numeric window: the kind and phase it introduces, and the
# columns it reuses from the shared optional union. Everything else keeps the
# shared table, so the NPC continuation stays signed 16-bit state.
OVERRIDES = {6: (1, KIND), 42: (0, FLIGHT),
             ROLE: (0, 1), SUBTYPE: (0, 2), SPEED: (0, 2), REPEAT: (1, 255),
             INITIAL: (2, 254), INSTANCE: (0, MAX_TOKEN),
             RECEIPT: (0, MAX_TOKEN), ESTABLISHED: (0, 1), PRESENT: (0, 1)}

# Fixed recipe per room and one-based parent index: subtype, speed, repeat,
# initial. It is part of the binding, so the wire and the native capture are
# both checked against it.
PARAMS = {0xab: {6: (0, 0, 120, 120), 7: (1, 1, 120, 180)},
          0xac: {6: (2, 2, 60, 30), 7: (2, 1, 50, 30),
                 8: (2, 0, 40, 40), 9: (2, 1, 50, 20)}}


def params(room, parent):
    """The fixed (subtype, speed, repeat, initial) tuple, or None."""
    return PARAMS.get(room, {}).get(parent)


def bounds(shared):
    """The shared numeric table with the slicer columns overridden."""
    table = list(shared)
    for index, (lo, hi) in OVERRIDES.items():
        table[index] = (lo, hi)
    return table


def scope(row, room):
    """Exact room and parent binding, matching anchor_world_slicer_scope."""
    fixed = params(room, row[7])
    return bool(fixed) and tuple(row[SUBTYPE:INITIAL + 1]) == fixed


def valid(row):
    """The typed recipe, mirroring anchor_world_slicer_valid exactly."""
    if any(type(v) is not int for v in row):
        return False
    role = row[ROLE]
    if (role not in (ROOT, BLADE) or not 0 <= row[SUBTYPE] <= 2 or
            not 0 <= row[SPEED] <= 2 or not 1 <= row[REPEAT] <= 255 or
            not 2 <= row[INITIAL] <= 254 or row[INITIAL] & 1 or
            row[INSTANCE] < 0 or row[RECEIPT] < 0 or
            not 0 <= row[ESTABLISHED] <= 1 or not 0 <= row[PRESENT] <= 1 or
            row[8] != ENTITY or row[9] != ENTITY or row[10] or
            row[11] != role or not 1 <= row[7] <= 256 or
            row[32] != 163 or row[27] != 0x6e1 or row[28] != 0x20 or
            row[20] or row[19] != (256 if role else 0) or
            row[18] > (25500 if role else 0) or row[67] != (1 if role else 17) or
            not 0 <= row[43] <= 255):
        return False
    if any(row[i] != 100 for i in (24, 25, 26)):
        return False
    if any(row[i] for i in range(33, 42)) or row[65]:
        return False
    if role:
        return row[64] >= 1 and row[42] == FLIGHT and -1 <= row[31] <= 120
    if row[21] or row[22] or row[23]:
        return False
    return ((row[42] == INITIAL_WAIT and 0 <= row[31] <= row[INITIAL] and
             not row[31] & 1) or
            (row[42] == BIRTH and -1 <= row[31] <= 0) or
            (row[42] == REPEAT_WAIT and 0 <= row[31] <= row[REPEAT]))


def drop_role(row):
    """The emitting role a dropped item carries in BASE_Y, or None."""
    return row[45] - 2 if row[45] in (ROOT_DROP, BLADE_DROP) else None


def loot_shape(row):
    """Room-independent shape of a slicer drop.

    The native child keeps its own item kind and recipe, so only the inherited
    entity, the BASE_Y marker, the placed parent, the emitter ordinal and an
    unused optional union type it: BASE_Y is 2 for a root drop and 3 for a blade
    drop, and the ordinal keeps two blades of the same root apart."""
    role = drop_role(row)
    if role is None or row[6] not in DROPS or not 1 <= row[7] <= 256:
        return False
    if row[8] != ENTITY or any(row[74:]):
        return False
    return bool(row[64]) if role == BLADE else not row[64]


def loot(row, room):
    """A dropped item bound to a placed slicer parent in this room."""
    return loot_shape(row) and params(room, row[7]) is not None


def root_identity(row, placed):
    return [ROOT_ORIGIN, placed.signature, placed.room + 1, row[7]]


def blade_identity(row, placed):
    return [BLADE_ORIGIN, placed.signature, (row[7] << 10) | (placed.room + 1), row[64]]


def loot_identity(row, placed):
    # The kind and the emitter role are packed beside the parent. The serial is
    # never zero: a root drop uses 1, because the role field already keeps it
    # apart from a blade drop of the same root.
    return [LOOT_ORIGIN, placed.signature,
            (row[7] << 13) | ((row[6] - 2) << 11) | ((row[45] - 2) << 10) |
            (placed.room + 1), row[64] or 1]


def identity(row, placed):
    """Canonical origin for a typed slicer row, or None when it is not one."""
    if loot(row, placed.room):
        return loot_identity(row, placed)
    if not valid(row) or not scope(row, placed.room):
        return None
    if row[ROLE] == ROOT:
        return root_identity(row, placed)
    return blade_identity(row, placed)


def family(k):
    """The root origin a slicer key belongs to.

    A kill is arbitrated once per family, so a blade and a drop resolve to the
    root that emitted them instead of carrying their own arbiter."""
    if k[0] == ROOT_ORIGIN:
        return tuple(k)
    if k[0] == BLADE_ORIGIN:
        return (ROOT_ORIGIN, k[1], k[2] & 0x3ff, k[2] >> 10)
    return (ROOT_ORIGIN, k[1], k[2] & 0x3ff, k[2] >> 13)


def typed(row, room):
    """True when the row is a well-formed actor or loot row for this room."""
    return loot(row, room) or (valid(row) and scope(row, room))


class Tracker:
    """Per-key local instance, established flag and pending apply token.

    A key is only locally established once the native side echoed the token
    that addressed the current incarnation. A reloaded native actor is a new
    incarnation, so its predecessor's receipt and establishment do not carry
    over and a stale echo cannot establish it.

    The token is retained after the echo instead of being cleared: the bridge
    expects a live receipt word on every update, and dropping back to zero would
    let the native side freeze the row it already applied."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.entries = {}
        self.serial = 0

    def entry(self, k):
        return self.entries.setdefault(k, dict(instance=0, established=False, token=0))

    def observe(self, k, instance, receipt):
        """Fold one bridge-facing capture row into the tracker."""
        e = self.entry(k)
        if instance > 0 and instance != e['instance']:
            e['instance'] = instance
            e['established'] = False
            e['token'] = 0
        if receipt > 0 and e['token'] and e['token'] == receipt:
            e['established'] = True
        return e

    def offer(self, k, instance):
        """The token that addresses this instance, minted on a fresh offer.

        A missing native instance is still offered: the bridge mints the first
        serial on apply, and the owner bootstrap needs a token as much as a
        replica does."""
        e = self.entry(k)
        if e['instance'] != instance:
            e['instance'] = instance
            e['established'] = False
            e['token'] = 0
        if not e['token'] and self.serial < MAX_TOKEN - 1:
            self.serial += 1
            e['token'] = self.serial
        return e['token']
