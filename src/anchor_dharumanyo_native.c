#include "anchor_dharumanyo_native.h"
#include "anchor_dharumanyo_damage.h"
#include "anchor_boss_invite_world.h"
#include "anchor_remote_model_pool.h"
#include "boss_sync.h"

#ifndef ANCHOR_DHARUMANYO_NATIVE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

typedef void (*DharumanyoCallback)(void *, void *);

#define U8(p, o) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)))
#define U16(p, o) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)))
#define U32(p, o) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)))
#define F32(p, o) \
    (*(volatile float *)((unsigned char *)(p) + (o)))
#ifndef DHARUMANYO_PTR
#define DHARUMANYO_PTR(p, o) \
    (*(void *volatile *)((unsigned char *)(p) + (o)))
#endif
#ifndef DHARUMANYO_AI
#define DHARUMANYO_AI(p) \
    (*(DharumanyoCallback volatile *)((unsigned char *)(p) + 0x0c))
#define DHARUMANYO_POST(p) \
    (*(DharumanyoCallback volatile *)((unsigned char *)(p) + 0x10))
#endif

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern unsigned char D_8015CC30_15D830[];
extern void *func_800141C4_14DC4(unsigned int file);
extern void *func_8021DDE8_5D92B8(void *, DharumanyoCallback,
                                  unsigned char, float, float, float, int);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern float func_8001B5AC_1C1AC(void *);
extern int func_800240DC_24CDC(int flag_id);

extern void func_08000104_6C8314(void *, void *);
extern void func_08000970_6C8B80(void *, void *);
extern void func_08000A0C_6C8C1C(void *, void *);
extern void func_08000AB0_6C8CC0(void *, void *);
extern void func_08000BD8_6C8DE8(void *, void *);
extern void func_08000CCC_6C8EDC(void *, void *);
extern void func_08000E98_6C90A8(void *, void *);
extern void func_08000F70_6C9180(void *, void *);
extern void func_080010A0_6C92B0(void *, void *);
extern void func_08001148_6C9358(void *, void *);
extern void func_080011C8_6C93D8(void *, void *);
extern void func_08001260_6C9470(void *, void *);
extern void func_08001360_6C9570(void *, void *);
extern void func_08001540_6C9750(void *, void *);
extern void func_08001618_6C9828(void *, void *);
extern void func_08001730_6C9940(void *, void *);
extern void func_080017B8_6C99C8(void *, void *);
extern void func_08001810_6C9A20(void *, void *);
extern void func_080018B4_6C9AC4(void *, void *);
extern void func_0800195C_6C9B6C(void *, void *);
extern void func_08001A24_6C9C34(void *, void *);
extern void func_08001B1C_6C9D2C(void *, void *);
extern void func_08001C1C_6C9E2C(void *, void *);
extern void func_08001CC8_6C9ED8(void *, void *);
extern void func_08001E4C_6CA05C(void *, void *);
extern void func_08001F08_6CA118(void *, void *);
extern void func_080020B4_6CA2C4(void *, void *);
extern void func_0800218C_6CA39C(void *, void *);
extern void func_080022BC_6CA4CC(void *, void *);
extern void func_08002380_6CA590(void *, void *);
extern void func_08002410_6CA620(void *, void *);
extern void func_080025C8_6CA7D8(void *, void *);
extern void func_08002620_6CA830(void *, void *);
extern void func_0800284C_6CAA5C(void *, void *);
extern void func_08002A40_6CAC50(void *, void *);
extern void func_08003810_6CBA20(void *, void *);
extern void func_08003F78_6CC188(void *, void *);
extern void func_08003F84_6CC194(void *, void *);
extern void func_0800432C_6CC53C(void *, void *);
extern void func_80218F30_5D4400(void *, void *);

#define DHARUMANYO_ROOM 0x49u
#define DHARUMANYO_FILE 0x1fu
#define DHARUMANYO_ACTOR 0xccu
#define DHARUMANYO_ENTITY 0xccu
#define DHARUMANYO_PAUSE_FLAG 0x16cu
#ifndef CALLBACK_DISABLED
#define CALLBACK_DISABLED 0x00800000ul
#endif
#define REMOVE_PENDING 0x00000002u
#define RECOVERY 0x00000001u
#define ROOT_FLAGS_MASK 0x016002e1u
#define ROOT_FLAGS2_MASK 0x00000002u
#define PHASE_FLAGS_MASK 0x00001fffu
#define PROJECTILE_MAX_AGE 1800u
#define PROJECTILE_SERIAL_MAX 0x7fffffffu
#define TICK_MAX 0x7fffffffu
#define ROOT_CLIP_COUNT 19u
#define PHASE_COUNT 33u
#define ROOT_ATTACK_SELECTOR_MAX 4u
#define ROOT_PROJECTILE_BURST_MAX 2u
#define ROOT_FADE_STEP_MAX 0x180u
#define ROOT_OPACITY_MAX 0xffu
#define ROOT_COLLIDER_0 150u
#define ROOT_COLLIDER_1 300u
#define ROOT_COLLIDER_2 230u

typedef struct DharumanyoActor
{
    void *task;
    void *object;
    unsigned short actor;
    unsigned char generation;
    DharumanyoCallback held_ai;
    DharumanyoCallback held_post;
    unsigned char held;
} DharumanyoActor;

typedef struct DharumanyoProjectile
{
    DharumanyoActor actor;
    unsigned int id;
    unsigned int born;
} DharumanyoProjectile;

/* Overlay function addresses cannot live in initialized pointer data in a
 * Recomp mod. Populate this volatile BSS table after file 31 is resident. */
static DharumanyoCallback volatile s_phases[PHASE_COUNT];
static DharumanyoActor s_root;
static DharumanyoActor s_carrier;
static DharumanyoProjectile
    s_projectiles[ANCHOR_DHARUMANYO_MAX_PROJECTILES];
