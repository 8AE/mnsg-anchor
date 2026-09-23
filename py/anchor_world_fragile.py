"""File30 destructible placed props and their linked children (kind 12).

Six placed families share one portable recipe. Every root is bound to an exact
(room, one-based roster slot, entity) triple taken from the production roster,
so a peer cannot invent a placement or move one into another room. Only
0x331/0x332 install a linked child, and a placed root has at most one.

The native constructor of 0x332 switches on the placement's definition byte D0
(0..4), which is why the variant word is only meaningful for that family. The
common one-hit death runs through the incoming-hit bit and spawns an ordinary
item entity, so a drop keeps its normal item kind and is typed by the emitting
role in BASE_Y instead of widening the optional union.

Wire rows never carry the local native instance serial or its apply receipt:
those two words stay on the bridge-facing row, and a replica only counts a row
as established once the native side echoed the token after the real apply.
"""

KIND = 12
# Placed families. Only 0x331/0x332 install a linked child.
ROOT_ENTITIES = (0x196, 0x330, 0x331, 0x332, 0x339, 0x3EC)
CHILD_ENTITIES = (0x331, 0x332)
# The shared table resolves the markers the slicer and the random spawner share
# through a single-entity match; this family is multi-entity and owns its
# markers outright, so the value only has to stay distinct from every entity.
ENTITY = ROOT_ENTITIES
ROOT, CHILD = 0, 1
ROLE, VARIANT, RESERVED0, RESERVED1, RESERVED2 = range(74, 79)
INSTANCE, RECEIPT, ESTABLISHED, PRESENT = range(79, 83)
# Shared words the recipe reads. They mirror anchor_world_dynamic.h; the module
# cannot import the transport that imports it.
LIFE, ROUTE, TALKABLE, DIALOG, LANDED, ORDINAL, BUSY, COMMITTER = 5, 32, 40, 41, 44, 64, 65, 72
LIVE, CLAIM, REMOVED = range(3)
ROOT_ORIGIN, CHILD_ORIGIN, LOOT_ORIGIN = 0x7FFFFFEF, 0x7FFFFFEE, 0x7FFFFFED
# The shared family table reads the child origin under this name.
BLADE_ORIGIN = CHILD_ORIGIN
ROOT_DROP, CHILD_DROP = 4, 5
DROPS = (2, 3, 4)
MAX_TOKEN = 0x7FFFFFFF
MAX_PARENT = 256
ROUTE_MARK = 163
PHASE = 23
VARIANT_MAX = 4
# 0x339 is a save-gated presentation actor, so its model and clip are fixed and
# its variant is always zero; the native adapter withholds the root entirely
# while save flag 0x32 is set.
SAVE_ENTITY, SAVE_MODEL = 0x339, 0x24F
VARIANT_ENTITY = 0x332
# The native definition byte D0 reaches the wire only for the families that
# switch on it; 0x330 keys its clip off the same byte.
VARIANT_MAX = {0x330: 2, VARIANT_ENTITY: 4, SAVE_ENTITY: 0}
# The clip a placed root or its linked child installs. 0x330 is variant
# dependent and handled separately.
CLIPS = {(SAVE_ENTITY, ROOT): (4,), (0x331, ROOT): (1,), (0x331, CHILD): (0,),
         (VARIANT_ENTITY, ROOT): (0,), (VARIANT_ENTITY, CHILD): (1, 3)}
