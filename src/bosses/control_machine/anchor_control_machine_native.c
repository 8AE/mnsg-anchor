#include "bosses/control_machine/anchor_control_machine_native.h"
#include "bosses/control_machine/anchor_control_machine_hud.h"
#include "bosses/control_machine/anchor_control_machine_projectiles.h"
#include "bosses/anchor_boss_invite_world.h"
#include "world/anchor_world_quest.h"

#ifndef ANCHOR_CONTROL_MACHINE_NATIVE_HOST_TEST
#include "platform/modding.h"
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
#include "platform/recomputils.h"
#endif
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define RECOMP_PATCH
#endif

typedef void (*ControlMachineCallback)(void *, void *);
#define U8(p,o) (*(volatile unsigned char *)((unsigned char *)(p)+(o)))
#define U16(p,o) (*(volatile unsigned short *)((unsigned char *)(p)+(o)))
#define U32(p,o) (*(volatile unsigned int *)((unsigned char *)(p)+(o)))
#define F32(p,o) (*(volatile float *)((unsigned char *)(p)+(o)))
#ifndef CM_NATIVE_PTR
#define CM_NATIVE_PTR(p,o) (*(void *volatile *)((unsigned char *)(p)+(o)))
#endif
#ifndef CM_NATIVE_AI
#define CM_NATIVE_AI(p) (*(ControlMachineCallback volatile *)((unsigned char *)(p)+0x0c))
#endif

#define CM_ROOM 0x155u
#define CM_CALLBACK_DISABLED 0x00800000ul
#define CM_HIT_PENDING 1u
#define CM_REMOVE_PENDING 2u
#define CM_HIT_QUEUE 32u
#define CM_HIT_SEEN 64u
#define CM_PENDING_NONE 0
#define CM_PENDING_HAZARDS 1
#define CM_PENDING_FULL 2

extern unsigned short D_800C7AB2;
extern unsigned short D_8015CDB6;
#ifndef ANCHOR_CONTROL_MACHINE_NATIVE_HOST_TEST
/* The native HP mirror at 0x8015CDBC is not a standalone exported symbol. */
extern unsigned char D_8015CDB8;
#define CM_STATUS_HP_LOCAL U16(&D_8015CDB8, 4)
#else
extern unsigned short D_8015CDBC;
#define CM_STATUS_HP_LOCAL D_8015CDBC
#endif
extern unsigned char *D_8015C5C8_15D1C8;
extern void func_80023DF0_249F0(unsigned int event);
extern void func_8022026C_5DB73C(void *, unsigned int, unsigned int,
                                  unsigned int);
extern void *func_80036158_36D58(void *, void *, unsigned int);
extern void *func_802171A8_5D2678(void *, ControlMachineCallback,
                                   unsigned char);
extern float func_80003E10_4A10(unsigned int);
extern float func_80003EA0_4AA0(unsigned int);
extern void func_8001E4A4_1F0A4(float [16], short, short, short);
extern void func_8021A858_5D5D28(float [16], float [3]);
extern unsigned int func_800141C4_14DC4(unsigned int);
extern unsigned int func_8021B988_5D6E58(void *, float, float, float, float);
extern unsigned short D_8015CC30_15D830;
extern void func_0800413C_70523C(void *, void *);
extern void func_802112EC_5CC7BC(void *, void *);
extern void func_08002F58_704058(void *, void *);
extern void func_08002F8C_70408C(void *, void *);
extern void func_08003054_704154(void *, void *);
extern void func_080030B0_7041B0(void *, void *);
extern void func_0800316C_70426C(void *, void *);
extern void func_080031D8_7042D8(void *, void *);
extern void func_08003634_704734(void *, void *);
extern void func_08003678_704778(void *, void *);
extern void func_080036CC_7047CC(void *, void *);

static ControlMachineCallback volatile s_phases[10];
static void *s_task;
static void *s_object;
static void *s_root;
static unsigned int s_invite_visit, s_binding_serial, s_visit;
static unsigned char s_generation, s_root_generation;
static unsigned int s_terminal_event_visit;
static unsigned int s_terminal_release_visit;
static int s_active, s_owner, s_paused, s_held;
static ControlMachineCallback s_held_ai;
static AnchorControlMachineSnapshot s_pending;
static int s_pending_valid;
static int s_pending_kind, s_live_command_ready, s_command_observed;
static unsigned int s_command_serial, s_applied_command;
static unsigned int s_command_phase, s_command_hp, s_command_status;
static int s_local_hits[8];
static unsigned int s_local_read, s_local_count, s_local_sequence;
static int s_hits[CM_HIT_QUEUE][4];
static unsigned int s_hit_read, s_hit_count;
static int s_seen[CM_HIT_SEEN][4];
static unsigned int s_seen_next;
static int s_injecting;
static int s_terminal_fallback, s_force_terminal_hit;
static void *s_flash;
static int s_flash_replica;
static unsigned int s_last_death_timer, s_last_death_phase;
static struct {
    int valid;
    float xyz[3], scale[3];
    unsigned short rotation[3];
} s_pose;

#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
static AnchorControlMachineNativeDebug s_debug;
#define CM_RECORD_FAILURE(kind) (++s_debug.failure[(kind)])

static void debug_reset(void)
{
    volatile unsigned char *bytes = (volatile unsigned char *)&s_debug;
    unsigned int i;
    for (i = 0; i < sizeof(s_debug); ++i) bytes[i] = 0;
}

void anchor_control_machine_native_debug_take(
    AnchorControlMachineNativeDebug *out)
{
    unsigned int i;
    volatile unsigned char *destination;
    const volatile unsigned char *source;
    if (!out) return;
    destination = (volatile unsigned char *)out;
    source = (const volatile unsigned char *)&s_debug;
    for (i = 0; i < sizeof(s_debug); ++i)
        destination[i] = source[i];
    s_debug.applied_count = 0;
    for (i = 0; i < CM_DEBUG_FAILURE_COUNT; ++i)
        s_debug.failure[i] = 0;
}