static DharumanyoActor s_pending_projectile;
static AnchorDharumanyoNativeSnapshot s_pending_snapshot;
static AnchorDharumanyoNativeSnapshot s_terminal_snapshot;
static unsigned int s_visit;
static unsigned int s_tick;
static unsigned int s_projectile_serial;
static unsigned int s_root_clip;
static unsigned int s_injected_id;
static unsigned int s_injected_born;
static int s_active;
static int s_owner;
static int s_paused;
static int s_adopted;
static int s_combat_active;
static int s_terminal_started;
static int s_terminal_snapshot_valid;
static int s_pending_valid;
static int s_pending_projectile_valid;
static int s_reconstructing;
static int s_injected_projectile;
static int s_root_post_seen;
static int s_root_pause_mirror;
static int s_target_valid;
static float s_target_object[8];
static void *s_target_saved;
static void *s_target_root;

static void initialize_callbacks(void)
{
    s_phases[0] = 0;
    s_phases[1] = func_08000970_6C8B80;
    s_phases[2] = func_08000A0C_6C8C1C;
    s_phases[3] = func_08000AB0_6C8CC0;
    s_phases[4] = func_08000BD8_6C8DE8;
    s_phases[5] = func_08000CCC_6C8EDC;
    s_phases[6] = func_08000E98_6C90A8;
    s_phases[7] = func_08000F70_6C9180;
    s_phases[8] = func_080010A0_6C92B0;
    s_phases[9] = func_08001148_6C9358;
    s_phases[10] = func_080011C8_6C93D8;
    s_phases[11] = func_08001260_6C9470;
    s_phases[12] = func_08001360_6C9570;
    s_phases[13] = func_08001540_6C9750;
    s_phases[14] = func_08001618_6C9828;
    s_phases[15] = func_08001730_6C9940;
    s_phases[16] = func_080017B8_6C99C8;
    s_phases[17] = func_08001810_6C9A20;
    s_phases[18] = func_080018B4_6C9AC4;
    s_phases[19] = func_0800195C_6C9B6C;
    s_phases[20] = func_08001A24_6C9C34;
    s_phases[21] = func_08001B1C_6C9D2C;
    s_phases[22] = func_08001C1C_6C9E2C;
    s_phases[23] = func_08001CC8_6C9ED8;
    s_phases[24] = func_08001E4C_6CA05C;
    s_phases[25] = func_08001F08_6CA118;
    s_phases[26] = func_080020B4_6CA2C4;
    s_phases[27] = func_0800218C_6CA39C;
    s_phases[28] = func_080022BC_6CA4CC;
    s_phases[29] = func_08002380_6CA590;
    s_phases[30] = func_08002410_6CA620;
    s_phases[31] = func_080025C8_6CA7D8;
    s_phases[32] = 0;
}

static void zero_bytes(void *pointer, unsigned int count)
{
    volatile unsigned char *out = pointer;
    while (count--)
        *out++ = 0;
}

static void copy_bytes(void *destination, const void *source,
                       unsigned int count)
{
    volatile unsigned char *out = destination;
    const unsigned char *in = source;
    while (count--)
        *out++ = *in++;
}

static unsigned int float_bits(float value)
{
    union { float f; unsigned int u; } bits;
    bits.f = value;
    return bits.u;
}

static float bits_float(unsigned int value)
{
    union { float f; unsigned int u; } bits;
    bits.u = value;
    return bits.f;
}

static int sane_float(unsigned int value, float low, float high)
{
    float number = bits_float(value);
    return (value & 0x7f800000u) != 0x7f800000u &&
           number >= low && number <= high;
}

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_DHARUMANYO_NATIVE_HOST_TEST
    return pointer != 0 && anchor_remote_model_pool_contains(pointer);
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           ((address >= 0x80001000u && address < 0x80800000u) ||
            anchor_remote_model_pool_contains(pointer));
#endif
}

static int callback_is(DharumanyoCallback a, DharumanyoCallback b)
{
    return ((unsigned long)a & ~CALLBACK_DISABLED) ==
           ((unsigned long)b & ~CALLBACK_DISABLED);
}

static void bind(DharumanyoActor *actor, void *task)
{
    actor->task = task;
    actor->object = DHARUMANYO_PTR(task, 0x18);
    actor->actor = U16(task, 0x5c);
    actor->generation = U8(task, 0x74);
    actor->held = 0;
}

static int live(const DharumanyoActor *actor)
{
    void *backlink;
    if (!pointer_valid(actor->task) || !pointer_valid(actor->object) ||
        D_800C7AB2 != DHARUMANYO_ROOM ||
        s_visit != anchor_boss_invite_world_visit() ||
        DHARUMANYO_PTR(actor->task, 0x18) != actor->object ||
        U16(actor->task, 0x5c) != actor->actor ||
        U8(actor->task, 0x74) != actor->generation ||
        (U32(actor->task, 0x68) & REMOVE_PENDING))
        return 0;
    backlink = DHARUMANYO_PTR(actor->task, 0x04);
    return pointer_valid(backlink) &&
           DHARUMANYO_PTR(backlink, 0) == actor->task;
}

static unsigned int phase_of(DharumanyoCallback callback)
{
    unsigned int index;
    for (index = 1; index < DHAR_PHASE_TERMINAL; ++index)
        if (callback_is(callback, s_phases[index]))
            return index;
    return 0;
}

static DharumanyoCallback root_ai(void)
{
    return s_root.held ? s_root.held_ai : DHARUMANYO_AI(s_root.task);
}

static DharumanyoCallback actor_post(const DharumanyoActor *actor)
{
    /* Checkpoints are adopted by the native pre callback inside the
     * scheduler. Its temporary hold may already have replaced the live post
     * slot, so identity checks must use the saved native callback. */
    return actor->held ? actor->held_post : DHARUMANYO_POST(actor->task);
}

int anchor_dharumanyo_native_is_root(const void *task)
{
    return task && task == s_root.task && live(&s_root) &&
           U16(task, 0x5c) == DHARUMANYO_ACTOR &&
           callback_is(actor_post(&s_root), func_08003810_6CBA20);
}

void *anchor_dharumanyo_native_root_task(void)
{
    return anchor_dharumanyo_native_is_root(s_root.task) ? s_root.task : 0;
}

unsigned int anchor_dharumanyo_native_visit(void)
{
    return s_visit;
}

static int carrier_ready(void)
{
    return live(&s_carrier) &&
           U16(s_carrier.task, 0x5c) == DHARUMANYO_ACTOR &&
           U16(s_carrier.task, 0x5e) == DHARUMANYO_ENTITY &&
           callback_is(actor_post(&s_carrier),
                       func_08003F84_6CC194) &&
           DHARUMANYO_PTR(s_root.task, 0xdc) == s_carrier.task &&
           DHARUMANYO_PTR(s_carrier.task, 0xdc) == s_root.task;
}

