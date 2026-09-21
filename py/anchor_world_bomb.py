"""File40 falling bomb blocks and the twelve children of one explosion."""
KIND, ENTITY, CHILD_MODEL = 10, 0x1a9, 0x7e
ROLE, ALPHA, CAUSE, VARIANT, UNUSED, INSTANCE, RECEIPT, ESTABLISHED, PRESENT = range(74,83)
IDLE, FALL, FUSE, EXPLODE, RING, PARTICLE = range(24,30)
ROOT_ORIGIN, BLADE_ORIGIN = 0x7ffffff3, 0x7ffffff2
LOOT_ORIGIN = None
POSES = {0x65:{8:(4000,12000,10000),9:(4000,12000,2000),10:(-3000,12000,-8000)},
         0x66:{16:(9000,12000,27000),17:(14000,12000,8000),
               18:(10000,12000,-2500),19:(14000,12000,-17000)}}
VARIANTS = (0,1,4,2,8)


def bounds(shared):
    b=list(shared)
    b[6]=(1,KIND);b[42]=(0,PARTICLE)
    b[ROLE]=(0,12);b[ALPHA]=(0,255);b[CAUSE]=(0,3)
    b[VARIANT]=(0,8);b[UNUSED]=(0,0)
    b[INSTANCE]=b[RECEIPT]=(0,0x7fffffff)
    b[ESTABLISHED]=b[PRESENT]=(0,1)
    return b


def scope(row,room):
    return tuple(row[61:64])==POSES.get(room,{}).get(row[7])


def valid(r):
    role,phase,variant=r[ROLE],r[42],r[VARIANT]
    if (r[6]!=KIND or r[8]!=ENTITY or not 0<=role<=12 or
            not 0<=r[ALPHA]<=255 or not 0<=r[CAUSE]<=3 or r[UNUSED] or
            r[INSTANCE]<0 or r[RECEIPT]<0 or r[ESTABLISHED] not in (0,1) or
            r[PRESENT] not in (0,1) or not 1<=r[7]<=19 or r[32]!=163 or
            r[11] or r[40] or r[41] or r[65] or r[66] not in (0,1) or
            not 0<=r[43]<=255 or r[44] not in (0,1) or r[45] not in (0,1) or
            not 0<=r[64]<=12 or r[47:50]!=[30,60,0] or r[51] or r[52] or
            r[50] not in (0,90) or any(r[33:36])):
        return False
    if not role:
        return (r[64]==0 and r[9]==ENTITY and r[10]==0 and variant==0 and
                r[53]==0 and r[50]==0 and r[67]==0 and phase in (IDLE,FALL,FUSE,EXPLODE))
    if r[9]!=CHILD_MODEL or r[64]!=role or r[45]:
        return False
    if role<=4:
        attack=role in (2,4)
        return (phase==RING and variant==VARIANTS[role] and r[10]==8 and
                r[53]==(6 if attack else 0) and r[50]==(90 if attack else 0) and
                r[67]==(17 if attack else 0))
    return phase==PARTICLE and variant==0 and r[10]==0 and not r[53] and not r[50] and not r[67]


def loot_shape(row):
    return False


def loot(row,room):
    return False


def identity(row,placed):
    if not valid(row) or not scope(row,placed.room):
        return None
    if not row[ROLE]:
        return [ROOT_ORIGIN,placed.signature,placed.room+1,row[7]]
    return [BLADE_ORIGIN,placed.signature,(row[7]<<10)|(placed.room+1),row[64]]


def family(k):
    return tuple(k) if k[0]==ROOT_ORIGIN else (ROOT_ORIGIN,k[1],k[2]&0x3ff,k[2]>>10)


def typed(row,room):
    return valid(row) and scope(row,room)
