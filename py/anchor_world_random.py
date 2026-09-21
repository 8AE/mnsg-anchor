"""The File30 random spawner and its independently scheduled children."""
KIND, ENTITY, CHILD_MODEL, ROOM, PARENT = 9, 0x3ef, 0x12f, 0x91, 36
ROOT, BLADE = 0, 1
ROOT_PHASE, CHILD_PHASE = 22, 23
ROLE, TARGET_X, TARGET_Z, RESERVED0, RESERVED1, INSTANCE, RECEIPT, ESTABLISHED, PRESENT = range(74, 83)
ROOT_ORIGIN, BLADE_ORIGIN, LOOT_ORIGIN = 0x7ffffff6, 0x7ffffff5, 0x7ffffff4
DROPS = (2, 3, 4)


def bounds(shared):
    table = list(shared)
    table[6] = (1, KIND)
    table[42] = (0, CHILD_PHASE)
    table[ROLE] = (0, 1)
    table[TARGET_X] = table[TARGET_Z] = (-3200000, 3200000)
    table[RESERVED0] = table[RESERVED1] = (0, 0)
    table[INSTANCE] = table[RECEIPT] = (0, 0x7fffffff)
    table[ESTABLISHED] = table[PRESENT] = (0, 1)
    return table


def scope(row, room):
    return (room == ROOM and row[7] == PARENT and row[8] == ENTITY and
            row[9] == (CHILD_MODEL if row[ROLE] else ENTITY))


def valid(row):
    role = row[ROLE]
    if (role not in (0, 1) or row[42] != (CHILD_PHASE if role else ROOT_PHASE) or
            not scope(row, ROOM) or row[32] != 163 or row[RESERVED0] or row[RESERVED1] or
            row[INSTANCE] < 0 or row[RECEIPT] < 0 or row[ESTABLISHED] not in (0, 1) or
            row[PRESENT] not in (0, 1) or row[10] or row[20] != role or row[67] != 17 or
            not 0 <= row[43] <= 255 or any(row[33:42]) or row[65]):
        return False
    if not role:
        return (row[5] != 1 and not any(row[27:31]) and not row[11] and
                not any(row[21:24]) and not row[18] and not row[19] and
                not row[TARGET_X] and not row[TARGET_Z] and row[64] >= 0)
    return (row[27:29] == [0x6e7, 0x20] and row[11] == 1 and row[19] == 64 and
            row[18] >= 0 and (-1 if row[5] == 2 else 0) <= row[31] <= 90 and row[64] >= 1 and
            -3200000 <= row[TARGET_X] <= 3200000 and
            -3200000 <= row[TARGET_Z] <= 3200000)


def loot_shape(row):
    # Only children can die and drop an item; the placed scheduler is invisible
    # and has no damage-admission flag.
    return (row[6] in DROPS and row[7] == PARENT and row[8] == ENTITY and
            row[45] == 3 and row[64] >= 1 and not any(row[74:]))


def loot(row, room):
    return room == ROOM and loot_shape(row)


def root_identity(row, placed):
    return [ROOT_ORIGIN, placed.signature, placed.room + 1, PARENT]


def blade_identity(row, placed):
    return [BLADE_ORIGIN, placed.signature, (PARENT << 10) | (placed.room + 1), row[64]]


def loot_identity(row, placed):
    return [LOOT_ORIGIN, placed.signature,
            (PARENT << 13) | ((row[6] - 2) << 11) | (1 << 10) | (placed.room + 1), row[64]]


def identity(row, placed):
    if loot(row, placed.room):
        return loot_identity(row, placed)
    if not valid(row) or not scope(row, placed.room):
        return None
    return blade_identity(row, placed) if row[ROLE] else root_identity(row, placed)


def family(k):
    return (ROOT_ORIGIN, k[1], k[2] if k[0] == ROOT_ORIGIN else k[2] & 0x3ff, PARENT)


def typed(row, room):
    return loot(row, room) or (valid(row) and scope(row, room))