int anchor_dharumanyo_native_ready(void)
{
    return s_combat_active && anchor_dharumanyo_native_root_task() &&
           carrier_ready();
}

int anchor_dharumanyo_native_snapshot_ready(void)
{
    if (anchor_dharumanyo_native_ready())
        return 1;
    return s_terminal_snapshot_valid && s_terminal_started && s_visit &&
           D_800C7AB2 == DHARUMANYO_ROOM &&
           s_visit == anchor_boss_invite_world_visit();
}

void anchor_dharumanyo_native_finish_terminal(void)
{
    s_terminal_snapshot_valid = 0;
}

static void hold_noop(void *task, void *object)
{
    (void)task;
    (void)object;
}

static void hold(DharumanyoActor *actor, int hold_post)
{
    if (!live(actor) || actor->held)
        return;
    actor->held_ai = DHARUMANYO_AI(actor->task);
    actor->held_post = DHARUMANYO_POST(actor->task);
    actor->held = 1;
    DHARUMANYO_AI(actor->task) = hold_noop;
    if (hold_post)
        DHARUMANYO_POST(actor->task) = hold_noop;
}

static void unhold(DharumanyoActor *actor)
{
    if (actor->held && live(actor))
    {
        unsigned long ai = (unsigned long)DHARUMANYO_AI(actor->task);
        unsigned long post = (unsigned long)DHARUMANYO_POST(actor->task);
        unsigned long ours = (unsigned long)hold_noop & ~CALLBACK_DISABLED;
        if ((ai & ~CALLBACK_DISABLED) == ours)
            DHARUMANYO_AI(actor->task) =
                (DharumanyoCallback)(((unsigned long)actor->held_ai &
                                       ~CALLBACK_DISABLED) |
                                      (ai & CALLBACK_DISABLED));
        if ((post & ~CALLBACK_DISABLED) == ours)
            DHARUMANYO_POST(actor->task) =
                (DharumanyoCallback)(((unsigned long)actor->held_post &
                                       ~CALLBACK_DISABLED) |
                                      (post & CALLBACK_DISABLED));
    }
    actor->held = 0;
}

static void restore_target(void)
{
    if (s_target_root && anchor_dharumanyo_native_is_root(s_target_root) &&
        DHARUMANYO_PTR(s_target_root, 0x84) == s_target_object)
        DHARUMANYO_PTR(s_target_root, 0x84) = s_target_saved;
    s_target_root = 0;
    s_target_saved = 0;
}

static int projectile_live(const DharumanyoProjectile *projectile)
{
    DharumanyoCallback ai;
    if (!live(&projectile->actor) || !projectile->id ||
        U16(projectile->actor.task, 0x5c) != DHARUMANYO_ACTOR ||
        U16(projectile->actor.task, 0x5e) != DHARUMANYO_ENTITY ||
        DHARUMANYO_PTR(projectile->actor.task, 0xdc) != s_root.task ||
        !callback_is(actor_post(&projectile->actor),
                     func_80218F30_5D4400))
        return 0;
    ai = projectile->actor.held ? projectile->actor.held_ai
                                : DHARUMANYO_AI(projectile->actor.task);
    return callback_is(ai, func_0800284C_6CAA5C) ||
           callback_is(ai, func_08002A40_6CAC50);
}

static void release_all(void)
{
    unsigned int index;
    unhold(&s_root);
    unhold(&s_carrier);
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        unhold(&s_projectiles[index].actor);
    restore_target();
}

void anchor_dharumanyo_native_reset(void)
{
    unsigned int index;
    release_all();
    zero_bytes(&s_root, sizeof(s_root));
    zero_bytes(&s_carrier, sizeof(s_carrier));
    zero_bytes(&s_pending_projectile, sizeof(s_pending_projectile));
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        zero_bytes(&s_projectiles[index], sizeof(s_projectiles[index]));
    s_visit = 0;
    s_tick = 0;
    s_projectile_serial = 0;
    s_root_clip = 0;
    s_injected_id = 0;
    s_injected_born = 0;
    s_active = 0;
    s_owner = 0;
    s_paused = 0;
    s_adopted = 0;
    s_combat_active = 0;
    s_terminal_started = 0;
    s_terminal_snapshot_valid = 0;
    s_pending_valid = 0;
    s_pending_projectile_valid = 0;
    s_reconstructing = 0;
    s_injected_projectile = 0;
    s_root_post_seen = 0;
    s_root_pause_mirror = 0;
    s_target_valid = 0;
    anchor_dharumanyo_damage_bind(0, 0);
}

void anchor_dharumanyo_native_set_role(int active, int owner, int paused)
{
    if (!active)
    {
        release_all();
        s_pending_valid = 0;
    }
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
}

void anchor_dharumanyo_native_set_target(float x, float y, float z)
{
    s_target_object[2] = x;
    s_target_object[3] = y;
    s_target_object[4] = z;
    s_target_valid = 1;
}

void anchor_dharumanyo_native_clear_target(void)
{
    s_target_valid = 0;
}

RECOMP_HOOK_RETURN("func_08000104_6C8314")
void anchor_dharumanyo_native_bind_root(void)
{
    void *task = D_8016DAB4_16E6B4;
    if (!pointer_valid(task) ||
        !pointer_valid(DHARUMANYO_PTR(task, 0x18)) ||
        D_800C7AB2 != DHARUMANYO_ROOM ||
        U16(task, 0x5c) != DHARUMANYO_ACTOR ||
        !callback_is(DHARUMANYO_POST(task), func_08003810_6CBA20) ||
        (U32(task, 0x68) & REMOVE_PENDING))
        return;
    initialize_callbacks();
    anchor_dharumanyo_native_reset();
    s_visit = anchor_boss_invite_world_visit();
    bind(&s_root, task);
    s_root_clip = 0x12u;
}

