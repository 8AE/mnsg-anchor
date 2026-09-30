#include "bosses/control_machine/anchor_control_machine_projectiles.h"
#include "bosses/control_machine/anchor_control_machine_hud.h"

#ifndef ANCHOR_CONTROL_MACHINE_PROJECTILES_HOST_TEST
#include "platform/modding.h"
#else
#define RECOMP_HOOK_RETURN(name)
#endif

typedef void (*ControlMachineProjectileCallback)(void *, void *);
#define CM_P_U8(p,o) (*(volatile unsigned char *)((unsigned char *)(p)+(o)))
#define CM_P_U16(p,o) (*(volatile unsigned short *)((unsigned char *)(p)+(o)))
#define CM_P_U32(p,o) (*(volatile unsigned int *)((unsigned char *)(p)+(o)))
#define CM_P_F32(p,o) (*(volatile float *)((unsigned char *)(p)+(o)))
#ifndef CM_PROJECTILE_PTR
#define CM_PROJECTILE_PTR(p,o) (*(void *volatile *)((unsigned char *)(p)+(o)))
#endif
#ifndef CM_PROJECTILE_AI
#define CM_PROJECTILE_AI(p) (*(ControlMachineProjectileCallback volatile *)((unsigned char *)(p)+0x0c))
#endif
#ifndef CM_PROJECTILE_POST
#define CM_PROJECTILE_POST(p) (*(ControlMachineProjectileCallback volatile *)((unsigned char *)(p)+0x10))
#endif

#define CM_P_ROOM 0x155u
#define CM_P_RESOURCE 0x2eu
#define CM_P_ENTITY 0x7eu
#define CM_P_REMOVE_PENDING 2u
#define CM_P_DISABLED 0x00800000ul
#define CM_P_SERIAL_MAX 0x7fffffffu

typedef struct ControlMachineProjectile {
    void *task;
    void *object;
    unsigned int id;
    unsigned int born;
    unsigned char generation;
    unsigned char replica;
    unsigned char held;
    ControlMachineProjectileCallback saved_ai;
} ControlMachineProjectile;

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void *func_800141C4_14DC4(unsigned int file);
extern void *func_802171A8_5D2678(void *parent,
                                   ControlMachineProjectileCallback initializer,
                                   unsigned char group);
extern void func_0800413C_70523C(void *task, void *object);
extern void func_08004214_705314(void *task, void *object);
extern void func_80218F30_5D4400(void *task, void *object);

static ControlMachineProjectile s_projectiles[ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES];
static void *s_child;
static unsigned int s_visit, s_serial, s_tick;
static unsigned char s_child_generation;
static int s_active, s_owner, s_paused, s_reconstructing, s_incomplete;

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_CONTROL_MACHINE_PROJECTILES_HOST_TEST
    extern int anchor_control_machine_projectile_test_pointer_valid(
        const void *pointer);
    return anchor_control_machine_projectile_test_pointer_valid(pointer);
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0u && address >= 0x80001000u &&
           address < 0x80800000u;
#endif
}

/* Resource registry values are opaque engine handles. Some resident files
 * carry a cache tag, so the actor-task RDRAM range check does not apply. */
static int resource_available(const void *resource)
{
    return resource && (unsigned long)resource != 0xfffffffful;
}

