/* Retained render-only children for the shared Impact battles.
 *
 * This module owns two presentation pools: one stock reticle per remote
 * participant and one collision-free visual per remote Ryo shot. It never
 * creates a native attack task, never applies damage and never touches the
 * shared boss/mech health authority. Local successful shots are captured from
 * the native constructor and republished only as visuals.
 *
 * Both pools reuse retained tasks parented to the native battle manager at
 * shared state +0x1BC. Inactive objects are hidden; the engine destroys the
 * whole manager subtree once, so this module never frees a child task.
 */

#include "anchor_impact_players.h"
#include "anchor_impact_native.h"
#include "anchor_remote_model_pool.h"
#include "utils/anchor_impact_players_codec.h"

#ifdef ANCHOR_IMPACT_PLAYERS_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#else
#include "anchor.h"
#include "modding.h"
#include "recomputils.h"
#endif

/* Shared file_13 battle-state block (D_8020EED0_63A2B0). */
extern void *D_8020EED0_63A2B0;
extern unsigned char *D_8015C5C8_15D1C8;

/* Stock Impact reticle/shot materials and models. */
extern unsigned char D_8020A728_635B08[];
extern unsigned char D_8020A7D0_635BB0[];

extern void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                                 unsigned short flags);
extern void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                                float x, float y, float z,
                                short rx, short ry, short rz,
                                float sx, float sy, float sz,
                                short file8, short file9);
extern void func_801DB200_6065E0(void);
extern void *func_800141C4_14DC4(unsigned int file_id);

#define IP_STATE_MANAGER 0x1BCu
#define IP_STATE_CURSOR 0x00u

#define IP_CURSOR_MAX 16
#define IP_SHOT_MAX 32
#define IP_PENDING_MAX 32

/* FUN_801CCA5C creates this mesh; FUN_801CCF74 rotates it to aim.
 * 0x480099F0 / D_8020A2D0 is the full-screen damage flash, NOT a cursor. */
#define IP_CURSOR_MODEL 0x4800A1E0u
#define IP_CURSOR_SCALE 0.2f
#define IP_CURSOR_MODE 9
#define IP_SHOT_MODEL 0x48009AC0u
#define IP_SHOT_MODE 5
#define IP_SHOT_SCALE 0.2f
#define IP_ASSET_FILE 0x4A8

#define IP_SHOT_GRAVITY -0.004f
#define IP_SHOT_MAX_AGE 180
#define IP_SHOT_MAX_DISTANCE_SQ (1500.0f * 1500.0f)
#define IP_SHOT_KIND 1u

#define IP_READ_U8(p, o) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)))
#define IP_WRITE_U8(p, o, v) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)) = (unsigned char)(v))
#define IP_READ_U16(p, o) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)))
#define IP_READ_U32(p, o) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)))
#define IP_WRITE_U16(p, o, v) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)) = (unsigned short)(v))
#define IP_WRITE_U32(p, o, v) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)) = (unsigned int)(v))
#define IP_READ_F32(p, o) \
    (*(volatile float *)((unsigned char *)(p) + (o)))
#define IP_WRITE_F32(p, o, v) \
    (*(volatile float *)((unsigned char *)(p) + (o)) = (float)(v))
#ifndef IP_READ_PTR
#define IP_READ_PTR(p, o) \
    (*(void *volatile *)((unsigned char *)(p) + (o)))
#endif

typedef struct ImpactCursorSlot
{
    void *task;
    void *object;
    unsigned int cid, session;
    int active, seen;
    float position[3];
    unsigned short rotation[3];
} ImpactCursorSlot;

typedef struct ImpactShotSlot
{
    void *task;
    void *object;
    unsigned int cid, session, sequence;
    int active, age;
    float position[3], velocity[3], origin[3];
} ImpactShotSlot;

typedef struct ImpactPending
{
    unsigned int sequence;
    unsigned int row[8];
} ImpactPending;

static ImpactCursorSlot s_cursors[IP_CURSOR_MAX];
static ImpactShotSlot s_shots[IP_SHOT_MAX];
static ImpactPending s_pending[IP_PENDING_MAX];
static int s_pending_head, s_pending_count;
static unsigned int s_next_sequence = 1, s_accepted;
static void *s_manager;
static unsigned int s_scope_stage, s_scope_encounter, s_scope_visit;
static unsigned int s_authority, s_authority_term;