RECOMP_HOOK_RETURN("func_08003E64_6CC074")
void anchor_dharumanyo_native_bind_carrier(void)
{
    void *task = D_8016DAB4_16E6B4;
    if (!anchor_dharumanyo_native_root_task() || !pointer_valid(task) ||
        !pointer_valid(DHARUMANYO_PTR(task, 0x18)) ||
        U16(task, 0x5c) != DHARUMANYO_ACTOR ||
        U16(task, 0x5e) != DHARUMANYO_ENTITY ||
        !callback_is(DHARUMANYO_POST(task), func_08003F84_6CC194) ||
        DHARUMANYO_PTR(task, 0xdc) != s_root.task ||
        DHARUMANYO_PTR(s_root.task, 0xdc) != task)
        return;
    bind(&s_carrier, task);
    anchor_dharumanyo_damage_bind(s_root.task, s_carrier.task);
}

RECOMP_HOOK("func_08000970_6C8B80")
void anchor_dharumanyo_native_start_combat(void *task)
{
    void *carrier;
    if (!anchor_dharumanyo_native_is_root(task))
        return;
    carrier = DHARUMANYO_PTR(task, 0xdc);
    if ((!live(&s_carrier) || s_carrier.task != carrier) &&
        pointer_valid(carrier) &&
        callback_is(DHARUMANYO_POST(carrier), func_08003F84_6CC194))
        bind(&s_carrier, carrier);
    if (!carrier_ready())
        return;
    s_combat_active = 1;
    anchor_dharumanyo_damage_bind(s_root.task, s_carrier.task);
}

RECOMP_HOOK("func_8021664C_5D1B1C")
void anchor_dharumanyo_native_animation(void *task, unsigned int clip)
{
    if (anchor_dharumanyo_native_is_root(task) && clip < ROOT_CLIP_COUNT)
        s_root_clip = clip;
}

RECOMP_HOOK("func_0800432C_6CC53C")
void anchor_dharumanyo_native_terminal(void *task)
{
    if (task == s_carrier.task && carrier_ready() &&
        U16(task, 0xd6) == 0 && U8(task, 0xd1) == 1)
    {
        s_terminal_started = 1;
        s_pending_valid = 0;
        release_all();
        /* The stock final-life callback keeps both tasks alive for a long
         * destruction chain, but retain a complete terminal checkpoint here
         * as well. It remains publishable even after native task removal. */
        if (anchor_dharumanyo_native_capture(&s_terminal_snapshot))
            s_terminal_snapshot_valid = 1;
    }
}

static unsigned int next_projectile_id(void)
{
    unsigned int index;
    if (s_projectile_serial < PROJECTILE_SERIAL_MAX)
        return ++s_projectile_serial;
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        if (projectile_live(&s_projectiles[index]))
            return 0;
    s_projectile_serial = 1;
    return s_projectile_serial;
}

static void register_projectile(const DharumanyoActor *actor)
{
    unsigned int index;
    unsigned int empty = ANCHOR_DHARUMANYO_MAX_PROJECTILES;
    unsigned int id;
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
    {
        if (s_projectiles[index].actor.task == actor->task &&
            s_projectiles[index].actor.object == actor->object &&
            s_projectiles[index].actor.generation == actor->generation)
        {
            s_projectiles[index].actor = *actor;
            if (s_injected_projectile)
            {
                s_projectiles[index].id = s_injected_id;
                s_projectiles[index].born = s_injected_born;
            }
            return;
        }
        if (!projectile_live(&s_projectiles[index]) &&
            empty == ANCHOR_DHARUMANYO_MAX_PROJECTILES)
            empty = index;
    }
    if (empty == ANCHOR_DHARUMANYO_MAX_PROJECTILES)
    {
        U32(actor->task, 0x68) |= REMOVE_PENDING;
        return;
    }
    if (s_injected_projectile)
    {
        s_projectiles[empty].actor = *actor;
        s_projectiles[empty].id = s_injected_id;
        s_projectiles[empty].born = s_injected_born;
    }
    else
    {
        id = next_projectile_id();
        if (!id)
        {
            U32(actor->task, 0x68) |= REMOVE_PENDING;
            return;
        }
        s_projectiles[empty].actor = *actor;
        s_projectiles[empty].id = id;
        s_projectiles[empty].born = s_tick;
    }
}

RECOMP_HOOK("func_08002620_6CA830")
void anchor_dharumanyo_native_projectile_begin(void *task)
{
    s_pending_projectile_valid = 0;
    if (!anchor_dharumanyo_native_root_task() || !pointer_valid(task) ||
        !pointer_valid(DHARUMANYO_PTR(task, 0x18)) ||
        U16(task, 0x5c) != DHARUMANYO_ACTOR ||
        DHARUMANYO_PTR(task, 0xdc) != s_root.task)
        return;
    bind(&s_pending_projectile, task);
    s_pending_projectile_valid = 1;
}

RECOMP_HOOK_RETURN("func_08002620_6CA830")
void anchor_dharumanyo_native_projectile_end(void)
{
    DharumanyoActor actor;
    if (!s_pending_projectile_valid)
        return;
    actor = s_pending_projectile;
    s_pending_projectile_valid = 0;
    zero_bytes(&s_pending_projectile, sizeof(s_pending_projectile));
    if (!live(&actor) || U16(actor.task, 0x5e) != DHARUMANYO_ENTITY ||
        DHARUMANYO_PTR(actor.task, 0xdc) != s_root.task ||
        (!callback_is(DHARUMANYO_AI(actor.task), func_0800284C_6CAA5C) &&
         !callback_is(DHARUMANYO_AI(actor.task), func_08002A40_6CAC50)) ||
        !callback_is(DHARUMANYO_POST(actor.task), func_80218F30_5D4400))
        return;
    register_projectile(&actor);
}

static void scope_target(void)
{
    if (s_active && s_owner && s_target_valid && !s_target_root &&
        anchor_dharumanyo_native_root_task())
    {
        s_target_root = s_root.task;
        s_target_saved = DHARUMANYO_PTR(s_root.task, 0x84);
        DHARUMANYO_PTR(s_root.task, 0x84) = s_target_object;
    }
}