static void debug_applied(const AnchorControlMachineSnapshot *s)
{
    const unsigned int *r = s->root;
    ++s_debug.applied_count;
    s_debug.phase = r[CM_PHASE];
    s_debug.timer = r[CM_TIMER];
    s_debug.hp = r[CM_HP];
    s_debug.status = r[CM_STATUS];
    s_debug.x = r[CM_X];
    s_debug.y = r[CM_Y];
    s_debug.z = r[CM_Z];
    s_debug.shots = s->projectile_count;
}
#define CM_DEBUG_APPLIED(s) debug_applied(s)
#else
#define CM_RECORD_FAILURE(kind) ((void)0)
#define CM_DEBUG_APPLIED(s) ((void)0)
#endif

static int bound_current(void);
static void drain_rejected_hits(void);
static void update_projectile_context(void);
static int finite_between(unsigned int bits, float low, float high);

#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && \
    !defined(ANCHOR_CONTROL_MACHINE_NATIVE_HOST_TEST)
static unsigned int s_last_capture_issue = 0xffffffffu;
static void capture_diagnostic(unsigned int issue, unsigned int field,
                               const AnchorControlMachineSnapshot *snapshot)
{
    const unsigned int *r = snapshot ? snapshot->root : 0;
    if (issue == s_last_capture_issue) return;
    s_last_capture_issue = issue;
    recomp_printf("[CM] capture issue=%u field=%u value=%u phase=%u hp=%u status=%u mirror=%u frame=%u shots=%u\n",
                  issue, field, r && field && field <= 24u ? r[field - 1u] : 0u,
                  r ? r[CM_PHASE] : 0u,
                  r ? r[CM_HP] : 0u, r ? r[CM_STATUS] : 0u,
                  r ? r[CM_STATUS_HP] : 0u,
                  r ? r[CM_FRAME] : 0u,
                  snapshot ? snapshot->projectile_count : 0u);
}

static unsigned int capture_invalid_field(const AnchorControlMachineSnapshot *s)
{
    const unsigned int *r = s->root;
    unsigned int i, j;
    if (r[CM_PHASE] < 1u || r[CM_PHASE] > 9u) return CM_PHASE + 1u;
    if (r[CM_TIMER] > 0xffffu) return CM_TIMER + 1u;
    if (r[CM_HP] > (r[CM_PHASE] <= 5u ? 5u : 255u) ||
        (r[CM_PHASE] >= 6u && r[CM_HP] != 255u)) return CM_HP + 1u;
    for (i = CM_FLASH; i <= CM_YAW_CACHE; ++i)
        if (r[i] > 0xffffu) return i + 1u;
    for (i = CM_X; i <= CM_Z; ++i)
        if (!finite_between(r[i], -1000000.f, 1000000.f)) return i + 1u;
    for (i = CM_RX; i <= CM_RZ; ++i)
        if (r[i] > 0xffffu) return i + 1u;
    for (i = CM_SCALE_X; i <= CM_SCALE_Z; ++i)
        if (!finite_between(r[i], 0.f, 1000.f)) return i + 1u;
    if (!finite_between(r[CM_FRAME], 0.f, 100000.f)) return CM_FRAME + 1u;
    if (r[CM_STATUS] > 4u ||
        (r[CM_PHASE] <= 5u && r[CM_STATUS] != 0u &&
         r[CM_STATUS] != 1u) ||
        (r[CM_PHASE] >= 6u && (r[CM_STATUS] < 2u ||
                               r[CM_STATUS] > 4u))) return CM_STATUS + 1u;
    if (r[CM_STATUS_HP] > 5u) return CM_STATUS_HP + 1u;
    if (r[CM_FLASH_ALPHA] > 256u) return CM_FLASH_ALPHA + 1u;
    if (!r[CM_COMMAND]) return CM_COMMAND + 1u;
    if (s->projectile_count > ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES)
        return 100u;
    for (i = 0; i < s->projectile_count; ++i) {
        const unsigned int *p = s->projectile[i];
        if (!p[CM_PROJECTILE_ID] || p[CM_PROJECTILE_ID] > 0x7fffffffu)
            return 101u + i * ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS;
        if (p[CM_PROJECTILE_BORN] > 0x7fffffffu ||
            p[CM_PROJECTILE_TIMER] > 80u)
            return 102u + i * ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS;
        for (j = CM_PROJECTILE_X; j <= CM_PROJECTILE_Z; ++j)
            if (!finite_between(p[j], -1000000.f, 1000000.f))
                return 101u + i * ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS + j;
        for (j = CM_PROJECTILE_VX; j <= CM_PROJECTILE_VZ; ++j)
            if (!finite_between(p[j], -10000.f, 10000.f))
                return 101u + i * ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS + j;
        for (j = 0; j < i; ++j)
            if (s->projectile[j][CM_PROJECTILE_ID] == p[CM_PROJECTILE_ID])
                return 101u + i * ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS;
    }
    return 999u;
}
#else
#define capture_diagnostic(issue, field, snapshot) ((void)0)
#endif

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

static int finite_between(unsigned int bits, float low, float high)
{
    float value = bits_float(bits);
    return (bits & 0x7f800000u) != 0x7f800000u &&
           value >= low && value <= high;
}

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_CONTROL_MACHINE_NATIVE_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           address >= 0x80001000u && address < 0x80800000u;
#endif
}

static void *current_flash(void)
{
    if (!pointer_valid(D_8015C5C8_15D1C8)) return 0;
    return CM_NATIVE_PTR(D_8015C5C8_15D1C8, 0x3b018);
}