# room -> {one-based roster slot: entity}. The slot is the native placed index
# plus one, which is what anchor_world_fragile_scope matches as a bit mask; the
# production roster (rom sha256
# e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c) holds 44x
# 0x196, 58x 0x330, 1x 0x331, 5x 0x332, 2x 0x339 and 7x 0x3EC over 117 slots.
SCOPE = {
    0x5:{10:0x196,11:0x196},
    0x7:{7:0x196,8:0x196,9:0x196,10:0x196},
    0x11:{10:0x196,11:0x196,12:0x196},
    0x14:{11:0x196,12:0x196},
    0x54:{2:0x196,3:0x196,4:0x196,5:0x196},
    0x61:{11:0x196,12:0x196,13:0x196,14:0x196,15:0x196},
    0x6A:{8:0x196,9:0x196},
    0x81:{6:0x3EC,8:0x330},
    0x82:{10:0x3EC,16:0x330,17:0x330,18:0x330,19:0x330,20:0x330},
    0x83:{10:0x330,11:0x330,12:0x330,13:0x330,14:0x330,15:0x330},
    0x84:{1:0x330,2:0x330},
    0x85:{12:0x332,13:0x332,14:0x332,15:0x332,16:0x332,20:0x330,21:0x330},
    0x86:{19:0x3EC,25:0x196,26:0x196,27:0x196,28:0x196,29:0x330,30:0x330},
    0x87:{34:0x330,35:0x330},
    0x88:{4:0x330},
    0x89:{19:0x330,20:0x330},
    0x8A:{14:0x330},
    0x8B:{7:0x330},
    0x8C:{15:0x3EC,23:0x330,24:0x330,25:0x330,26:0x330,27:0x330},
    0x8D:{13:0x330,14:0x330,15:0x330},
    0x8E:{9:0x330,10:0x330},
    0x90:{9:0x3EC,13:0x330,14:0x330},
    0x91:{19:0x3EC,28:0x331,29:0x330,30:0x330,31:0x330,32:0x330,33:0x330,34:0x330,35:0x330},
    0x94:{7:0x330},
    0x95:{10:0x196,11:0x196,13:0x330,14:0x330,15:0x330},
    0x96:{12:0x330,13:0x330},
    0x97:{9:0x196,10:0x196,11:0x196,12:0x330,13:0x330},
    0x98:{7:0x330,8:0x330},
    0x99:{9:0x3EC,16:0x330,17:0x330,18:0x330},
    0x9D:{5:0x330},
    0xAE:{16:0x196},
    0xBA:{15:0x196,16:0x196},
    0x142:{15:0x196,16:0x196,17:0x196,19:0x196,20:0x196,23:0x196},
    0x14E:{20:0x339},
    0x15C:{15:0x339},
    0x164:{9:0x196,10:0x196,11:0x196,12:0x196},
}


def bounds(shared):
    """The shared numeric table with the fragile columns overridden."""
    table = list(shared)
    table[6] = (1, KIND)
    table[ROUTE] = (ROUTE_MARK, ROUTE_MARK)
    table[42] = (PHASE, PHASE)
    table[ORDINAL] = (ROOT, CHILD)
    table[ROLE] = (ROOT, CHILD)
    table[RESERVED0] = table[RESERVED1] = table[RESERVED2] = (0, 0)
    table[INSTANCE] = table[RECEIPT] = (0, MAX_TOKEN)
    table[ESTABLISHED] = table[PRESENT] = (0, 1)
    return table


def entity(room, parent):
    """The exact placed entity for one room and one-based roster slot."""
    return SCOPE.get(room, {}).get(parent)


def scope(row, room):
    """Exact room, roster slot and entity binding, matching the native table."""
    placed = entity(room, row[7])
    return (placed is not None and row[8] == placed and
            (row[ROLE] != CHILD or placed in CHILD_ENTITIES))


def appearance(entity, role, variant, model, clip, animated):
    """The model/clip pair a placed root or its linked child installs.

    This is a direct mirror of the per-entity checks in
    anchor_world_fragile_valid, including 0x330's variant-keyed clip. The
    variant word itself keeps its shared bound, so a family that ignores D0 is
    not tightened beyond what the native adapter validates.
    """
    if model != (SAVE_MODEL if entity == SAVE_ENTITY else entity):
        return False
    limit = VARIANT_MAX.get(entity)
    if limit is not None and variant > limit:
        return False
    if entity == 0x330:
        # The native tint continuation installs clip 1 only for variant 2.
        return not animated and clip == (1 if variant == 2 else 0)
    if entity in (0x196, 0x3EC):
        return not animated and clip == 0
    if entity == SAVE_ENTITY:
        return role == ROOT and not variant and not animated and clip == 4
    if entity == 0x331:
        return not animated and clip == (0 if role == CHILD else 1)
    # A linked 0x332 child is the only animated role in the family.
    return (animated == role and
            (clip in (1, 3) if role == CHILD else clip == 0))