RECOMP_HOOK("func_80034734_35334")
void anchor_dharumanyo_native_scheduler_begin(void)
{
    unsigned int index;
    scope_target();
    if (s_terminal_started && anchor_dharumanyo_native_root_task())
    {
        release_all();
        return;
    }
    if (!s_active || !anchor_dharumanyo_native_ready() ||
        !phase_of(root_ai()))
        return;
    if (!s_owner || s_paused || s_pending_valid)
        hold(&s_root, s_paused || s_pending_valid);
    if (s_paused || s_pending_valid)
    {
        hold(&s_carrier, 1);
        for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
            if (projectile_live(&s_projectiles[index]))
                hold(&s_projectiles[index].actor, 1);
    }
}

RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_dharumanyo_native_scheduler_end(void)
{
    release_all();
}

RECOMP_HOOK("func_08003810_6CBA20")
void anchor_dharumanyo_native_observe_post(void *task)
{
    if (anchor_dharumanyo_native_is_root(task) && !s_reconstructing)
    {
        s_root_post_seen = 1;
        /* This native post mirrors flag 0x16C into D4 bit 0. Remember its
         * ownership so clearing the flag lets the same post clear the bit. */
        s_root_pause_mirror =
            func_800240DC_24CDC(DHARUMANYO_PAUSE_FLAG) != 0;
    }
}

void anchor_dharumanyo_native_tick(void)
{
    unsigned int index;
    int advanced = s_root_post_seen;
    s_root_post_seen = 0;
    if (advanced && s_active && (!s_paused || s_terminal_started) &&
        anchor_dharumanyo_native_ready() &&
        (s_owner || s_adopted))
    {
        if (s_tick < TICK_MAX)
            ++s_tick;
    }
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        if (!projectile_live(&s_projectiles[index]))
            zero_bytes(&s_projectiles[index], sizeof(s_projectiles[index]));
}

static int snapshot_valid(const AnchorDharumanyoNativeSnapshot *snapshot)
{
    unsigned int index;
    unsigned int previous;
    const unsigned int *root;
    const unsigned int *carrier;
    if (!snapshot)
        return 0;
    root = snapshot->root;
    carrier = snapshot->carrier;
    if (!root[DHAR_PHASE] || root[DHAR_PHASE] > DHAR_PHASE_TERMINAL ||
        root[DHAR_TIMER] > 65535u || root[DHAR_CLIP] >= ROOT_CLIP_COUNT ||
        !sane_float(root[DHAR_FRAME], 0.0f, 65536.0f) ||
        (root[DHAR_ANIM_STATE] & 0xff000000u) ||
        (root[DHAR_FLAGS] & ~ROOT_FLAGS_MASK) ||
        (root[DHAR_FLAGS2] & ~ROOT_FLAGS2_MASK) ||
        (root[DHAR_PHASE_FLAGS] & ~PHASE_FLAGS_MASK) ||
        (root[DHAR_PRIVATE_D0] & 0xffff0000u) ||
        (root[DHAR_PRIVATE_D0] & 0xffu) > ROOT_ATTACK_SELECTOR_MAX ||
        ((root[DHAR_PRIVATE_D0] >> 8) & 0xffu) >
            ROOT_PROJECTILE_BURST_MAX ||
        root[DHAR_PRIVATE_D4] > ROOT_FADE_STEP_MAX ||
        !sane_float(root[DHAR_PRIVATE_D8], -100.0f, 100.0f) ||
        !sane_float(root[DHAR_PRIVATE_E0], -4096.0f, 4096.0f) ||
        !sane_float(root[DHAR_PRIVATE_E4], -4096.0f, 4096.0f) ||
        root[DHAR_PRIVATE_E8] > ROOT_OPACITY_MAX ||
        !sane_float(root[DHAR_PRIVATE_EC], -64.0f, 4096.0f) ||
        (root[DHAR_COLLIDER_XY] & 0xffffu) != ROOT_COLLIDER_0 ||
        (root[DHAR_COLLIDER_XY] >> 16) != ROOT_COLLIDER_1 ||
        root[DHAR_COLLIDER_Z] != ROOT_COLLIDER_2 ||
        snapshot->projectile_count > ANCHOR_DHARUMANYO_MAX_PROJECTILES ||
        snapshot->projectile_serial > PROJECTILE_SERIAL_MAX ||
        snapshot->tick > TICK_MAX ||
        carrier[DHAR_CARRIER_LIVES] > 12u ||
        carrier[DHAR_CARRIER_HURT] > 90u ||
        carrier[DHAR_CARRIER_YAW] > 1023u)
        return 0;
    for (index = DHAR_X; index <= DHAR_Z; ++index)
        if (!sane_float(root[index], -32768.0f, 32768.0f))
            return 0;
    for (index = DHAR_RX; index <= DHAR_RZ; ++index)
        if (root[index] > 1023u)
            return 0;
    for (index = DHAR_SCALE_X; index <= DHAR_SCALE_Z; ++index)
        if (!sane_float(root[index], 0.0f, 32.0f))
            return 0;
    for (index = DHAR_VX; index <= DHAR_VZ; ++index)
        if (!sane_float(root[index], -100.0f, 100.0f))
            return 0;
    for (index = DHAR_CARRIER_X; index <= DHAR_CARRIER_Z; ++index)
        if (!sane_float(carrier[index], -32768.0f, 32768.0f))
            return 0;
    for (index = DHAR_CARRIER_SCALE_X;
         index <= DHAR_CARRIER_SCALE_Z; ++index)
        if (!sane_float(carrier[index], 0.0f, 32.0f))
            return 0;
    for (index = 0; index < snapshot->projectile_count; ++index)
    {
        const unsigned int *projectile = snapshot->projectile[index];
        unsigned int other;
        if (!projectile[DHAR_PROJECTILE_ID] ||
            projectile[DHAR_PROJECTILE_ID] > snapshot->projectile_serial ||
            projectile[DHAR_PROJECTILE_VARIANT] > 1u ||
            projectile[DHAR_PROJECTILE_YAW] > 1023u ||
            projectile[DHAR_PROJECTILE_BORN] > snapshot->tick ||
            snapshot->tick - projectile[DHAR_PROJECTILE_BORN] >
                PROJECTILE_MAX_AGE)
            return 0;
        for (other = DHAR_PROJECTILE_X; other <= DHAR_PROJECTILE_Z; ++other)
            if (!sane_float(projectile[other], -32768.0f, 32768.0f))
                return 0;
        for (other = DHAR_PROJECTILE_DEST_X;
             other <= DHAR_PROJECTILE_DEST_Z; ++other)
            if (!sane_float(projectile[other], -32768.0f, 32768.0f))
                return 0;
        if (!sane_float(projectile[DHAR_PROJECTILE_VY],
                        -100.0f, 100.0f) ||
            projectile[DHAR_PROJECTILE_TIMER] > 65535u)
            return 0;
        previous = projectile[DHAR_PROJECTILE_ID];
        for (other = 0; other < index; ++other)
            if (snapshot->projectile[other][DHAR_PROJECTILE_ID] == previous)
                return 0;
    }
    return 1;
}

