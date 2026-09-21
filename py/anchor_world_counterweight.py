"""File40's three six-piece counterweights in room 0x6B.

Each root checkpoint contains all six heights; rider inputs travel separately
so an observer can operate a platform without becoming its simulator.
"""
KIND, ENTITY, ROOM = 12, 0x3cb, 0x6b
INPUT, AGGREGATE = 46, 47
POSES = {13: (0, -8000, -2000, 0, 256, 0),
         14: (-30000, -8000, 0, 0, 512, 0),
         15: (30000, -8000, 0, 0, 0, 0)}
RANGES = ((0, 10000), (0, 6000), (0, 2000),
          (-2000, 0), (-6000, 0), (-10000, 0))
SPEEDS = {0: 0, 1: 2400, 2: 1600, 4: 800, 8: 800, 16: 1600, 32: 2400}


def valid(row):
    if (len(row) != 50 or any(type(v) is not int for v in row) or
            row[0] not in POSES or row[1:3] != [ENTITY, KIND] or
            tuple(row[4:10]) != POSES[row[0]] or
            row[3] not in (0, 1) or row[16] != 63 or
            row[17] not in SPEEDS or row[18] != SPEEDS[row[17]] or
            row[38] not in (0, 1) or
            not 0 <= row[INPUT] <= 63 or not 0 <= row[AGGREGATE] <= 63 or
            any(not 0 <= row[i] <= 0x7fffffff for i in (48, 49))):
        return False
    if any(row[i] for i in (*range(19, 38), *range(39, 46))):
        return False
    return all(row[5] + lo <= row[10+i] <= row[5] + hi
               for i, (lo, hi) in enumerate(RANGES))
