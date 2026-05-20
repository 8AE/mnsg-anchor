#include "modding.h"
#include "recomputils.h"
#include "anchor_runtime.h"

typedef struct
{
    short x;
    short y;
    short z;
} RaceEnemyVec3s;

typedef struct
{
    RaceEnemyVec3s position;
    RaceEnemyVec3s rotation;
    void *definition;
    unsigned char is_spawned;
    unsigned char unk_11;
    unsigned char unk_12;
    unsigned char unk_13;
} RaceEnemyActorInstance;

typedef struct
{
    unsigned short actor_id;
    unsigned short params;
    unsigned int data[3];
} RaceEnemyActorDefinition;

extern int anchor_race_is_active(void);
extern int *func_80219CA0_5D5170(int *task, unsigned char actor_type);
extern int func_8003555C_3615C(int *owner, void *entry, int arg2, int flags,
                               int x, int y, int z, short pitch, short yaw, short roll,
                               float scale_x, float scale_y, float scale_z,
                               short arg13, short arg14, short model_id);
extern void func_80218A54_5D3F24(int actor, RaceEnemyActorInstance *instance);

extern void *D_802287BC_5E3C8C[];
extern unsigned char D_802297D6_5E4CA6[];
extern float D_80239AFC_5F4FCC;
extern RaceEnemyActorInstance *D_8015CDDC;
extern RaceEnemyActorDefinition *D_8015CDE0;

#define ACTOR_CAP_TOTAL (*(volatile int *)0x8015CCFC)
#define ACTOR_CAP_KIND_8 (*(volatile int *)0x8015CCE0)
#define ACTOR_CAP_KIND_9 (*(volatile int *)0x8015CCF0)
#define ACTOR_CAP_KIND_10 (*(volatile int *)0x8015CCE4)
#define ACTOR_CAP_KIND_6 (*(volatile int *)0x8015CCE8)
#define ACTOR_CAP_KIND_12 (*(volatile int *)0x8015CCEC)
#define DUPLICATE_POSITION_OFFSET 35

static int s_duplicate_enemy_guard = 0;

static void raise_enemy_actor_caps(void)
{
    if (ACTOR_CAP_TOTAL < 0x78)
        ACTOR_CAP_TOTAL = 0x78;
    if (ACTOR_CAP_KIND_8 < 0x40)
        ACTOR_CAP_KIND_8 = 0x40;
    if (ACTOR_CAP_KIND_9 < 0x40)
        ACTOR_CAP_KIND_9 = 0x40;
    if (ACTOR_CAP_KIND_10 < 0x30)
        ACTOR_CAP_KIND_10 = 0x30;
    if (ACTOR_CAP_KIND_6 < 0x40)
        ACTOR_CAP_KIND_6 = 0x40;
    if (ACTOR_CAP_KIND_12 < 0x10)
        ACTOR_CAP_KIND_12 = 0x10;
}

static int is_enemy_actor_id(unsigned short actor_id)
{
    switch (actor_id)
    {
    case 0x0CB: /* Ghost Robot Tsurami */
    case 0x0CA: /* Congo's Hand */
    case 0x0FA: /* Robot Bee */
    case 0x0FB:
    case 0x0FC:
    case 0x0FD:
    case 0x0FE:
    case 0x0FF:
    case 0x100:
    case 0x102:
    case 0x103:
    case 0x104:
    case 0x105:
    case 0x106:
    case 0x107:
    case 0x108:
    case 0x109:
    case 0x10A:
    case 0x10B:
    case 0x10C:
    case 0x110:
    case 0x12C:
    case 0x12D:
    case 0x12E:
    case 0x12F:
    case 0x130:
    case 0x131:
    case 0x132:
    case 0x133:
    case 0x136:
    case 0x13A:
    case 0x13B:
    case 0x13C:
    case 0x13D:
    case 0x13E:
    case 0x13F:
    case 0x140:
    case 0x141:
    case 0x144:
    case 0x145:
    case 0x147:
    case 0x148:
    case 0x149:
    case 0x190:
    case 0x191:
    case 0x198:
    case 0x19A:
    case 0x19D:
    case 0x1A6:
    case 0x1A9:
    case 0x1AA:
    case 0x1B0:
    case 0x1B6:
    case 0x2F6:
    case 0x323:
    case 0x334:
    case 0x354:
    case 0x362:
    case 0x365:
    case 0x3CA:
    case 0x3D1:
    case 0x3DA:
    case 0x3EF:
    case 0x3FC:
        return 1;
    default:
        return 0;
    }
}

static short offset_coord(short value, short offset)
{
    int result = (int)value + (int)offset;

    if (result > 32767)
        return 32767;
    if (result < -32768)
        return -32768;
    return (short)result;
}

RECOMP_HOOK("func_80218A54_5D3F24")
void anchor_race_double_enemy_spawn_hook(int actor, RaceEnemyActorInstance *instance)
{
    RaceEnemyActorInstance duplicate_instance;
    RaceEnemyActorDefinition *definition;
    unsigned short actor_id;
    unsigned char actor_type;
    short model_id;
    int *owner;
    int duplicate;
    float scale;

    if (s_duplicate_enemy_guard || !anchor_race_is_active() ||
        !anchor_runtime_double_enemies_enabled() || !instance)
        return;

    definition = D_8015CDE0;
    if (D_8015CDDC != instance || !definition)
        return;

    actor_id = definition->actor_id;
    if (!is_enemy_actor_id(actor_id))
        return;

    raise_enemy_actor_caps();
    actor_type = D_802297D6_5E4CA6[actor_id + 0x80D];
    owner = func_80219CA0_5D5170(0, actor_type);
    if (!owner)
        return;

    model_id = *(short *)&D_802297D6_5E4CA6[actor_id * 2];
    scale = D_80239AFC_5F4FCC;
    duplicate = func_8003555C_3615C(owner, D_802287BC_5E3C8C[actor_id], 0, 0xC006D920,
                                    0, 0, 0, 0, 0, 0,
                                    scale, scale, scale, 0, 0, model_id);
    if (!duplicate)
        return;

    duplicate_instance = *instance;
    duplicate_instance.position.x = offset_coord(instance->position.x, DUPLICATE_POSITION_OFFSET);
    duplicate_instance.position.z = offset_coord(instance->position.z, DUPLICATE_POSITION_OFFSET);

    s_duplicate_enemy_guard = 1;
    func_80218A54_5D3F24(duplicate, &duplicate_instance);
    s_duplicate_enemy_guard = 0;
}