static void capture_root(unsigned int *root)
{
    unsigned int index;
    void *task = s_root.task;
    void *object = s_root.object;
    root[DHAR_PHASE] = s_terminal_started || U8(s_carrier.task, 0xd1) == 0
                           ? DHAR_PHASE_TERMINAL : phase_of(root_ai());
    root[DHAR_TIMER] = U16(task, 0x8a);
    root[DHAR_CLIP] = s_root_clip;
    root[DHAR_FRAME] = float_bits(F32(object, 0x28));
    root[DHAR_ANIM_STATE] = U8(object, 0x7c) |
                            ((unsigned int)U16(object, 0x7e) << 8);
    root[DHAR_FLAGS] = U32(task, 0x60) & ROOT_FLAGS_MASK;
    root[DHAR_FLAGS2] = U32(task, 0x64) & ROOT_FLAGS2_MASK;
    root[DHAR_PHASE_FLAGS] = U32(D_8015CC30_15D830, 0x184) &
                             PHASE_FLAGS_MASK;
    root[DHAR_PRIVATE_D0] = U8(task, 0xd0) |
                            ((unsigned int)U8(task, 0xd2) << 8);
    root[DHAR_PRIVATE_D4] = U16(task, 0xd4);
    root[DHAR_PRIVATE_D8] = U32(task, 0xd8);
    root[DHAR_PRIVATE_E0] = U32(task, 0xe0);
    root[DHAR_PRIVATE_E4] = U32(task, 0xe4);
    root[DHAR_PRIVATE_E8] = U16(task, 0xe8);
    root[DHAR_PRIVATE_EC] = U32(task, 0xec);
    for (index = 0; index < 3; ++index)
    {
        root[DHAR_X + index] = float_bits(F32(object, 8 + index * 4));
        root[DHAR_RX + index] = U16(object, 0x14 + index * 2);
        root[DHAR_SCALE_X + index] =
            float_bits(F32(object, 0x1c + index * 4));
        root[DHAR_VX + index] = float_bits(F32(task, 0x78 + index * 4));
    }
    root[DHAR_COLLIDER_XY] = U16(task, 0x3c) |
                             ((unsigned int)U16(task, 0x3e) << 16);
    root[DHAR_COLLIDER_Z] = U16(task, 0x40);
}

static void capture_carrier(unsigned int *carrier)
{
    unsigned int index;
    carrier[DHAR_CARRIER_LIVES] = U8(s_carrier.task, 0xd1);
    carrier[DHAR_CARRIER_HURT] = U16(s_carrier.task, 0xd6);
    carrier[DHAR_CARRIER_YAW] = U16(s_carrier.task, 0xd8);
    for (index = 0; index < 3; ++index)
    {
        carrier[DHAR_CARRIER_X + index] =
            float_bits(F32(s_carrier.object, 8 + index * 4));
        carrier[DHAR_CARRIER_SCALE_X + index] =
            float_bits(F32(s_carrier.object, 0x1c + index * 4));
    }
}

static void capture_projectile(const DharumanyoProjectile *source,
                               unsigned int *destination)
{
    void *task = source->actor.task;
    void *object = source->actor.object;
    unsigned int index;
    DharumanyoCallback ai = source->actor.held ? source->actor.held_ai
                                               : DHARUMANYO_AI(task);
    destination[DHAR_PROJECTILE_ID] = source->id;
    destination[DHAR_PROJECTILE_BORN] = source->born;
    destination[DHAR_PROJECTILE_VARIANT] =
        callback_is(ai, func_08002A40_6CAC50) ? 1u : 0u;
    for (index = 0; index < 3; ++index)
        destination[DHAR_PROJECTILE_X + index] =
            float_bits(F32(object, 8 + index * 4));
    destination[DHAR_PROJECTILE_YAW] = U16(object, 0x16);
    destination[DHAR_PROJECTILE_DEST_X] = U32(task, 0xe0);
    destination[DHAR_PROJECTILE_DEST_Z] = U32(task, 0xe4);
    destination[DHAR_PROJECTILE_VY] = U32(task, 0xd8);
    destination[DHAR_PROJECTILE_TIMER] = U16(task, 0x8a);
}

int anchor_dharumanyo_native_capture(AnchorDharumanyoNativeSnapshot *snapshot)
{
    unsigned int index;
    if (!snapshot)
        return 0;
    if (s_terminal_snapshot_valid)
    {
        copy_bytes(snapshot, &s_terminal_snapshot, sizeof(*snapshot));
        return snapshot_valid(snapshot);
    }
    if (s_pending_valid || !anchor_dharumanyo_native_ready())
        return 0;
    zero_bytes(snapshot, sizeof(*snapshot));
    capture_root(snapshot->root);
    capture_carrier(snapshot->carrier);
    snapshot->tick = s_tick;
    snapshot->projectile_serial = s_projectile_serial;
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        if (projectile_live(&s_projectiles[index]))
            capture_projectile(&s_projectiles[index],
                snapshot->projectile[snapshot->projectile_count++]);
    return snapshot_valid(snapshot);
}

static void set_callback(DharumanyoCallback volatile *slot,
                         DharumanyoCallback callback)
{
    unsigned long current = (unsigned long)*slot;
    *slot = (DharumanyoCallback)(((unsigned long)callback &
                                  ~CALLBACK_DISABLED) |
                                 (current & CALLBACK_DISABLED));
}

