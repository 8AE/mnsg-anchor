"""Portable File 59 continuation validation, matching anchor_world_npc.h."""

MODELS = [0x2bd,0x2be,0x2bf,0x2c0,0x2c1,0x2c2,0x2c2] + [0x2c4]*6
MODELS += [0x2c5,0x2c6,0x2c7,0x2ca,0x2c9,0x2c8,0x2cb,0x2cc,0x2cd]


def valid(entity, model, r):
    if len(r) != 9 or any(type(v) is not int for v in r): return False
    phase = r[0]
    if phase == 0: return not any(r)
    if not 1 <= phase <= 22: return False
    if model != MODELS[phase-1] and (phase,model) != (19,0x2ce): return False
    if entity != model and (entity,phase) not in ((0x2c3,7),(0x2ce,19),(0x2d8,3),(0x2d9,5)):
        return False
    if any(not -32768 <= v <= 32767 for v in r[1:8]): return False
    if not 0 <= r[8] <= 6 or r[8]&3 == 3: return False
    if model == 0x2c4:
        if r[8]&3 not in (1,2): return False
        if not 0 <= r[4] <= (16 if r[8]&3 == 1 else 24) or r[4] & 1: return False
        if r[5] not in (-2,0,2) or r[6] not in (0,1) or not 0 <= r[7] <= 9999:
            return False
        if phase == 13 and (r[4] < 2 or not r[5]): return False
    else:
        if r[8]&3 or any(r[3:8]): return False
        if phase in (1,15):
            if r[1] or not 0 <= r[2] <= 2: return False
        elif r[1] or r[2]: return False
    return True