static int flash_owned(void *node)
{
    void *cursor;
    unsigned int i;
    if (!bound_current() || !pointer_valid(node) ||
        U8(node, 4) != 1u || U8(node, 5) != 11u) return 0;
    cursor = CM_NATIVE_PTR(s_task, 0x18);
    for (i = 0; i < 64u && pointer_valid(cursor); ++i) {
        if (cursor == node) return 1;
        cursor = CM_NATIVE_PTR(cursor, 0);
    }
    return 0;
}

static void release_replica_flash(void)
{
    if (s_flash_replica && flash_owned(s_flash))
        (void)func_80036158_36D58(s_task, s_flash, 1);
    s_flash = 0;
    s_flash_replica = 0;
}

static unsigned int capture_flash(void)
{
    void *current = current_flash();
    if (flash_owned(current)) s_flash = current;
    return flash_owned(s_flash) ? U8(s_flash, 0x10) : 256u;
}

static void correct_flash(unsigned int alpha)
{
    void *current;
    if (alpha == 256u) {
        release_replica_flash();
        return;
    }
    if (!flash_owned(s_flash)) {
        s_flash = 0;
        current = current_flash();
        /* Never replace a foreign live singleton to approximate a flash. */
        if (pointer_valid(current) && !(U8(current, 4) & 0x80u)) return;
        func_8022026C_5DB73C(s_task, 255u, 255u, 255u);
        s_flash = current_flash();
        s_flash_replica = flash_owned(s_flash);
    }
    if (flash_owned(s_flash)) U8(s_flash, 0x10) = (unsigned char)alpha;
}

static void death_cosmetic_tick(const unsigned int *r)
{
    void *effect, *object;
    unsigned int previous = s_last_death_timer, timer = r[CM_TIMER], i;
    if (r[CM_PHASE] == 6u && s_last_death_phase == 6u &&
        timer > 0u && timer < previous && previous - timer <= 10u &&
        previous / 10u == timer / 10u + 1u) {
        effect = func_802171A8_5D2678(s_task,
                                       func_802112EC_5CC7BC, 8u);
        if (pointer_valid(effect)) {
            U16(effect, 0xd4) = 1u; /* suppress its own expiry sound/debris */
            object = CM_NATIVE_PTR(effect, 0x18);
            if (pointer_valid(object))
                for (i = 0; i < 3u; ++i) {
                    F32(object, 8 + i * 4u) = bits_float(r[CM_X + i]);
                    F32(object, 0x1c + i * 4u) =
                        bits_float(r[CM_SCALE_X + i]);
                }
        }
    }
    s_last_death_phase = r[CM_PHASE];
    s_last_death_timer = timer;
}

static void copy_bytes(void *dst, const void *src, unsigned int count)
{
    unsigned char *out = dst;
    const unsigned char *in = src;
    while (count--) *out++ = *in++;
}

static void init_phases(void)
{
    s_phases[0] = 0;
    s_phases[1] = func_08002F58_704058;
    s_phases[2] = func_08002F8C_70408C;
    s_phases[3] = func_08003054_704154;
    s_phases[4] = func_080030B0_7041B0;
    s_phases[5] = func_0800316C_70426C;
    s_phases[6] = func_080031D8_7042D8;
    s_phases[7] = func_08003634_704734;
    s_phases[8] = func_08003678_704778;
    s_phases[9] = func_080036CC_7047CC;
}

static int callback_is(ControlMachineCallback current,
                       ControlMachineCallback expected)
{
    return ((unsigned long)current & ~CM_CALLBACK_DISABLED) ==
           ((unsigned long)expected & ~CM_CALLBACK_DISABLED);
}

static unsigned int phase_of(ControlMachineCallback callback)
{
    unsigned int phase;
    for (phase = 1; phase < 10; ++phase)
        if (callback_is(callback, s_phases[phase])) return phase;
    return 0;
}

static unsigned int phase_now(void)
{
    if (!s_task) return 0;
    return phase_of(s_held ? s_held_ai : CM_NATIVE_AI(s_task));
}

static void hold_noop(void *task, void *object)
{
    unsigned int i;
    if (task != s_task || object != s_object || !s_held || !s_pose.valid ||
        !s_active || s_owner || s_paused || !bound_current() ||
        phase_now() < 6u) return;
    for (i = 0; i < 3u; ++i) {
        unsigned int xyz_offset = 8u + i * 4u;
        unsigned int scale_offset = 0x1cu + i * 4u;
        unsigned int rotation_offset = 0x14u + i * 2u;
        int current = U16(object, rotation_offset);
        int target = s_pose.rotation[i];
        /* CM yaw is an unmasked 16-bit accumulator. A 10-bit shortest path
         * can reverse a legitimate fast spin between network samples. */
        int delta = i == 1u ?
            ((target - current + 32768) & 65535) - 32768 :
            (((target & 1023) - (current & 1023) + 512) & 1023) - 512;
        int step = (int)((float)delta * 0.35f);
        if (delta && !step) step = delta > 0 ? 1 : -1;
        F32(object, xyz_offset) +=
            (s_pose.xyz[i] - F32(object, xyz_offset)) * 0.35f;
        U16(object, rotation_offset) = (unsigned short)(i == 1u ?
            (current + step) & 65535 : (current + step) & 1023);
        F32(object, scale_offset) +=
            (s_pose.scale[i] - F32(object, scale_offset)) * 0.35f;
    }
}

