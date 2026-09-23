"""Koryuta's shared wave producer and its two damaging native wave models."""
KIND, ENTITY, ROOM, PARENT = 11, 0x1b0, 0x155, 5
ROLE, TARGET_X, TARGET_Y, TARGET_Z, STOP, INSTANCE, RECEIPT, ESTABLISHED, PRESENT = range(74,83)
WAIT, PRODUCER, FLIGHT, PURSUE, COAST, PURSUE2 = range(30,36)
ROOT_ORIGIN, BLADE_ORIGIN, LOOT_ORIGIN = 0x7ffffff1, 0x7ffffff0, None
MODELS = (ENTITY,0x12d,0xfa)


def bounds(shared):
    b=list(shared)
    b[6]=(1,KIND);b[42]=(0,PURSUE2)
    b[ROLE]=(0,2)
    b[TARGET_X]=b[TARGET_Y]=b[TARGET_Z]=(-3200000,3200000)
    b[STOP]=b[ESTABLISHED]=b[PRESENT]=(0,1)
    b[INSTANCE]=b[RECEIPT]=(0,0x7fffffff)
    return b


def scope(r,room):
    role=r[ROLE]
    return (room==ROOM and r[7]==PARENT and r[8]==ENTITY and
            ((role==0 and r[9] in (0,ENTITY)) or
             (role==1 and r[9]==MODELS[1] and r[42]==FLIGHT) or
             (role==2 and r[9]==MODELS[2] and PURSUE<=r[42]<=PURSUE2)))


def valid(r):
    role=r[ROLE]
    if (r[6]!=KIND or not scope(r,ROOM) or r[32]!=163 or r[INSTANCE]<0 or r[RECEIPT]<0 or
            r[ESTABLISHED] not in (0,1) or r[PRESENT] not in (0,1) or r[STOP] not in (0,1) or
            r[10] or r[65] or not 0<=r[43]<=255 or any(r[33:42]) or
            any(not -3200000<=v<=3200000 for v in r[TARGET_X:TARGET_Z+1])):
        return False
    if not role:
        return (not any(r[27:31]) and not r[11] and not any(r[18:24]) and
                not r[67] and not any(r[12:15]) and not any(r[TARGET_X:TARGET_Z+1]) and
                r[64]>=0 and r[5]!=1 and r[42] in (WAIT,PRODUCER) and -1<=r[31]<=512)
    return (r[27]==(0x7e3 | (4 if role==1 else 0)) and
            r[28:31]==[0x220,0x8000,0] and r[11]==1 and
            r[19:21]==[128,1] and r[67]==17 and r[18]>=0 and r[64]>=1 and
            r[53]==1 and r[50:53]==[1,10,0] and r[54]==0x21 and r[47:50]==[300,200,-40] and
            r[55:59]==[2,10,10,5] and
            not r[STOP] and (-1 if r[5]==2 else 0)<=r[31]<=(100 if role==1 else 80))


def loot_shape(row):
    return False


def loot(row,room):
    return False


def identity(r,placed):
    if not valid(r) or not scope(r,placed.room):
        return None
    if not r[ROLE]:
        return [ROOT_ORIGIN,placed.signature,placed.room+1,PARENT]
    return [BLADE_ORIGIN,placed.signature,(PARENT<<10)|(placed.room+1),r[64]]


def family(k):
    return (ROOT_ORIGIN,k[1],k[2] if k[0]==ROOT_ORIGIN else k[2]&0x3ff,PARENT)


def typed(r,room):
    return valid(r) and scope(r,room)