static void apply_root(const unsigned int *root)
{
    unsigned int index;
    float frame;
    float count;
    void *task = s_root.task;
    void *object = s_root.object;
    set_callback(&DHARUMANYO_AI(task), s_phases[root[DHAR_PHASE]]);
    U16(task, 0x8a) = (unsigned short)root[DHAR_TIMER];
    if (s_root_clip != root[DHAR_CLIP])
    {
        func_8021664C_5D1B1C(task, root[DHAR_CLIP], 0.05f, 0);
        s_root_clip = root[DHAR_CLIP];
    }
    frame = bits_float(root[DHAR_FRAME]);
    count = func_8001B5AC_1C1AC(object);
    if (!(count > 0.0f) || count > 65536.0f)
        frame = 0.0f;
    else if (frame >= count)
        frame = count > 1.0f ? count - 1.0f : 0.0f;
    F32(object, 0x28) = frame;
    U8(object, 0x7c) = (unsigned char)root[DHAR_ANIM_STATE];
    U16(object, 0x7e) = (unsigned short)(root[DHAR_ANIM_STATE] >> 8);
    U32(task, 0x60) = (U32(task, 0x60) & ~ROOT_FLAGS_MASK) |
                      root[DHAR_FLAGS];
    U32(task, 0x64) = (U32(task, 0x64) & ~ROOT_FLAGS2_MASK) |
                      root[DHAR_FLAGS2];
    U32(D_8015CC30_15D830, 0x184) =
        (U32(D_8015CC30_15D830, 0x184) & ~PHASE_FLAGS_MASK) |
        root[DHAR_PHASE_FLAGS];
    U8(task, 0xd0) = (unsigned char)root[DHAR_PRIVATE_D0];
    U8(task, 0xd2) = (unsigned char)(root[DHAR_PRIVATE_D0] >> 8);
    U16(task, 0xd4) = (unsigned short)root[DHAR_PRIVATE_D4];
    U32(task, 0xd8) = root[DHAR_PRIVATE_D8];
    U32(task, 0xe0) = root[DHAR_PRIVATE_E0];
    U32(task, 0xe4) = root[DHAR_PRIVATE_E4];
    U16(task, 0xe8) = (unsigned short)root[DHAR_PRIVATE_E8];
    U32(task, 0xec) = root[DHAR_PRIVATE_EC];
    for (index = 0; index < 3; ++index)
    {
        F32(object, 8 + index * 4) = bits_float(root[DHAR_X + index]);
        U16(object, 0x14 + index * 2) =
            (unsigned short)root[DHAR_RX + index];
        F32(object, 0x1c + index * 4) =
            bits_float(root[DHAR_SCALE_X + index]);
        F32(task, 0x78 + index * 4) =
            bits_float(root[DHAR_VX + index]);
    }
    U16(task, 0x3c) = (unsigned short)root[DHAR_COLLIDER_XY];
    U16(task, 0x3e) = (unsigned short)(root[DHAR_COLLIDER_XY] >> 16);
    U16(task, 0x40) = (unsigned short)root[DHAR_COLLIDER_Z];
}

static void apply_carrier(const unsigned int *carrier)
{
    unsigned int index;
    void *task = s_carrier.task;
    U8(task, 0xd1) = (unsigned char)carrier[DHAR_CARRIER_LIVES];
    U16(task, 0xd6) = (unsigned short)carrier[DHAR_CARRIER_HURT];
    U16(task, 0xd8) = (unsigned short)carrier[DHAR_CARRIER_YAW];
    U8(task, 0x8d) = 10;
    U32(task, 0x68) = (U32(task, 0x68) & ~RECOVERY) |
                      (carrier[DHAR_CARRIER_HURT] ? RECOVERY : 0u);
    set_callback(&DHARUMANYO_AI(task),
                 carrier[DHAR_CARRIER_HURT]
                     ? func_0800432C_6CC53C : func_08003F78_6CC188);
    for (index = 0; index < 3; ++index)
    {
        F32(s_carrier.object, 8 + index * 4) =
            bits_float(carrier[DHAR_CARRIER_X + index]);
        F32(s_carrier.object, 0x1c + index * 4) =
            bits_float(carrier[DHAR_CARRIER_SCALE_X + index]);
    }
}

static DharumanyoProjectile *find_projectile(unsigned int id)
{
    unsigned int index;
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        if (projectile_live(&s_projectiles[index]) &&
            s_projectiles[index].id == id)
            return &s_projectiles[index];
    return 0;
}

static void correct_projectile(DharumanyoProjectile *projectile,
                               const unsigned int *state)
{
    unsigned int index;
    void *task = projectile->actor.task;
    void *object = projectile->actor.object;
    for (index = 0; index < 3; ++index)
        F32(object, 8 + index * 4) =
            bits_float(state[DHAR_PROJECTILE_X + index]);
    U16(object, 0x16) = (unsigned short)state[DHAR_PROJECTILE_YAW];
    U8(task, 0xd3) = (unsigned char)state[DHAR_PROJECTILE_VARIANT];
    U32(task, 0xd4) = state[DHAR_PROJECTILE_X];
    U32(task, 0xd8) = state[DHAR_PROJECTILE_VY];
    U32(task, 0xe0) = state[DHAR_PROJECTILE_DEST_X];
    U32(task, 0xe4) = state[DHAR_PROJECTILE_DEST_Z];
    U32(task, 0xe8) = state[DHAR_PROJECTILE_Z];
    U32(task, 0xec) = state[DHAR_PROJECTILE_Y];
    U16(task, 0x8a) = (unsigned short)state[DHAR_PROJECTILE_TIMER];
    set_callback(&DHARUMANYO_AI(task),
                 state[DHAR_PROJECTILE_VARIANT]
                     ? func_08002A40_6CAC50 : func_0800284C_6CAA5C);
    projectile->born = state[DHAR_PROJECTILE_BORN];
}

static void call_as(DharumanyoActor *actor, DharumanyoCallback callback)
{
    void *previous;
    if (!live(actor))
        return;
    previous = D_8016DAB4_16E6B4;
    D_8016DAB4_16E6B4 = actor->task;
    callback(actor->task, actor->object);
    D_8016DAB4_16E6B4 = previous;
}