static void unhold(void)
{
    if (s_held && s_task && anchor_control_machine_bound_task() == s_task &&
        U8(s_task, 0x74) == s_generation &&
        CM_NATIVE_PTR(s_task, 0x18) == s_object &&
        anchor_control_machine_bound_root() == s_root &&
        callback_is(CM_NATIVE_AI(s_task), hold_noop)) {
        unsigned long current = (unsigned long)CM_NATIVE_AI(s_task);
        unsigned int phase = phase_of(s_held_ai);
        if (phase >= 7u) {
            /* Native fade phases 7/8 require a local flash node that a
             * checkpoint follower may never have allocated. Once event0 and
             * root departure are armed, the verified final callback only
             * counts down and deletes this child. */
            if (s_terminal_event_visit != s_visit &&
                (D_8015CDB6 == 3u || D_8015CDB6 == 4u)) {
                func_80023DF0_249F0(0);
                s_terminal_event_visit = s_visit;
            }
            if (s_terminal_release_visit != s_visit &&
                (D_8015CDB6 == 3u || D_8015CDB6 == 4u) &&
                anchor_world_quest_koryuta_release_terminal())
                s_terminal_release_visit = s_visit;
            if (s_terminal_event_visit != s_visit ||
                s_terminal_release_visit != s_visit) return;
            release_replica_flash();
            s_held_ai = s_phases[9];
            U16(s_task, 0x8a) = 5u;
        }
        CM_NATIVE_AI(s_task) = (ControlMachineCallback)(
            ((unsigned long)s_held_ai & ~CM_CALLBACK_DISABLED) |
            (current & CM_CALLBACK_DISABLED));
    }
    s_held = 0;
    s_held_ai = 0;
}

static void hold(void)
{
    if (!s_task || s_held || anchor_control_machine_bound_task() != s_task)
        return;
    s_held_ai = CM_NATIVE_AI(s_task);
    s_held = 1;
    CM_NATIVE_AI(s_task) = (ControlMachineCallback)(
        ((unsigned long)hold_noop & ~CM_CALLBACK_DISABLED) |
        ((unsigned long)s_held_ai & CM_CALLBACK_DISABLED));
}

static int bound_current(void)
{
    return D_800C7AB2 == CM_ROOM && s_task &&
           s_task == anchor_control_machine_bound_task() &&
           s_object == CM_NATIVE_PTR(s_task, 0x18) &&
           s_root == anchor_control_machine_bound_root() &&
           s_generation == U8(s_task, 0x74) &&
           s_root_generation == U8(s_root, 0x74) &&
           s_invite_visit == anchor_boss_invite_world_visit() &&
           !(U32(s_task, 0x68) & CM_REMOVE_PENDING);
}

static void update_projectile_context(void)
{
    anchor_control_machine_projectiles_set_context(
        s_task, s_visit, s_active,
        s_owner && !(s_pending_valid && s_pending_kind == CM_PENDING_FULL),
        s_paused || (s_pending_valid && s_pending_kind == CM_PENDING_FULL));
}

void anchor_control_machine_native_clear_hits(void)
{
    s_local_read = s_local_count = 0;
    s_hit_read = s_hit_count = s_seen_next = 0;
    s_injecting = 0;
    /* Keep local sequence monotonic across player/task reincarnations. */
}

void anchor_control_machine_native_clear_local_hits(void)
{
    s_local_read = s_local_count = 0;
}

void anchor_control_machine_native_discard_pending(void)
{
    s_pending_valid = 0;
    s_pending_kind = CM_PENDING_NONE;
    s_live_command_ready = s_command_observed = 0;
    s_command_serial = s_applied_command = 0;
    s_command_phase = s_command_hp = s_command_status = 0;
    s_pose.valid = 0;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    s_debug.pending_age = 0;
#endif
    update_projectile_context();
}

static void observe_local_hit(void)
{
    unsigned int flags;
    /* The owner must let native 02540 consume its own collision bit. Its
     * contact is already authoritative and must never round-trip as a hit. */
    if (!s_active || s_owner || !bound_current()) return;
    flags = U32(s_task, 0x68);
    if (!(flags & CM_HIT_PENDING)) return;
    /* Never let an unsynchronized native contact mutate follower HP. */
    U32(s_task, 0x68) = flags & ~CM_HIT_PENDING;
    if ((s_pending_valid && s_pending_kind == CM_PENDING_FULL) ||
        phase_now() != 5u ||
        U16(s_task, 0xda) != 0u ||
        U16(s_task, 0xd4) != 0u ||
        s_paused || s_local_count == 8u) return;
    s_local_sequence = s_local_sequence == 0x7fffffffu ? 1u :
                       s_local_sequence + 1u;
    s_local_hits[(s_local_read + s_local_count) % 8u] =
        (int)s_local_sequence;
    ++s_local_count;
}

void anchor_control_machine_native_tick(void)
{
    void *task = anchor_control_machine_bound_task();
    void *root = task ? anchor_control_machine_bound_root() : 0;
    unsigned int visit = anchor_boss_invite_world_visit();
    if (task == s_task && root == s_root && bound_current()) {
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
        if (s_pending_valid) {
            if (s_debug.pending_age != 0xffffffffu)
                ++s_debug.pending_age;
        } else s_debug.pending_age = 0;
#endif
        update_projectile_context();
        if (s_terminal_fallback &&
            (D_8015CDB6 == 3u || D_8015CDB6 == 4u) &&
            s_terminal_release_visit != s_visit &&
            anchor_world_quest_koryuta_release_terminal())
            s_terminal_release_visit = s_visit;
        observe_local_hit();
        return;
    }
    unhold();
    anchor_control_machine_projectiles_reset();
    s_flash = 0;
    s_flash_replica = 0;
    s_pose.valid = 0;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    debug_reset();
#endif
    s_last_death_phase = s_last_death_timer = 0;
    s_task = task;
    s_object = task ? CM_NATIVE_PTR(task, 0x18) : 0;
    s_root = root;
    s_generation = task ? U8(task, 0x74) : 0;
    s_root_generation = root ? U8(root, 0x74) : 0;
    s_invite_visit = task ? visit : 0;
    if (task) {
        s_binding_serial = s_binding_serial == 0x7fffffffu ? 1u :
                           s_binding_serial + 1u;
        s_visit = s_binding_serial;
    } else s_visit = 0;
    s_active = s_owner = s_paused = s_pending_valid = 0;
    s_pending_kind = CM_PENDING_NONE;
    s_live_command_ready = s_command_observed = 0;
    s_command_serial = s_applied_command = 0;
    s_command_phase = s_command_hp = s_command_status = 0;
    anchor_control_machine_native_clear_hits();
    s_terminal_event_visit = 0;
    s_terminal_release_visit = 0;
    s_terminal_fallback = s_force_terminal_hit = 0;
    init_phases();
    update_projectile_context();
}