static int callback_is(ControlMachineProjectileCallback a,
                       ControlMachineProjectileCallback b)
{
    return ((unsigned long)a & ~CM_P_DISABLED) ==
           ((unsigned long)b & ~CM_P_DISABLED);
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

static void clear_slot(ControlMachineProjectile *p)
{
    unsigned char *bytes = (unsigned char *)p;
    unsigned int i;
    for (i = 0; i < sizeof(*p); ++i) bytes[i] = 0;
}

static int bound_current(void)
{
    return D_800C7AB2 == CM_P_ROOM && s_visit &&
           pointer_valid(s_child) &&
           s_child == anchor_control_machine_bound_task() &&
           CM_P_U8(s_child, 0x74) == s_child_generation &&
           !(CM_P_U32(s_child, 0x68) & CM_P_REMOVE_PENDING);
}

static int slot_live(const ControlMachineProjectile *p)
{
    void *backlink;
    if (!p->id || !pointer_valid(p->task) ||
        !pointer_valid(p->object) ||
        CM_PROJECTILE_PTR(p->task, 0x18) != p->object ||
        CM_P_U8(p->task, 0x74) != p->generation ||
        (CM_P_U32(p->task, 0x68) & CM_P_REMOVE_PENDING) ||
        CM_P_U16(p->task, 0x5e) != CM_P_ENTITY ||
        CM_P_U16(p->task, 0x28) != CM_P_RESOURCE ||
        !callback_is(CM_PROJECTILE_POST(p->task),
                     func_80218F30_5D4400)) return 0;
    backlink = CM_PROJECTILE_PTR(p->task, 0x04);
    return pointer_valid(backlink) &&
           CM_PROJECTILE_PTR(backlink, 0) == p->task;
}

static int live(const ControlMachineProjectile *p)
{
    return bound_current() && slot_live(p);
}

static void noop(void *task, void *object)
{
    (void)task;
    (void)object;
}

static void unhold(ControlMachineProjectile *p)
{
    if (p->held && D_800C7AB2 == CM_P_ROOM && slot_live(p) &&
        callback_is(CM_PROJECTILE_AI(p->task), noop)) {
        unsigned long current = (unsigned long)CM_PROJECTILE_AI(p->task);
        CM_PROJECTILE_AI(p->task) = (ControlMachineProjectileCallback)(
            ((unsigned long)p->saved_ai & ~CM_P_DISABLED) |
            (current & CM_P_DISABLED));
    }
    p->held = 0;
    p->saved_ai = 0;
}

static void hold(ControlMachineProjectile *p)
{
    if (!live(p) || p->held) return;
    p->saved_ai = CM_PROJECTILE_AI(p->task);
    p->held = 1;
    CM_PROJECTILE_AI(p->task) = (ControlMachineProjectileCallback)(
        ((unsigned long)noop & ~CM_P_DISABLED) |
        ((unsigned long)p->saved_ai & CM_P_DISABLED));
}

static void retire(ControlMachineProjectile *p)
{
    if (D_800C7AB2 == CM_P_ROOM && slot_live(p)) {
        unhold(p);
        CM_P_U32(p->task, 0x68) |= CM_P_REMOVE_PENDING;
    }
    clear_slot(p);
}

void anchor_control_machine_projectiles_reset(void)
{
    unsigned int i;
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i) {
        ControlMachineProjectile *p = &s_projectiles[i];
        if (p->replica) retire(p);
        else { unhold(p); clear_slot(p); }
    }
    s_child = 0;
    s_visit = s_serial = s_tick = 0;
    s_child_generation = 0;
    s_active = s_owner = s_paused = s_reconstructing = s_incomplete = 0;
}

void anchor_control_machine_projectiles_set_context(void *child,
                                                     unsigned int binding_visit,
                                                     int active, int owner,
                                                     int paused)
{
    unsigned int i;
    if (child != s_child || binding_visit != s_visit ||
        (child && CM_P_U8(child, 0x74) != s_child_generation)) {
        anchor_control_machine_projectiles_reset();
        s_child = child;
        s_visit = binding_visit;
        s_child_generation = child ? CM_P_U8(child, 0x74) : 0;
    }
    if (!bound_current()) {
        s_active = s_owner = s_paused = 0;
        return;
    }
    /* IDs from a former local owner must never be mistaken for the new
     * authority's shots, even when both serial sequences begin at one. */
    if (s_active && s_owner && active && !owner)
        for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i)
            retire(&s_projectiles[i]);
    if (s_active && !active)
        for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i)
            if (s_projectiles[i].replica) retire(&s_projectiles[i]);
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i) {
        ControlMachineProjectile *p = &s_projectiles[i];
        if (!live(p)) { clear_slot(p); continue; }
        if (s_active && (!s_owner || s_paused)) hold(p);
        else unhold(p);
    }
}

static ControlMachineProjectile *find_id(unsigned int id)
{
    unsigned int i;
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i)
        if (s_projectiles[i].id == id && live(&s_projectiles[i]))
            return &s_projectiles[i];
    return 0;
}