static DharumanyoProjectile *spawn_projectile(const unsigned int *state)
{
    DharumanyoActor actor;
    DharumanyoProjectile *projectile;
    void *task;
    void *resource;
    void *target;
    float proxy[8];
    unsigned int index;

    resource = func_800141C4_14DC4(DHARUMANYO_FILE);
    if (!resource || (unsigned long)resource == 0xfffffffful)
        return 0;
    task = func_8021DDE8_5D92B8(s_root.task, func_08002620_6CA830,
                                10, -19.0f, 33.5f, 50.0f, 0);
    if (!pointer_valid(task) ||
        !pointer_valid(DHARUMANYO_PTR(task, 0x18)))
        return 0;
    U16(task, 0x28) = DHARUMANYO_FILE;
    DHARUMANYO_PTR(task, 0x2c) = resource;
    DHARUMANYO_PTR(task, 0xdc) = s_root.task;
    U8(task, 0xd3) = (unsigned char)state[DHAR_PROJECTILE_VARIANT];
    for (index = 0; index < 8; ++index)
        proxy[index] = 0.0f;
    proxy[2] = bits_float(state[DHAR_PROJECTILE_DEST_X]);
    proxy[3] = bits_float(state[DHAR_PROJECTILE_Y]);
    proxy[4] = bits_float(state[DHAR_PROJECTILE_DEST_Z]);
    target = DHARUMANYO_PTR(s_root.task, 0x84);
    DHARUMANYO_PTR(s_root.task, 0x84) = proxy;
    bind(&actor, task);
    s_injected_id = state[DHAR_PROJECTILE_ID];
    s_injected_born = state[DHAR_PROJECTILE_BORN];
    s_injected_projectile = 1;
    call_as(&actor, func_08002620_6CA830);
    s_injected_projectile = 0;
    DHARUMANYO_PTR(s_root.task, 0x84) = target;
    projectile = find_projectile(state[DHAR_PROJECTILE_ID]);
    if (!projectile && live(&actor) &&
        U16(task, 0x5e) == DHARUMANYO_ENTITY &&
        callback_is(DHARUMANYO_POST(task), func_80218F30_5D4400))
    {
        s_injected_projectile = 1;
        register_projectile(&actor);
        s_injected_projectile = 0;
        projectile = find_projectile(state[DHAR_PROJECTILE_ID]);
    }
    if (projectile)
        correct_projectile(projectile, state);
    else if (live(&actor))
        U32(actor.task, 0x68) |= REMOVE_PENDING;
    return projectile;
}

static int checkpoint_contains(const AnchorDharumanyoNativeSnapshot *snapshot,
                               unsigned int id)
{
    unsigned int index;
    for (index = 0; index < snapshot->projectile_count; ++index)
        if (snapshot->projectile[index][DHAR_PROJECTILE_ID] == id)
            return 1;
    return 0;
}

static int apply_now(const AnchorDharumanyoNativeSnapshot *snapshot)
{
    unsigned int index;
    DharumanyoProjectile *projectile;
    if (!snapshot_valid(snapshot))
        return 0;
    if (s_terminal_started)
    {
        release_all();
        return 1;
    }
    if (!anchor_dharumanyo_native_ready())
        return 0;
    release_all();
    if (snapshot->root[DHAR_PHASE] == DHAR_PHASE_TERMINAL)
    {
        if (!boss_sync_queue_darumanyo_shared_terminal())
            return 0;
        s_terminal_started = 1;
        s_adopted = 1;
        s_tick = snapshot->tick;
        s_projectile_serial = snapshot->projectile_serial;
        copy_bytes(&s_terminal_snapshot, snapshot,
                   sizeof(s_terminal_snapshot));
        s_terminal_snapshot_valid = 1;
        release_all();
        return 1;
    }
    s_reconstructing = 1;
    apply_root(snapshot->root);
    apply_carrier(snapshot->carrier);
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
        if (projectile_live(&s_projectiles[index]) &&
            !checkpoint_contains(snapshot, s_projectiles[index].id))
        {
            unhold(&s_projectiles[index].actor);
            U32(s_projectiles[index].actor.task, 0x68) |= REMOVE_PENDING;
        }
    for (index = 0; index < snapshot->projectile_count; ++index)
    {
        projectile = find_projectile(
            snapshot->projectile[index][DHAR_PROJECTILE_ID]);
        if (!projectile)
            projectile = spawn_projectile(snapshot->projectile[index]);
        if (!projectile)
        {
            s_reconstructing = 0;
            return 0;
        }
        correct_projectile(projectile, snapshot->projectile[index]);
    }
    s_tick = snapshot->tick;
    s_projectile_serial = snapshot->projectile_serial;
    s_adopted = 1;
    s_reconstructing = 0;
    anchor_dharumanyo_damage_bind(s_root.task, s_carrier.task);
    return 1;
}

int anchor_dharumanyo_native_apply(
    const AnchorDharumanyoNativeSnapshot *snapshot)
{
    if (!snapshot_valid(snapshot))
        return 0;
    if (s_terminal_started)
        return 1;
    if (!anchor_dharumanyo_native_ready())
        return 0;
    copy_bytes(&s_pending_snapshot, snapshot, sizeof(s_pending_snapshot));
    s_pending_valid = 1;
    return 1;
}

static int native_pause_gate(void)
{
    /* D4 also serves unrelated native pauses. Only disregard a stale bit
     * that this boss's own post wrote; never clear global pause memory here. */
    return ((U32(D_8015CC30_15D830, 0xd4) & 1u) &&
            !s_root_pause_mirror) ||
           func_800240DC_24CDC(DHARUMANYO_PAUSE_FLAG) != 0;
}

int anchor_dharumanyo_native_world_paused(void)
{
    unsigned long ai;
    unsigned long post;
    if (!anchor_dharumanyo_native_root_task() || !s_combat_active)
        return 0;
    ai = (unsigned long)(s_root.held ? s_root.held_ai
                                    : DHARUMANYO_AI(s_root.task));
    post = (unsigned long)actor_post(&s_root);
    return (ai & CALLBACK_DISABLED) != 0 ||
           (post & CALLBACK_DISABLED) != 0 ||
           native_pause_gate();
}

RECOMP_HOOK("func_8021925C_5D472C")
void anchor_dharumanyo_native_adopt_before_pre(void *task)
{
    if (!s_pending_valid || !anchor_dharumanyo_native_is_root(task) ||
        D_8016DAB4_16E6B4 != task ||
        native_pause_gate())
        return;
    if (apply_now(&s_pending_snapshot))
        s_pending_valid = 0;
    anchor_dharumanyo_native_scheduler_begin();
}