unsigned int anchor_control_machine_native_visit(void) { return s_visit; }
int anchor_control_machine_native_bound(void) { return bound_current(); }
int anchor_control_machine_native_pending(void) { return s_pending_valid; }

int anchor_control_machine_native_ready(void)
{
    return bound_current() &&
           (anchor_world_quest_koryuta_local_ready() ||
            s_terminal_release_visit == s_visit) &&
           phase_now() != 0;
}

int anchor_control_machine_native_transport_ready(void)
{
    return anchor_control_machine_native_ready();
}

int anchor_control_machine_snapshot_valid(const AnchorControlMachineSnapshot *s)
{
    const unsigned int *r;
    unsigned int i, j;
    if (!s || s->projectile_count > ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES)
        return 0;
    r = s->root;
    if (r[CM_PHASE] < 1u || r[CM_PHASE] > 9u ||
        r[CM_TIMER] > 0xffffu || r[CM_FLASH] > 0xffffu ||
        r[CM_ORBIT] > 0xffffu || r[CM_SPEED] > 0xffffu ||
        r[CM_SPEED_STEP] > 0xffffu || r[CM_INTENSITY] > 0xffffu ||
        r[CM_YAW_CACHE] > 0xffffu || r[CM_RX] > 0xffffu ||
        r[CM_RY] > 0xffffu || r[CM_RZ] > 0xffffu ||
        r[CM_STATUS] > 4u || r[CM_STATUS_HP] > 5u ||
        r[CM_FLASH_ALPHA] > 256u || !r[CM_COMMAND]) return 0;
    if (r[CM_PHASE] <= 5u) {
        if (r[CM_HP] > 5u ||
            (r[CM_STATUS] != 0u && r[CM_STATUS] != 1u)) return 0;
    } else if (r[CM_HP] != 255u || r[CM_STATUS] < 2u ||
               r[CM_STATUS] > 4u) return 0;
    for (i = CM_X; i <= CM_Z; ++i)
        if (!finite_between(r[i], -1000000.f, 1000000.f)) return 0;
    for (i = CM_SCALE_X; i <= CM_SCALE_Z; ++i)
        if (!finite_between(r[i], 0.f, 1000.f)) return 0;
    if (!finite_between(r[CM_FRAME], 0.f, 100000.f)) return 0;
    for (i = 0; i < s->projectile_count; ++i) {
        const unsigned int *p = s->projectile[i];
        if (!p[CM_PROJECTILE_ID] || p[CM_PROJECTILE_ID] > 0x7fffffffu ||
            p[CM_PROJECTILE_BORN] > 0x7fffffffu ||
            p[CM_PROJECTILE_TIMER] > 80u) return 0;
        for (j = CM_PROJECTILE_X; j <= CM_PROJECTILE_Z; ++j)
            if (!finite_between(p[j], -1000000.f, 1000000.f)) return 0;
        for (j = CM_PROJECTILE_VX; j <= CM_PROJECTILE_VZ; ++j)
            if (!finite_between(p[j], -10000.f, 10000.f)) return 0;
        for (j = 0; j < i; ++j)
            if (s->projectile[j][CM_PROJECTILE_ID] == p[CM_PROJECTILE_ID])
                return 0;
    }
    return 1;
}

static int should_hold_ai(void)
{
    if (!s_active) return 0;
    if (s_paused ||
        (s_pending_valid && s_pending_kind == CM_PENDING_FULL)) return 1;
    if (s_owner) return 0;
    return !s_live_command_ready || phase_now() >= 6u;
}

void anchor_control_machine_native_set_role(int active, int owner, int paused)
{
    if (s_terminal_fallback) active = owner = paused = 0;
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
    if (!s_active || s_owner) s_pose.valid = 0;
    if (!s_active) release_replica_flash();
    if (should_hold_ai()) hold();
    else unhold();
    update_projectile_context();
}

int anchor_control_machine_native_request_terminal(void)
{
    unsigned int phase;
    if (!bound_current() || !anchor_control_machine_native_ready()) return 0;
    phase = phase_now();
    if (phase < 1u || phase > 5u) return 0;
    s_pending_valid = 0;
    s_pending_kind = CM_PENDING_NONE;
    anchor_control_machine_native_clear_hits();
    s_terminal_fallback = s_force_terminal_hit = 1;
    anchor_control_machine_native_set_role(0, 0, 0);
    return 1;
}

int anchor_control_machine_native_terminal_fallback(void)
{
    return s_terminal_fallback && bound_current();
}

static unsigned int next_command(unsigned int command)
{
    return command == 0xffffffffu ? 1u : command + 1u;
}

static int command_newer(unsigned int candidate, unsigned int previous)
{
    return !previous || (candidate != previous &&
           (candidate - previous) < 0x80000000u);
}