typedef struct ImpactControls {
    unsigned int cid, session;
    unsigned short held, pressed, rx, ry;
    int age;
} ImpactControls;
static ImpactControls s_controls[IP_CURSOR_MAX];
static unsigned int s_input_tick, s_input_held;
static unsigned short s_saved_pad[2][2];
static unsigned int s_saved_aim[2];
static unsigned short s_saved_rotation[2];
static void *s_input_system, *s_input_state, *s_input_cursor;
static int s_input_swapped, s_aim_swapped;
static int s_controls_owner;

static int s_ctor_bracket;
static int s_ctor_captured;
static void *s_ctor_velocity_task;
static float s_ctor_position[3];
static float s_ctor_velocity[3];

static void impact_cursor_update(void *task, void *object);
static void impact_shot_update(void *task, void *object);

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_IMPACT_PLAYERS_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           ((address >= 0x80001000u && address < 0x80800000u) ||
            anchor_remote_model_pool_contains(pointer));
#endif
}

static int linked(const void *task)
{
    void *backlink;
    if (!pointer_valid(task))
        return 0;
    backlink = IP_READ_PTR(task, 0x04);
    return pointer_valid(backlink) && IP_READ_PTR(backlink, 0x00) == task;
}

static int owned_task(const void *task, void (*update)(void *, void *))
{
    return linked(task) &&
           IP_READ_PTR(task, 0x0C) == (void *)(unsigned long)update;
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

/* Bit-pattern finiteness; a float compare is unreliable under -ffast-math. */
static int finite_word(unsigned int value)
{
    return (value & 0x7f800000u) != 0x7f800000u;
}

static int finite_float(float value)
{
    return finite_word(float_bits(value));
}

static void hide_object(void *object)
{
    if (!pointer_valid(object))
        return;
    IP_READ_U8(object, 0x64) |= 1u;
    IP_READ_U8(object, 0x65) = 0;
}

/* Only touch an object that is still owned by the retained task. A stale
 * handle whose subtree was destroyed must never be written. */
static void hide_slot_object(void *task, void *object,
                             void (*update)(void *, void *))
{
    if (owned_task(task, update) && pointer_valid(object) &&
        IP_READ_PTR(task, 0x18) == object)
        hide_object(object);
}

static void show_object(void *object)
{
    IP_READ_U8(object, 0x64) &= (unsigned char)~1u;
    IP_READ_U8(object, 0x65) = 0;
}

/* Received shots must never run collision or boss damage. Zero the combat
 * fields the native attack constructor would otherwise populate. */
static void clear_combat_fields(void *task)
{
    IP_WRITE_U32(task, 0x30, 0);
    IP_WRITE_U32(task, 0x34, 0);
    IP_WRITE_U32(task, 0x38, 0);
    IP_WRITE_U32(task, 0x3C, 0);
    IP_WRITE_U32(task, 0x48, 0);
    IP_WRITE_U32(task, 0x5C, 0);
}

static void refresh_handle(void **task, void **object,
                           void (*update)(void *, void *))
{
    if (!owned_task(*task, update))
    {
        *task = 0;
        *object = 0;
        return;
    }
    if (IP_READ_PTR(*task, 0x18) != *object)
        *object = IP_READ_PTR(*task, 0x18);
}

static void cursor_render(ImpactCursorSlot *slot)
{
    refresh_handle(&slot->task, &slot->object, impact_cursor_update);
    if (!pointer_valid(slot->object))
        return;
    if (!slot->active)
    {
        hide_object(slot->object);
        return;
    }
    IP_WRITE_F32(slot->object, 0x08, slot->position[0]);
    IP_WRITE_F32(slot->object, 0x0C, slot->position[1]);
    IP_WRITE_F32(slot->object, 0x10, slot->position[2]);
    IP_WRITE_U16(slot->object, 0x14, slot->rotation[0]);
    IP_WRITE_U16(slot->object, 0x16, slot->rotation[1]);
    IP_WRITE_U16(slot->object, 0x18, slot->rotation[2]);
    IP_WRITE_U8(slot->object, 0x05, IP_CURSOR_MODE);
    /* Preserve the native cyan/red vertex colors and texture transparency. */
    show_object(slot->object);
}

static void shot_simulate(ImpactShotSlot *slot)
{
    float dx, dy, dz;

    refresh_handle(&slot->task, &slot->object, impact_shot_update);
    if (!pointer_valid(slot->object))
        return;
    if (!slot->active)
    {
        hide_object(slot->object);
        return;
    }
    if (++slot->age > IP_SHOT_MAX_AGE)
    {
        slot->active = 0;
        hide_object(slot->object);
        return;
    }
    slot->velocity[1] += IP_SHOT_GRAVITY;
    slot->position[0] += slot->velocity[0];
    slot->position[1] += slot->velocity[1];
    slot->position[2] += slot->velocity[2];
    dx = slot->position[0] - slot->origin[0];
    dy = slot->position[1] - slot->origin[1];
    dz = slot->position[2] - slot->origin[2];
    if (dx * dx + dy * dy + dz * dz > IP_SHOT_MAX_DISTANCE_SQ)
    {
        slot->active = 0;
        hide_object(slot->object);
        return;
    }
    IP_WRITE_F32(slot->object, 0x08, slot->position[0]);
    IP_WRITE_F32(slot->object, 0x0C, slot->position[1]);
    IP_WRITE_F32(slot->object, 0x10, slot->position[2]);
    IP_WRITE_U8(slot->object, 0x05, IP_SHOT_MODE);
    show_object(slot->object);
}

static int cursor_ensure(ImpactCursorSlot *slot)
{
    void *base = func_800141C4_14DC4(IP_ASSET_FILE);
    if (!pointer_valid(base))
        return 0;
    if (!owned_task(slot->task, impact_cursor_update))
    {
        slot->task = 0;
        slot->object = 0;
    }
    if (!slot->task)
        slot->task = func_80034E08_35A08(s_manager, impact_cursor_update, 0);
    if (!slot->task)
        return 0;
    if (!pointer_valid(slot->object))
    {
        slot->object = func_8000DBF0_E7F0(
            slot->task, IP_CURSOR_MODEL,
            (unsigned int)(unsigned long)D_8020A728_635B08,
            0.0f, 0.0f, 0.0f, 0, 0, 0,
            IP_CURSOR_SCALE, IP_CURSOR_SCALE, IP_CURSOR_SCALE,
            (short)IP_ASSET_FILE, 0);
        if (pointer_valid(slot->object))
        {
            unsigned int address = (unsigned int)(unsigned long)base;
            clear_combat_fields(slot->task);
            /* Match the native segment-8 resource binding. This mesh has no
             * screen-flash texture animation or billboard rotations. */
            if (pointer_valid(base) && address != 0xffffffffu)
                IP_WRITE_U32(slot->object, 0x38, address & 0xbfffffffu);
            hide_object(slot->object);
        }
    }
    return pointer_valid(slot->object);
}

static int shot_ensure(ImpactShotSlot *slot)
{
    if (!pointer_valid(func_800141C4_14DC4(IP_ASSET_FILE)))
        return 0;
    if (!owned_task(slot->task, impact_shot_update))
    {
        slot->task = 0;
        slot->object = 0;
    }
    if (!slot->task)
        slot->task = func_80034E08_35A08(s_manager, impact_shot_update, 0);
    if (!slot->task)
        return 0;
    if (!pointer_valid(slot->object))
    {
        slot->object = func_8000DBF0_E7F0(
            slot->task, IP_SHOT_MODEL,
            (unsigned int)(unsigned long)D_8020A7D0_635BB0,
            slot->position[0], slot->position[1], slot->position[2],
            (short)0x8000, (short)0x8000, (short)0x8000,
            IP_SHOT_SCALE, IP_SHOT_SCALE, IP_SHOT_SCALE,
            (short)IP_ASSET_FILE, 0);
        if (pointer_valid(slot->object))
            clear_combat_fields(slot->task);
    }
    return pointer_valid(slot->object);
}

static void impact_cursor_update(void *task, void *object)
{
    int index;
    (void)object;
    for (index = 0; index < IP_CURSOR_MAX; ++index)
    {
        if (s_cursors[index].task != task)
            continue;
        cursor_render(&s_cursors[index]);
        return;
    }
}

static void impact_shot_update(void *task, void *object)
{
    int index;
    (void)object;
    for (index = 0; index < IP_SHOT_MAX; ++index)
    {
        if (s_shots[index].task != task)
            continue;
        shot_simulate(&s_shots[index]);
        return;
    }
}

static void release_cursor(ImpactCursorSlot *slot)
{
    hide_slot_object(slot->task, slot->object, impact_cursor_update);
    refresh_handle(&slot->task, &slot->object, impact_cursor_update);
    slot->cid = slot->session = 0;
    slot->active = slot->seen = 0;
}

static void release_shot(ImpactShotSlot *slot)
{
    hide_slot_object(slot->task, slot->object, impact_shot_update);
    refresh_handle(&slot->task, &slot->object, impact_shot_update);
    slot->cid = slot->session = slot->sequence = 0;
    slot->active = 0;
}

void anchor_impact_players_reset(void)
{
    int index;
    for (index = 0; index < IP_CURSOR_MAX; ++index)
        release_cursor(&s_cursors[index]);
    for (index = 0; index < IP_SHOT_MAX; ++index)
        release_shot(&s_shots[index]);
    /* Keep live retained handles across pause/reconnect. Discarding them
     * leaks another hidden subtree on every pause. The manager-change path
     * below explicitly drops handles after hiding the previous pool. */
    s_pending_head = s_pending_count = 0;
    s_accepted = 0;
    s_ctor_bracket = s_ctor_captured = 0;
    s_ctor_velocity_task = 0;
    for (index = 0; index < IP_CURSOR_MAX; ++index)
    {
        s_controls[index].cid = s_controls[index].session = 0;
        s_controls[index].held = s_controls[index].pressed = 0;
        s_controls[index].age = 0;
    }
    s_input_tick = s_input_held = 0;
    s_controls_owner = 0;
    s_scope_stage = s_scope_encounter = s_scope_visit = 0;
}

void anchor_impact_players_set_authority(unsigned int owner, unsigned int term)
{
    if (owner != s_authority || term != s_authority_term)
        anchor_impact_players_reset();
    s_authority = owner;
    s_authority_term = term;
}

static void receive_controls(const unsigned int *row)
{
    int i, free_slot = -1;
    if (!anchor_impact_native_is_owner() || !s_authority_term ||
        row[8] != s_authority || row[9] != s_authority_term)
        return;
    for (i = 0; i < IP_CURSOR_MAX; ++i)
    {
        if (s_controls[i].cid == row[0] && s_controls[i].session == row[1]) break;
        if (!s_controls[i].cid || s_controls[i].age > 9) free_slot = i;
    }
    if (i == IP_CURSOR_MAX) i = free_slot;
    if (i < 0) return;
    if (s_controls[i].cid != row[0] || s_controls[i].session != row[1])
        s_controls[i].pressed = 0;
    s_controls[i].cid = row[0];
    s_controls[i].session = row[1];
    s_controls[i].held = (unsigned short)(row[4] & 0xE03Fu);
    s_controls[i].pressed |= (unsigned short)(row[5] & 0xE03Fu);
    s_controls[i].rx = (unsigned short)row[6];
    s_controls[i].ry = (unsigned short)row[7];
    s_controls[i].age = 0;
}

/* The native action interpreter reads held/pressed at system+3B07A/+3B07C,
 * stride 0x18. Keep each native cursor's analog aim local. Only the elected
 * owner interprets combat controls; that creates one mech action/shot and
 * one ammunition debit. Restore the actual controller state on return. */
RECOMP_HOOK("func_801D7670_602A50")
void anchor_impact_controls_begin(int primary, int secondary)
{
    unsigned int held, pressed, peer_held = 0, peer_pressed = 0;
    void *cursor, *task, *state = D_8020EED0_63A2B0;
    ImpactPending *pending;
    ImpactControls *aim = 0;
    int i;
    (void)secondary;
    s_input_swapped = s_aim_swapped = 0;
    if (!anchor_impact_native_ready() || !pointer_valid(state) ||
        !pointer_valid(D_8015C5C8_15D1C8) || primary < 0 || primary > 1) return;
    s_input_system = D_8015C5C8_15D1C8;
    s_input_state = state;
    held = IP_READ_U16(s_input_system, 0x3B07A + primary * 0x18);
    pressed = IP_READ_U16(s_input_system, 0x3B07C + primary * 0x18);
    task = IP_READ_PTR(state, 0x00);
    cursor = linked(task) ? IP_READ_PTR(task, 0x18) : 0;
    if (!anchor_impact_native_is_owner() && s_authority && s_authority_term &&
        (pressed || held != s_input_held || ++s_input_tick >= 6u) &&
        pointer_valid(cursor) && s_pending_count < IP_PENDING_MAX)
    {
        pending = &s_pending[(s_pending_head + s_pending_count) % IP_PENDING_MAX];
        pending->sequence = s_next_sequence++;
        pending->row[0] = pending->sequence;
        pending->row[1] = 2;
        pending->row[2] = held & 0xE03Fu;
        pending->row[3] = pressed & 0xE03Fu;
        pending->row[4] = IP_READ_U16(cursor, 0x14);
        pending->row[5] = IP_READ_U16(cursor, 0x16);
        pending->row[6] = s_authority;
        pending->row[7] = s_authority_term;
        ++s_pending_count;
        s_input_tick = 0;
        s_input_held = held;
    }
    for (i = 0; i < 2; ++i)
    {
        s_saved_pad[i][0] = IP_READ_U16(s_input_system, 0x3B07A + i * 0x18);
        s_saved_pad[i][1] = IP_READ_U16(s_input_system, 0x3B07C + i * 0x18);
    }
    s_input_swapped = 1;
    if (!anchor_impact_native_is_owner())
    {
        for (i = 0; i < 2; ++i)
        {
            IP_WRITE_U16(s_input_system, 0x3B07A + i * 0x18, 0);
            IP_WRITE_U16(s_input_system, 0x3B07C + i * 0x18, 0);
        }
        IP_WRITE_U8(state, 0x141, 0);
        IP_WRITE_U8(state, 0x145, 0);
        return;
    }
    for (i = 0; i < IP_CURSOR_MAX; ++i)
    {
        ImpactControls *control = &s_controls[i];
        if (!control->cid || control->age > 9) continue;
        peer_held |= control->held;
        peer_pressed |= control->pressed;
        if (control->pressed && (!aim || control->cid < aim->cid)) aim = control;
    }
    IP_WRITE_U16(s_input_system, 0x3B07A + primary * 0x18, held | peer_held);
    IP_WRITE_U16(s_input_system, 0x3B07C + primary * 0x18, pressed | peer_pressed);
    /* Ryo aims through the native shooting cursor at shared+0x0C. */
    task = IP_READ_PTR(state, 0x0C);
    cursor = linked(task) ? IP_READ_PTR(task, 0x18) : 0;
    if (!pressed && aim && pointer_valid(cursor))
    {
        int yaw = ((int)aim->ry - 512) & 1023;
        int pitch = aim->rx & 1023;
        if (yaw > 511) yaw -= 1024;
        if (pitch > 511) pitch -= 1024;
        s_input_cursor = cursor;
        s_saved_aim[0] = IP_READ_U32(state, 0x04);
        s_saved_aim[1] = IP_READ_U32(state, 0x08);
        s_saved_rotation[0] = IP_READ_U16(cursor, 0x14);
        s_saved_rotation[1] = IP_READ_U16(cursor, 0x16);
        IP_WRITE_F32(state, 0x04, (float)yaw * (360.0f / 1024.0f));
        IP_WRITE_F32(state, 0x08, (float)pitch * (360.0f / 1024.0f));
        IP_WRITE_U16(cursor, 0x14, aim->rx);
        IP_WRITE_U16(cursor, 0x16, aim->ry);
        s_aim_swapped = 1;
    }
    for (i = 0; i < IP_CURSOR_MAX; ++i) s_controls[i].pressed = 0;
}

RECOMP_HOOK_RETURN("func_801D7670_602A50")
void anchor_impact_controls_end(void)
{
    int i;
    if (s_input_swapped && s_input_system == D_8015C5C8_15D1C8)
        for (i = 0; i < 2; ++i)
        {
            IP_WRITE_U16(s_input_system, 0x3B07A + i * 0x18, s_saved_pad[i][0]);
            IP_WRITE_U16(s_input_system, 0x3B07C + i * 0x18, s_saved_pad[i][1]);
        }
    if (s_aim_swapped && s_input_state == D_8020EED0_63A2B0)
    {
        IP_WRITE_U32(s_input_state, 0x04, s_saved_aim[0]);
        IP_WRITE_U32(s_input_state, 0x08, s_saved_aim[1]);
        IP_WRITE_U16(s_input_cursor, 0x14, s_saved_rotation[0]);
        IP_WRITE_U16(s_input_cursor, 0x16, s_saved_rotation[1]);
    }
    s_input_swapped = s_aim_swapped = 0;
}

static void enqueue_shot(const float position[3], const float velocity[3])
{
    ImpactPending *pending;
    if (s_pending_count >= IP_PENDING_MAX)
        return;
    pending = &s_pending[(s_pending_head + s_pending_count) % IP_PENDING_MAX];
    pending->sequence = s_next_sequence++;
    pending->row[0] = pending->sequence;
    pending->row[1] = IP_SHOT_KIND;
    pending->row[2] = float_bits(position[0]);
    pending->row[3] = float_bits(position[1]);
    pending->row[4] = float_bits(position[2]);
    pending->row[5] = float_bits(velocity[0]);
    pending->row[6] = float_bits(velocity[1]);
    pending->row[7] = float_bits(velocity[2]);
    ++s_pending_count;
}

/* Capture a successful native Ryo shot. func_801DAFCC is the shared shot
 * constructor used by both the action dispatcher and func_801D8184. The task
 * is only visible through func_8000E39C, so the constructor is bracketed and
 * the callback identity is checked before the object position and the just
 * written task velocity are read. */
RECOMP_HOOK("func_801DAFCC_6063AC")
void anchor_impact_shot_constructor_begin(void)
{
    s_ctor_bracket = 1;
    s_ctor_captured = 0;
    s_ctor_velocity_task = 0;
}

RECOMP_HOOK_RETURN("func_801DAFCC_6063AC")
void anchor_impact_shot_constructor_end(void)
{
    if (s_ctor_captured)
        enqueue_shot(s_ctor_position, s_ctor_velocity);
    s_ctor_bracket = 0;
    s_ctor_captured = 0;
    s_ctor_velocity_task = 0;
}

RECOMP_HOOK("func_8000E39C_EF9C")
void anchor_impact_shot_velocity_begin(float speed, float x, float y, float z,
                                       void *task)
{
    (void)speed; (void)x; (void)y; (void)z;
    if (!s_ctor_bracket || !linked(task))
        return;
    if (IP_READ_PTR(task, 0x0C) != (void *)(unsigned long)func_801DB200_6065E0)
        return;
    s_ctor_velocity_task = task;
}

RECOMP_HOOK_RETURN("func_8000E39C_EF9C")
void anchor_impact_shot_velocity_end(void)
{
    void *task = s_ctor_velocity_task;
    void *object;
    if (!task)
        return;
    s_ctor_velocity_task = 0;
    if (!linked(task))
        return;
    object = IP_READ_PTR(task, 0x18);
    if (!pointer_valid(object))
        return;
    s_ctor_position[0] = IP_READ_F32(object, 0x08);
    s_ctor_position[1] = IP_READ_F32(object, 0x0C);
    s_ctor_position[2] = IP_READ_F32(object, 0x10);
    s_ctor_velocity[0] = IP_READ_F32(task, 0x70);
    s_ctor_velocity[1] = IP_READ_F32(task, 0x74);
    s_ctor_velocity[2] = IP_READ_F32(task, 0x78);
    if (finite_float(s_ctor_position[0]) && finite_float(s_ctor_position[1]) &&
        finite_float(s_ctor_position[2]) && finite_float(s_ctor_velocity[0]) &&
        finite_float(s_ctor_velocity[1]) && finite_float(s_ctor_velocity[2]))
        s_ctor_captured = 1;
}

static void apply_cursors(const AnchorImpactPlayerStatus *status)
{
    int index, found;
    for (index = 0; index < IP_CURSOR_MAX; ++index)
        s_cursors[index].seen = 0;
    for (index = 0; index < (int)status->cursor_count; ++index)
    {
        const unsigned int *row = status->cursors[index];
        ImpactCursorSlot *slot = 0;
        for (found = 0; found < IP_CURSOR_MAX; ++found)
        {
            if (s_cursors[found].cid == row[0] &&
                s_cursors[found].session == row[1])
            {
                slot = &s_cursors[found];
                break;
            }
        }
        if (!slot)
        {
            for (found = 0; found < IP_CURSOR_MAX; ++found)
            {
                if (!s_cursors[found].seen && !s_cursors[found].cid)
                {
                    slot = &s_cursors[found];
                    break;
                }
            }
        }
        if (!slot)
            continue;
        slot->cid = row[0];
        slot->session = row[1];
        slot->active = row[3] == 1u;
        slot->position[0] = bits_float(row[4]);
        slot->position[1] = bits_float(row[5]);
        slot->position[2] = bits_float(row[6]);
        slot->rotation[0] = (unsigned short)row[7];
        slot->rotation[1] = (unsigned short)row[8];
        slot->rotation[2] = (unsigned short)row[9];
        slot->seen = 1;
        if ((slot->active && cursor_ensure(slot)) || slot->object)
            cursor_render(slot);
    }
    for (index = 0; index < IP_CURSOR_MAX; ++index)
    {
        if (!s_cursors[index].seen && s_cursors[index].cid)
        {
            s_cursors[index].active = 0;
            hide_slot_object(s_cursors[index].task, s_cursors[index].object,
                             impact_cursor_update);
            s_cursors[index].cid = s_cursors[index].session = 0;
        }
    }
}

static ImpactShotSlot *shot_lookup(unsigned int cid, unsigned int session,
                                   unsigned int sequence)
{
    int index;
    for (index = 0; index < IP_SHOT_MAX; ++index)
        if (s_shots[index].cid == cid && s_shots[index].session == session &&
            s_shots[index].sequence == sequence)
            return &s_shots[index];
    return 0;
}

static void apply_attacks(const AnchorImpactPlayerStatus *status)
{
    int index, found;
    for (index = 0; index < (int)status->attack_count; ++index)
    {
        const unsigned int *row = status->attacks[index];
        if (row[3] == 2u)
        {
            receive_controls(row);
            continue;
        }
        ImpactShotSlot *slot = shot_lookup(row[0], row[1], row[2]);
        if (slot && slot->active)
            continue;
        if (!slot)
        {
            for (found = 0; found < IP_SHOT_MAX; ++found)
            {
                if (!s_shots[found].active)
                {
                    slot = &s_shots[found];
                    break;
                }
            }
        }
        if (!slot)
            continue;
        slot->cid = row[0];
        slot->session = row[1];
        slot->sequence = row[2];
        slot->active = 1;
        slot->age = 0;
        slot->position[0] = bits_float(row[4]);
        slot->position[1] = bits_float(row[5]);
        slot->position[2] = bits_float(row[6]);
        slot->origin[0] = slot->position[0];
        slot->origin[1] = slot->position[1];
        slot->origin[2] = slot->position[2];
        slot->velocity[0] = bits_float(row[7]);
        slot->velocity[1] = bits_float(row[8]);
        slot->velocity[2] = bits_float(row[9]);
        if (shot_ensure(slot))
            shot_simulate(slot);
    }
}

void anchor_impact_players_tick(int active)
{
    AnchorImpactPlayerSample sample;
    AnchorImpactPlayerStatus status;
    char json_out[ANCHOR_IMPACT_PLAYER_JSON_SIZE];
    char *json;
    void *state, *manager, *cursor_task, *cursor_object;
    unsigned int stage, encounter, visit;
    int index, count;

    if (!active)
    {
        anchor_impact_players_reset();
        return;
    }
    state = D_8020EED0_63A2B0;
    if (!pointer_valid(state))
    {
        anchor_impact_players_reset();
        return;
    }
    manager = IP_READ_PTR(state, IP_STATE_MANAGER);
    if (!pointer_valid(manager))
    {
        anchor_impact_players_reset();
        return;
    }
    stage = anchor_impact_native_stage();
    encounter = anchor_impact_native_encounter();
    visit = anchor_impact_native_visit();
    if (stage != s_scope_stage || encounter != s_scope_encounter || visit != s_scope_visit)
    {
        anchor_impact_players_reset();
    }
    if (manager != s_manager)
    {
        anchor_impact_players_reset();
        for (index = 0; index < IP_CURSOR_MAX; ++index)
            s_cursors[index].task = s_cursors[index].object = 0;
        for (index = 0; index < IP_SHOT_MAX; ++index)
            s_shots[index].task = s_shots[index].object = 0;
        s_manager = manager;
    }
    s_scope_stage = stage;
    s_scope_encounter = encounter;
    s_scope_visit = visit;
    /* Expire on every frame, including followers. A promotion must never
     * replay presses accumulated under the former owner. */
    for (index = 0; index < IP_CURSOR_MAX; ++index)
    {
        ImpactControls *control = &s_controls[index];
        if (s_controls_owner != anchor_impact_native_is_owner() ||
            (control->cid && ++control->age > 9))
        {
            control->cid = control->session = 0;
            control->held = control->pressed = 0;
        }
    }
    s_controls_owner = anchor_impact_native_is_owner();

    for (index = 0; index < 7; ++index)
        sample.cursor[index] = 0;
    cursor_task = IP_READ_PTR(state, IP_STATE_CURSOR);
    cursor_object = linked(cursor_task) ? IP_READ_PTR(cursor_task, 0x18) : 0;
    if (pointer_valid(cursor_object) &&
        IP_READ_U32(cursor_object, 0x2C) == IP_CURSOR_MODEL &&
        !(IP_READ_U8(cursor_object, 0x64) & 1u))
    {
        sample.cursor[0] = 1;
        for (index = 0; index < 3; ++index)
        {
            sample.cursor[1 + index] = float_bits(
                IP_READ_F32(cursor_object, 0x08 + index * 4));
            sample.cursor[4 + index] = IP_READ_U16(cursor_object, 0x14 + index * 2);
        }
    }
    for (index = 0; index < 3; ++index)
    {
        if (finite_word(sample.cursor[index + 1]))
            continue;
        /* Position words must remain finite even when hidden. */
        sample.cursor[0] = 0;
        for (index = 1; index < 7; ++index)
            sample.cursor[index] = 0;
        break;
    }

    sample.attack_count = 0;
    for (index = 0; index < s_pending_count; ++index)
    {
        ImpactPending *pending =
            &s_pending[(s_pending_head + index) % IP_PENDING_MAX];
        if (pending->sequence <= s_accepted)
            continue;
        if (sample.attack_count >= 16u)
            break;
        for (count = 0; count < 8; ++count)
            sample.attacks[sample.attack_count][count] = pending->row[count];
        ++sample.attack_count;
    }
    if (!anchor_impact_players_encode(&sample, json_out,
                                      (unsigned int)sizeof(json_out)))
        return;
    json = anchor_impact_players_update(1, stage, encounter, visit, json_out);
    if (!json)
        return;
    if (!anchor_impact_players_decode(json, &status))
    {
        recomp_free(json);
        return;
    }
    recomp_free(json);
    s_accepted = status.accepted;
    while (s_pending_count > 0 &&
           s_pending[s_pending_head].sequence <= s_accepted)
    {
        s_pending_head = (s_pending_head + 1) % IP_PENDING_MAX;
        --s_pending_count;
    }
    apply_cursors(&status);
    apply_attacks(&status);
}

/* The authoritative graph includes actual shot lifetime and impact effects. */
void anchor_impact_players_hide_shots(void)
{
    int i;
    for (i = 0; i < IP_SHOT_MAX; ++i)
        hide_slot_object(s_shots[i].task, s_shots[i].object, impact_shot_update);
}