static ControlMachineProjectile *register_projectile(void *task,
                                                     unsigned int id,
                                                     unsigned int born,
                                                     int replica)
{
    unsigned int i, empty = ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES;
    ControlMachineProjectile *p;
    if (!bound_current() || !pointer_valid(task) ||
        !pointer_valid(CM_PROJECTILE_PTR(task, 0x18)) ||
        CM_P_U16(task, 0x5e) != CM_P_ENTITY ||
        CM_P_U16(task, 0x28) != CM_P_RESOURCE ||
        !callback_is(CM_PROJECTILE_POST(task),
                     func_80218F30_5D4400)) return 0;
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i) {
        p = &s_projectiles[i];
        if (live(p) && p->task == task) return p;
        if (!live(p) && empty == ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES)
            empty = i;
    }
    if (empty == ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES) {
        if (s_active) CM_P_U32(task, 0x68) |= CM_P_REMOVE_PENDING;
        else s_incomplete = 1;
        return 0;
    }
    if (!id && s_serial == CM_P_SERIAL_MAX) {
        /* Never reuse an ID while its old projectile may still be live. */
        for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i)
            if (live(&s_projectiles[i])) break;
        if (i != ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES) {
            if (s_active) CM_P_U32(task, 0x68) |= CM_P_REMOVE_PENDING;
            else s_incomplete = 1;
            return 0;
        }
        s_serial = 0;
    }
    p = &s_projectiles[empty];
    clear_slot(p);
    p->task = task;
    p->object = CM_PROJECTILE_PTR(task, 0x18);
    p->generation = CM_P_U8(task, 0x74);
    p->id = id ? id : ++s_serial;
    p->born = id ? born : s_tick;
    p->replica = !!replica;
    return p;
}

RECOMP_HOOK_RETURN("func_0800413C_70523C")
void anchor_control_machine_projectiles_birth(void)
{
    void *task = D_8016DAB4_16E6B4;
    ControlMachineProjectile *p;
    /* File_46 has one xref to this initializer: the health child's
     * func_080029CC attack producer. The native constructor has no stable
     * parent pointer in the new task; binding to its exact callback and the
     * current room/child incarnation is the available provenance check. */
    if (s_reconstructing || !bound_current() || !pointer_valid(task)) return;
    if (s_active && !s_owner) {
        CM_P_U32(task, 0x68) |= CM_P_REMOVE_PENDING;
        return;
    }
    p = register_projectile(task, 0, 0, 0);
    if (!p) {
        if (s_active) CM_P_U32(task, 0x68) |= CM_P_REMOVE_PENDING;
        else s_incomplete = 1;
    }
    if (p && s_active && s_paused) hold(p);
}

static void capture_one(const ControlMachineProjectile *p, unsigned int *row)
{
    unsigned int i;
    row[CM_PROJECTILE_ID] = p->id;
    row[CM_PROJECTILE_BORN] = p->born;
    row[CM_PROJECTILE_TIMER] = CM_P_U16(p->task, 0x8a);
    for (i = 0; i < 3u; ++i) {
        row[CM_PROJECTILE_X + i] = float_bits(CM_P_F32(p->object, 8u + i * 4u));
        row[CM_PROJECTILE_VX + i] = float_bits(CM_P_F32(p->task, 0x78u + i * 4u));
    }
}

int anchor_control_machine_projectiles_capture(AnchorControlMachineSnapshot *out)
{
    unsigned int i, count = 0;
    /* Election discovery may take several frames while native AI is held.
     * The sync layer alone decides which captures are published. */
    if (!out || !bound_current() || s_incomplete) return 0;
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i) {
        ControlMachineProjectile *p = &s_projectiles[i];
        if (!live(p)) { clear_slot(p); continue; }
        capture_one(p, out->projectile[count++]);
    }
    out->projectile_count = count;
    if (s_tick < CM_P_SERIAL_MAX) ++s_tick;
    return 1;
}

static int in_snapshot(const AnchorControlMachineSnapshot *in, unsigned int id)
{
    unsigned int i;
    for (i = 0; i < in->projectile_count; ++i)
        if (in->projectile[i][CM_PROJECTILE_ID] == id) return 1;
    return 0;
}