int anchor_control_machine_native_capture(AnchorControlMachineSnapshot *s)
{
    unsigned int *r;
    unsigned int i, command;
    int valid;
    if (!s || !anchor_control_machine_native_ready()) {
        capture_diagnostic(1u, 0u, 0);
        return 0;
    }
    if (s_pending_valid) {
        capture_diagnostic(2u, 0u, 0);
        return 0;
    }
    if (anchor_control_machine_native_hit_pending()) {
        capture_diagnostic(3u, 0u, 0);
        return 0;
    }
    r = s->root;
    s->projectile_count = 0;
    r[CM_PHASE] = phase_now();
    r[CM_TIMER] = U16(s_task, 0x8a);
    r[CM_HP] = U8(s_task, 0x8d);
    r[CM_FLASH] = U16(s_task, 0xd4);
    r[CM_ORBIT] = U16(s_task, 0xd8);
    r[CM_SPEED] = U16(s_task, 0xda);
    r[CM_SPEED_STEP] = U16(s_task, 0xdc);
    r[CM_INTENSITY] = U16(s_task, 0xde);
    r[CM_YAW_CACHE] = U16(s_task, 0xe0);
    for (i = 0; i < 3; ++i) {
        r[CM_X + i] = float_bits(F32(s_object, 8 + i * 4));
        r[CM_RX + i] = U16(s_object, 0x14 + i * 2);
        r[CM_SCALE_X + i] = float_bits(F32(s_object, 0x1c + i * 4));
    }
    r[CM_FRAME] = float_bits(F32(s_object, 0x28));
    r[CM_STATUS] = D_8015CDB6;
    r[CM_STATUS_HP] = CM_STATUS_HP_LOCAL;
    r[CM_FLASH_ALPHA] = capture_flash();
    if (!(U32(s_object, 0x30) & 0x40000000u)) {
        capture_diagnostic(4u, 0u, 0);
        return 0;
    }
    r[CM_COLOUR] = U32(s_object, 0x8c);
    command = s_command_serial ? s_command_serial : 1u;
    if (s_command_observed &&
        (r[CM_PHASE] != s_command_phase || r[CM_HP] != s_command_hp ||
         r[CM_STATUS] != s_command_status))
        command = next_command(command);
    r[CM_COMMAND] = command;
    if (!anchor_control_machine_projectiles_capture(s)) {
        capture_diagnostic(5u, 0u, 0);
        return 0;
    }
    valid = anchor_control_machine_snapshot_valid(s);
    if (valid) {
        s_command_serial = command;
        s_command_phase = r[CM_PHASE];
        s_command_hp = r[CM_HP];
        s_command_status = r[CM_STATUS];
        s_command_observed = 1;
    }
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && \
    !defined(ANCHOR_CONTROL_MACHINE_NATIVE_HOST_TEST)
    capture_diagnostic(valid ? 0u : 6u,
                       valid ? 0u : capture_invalid_field(s), s);
#endif
    return valid;
}

static int apply_now(const AnchorControlMachineSnapshot *s)
{
    const unsigned int *r = s->root;
    unsigned int i;
    int snap_pose;
    if (!anchor_control_machine_native_ready()) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_READY);
        return 0;
    }
    if (!(U32(s_object, 0x30) & 0x40000000u)) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_MODEL);
        return 0;
    }
    if (!anchor_control_machine_snapshot_valid(s)) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_VALID);
        return 0;
    }
    /* Projectile creation can fail. Keep the root and terminal globals
     * unchanged unless the full current hazard set can be reconstructed. */
    anchor_control_machine_projectiles_set_context(s_task, s_visit, 1, 0, 1);
    if (!anchor_control_machine_projectiles_apply(s)) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_PROJECTILES);
        update_projectile_context();
        return 0;
    }
    if (!s_held) hold();
    if (!s_held) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_HOLD);
        return 0;
    }
    s_held_ai = s_phases[r[CM_PHASE]];
    snap_pose = r[CM_PHASE] <= 5u || !s_pose.valid || s_owner;
    if (r[CM_PHASE] >= 6u) drain_rejected_hits();
    U16(s_task, 0x8a) = (unsigned short)r[CM_TIMER];
    U8(s_task, 0x8d) = (unsigned char)r[CM_HP];
    U16(s_task, 0xd4) = (unsigned short)r[CM_FLASH];
    U16(s_task, 0xd8) = (unsigned short)r[CM_ORBIT];
    U16(s_task, 0xda) = (unsigned short)r[CM_SPEED];
    U16(s_task, 0xdc) = (unsigned short)r[CM_SPEED_STEP];
    U16(s_task, 0xde) = (unsigned short)r[CM_INTENSITY];
    U16(s_task, 0xe0) = (unsigned short)r[CM_YAW_CACHE];
    for (i = 0; i < 3; ++i) {
        s_pose.xyz[i] = bits_float(r[CM_X + i]);
        s_pose.rotation[i] = (unsigned short)r[CM_RX + i];
        s_pose.scale[i] = bits_float(r[CM_SCALE_X + i]);
        if (snap_pose) {
            F32(s_object, 8 + i * 4) = s_pose.xyz[i];
            U16(s_object, 0x14 + i * 2) = s_pose.rotation[i];
            F32(s_object, 0x1c + i * 4) = s_pose.scale[i];
        }
    }
    s_pose.valid = 1;
    F32(s_object, 0x28) = bits_float(r[CM_FRAME]);
    U32(s_object, 0x8c) = r[CM_COLOUR];
    correct_flash(r[CM_FLASH_ALPHA]);
    death_cosmetic_tick(r);
    D_8015CDB6 = (unsigned short)r[CM_STATUS];
    CM_STATUS_HP_LOCAL = (unsigned short)r[CM_STATUS_HP];
    if (r[CM_STATUS] == 3u || r[CM_STATUS] == 4u)
        U32(s_task, 0x60) = 0;
    if (r[CM_PHASE] >= 6u && r[CM_STATUS] >= 2u &&
        r[CM_STATUS] <= 4u && s_terminal_event_visit != s_visit) {
        func_80023DF0_249F0(0);
        s_terminal_event_visit = s_visit;
    }
    if (r[CM_PHASE] >= 7u &&
        (r[CM_STATUS] == 3u || r[CM_STATUS] == 4u) &&
        s_terminal_release_visit != s_visit) {
        if (!anchor_world_quest_koryuta_release_terminal()) {
            CM_RECORD_FAILURE(CM_DEBUG_FAIL_RELEASE);
            return 0;
        }
        s_terminal_release_visit = s_visit;
    }
    s_applied_command = s_command_serial = r[CM_COMMAND];
    s_command_phase = r[CM_PHASE];
    s_command_hp = r[CM_HP];
    s_command_status = r[CM_STATUS];
    s_command_observed = 1;
    s_live_command_ready = r[CM_PHASE] <= 5u;
    CM_DEBUG_APPLIED(s);
    update_projectile_context();
    return 1;
}