def valid(row):
    """The typed recipe, mirroring anchor_world_fragile_valid exactly."""
    if any(type(v) is not int for v in row):
        return False
    # A drop keeps the shared item kind, so it is validated by the generic item
    # recipe and only *typed* here through loot()/identity(). This early return
    # keeps the fragile recipe on the placed actor it actually owns.
    if (row[6] != KIND or row[8] not in ROOT_ENTITIES or
            row[ROLE] not in (ROOT, CHILD) or row[ORDINAL] != row[ROLE] or
            not 1 <= row[7] <= MAX_PARENT or row[ROUTE] != ROUTE_MARK or
            row[42] != PHASE or not 0 <= row[VARIANT] <= 255 or
            row[RESERVED0] or row[RESERVED1] or row[RESERVED2] or
            row[INSTANCE] < 0 or row[RECEIPT] < 0 or
            row[ESTABLISHED] not in (0, 1) or row[PRESENT] not in (0, 1) or
            row[TALKABLE] or row[DIALOG] or row[BUSY]):
        return False
    if row[ROLE] == CHILD and row[8] not in CHILD_ENTITIES:
        return False
    # A claim is only a hit, and a landed removal is only a kill once the
    # arbitrating committer accepted it. The native fast death path is armed
    # from LANDED, so a route exit relayed with the flag clear stays inert.
    if row[LIFE] == CLAIM and not row[LANDED]:
        return False
    if row[LIFE] == REMOVED and row[LANDED] and not row[COMMITTER]:
        return False
    return appearance(row[8], row[ROLE], row[VARIANT], row[9], row[10], row[11])


def loot_shape(row):
    """Room-independent shape of a fragile drop.

    The native death path spawns an ordinary item, so the row keeps the shared
    item kind and is typed by the emitting role in BASE_Y rather than by an
    entity of its own. The ordinal repeats that role and the union stays empty,
    which keeps a root drop and a child drop of the same slot from ever sharing
    a key.

    This mirrors the shared item recipe in the codec, not just the marker: a
    malformed drop must not be *typed* as fragile loot while the codec would
    reject the same row, or a rejected snapshot could poison the merged
    response. Tombstones keep the marker check but skip the appearance and
    phase windows, exactly like the codec's removal early return."""
    if row[6] not in DROPS or row[45] not in (ROOT_DROP, CHILD_DROP):
        return False
    if row[ORDINAL] != row[45] - ROOT_DROP or any(row[74:]):
        return False
    if row[LIFE] == REMOVED:
        return True
    model, clip, animated, phase = row[9], row[10], row[11], row[42]
    if row[6] == 2:
        return (model, clip, animated) == (1, 4, 0) and phase in (1, 2, 13)
    if row[6] == 3:
        return (model, clip, animated) == (1, 3, 0) and phase in (3, 14)
    return (model, clip, animated) == (0x85, 0, 0) and phase == 4


def loot(row, room):
    """A dropped item bound to a placed fragile parent in this room.

    The native capture keeps the spawned item's own entity, so the placed slot
    and the emitting role are what bind the drop to its root or child. A child
    drop can only come from a family that installs one."""
    if not loot_shape(row):
        return False
    placed = entity(room, row[7])
    return placed is not None and (row[45] == ROOT_DROP or placed in CHILD_ENTITIES)


def root_identity(row, placed):
    return [ROOT_ORIGIN, placed.signature, placed.room + 1, row[7]]


def child_identity(row, placed):
    return [CHILD_ORIGIN, placed.signature, placed.room + 1, row[7]]


def loot_identity(row, placed):
    # The item kind and the emitting role are packed beside the parent. The
    # serial is always 1: the role field already keeps a root drop apart from a
    # child drop of the same placed slot.
    return [LOOT_ORIGIN, placed.signature,
            (row[7] << 13) | ((row[6] - 2) << 11) |
            ((row[45] - ROOT_DROP) << 10) | (placed.room + 1), 1]


def identity(row, placed):
    """Canonical origin for a typed fragile row, or None when it is not one."""
    if loot(row, placed.room):
        return loot_identity(row, placed)
    if not valid(row) or not scope(row, placed.room):
        return None
    if row[ROLE] == ROOT:
        return root_identity(row, placed)
    return child_identity(row, placed)


def family(k):
    """The root origin a fragile key belongs to.

    A kill is arbitrated once per family, so a linked child and a drop resolve
    to the root that owns them instead of carrying their own arbiter."""
    if k[0] == ROOT_ORIGIN:
        return tuple(k)
    if k[0] == CHILD_ORIGIN:
        return (ROOT_ORIGIN, k[1], k[2], k[3])
    return (ROOT_ORIGIN, k[1], k[2] & 0x3FF, k[2] >> 13)


def typed(row, room):
    """True when the row is a well-formed actor or loot row for this room."""
    return loot(row, room) or (valid(row) and scope(row, room))