static void correct(ControlMachineProjectile *p, const unsigned int *row)
{
    unsigned int i;
    unsigned long callback_bits;
    if (!live(p)) return;
    unhold(p);
    callback_bits = (unsigned long)CM_PROJECTILE_AI(p->task);
    CM_PROJECTILE_AI(p->task) = (ControlMachineProjectileCallback)(
        ((unsigned long)func_08004214_705314 & ~CM_P_DISABLED) |
        (callback_bits & CM_P_DISABLED));
    CM_P_U16(p->task, 0x8a) = (unsigned short)row[CM_PROJECTILE_TIMER];
    for (i = 0; i < 3u; ++i) {
        CM_P_F32(p->object, 8u + i * 4u) = bits_float(row[CM_PROJECTILE_X + i]);
        CM_P_F32(p->task, 0x78u + i * 4u) = bits_float(row[CM_PROJECTILE_VX + i]);
    }
    p->born = row[CM_PROJECTILE_BORN];
    if (s_active && (!s_owner || s_paused)) hold(p);
}

static ControlMachineProjectile *spawn(const unsigned int *row)
{
    void *resource = func_800141C4_14DC4(CM_P_RESOURCE);
    void *task, *object, *previous;
    ControlMachineProjectile *p;
    if (!resource_available(resource)) return 0;
    task = func_802171A8_5D2678(s_child, func_0800413C_70523C, 10);
    if (!pointer_valid(task)) return 0;
    object = CM_PROJECTILE_PTR(task, 0x18);
    if (!pointer_valid(object)) {
        CM_P_U32(task, 0x68) |= CM_P_REMOVE_PENDING;
        return 0;
    }
    CM_P_U16(task, 0x28) = CM_P_RESOURCE;
    CM_PROJECTILE_PTR(task, 0x2c) = resource;
    previous = D_8016DAB4_16E6B4;
    D_8016DAB4_16E6B4 = task;
    func_0800413C_70523C(task, object);
    D_8016DAB4_16E6B4 = previous;
    p = register_projectile(task, row[CM_PROJECTILE_ID],
                            row[CM_PROJECTILE_BORN], 1);
    if (!p) CM_P_U32(task, 0x68) |= CM_P_REMOVE_PENDING;
    return p;
}

int anchor_control_machine_projectiles_apply(const AnchorControlMachineSnapshot *in)
{
    ControlMachineProjectile *created[ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES];
    unsigned int i, created_count = 0, max_id = s_serial, max_tick = s_tick;
    void *resource;
    if (!in || !bound_current() || !s_active || s_owner ||
        !anchor_control_machine_snapshot_valid(in)) return 0;
    /* Preflight all fallible conditions before changing the live set. A
     * failed constructor rolls back only tasks allocated by this apply. */
    for (i = 0; i < in->projectile_count; ++i) {
        const unsigned int *row = in->projectile[i];
        if (row[CM_PROJECTILE_ID] > max_id) max_id = row[CM_PROJECTILE_ID];
        if (row[CM_PROJECTILE_BORN] > max_tick)
            max_tick = row[CM_PROJECTILE_BORN];
    }
    resource = 0;
    for (i = 0; i < in->projectile_count; ++i)
        if (!find_id(in->projectile[i][CM_PROJECTILE_ID])) {
            resource = func_800141C4_14DC4(CM_P_RESOURCE);
            if (!resource_available(resource)) return 0;
            break;
        }
    s_reconstructing = 1;
    for (i = 0; i < in->projectile_count; ++i) {
        const unsigned int *row = in->projectile[i];
        ControlMachineProjectile *p = find_id(row[CM_PROJECTILE_ID]);
        if (p) continue;
        p = spawn(row);
        if (!p) {
            unsigned int j;
            for (j = 0; j < created_count; ++j) retire(created[j]);
            s_reconstructing = 0;
            /* Expire shots absent from the authority before retrying. A
             * full native group may otherwise make every future attempt
             * fail, while no new checkpoint has been committed. */
            for (j = 0; j < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++j)
                if (live(&s_projectiles[j]) &&
                    !in_snapshot(in, s_projectiles[j].id))
                    retire(&s_projectiles[j]);
            return 0;
        }
        created[created_count++] = p;
    }
    s_reconstructing = 0;
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i)
        if (live(&s_projectiles[i]) &&
            !in_snapshot(in, s_projectiles[i].id)) retire(&s_projectiles[i]);
    for (i = 0; i < in->projectile_count; ++i)
        correct(find_id(in->projectile[i][CM_PROJECTILE_ID]), in->projectile[i]);
    s_serial = max_id;
    s_tick = max_tick;
    return 1;
}