static int apply_hazards(const AnchorControlMachineSnapshot *s)
{
    anchor_control_machine_projectiles_set_context(s_task, s_visit, 1, 0, 1);
    if (!anchor_control_machine_projectiles_apply(s)) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_PROJECTILES);
        update_projectile_context();
        return 0;
    }
    update_projectile_context();
    return 1;
}

int anchor_control_machine_native_apply(const AnchorControlMachineSnapshot *s,
                                        int force_recovery)
{
    unsigned int command;
    int mode;
    if (!s || !anchor_control_machine_snapshot_valid(s)) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_VALID);
        return 0;
    }
    if (s_terminal_release_visit == s_visit &&
        s->root[CM_STATUS] != 3u &&
        s->root[CM_STATUS] != 4u) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_REWIND);
        return 0;
    }
    if (!anchor_control_machine_native_ready()) {
        CM_RECORD_FAILURE(CM_DEBUG_FAIL_READY);
        return 0;
    }
    command = s->root[CM_COMMAND];
    if (!force_recovery && s_pending_valid &&
        command != s_pending.root[CM_COMMAND] &&
        !command_newer(command, s_pending.root[CM_COMMAND])) return 1;
    if (!force_recovery && s_applied_command &&
        command != s_applied_command &&
        !command_newer(command, s_applied_command)) return 1;
    mode = force_recovery || !s_live_command_ready ||
           !s_applied_command || command != s_applied_command ||
           s->root[CM_PHASE] >= 6u ? CM_PENDING_FULL :
           CM_PENDING_HAZARDS;
    if (s_pending_valid && s_pending_kind == CM_PENDING_FULL &&
        command == s_pending.root[CM_COMMAND]) mode = CM_PENDING_FULL;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    if (!s_pending_valid) s_debug.pending_age = 0;
#endif
    copy_bytes(&s_pending, s, sizeof(s_pending));
    s_pending_valid = 1;
    s_pending_kind = mode;
    return 1;
}

int anchor_control_machine_native_take_local_hit(int *sequence)
{
    if (phase_now() >= 6u) anchor_control_machine_native_clear_local_hits();
    if (!sequence || !s_active || !s_local_count) return 0;
    *sequence = s_local_hits[s_local_read];
    s_local_read = (s_local_read + 1u) % 8u;
    --s_local_count;
    return 1;
}

static int same_hit(const int *a, const int *b)
{
    unsigned int i;
    for (i = 0; i < 4u; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static int hit_window(void)
{
    return bound_current() && phase_now() == 5u &&
           U16(s_task, 0xda) == 0u && U16(s_task, 0xd4) == 0u;
}

int anchor_control_machine_native_local_hit_window(void)
{
    return s_active && !s_owner && !s_paused &&
           !(s_pending_valid && s_pending_kind == CM_PENDING_FULL) &&
           hit_window();
}

static void drain_rejected_hits(void)
{
    unsigned int i;
    while (s_hit_count) {
        for (i = 0; i < 4u; ++i)
            s_seen[s_seen_next][i] = s_hits[s_hit_read][i];
        s_seen_next = (s_seen_next + 1u) % CM_HIT_SEEN;
        s_hit_read = (s_hit_read + 1u) % CM_HIT_QUEUE;
        --s_hit_count;
    }
    anchor_control_machine_native_clear_local_hits();
}

int anchor_control_machine_native_queue_hit(const int identity[5])
{
    unsigned int i, index;
    if (!identity || identity[4] != 1 || !s_active || !s_owner ||
        !bound_current()) return 0;
    for (i = 0; i < s_hit_count; ++i)
        if (same_hit(identity, s_hits[(s_hit_read + i) % CM_HIT_QUEUE]))
            return 1;
    for (i = 0; i < CM_HIT_SEEN; ++i)
        if (same_hit(identity, s_seen[i])) return 1;
    if (s_pending_valid || !hit_window()) {
        /* Native would ignore contact outside the vulnerable wait, including
         * the full D4 hurt flash. Never carry it into a later cycle. */
        for (i = 0; i < 4u; ++i)
            s_seen[s_seen_next][i] = identity[i];
        s_seen_next = (s_seen_next + 1u) % CM_HIT_SEEN;
        return 1;
    }
    if (s_hit_count == CM_HIT_QUEUE) return 0;
    index = (s_hit_read + s_hit_count) % CM_HIT_QUEUE;
    for (i = 0; i < 4u; ++i) s_hits[index][i] = identity[i];
    ++s_hit_count;
    return 1;
}

int anchor_control_machine_native_hit_pending(void)
{
    if (phase_now() >= 6u) anchor_control_machine_native_clear_local_hits();
    if (s_owner && s_hit_count && !hit_window()) drain_rejected_hits();
    return s_hit_count || s_local_count || s_injecting;
}

RECOMP_HOOK("func_0800316C_70426C")
void anchor_control_machine_native_hit_begin(void *task, void *object)
{
    (void)object;
    if (task != s_task || !bound_current()) return;
    if (s_force_terminal_hit && phase_now() == 5u &&
        U16(task, 0xda) == 0u) {
        /* Authenticated durable defeat with no live owner: let the one
         * scheduled native helper consume its final old-HP-zero hit. */
        U8(task, 0x8d) = 0u;
        U32(task, 0x68) |= CM_HIT_PENDING;
        return;
    }
    if (!s_active) return;
    if (!s_owner) {
        observe_local_hit();
        return;
    }
    if (!hit_window()) {
        drain_rejected_hits();
        return;
    }
    if (s_paused || !s_hit_count || s_injecting ||
        (U32(task, 0x68) & CM_HIT_PENDING)) return;
    s_injecting = 1;
    U32(task, 0x68) |= CM_HIT_PENDING;
}

/* File_46's per-frame orbit and boss-relative pose are native behavior on
 * every client. Only the owner may enter its attack projectile producer: a
 * birth-return deletion is too late to prevent the initializer's sound and
 * first common collision update on a follower. */
RECOMP_PATCH void func_080029CC_703ACC(void *task)
{
    void *object = CM_NATIVE_PTR(task, 0x18);
    void *root = CM_NATIVE_PTR(task, 0xd0);
    void *root_object = CM_NATIVE_PTR(root, 0x18);
    float matrix[16];
    float position[3];
    float target[3];
    void *shot;
    void *shot_object;
    int speed = (short)U16(task, 0xda);
    unsigned short orbit = (unsigned short)(U16(task, 0xd8) + speed / 4);
    unsigned short yaw = (unsigned short)(U16(object, 0x16) + 2 * speed);
    unsigned int i;

    U16(task, 0xd8) = orbit;
    U16(object, 0x18) = U16(root_object, 0x18);
    U16(object, 0x16) = yaw;
    U16(task, 0xe0) = yaw;
    func_8001E4A4_1F0A4(matrix, 0, 0, (short)U16(root_object, 0x18));
    position[0] = func_80003E10_4A10(orbit) * 10.0f;
    position[1] = 40.0f;
    position[2] = func_80003EA0_4AA0(orbit) * 50.0f;
    func_8021A858_5D5D28(matrix, position);
    for (i = 0; i < 3u; ++i)
        F32(object, 8u + 4u * i) =
            F32(root_object, 8u + 4u * i) + position[i];

    if (s_active && !s_owner && task == s_task && object == s_object &&
        bound_current()) return;
    if (yaw <= 0x140u || yaw >= 0x2c0u ||
        (speed < 0 ? -speed : speed) <= 63 ||
        D_8015CC30_15D830 % 3u) return;

    shot = func_802171A8_5D2678(task, func_0800413C_70523C, 10u);
    if (!shot) return;
    U16(shot, 0x28) = 0x2eu;
    U32(shot, 0x2c) = func_800141C4_14DC4(0x2eu);
    shot_object = CM_NATIVE_PTR(shot, 0x18);
    position[0] = func_80003E10_4A10(orbit) * 10.0f;
    position[1] = 50.0f;
    position[2] = func_80003EA0_4AA0(orbit) * 100.0f;
    target[0] = position[0] - func_80003E10_4A10(yaw) * 100.0f;
    target[1] = 50.0f;
    target[2] = position[2] + func_80003EA0_4AA0(yaw) * 100.0f;
    func_8021A858_5D5D28(matrix, position);
    func_8021A858_5D5D28(matrix, target);
    for (i = 0; i < 3u; ++i)
        F32(shot_object, 8u + 4u * i) =
            F32(root_object, 8u + 4u * i) + position[i];
    func_8021B988_5D6E58(shot,
        F32(root_object, 8u) + target[0],
        F32(root_object, 0xcu) + target[1],
        F32(root_object, 0x10u) + target[2], 2.0f);
}

RECOMP_HOOK_RETURN("func_0800316C_70426C")
void anchor_control_machine_native_hit_end(void)
{
    unsigned int i;
    if (s_force_terminal_hit && bound_current() &&
        D_8015CDB6 == 2u && phase_now() == 6u) {
        s_force_terminal_hit = 0;
        s_terminal_event_visit = s_visit; /* native helper set event0 */
    }
    if (!s_injecting) {
        if (s_owner && s_hit_count && !hit_window())
            drain_rejected_hits();
        return;
    }
    s_injecting = 0;
    if (!bound_current()) return;
    /* Native may reject the injected contact (for example, a same-frame
     * invulnerability transition). Acknowledge that deliberate rejection;
     * retaining it would create a phantom hit on a later vulnerable cycle. */
    for (i = 0; i < 4u; ++i)
        s_seen[s_seen_next][i] = s_hits[s_hit_read][i];
    s_seen_next = (s_seen_next + 1u) % CM_HIT_SEEN;
    s_hit_read = (s_hit_read + 1u) % CM_HIT_QUEUE;
    --s_hit_count;
    if (!hit_window()) drain_rejected_hits();
}

RECOMP_HOOK("func_8021925C_5D472C")
void anchor_control_machine_native_adopt_before_pre(void *task)
{
    int applied;
    if (task == s_task && s_pending_valid && s_active && !s_paused) {
        applied = s_pending_kind == CM_PENDING_HAZARDS ?
                  apply_hazards(&s_pending) : apply_now(&s_pending);
        if (!applied) return;
        s_pending_valid = 0;
        s_pending_kind = CM_PENDING_NONE;
        update_projectile_context();
        if (should_hold_ai()) hold();
        else unhold();
    }
}
