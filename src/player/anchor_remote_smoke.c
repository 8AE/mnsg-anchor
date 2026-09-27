/**
 * @file anchor_remote_smoke.c
 * @brief Bounded, visual-only copies of the native switch and Sasuke smoke.
 *
 * File_11 selector 3 (func_801EA860_5A6770) creates six common-file smoke
 * records. Its update aims through the global local-player target, so it
 * cannot be replayed for a remote. Selector 0x16 (func_801EDC08_5A9B18)
 * creates two seven-puff jet emitters. Its callback writes local player work
 * and queues a global sound stop. This module creates only kind-2 display
 * records and drives their visual state from the remote render object.
 */

#include "player/anchor_remote_smoke.h"

#define SMOKE_FILE 0x80u
#define SMOKE_TEXTURE_FILE 0x152u
#define SWITCH_MODEL 0x490024f0u
#define JET_MODEL 0x490026b0u
#define ACTION_SWITCH 0xbau
#define ACTION_JETPACK 0x9bu
#define CHARACTER_SASUKE 2
#define SWITCH_RETRY_FRAMES 60
#define MATERIAL_WORDS 8
#define MATERIAL_BYTES (MATERIAL_WORDS * sizeof(unsigned int))
#define MATERIAL_TOTAL_BYTES \
    (ANCHOR_REMOTE_SMOKE_BANK_LIMIT * ANCHOR_REMOTE_SMOKE_PARTS * 2 * MATERIAL_BYTES)
#if DEBUG_BUTTON_ENABLED && !defined(ANCHOR_REMOTE_SMOKE_HOST_TEST)
extern void recomp_printf(const char *, ...);
#define SMOKE_LOG(...) recomp_printf(__VA_ARGS__)
#else
#define SMOKE_LOG(...) ((void)0)
#endif

#define SMOKE_DIAG_JET_STARTED 0x40u

typedef struct SmokeSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} SmokeSceneResource;

extern SmokeSceneResource D_80167FC0_168BC0[48];
extern short D_800C7A72_C8672;
extern void *func_80013B14_14714(unsigned int file_id);
extern void *func_800141C4_14DC4(unsigned int file_id);
extern void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                                 unsigned short flags);
extern void func_80034EF8_35AF8(void *task);
extern void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                                float x, float y, float z,
                                short rx, short ry, short rz,
                                float sx, float sy, float sz,
                                short file8, short file9);
extern void func_80033898_34498(unsigned short rx, unsigned short ry,
                                unsigned short rz, float *x, float *y, float *z);
extern void func_801E8820_5A4730(void *object, unsigned short segment,
                                  const void *descriptor);
extern void func_801E8858_5A4768(void *object, unsigned short segment);
extern const void *D_80204A10_5C0920[];
extern const unsigned char D_80204AD0_5C09E0[];

static const float s_switch_x[ANCHOR_REMOTE_SMOKE_SWITCH_PARTS] =
    {0.0f, 0.0f, 12.5f, -12.5f, -32.5f, 0.0f};
static const float s_switch_y[ANCHOR_REMOTE_SMOKE_SWITCH_PARTS] =
    {45.0f, 75.0f, 45.0f, 15.0f, 45.0f, 130.0f};
static const float s_switch_vy[ANCHOR_REMOTE_SMOKE_SWITCH_PARTS] =
    {4.0f, 6.0f, 2.5f, 2.0f, 5.0f, 2.5f};
static const float s_switch_second_vx[ANCHOR_REMOTE_SMOKE_SWITCH_PARTS] =
    {0.0f, 2.5f, 2.0f, -2.5f, -10.0f, 0.0f};
static const unsigned char s_switch_gray[ANCHOR_REMOTE_SMOKE_SWITCH_PARTS] =
    {0xff, 0x80, 0x90, 0xff, 0xc0, 0xff};
static const float s_jet_x[2] = {15.0f, -15.0f};

static AnchorRemoteSmokeState *s_bank_owner[ANCHOR_REMOTE_SMOKE_BANK_LIMIT];
static unsigned int *s_material_arena;
static unsigned char *s_common;
static unsigned char *s_texture;
static unsigned int s_arena_generation;

#if DEBUG_BUTTON_ENABLED && !defined(ANCHOR_REMOTE_SMOKE_HOST_TEST)
static void log_first_failure(AnchorRemoteSmokeState *state, int index,
                              int reason)
{
    unsigned int bit = 1u << ((index >= ANCHOR_REMOTE_SMOKE_SWITCH_PARTS ? 3 : 0) +
                              reason);
    static const char *reasons[3] = {"bank", "task", "object"};
    if (state->diagnostic_flags & bit)
        return;
    state->diagnostic_flags |= bit;
    SMOKE_LOG("[remote_smoke] %s first %s failure cid=%d room=%u epoch=%d\n",
              index >= ANCHOR_REMOTE_SMOKE_SWITCH_PARTS ? "jet" : "switch",
              reasons[reason], state->cid, state->room, state->epoch);
}
#define LOG_FIRST_FAILURE(state, index, reason) \
    log_first_failure((state), (index), (reason))
#else
#define LOG_FIRST_FAILURE(state, index, reason) ((void)0)
#endif

#ifdef ANCHOR_REMOTE_SMOKE_HOST_TEST
static unsigned int s_host_material_arena[MATERIAL_TOTAL_BYTES / sizeof(unsigned int)];
#endif

#ifndef ANCHOR_REMOTE_SMOKE_HOST_TEST
static unsigned int physical(unsigned int value)
{
    return value & 0x1fffffffu;
}
#endif

static int native_pointer(const void *pointer)
{
#ifdef ANCHOR_REMOTE_SMOKE_HOST_TEST
    return pointer && pointer != (void *)-1;
#else
    unsigned int address = physical((unsigned int)(unsigned long)pointer);
    return address >= 0x1000u && address < 0x800000u;
#endif
}

static unsigned char *resident(unsigned int file_id)
{
    void *pointer = func_800141C4_14DC4(file_id);
    if (!native_pointer(pointer))
        return 0;
#ifdef ANCHOR_REMOTE_SMOKE_HOST_TEST
    return (unsigned char *)pointer;
#else
    return (unsigned char *)(unsigned long)
        (physical((unsigned int)(unsigned long)pointer) | 0x80000000u);
#endif
}

static void smoke_task_update(void *task, void *object)
{
    (void)task;
    (void)object;
}

static void write_pointer(void *record, unsigned int offset, const void *pointer)
{
    __builtin_memcpy((unsigned char *)record + offset, &pointer, sizeof(pointer));
}

static void hide_object(void *object)
{
    if (!object)
        return;
    *(unsigned int *)((unsigned char *)object + 0x2c) = 0;
    ((unsigned char *)object)[0x64] |= 1u;
}

void anchor_remote_smoke_reset(AnchorRemoteSmokeState *state, int owner_live)
{
    int i;
    if (!state)
        return;
    if (state->task && owner_live)
        func_80034EF8_35AF8(state->task);
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT; ++i)
        if (s_bank_owner[i] == state)
            s_bank_owner[i] = 0;
    state->owner_task = 0;
    state->owner_object = 0;
    state->task = 0;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
    {
        state->object[i] = 0;
        state->particle[i].active = 0;
    }
    state->cid = 0;
    state->session = 0;
    state->epoch = 0;
    state->last_action = -1;
    state->last_ch = -1;
    state->jet_tick = 0;
    state->bank = -1;
    state->room = 0;
    state->initialized = 0;
    state->switch_pending = 0;
    state->switch_retry = 0;
    state->switch_await_ch = 0;
    state->switch_await_action = 0;
    state->release_idle = 0;
    state->suspended = 0;
    state->material_bank = 0;
    state->diagnostic_flags = 0;
    state->diagnostic_switch_starts = 0;
    state->arena_generation = s_arena_generation;
}

void anchor_remote_smoke_load_resources(void)
{
    int i;
#ifndef ANCHOR_REMOTE_SMOKE_HOST_TEST
    unsigned int start, end;
#endif

    /* Called after native stage resources, never from a per-frame callback. */
    ++s_arena_generation;
    s_material_arena = 0;
    s_common = 0;
    s_texture = 0;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT; ++i)
        s_bank_owner[i] = 0;
    func_80013B14_14714(SMOKE_FILE);
    func_80013B14_14714(SMOKE_TEXTURE_FILE);
    s_common = resident(SMOKE_FILE);
    s_texture = resident(SMOKE_TEXTURE_FILE);
    if (!s_common || !s_texture)
    {
        SMOKE_LOG("[remote_smoke] stage resources unavailable common=%d texture=%d\n",
                  s_common != 0, s_texture != 0);
        return;
    }
#ifdef ANCHOR_REMOTE_SMOKE_HOST_TEST
    s_material_arena = s_host_material_arena;
#else
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !native_pointer(D_80167FC0_168BC0[i].data))
    {
        SMOKE_LOG("[remote_smoke] stage material arena unavailable slot=%d\n", i);
        return;
    }
    start = physical((unsigned int)(unsigned long)D_80167FC0_168BC0[i].data);
    start = ((start + 15u) & ~15u) | 0x80000000u;
    end = start + MATERIAL_TOTAL_BYTES;
    if (end < start || end > 0x80800000u)
    {
        SMOKE_LOG("[remote_smoke] stage material arena out of range %x..%x\n",
                  start, end);
        return;
    }
    D_80167FC0_168BC0[i].data = (unsigned char *)(unsigned long)end;
    s_material_arena = (unsigned int *)(unsigned long)start;
#endif
    SMOKE_LOG("[remote_smoke] stage ready common=%x texture=%x arena=%x\n",
              (unsigned int)(unsigned long)s_common,
              (unsigned int)(unsigned long)s_texture,
              (unsigned int)(unsigned long)s_material_arena);
}

static int switch_live(const AnchorRemoteSmokeState *state);

static int epoch_is_next(int previous, int current)
{
    return current == (previous == 0x7fffffff ? 1 : previous + 1);
}

static int jet_emission_active(const AnchorPlayerModelRemote *remote)
{
    return remote->ch == CHARACTER_SASUKE &&
        remote->action == ACTION_JETPACK &&
        (remote->jet_velocity_100 == ANCHOR_REMOTE_JET_SPEED_UNAVAILABLE ||
         (remote->jet_velocity_100 >= 0 && remote->jet_velocity_100 <= 10000));
}

void anchor_remote_smoke_set_context(AnchorRemoteSmokeState *state,
    void *remote_task, void *remote_object,
    const AnchorPlayerModelRemote *remote, unsigned short room)
{
    int i;
    int same_peer;
    int same_presence;
    int same_owner;
    int was_initialized;
    int action_edge, character_edge, carry_event;
    int last_action, last_ch;
    unsigned char switch_pending, switch_retry, switch_await_ch;
    unsigned char switch_await_action, release_idle;
    if (!state || !remote || !remote_task || !remote_object)
        return;
    if (state->initialized && state->arena_generation == s_arena_generation &&
        state->owner_task == remote_task && state->owner_object == remote_object &&
        state->cid == remote->cid &&
        state->session == remote->interaction_session &&
        state->epoch == remote->player_epoch && state->room == room)
    {
        /* The switch burst and departing jet puffs have finite lifetimes.
         * Reclaim their display task at frame end so another peer can use
         * one of the two shared smoke banks. Keep a live jet emitter while
         * Sasuke remains in action 0x9B, including its quiet alternate tick. */
        if (state->task && (!state->switch_pending || state->suspended) &&
            (state->release_idle || state->suspended ||
             !jet_emission_active(remote)))
        {
            int live = 0;
            for (i = 0; i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
                if (state->particle[i].active)
                    live = 1;
            if (!live)
            {
                func_80034EF8_35AF8(state->task);
                state->task = 0;
                for (i = 0; i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
                    state->object[i] = 0;
                if (state->bank >= 0 &&
                    state->bank < ANCHOR_REMOTE_SMOKE_BANK_LIMIT &&
                    s_bank_owner[state->bank] == state)
                    s_bank_owner[state->bank] = 0;
                state->bank = -1;
                state->suspended = 0;
                state->release_idle = 0;
            }
        }
        if (!state->task && (state->release_idle || state->suspended))
        {
            if (state->bank >= 0 &&
                state->bank < ANCHOR_REMOTE_SMOKE_BANK_LIMIT &&
                s_bank_owner[state->bank] == state)
                s_bank_owner[state->bank] = 0;
            state->bank = -1;
            state->release_idle = 0;
        }
        if (!state->task)
            state->suspended = 0;
        return;
    }
    was_initialized = state->initialized;
    same_presence = was_initialized &&
        state->arena_generation == s_arena_generation &&
        state->cid == remote->cid &&
        state->session == remote->interaction_session &&
        state->room == room;
    same_peer = same_presence && state->epoch == remote->player_epoch;
    same_owner = state->owner_task == remote_task &&
        state->owner_object == remote_object;
    last_action = state->last_action;
    last_ch = state->last_ch;
    switch_pending = state->switch_pending;
    switch_retry = state->switch_retry;
    switch_await_ch = state->switch_await_ch;
    switch_await_action = state->switch_await_action;
    release_idle = state->release_idle;
    action_edge = remote->action == ACTION_SWITCH && last_action != ACTION_SWITCH;
    character_edge = remote->ch != last_ch;
    carry_event = same_presence && (action_edge || character_edge ||
        (switch_pending && remote->action == ACTION_SWITCH));

    /* Native switching crosses two alive-state epoch edges while action BA
     * remains active on the same player task. Retain only this in-flight,
     * same-owner visual across the next epoch; plain epoch changes still
     * invalidate every smoke child and pending event. */
    if (same_presence && !same_peer && same_owner &&
        epoch_is_next(state->epoch, remote->player_epoch) &&
        (switch_live(state) || switch_pending) &&
        (remote->action == ACTION_SWITCH ||
         (character_edge && switch_await_ch)))
    {
        state->epoch = remote->player_epoch;
        for (i = ANCHOR_REMOTE_SMOKE_SWITCH_PARTS;
             i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
        {
            state->particle[i].active = 0;
            hide_object(state->object[i]);
        }
        state->jet_tick = 0;
        return;
    }
    anchor_remote_smoke_reset(state,
        state->arena_generation == s_arena_generation &&
        state->owner_task == remote_task);
    state->owner_task = remote_task;
    state->owner_object = remote_object;
    state->cid = remote->cid;
    state->session = remote->interaction_session;
    state->epoch = remote->player_epoch;
    state->room = room;
    /* Rebuilds and newly observed switches retain their prior event edge;
     * initial joins, reconnects, and room transitions seed a new baseline. */
    state->last_action = same_peer || carry_event ? last_action : remote->action;
    state->last_ch = same_peer || carry_event ? last_ch : remote->ch;
    if (same_peer || carry_event)
    {
        state->switch_pending = switch_pending &&
            (same_peer || remote->action == ACTION_SWITCH);
        state->switch_retry = state->switch_pending ? switch_retry : 0;
        state->switch_await_ch = switch_await_ch;
        state->switch_await_action = switch_await_action;
        state->release_idle = same_peer ? release_idle : 0;
    }
    state->initialized = 1;
}

void anchor_remote_smoke_suspend(AnchorRemoteSmokeState *state)
{
    int i;
    int started_switch;
    if (!state || !state->initialized)
        return;
    started_switch = switch_live(state);
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
    {
        state->particle[i].active = 0;
        hide_object(state->object[i]);
    }
    /* Keep a not-yet-started switch queued while the model bind recovers.
     * A started burst is hidden and must not restart on resume; its paired
     * action/character edge suppression remains until the transition ends. */
    if (started_switch)
    {
        state->switch_pending = 0;
        state->switch_retry = 0;
    }
    state->jet_tick = 0;
    state->suspended = 1;
}

static int acquire_bank(AnchorRemoteSmokeState *state)
{
    int i;
    if (state->bank >= 0 && state->bank < ANCHOR_REMOTE_SMOKE_BANK_LIMIT &&
        s_bank_owner[state->bank] == state)
        return 1;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT; ++i)
        if (!s_bank_owner[i])
        {
            s_bank_owner[i] = state;
            state->bank = i;
            return 1;
        }
    return 0;
}

static unsigned int *material_for(const AnchorRemoteSmokeState *state, int index)
{
    return s_material_arena +
        ((state->bank * ANCHOR_REMOTE_SMOKE_PARTS + index) * 2 +
         state->material_bank) * MATERIAL_WORDS;
}

static int ensure_object(AnchorRemoteSmokeState *state, int index)
{
    void *object;
    if (state->object[index])
        return 1;
    if (!s_material_arena || !s_common || !s_texture)
        return 0;
    if (!acquire_bank(state))
    {
        LOG_FIRST_FAILURE(state, index, 0);
        return 0;
    }
    if (!state->task)
        state->task = func_80034E08_35A08(state->owner_task, smoke_task_update, 0);
    if (!state->task)
    {
        LOG_FIRST_FAILURE(state, index, 1);
        return 0;
    }
    object = func_8000DBF0_E7F0(state->task, 0, 0,
                                0, 0, 0, (short)0x8000u,
                                (short)0x8000u, (short)0x8000u,
                                0, 0, 0, 0, 0);
    if (!object)
    {
        LOG_FIRST_FAILURE(state, index, 2);
        return 0;
    }
    state->object[index] = object;
    *(unsigned short *)((unsigned char *)object + 0x3c) = SMOKE_FILE;
    write_pointer(object, 0x40, s_common);
    *(unsigned short *)((unsigned char *)object + 0x44) = SMOKE_TEXTURE_FILE;
    write_pointer(object, 0x48, s_texture);
    func_801E8820_5A4730(object, 10,
        index < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS ?
        D_80204A10_5C0920[index] : D_80204AD0_5C09E0);
    return 1;
}

static void write_material(unsigned int *commands, unsigned char gray,
                           unsigned char alpha)
{
    /* Same environment-color display-list wrapper as 801DC554 with
     * D_802049C0, and the same 0x20000000 renderer style bit. */
    commands[0] = 0x06000000u;
    commands[1] = 0x802049c0u;
    commands[2] = 0xfb000000u;
    commands[3] = ((unsigned int)gray << 24) |
                  ((unsigned int)gray << 16) |
                  ((unsigned int)gray << 8) | alpha;
    commands[4] = 0xb8000000u;
    commands[5] = 0;
    commands[6] = 0;
    commands[7] = 0;
}

static void render_particle(AnchorRemoteSmokeState *state, int index,
                            float x, float y, float z)
{
    AnchorRemoteSmokeParticle *p = &state->particle[index];
    unsigned char *object = state->object[index];
    unsigned int *material;
    unsigned char gray;
    if (!p->active || !object)
    {
        hide_object(object);
        return;
    }
    material = material_for(state, index);
    gray = index < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS ?
        s_switch_gray[index] : 0xffu;
    write_material(material, gray, p->alpha);
    *(unsigned int *)(object + 0x2c) =
        index < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS ? SWITCH_MODEL : JET_MODEL;
    *(unsigned int *)(object + 0x30) =
        (unsigned int)(unsigned long)material | 0x20000000u;
    *(float *)(object + 8) = x;
    *(float *)(object + 0xc) = y;
    *(float *)(object + 0x10) = z;
    /* Native switch and jet smoke keep all three angle halfwords at this
     * camera-facing sentinel; ordinary 10-bit owner yaw is only for offsets. */
    *(unsigned short *)(object + 0x14) = 0x8000u;
    *(unsigned short *)(object + 0x16) = 0x8000u;
    *(unsigned short *)(object + 0x18) = 0x8000u;
    *(float *)(object + 0x1c) = p->scale;
    *(float *)(object + 0x20) = p->scale;
    *(float *)(object + 0x24) = p->scale;
    object[5] = index < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS ? 7u : 10u;
    object[0x64] &= ~1u;
    object[0x65] = 0;
    func_801E8858_5A4768(object, 10);
}

static void owner_position(const void *owner, float lx, float ly, float lz,
                           float *x, float *y, float *z)
{
    const unsigned char *object = (const unsigned char *)owner;
    unsigned short rx = *(const unsigned short *)(object + 0x14) & 0x3ffu;
    unsigned short ry = *(const unsigned short *)(object + 0x16) & 0x3ffu;
    unsigned short rz = *(const unsigned short *)(object + 0x18) & 0x3ffu;
    func_80033898_34498(rx, ry, rz, &lx, &ly, &lz);
    *x = *(const float *)(object + 8) + lx;
    *y = *(const float *)(object + 0xc) + ly;
    *z = *(const float *)(object + 0x10) + lz;
}

static void start_switch(AnchorRemoteSmokeState *state)
{
    const unsigned char *owner = state->owner_object;
    float sx = *(const float *)(owner + 0x1c);
    float sy = *(const float *)(owner + 0x20);
    int i;
    if (!s_material_arena)
        return;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS; ++i)
        if (!ensure_object(state, i))
            return;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS; ++i)
    {
        AnchorRemoteSmokeParticle *p = &state->particle[i];
        p->x = s_switch_x[i] * sx;
        p->y = s_switch_y[i] * sy;
        p->z = 0.0f;
        p->vx = 0.0f;
        p->vy = s_switch_vy[i] * sy;
        p->vz = 0.0f;
        p->scale = 0.03f;
        p->alpha = 0xffu;
        p->phase = 0;
        p->active = 1;
    }
    state->switch_pending = 0;
    state->switch_retry = 0;
    state->release_idle = 0;
    if (state->diagnostic_switch_starts < 4)
    {
        SMOKE_LOG("[remote_smoke] switch started cid=%d room=%u epoch=%d bank=%d task=%x\n",
                  state->cid, state->room, state->epoch, state->bank,
                  (unsigned int)(unsigned long)state->task);
        ++state->diagnostic_switch_starts;
    }
}

static void step_switch(AnchorRemoteSmokeState *state)
{
    const unsigned char *owner = state->owner_object;
    float sx = *(const float *)(owner + 0x1c);
    int i;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS; ++i)
    {
        AnchorRemoteSmokeParticle *p = &state->particle[i];
        float x, y, z;
        if (!p->active)
            continue;
        p->x += p->vx;
        p->y += p->vy;
        p->z += p->vz;
        if (p->phase == 0)
        {
            p->scale += 0.01f;
            if (p->scale >= 0.13f)
            {
                p->phase = 1;
                p->vx = s_switch_second_vx[i] * sx;
                p->vy *= 0.5f;
            }
        }
        else if (p->phase == 1)
        {
            p->scale += 0.002f;
            if (p->scale >= 0.15f)
                p->phase = 2;
        }
        else if (p->alpha <= 0x20u)
            p->active = 0;
        else
            p->alpha -= 0x20u;
        owner_position(owner, p->x, p->y, p->z, &x, &y, &z);
        render_particle(state, i, x, y, z);
    }
}

static void spawn_jet(AnchorRemoteSmokeState *state, int emitter)
{
    const unsigned char *owner = state->owner_object;
    float x = s_jet_x[emitter] * *(const float *)(owner + 0x1c);
    float y = 58.0f * *(const float *)(owner + 0x20);
    float z = -17.0f * *(const float *)(owner + 0x24);
    int i;
    for (i = ANCHOR_REMOTE_SMOKE_SWITCH_PARTS + emitter * 7;
         i < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS + (emitter + 1) * 7; ++i)
    {
        AnchorRemoteSmokeParticle *p = &state->particle[i];
        if (p->active || !ensure_object(state, i))
            continue;
        owner_position(owner, x, y, z, &p->x, &p->y, &p->z);
        p->vx = 0.0f;
        p->vy = -2.0f;
        p->vz = 0.0f;
        p->scale = 0.02f;
        p->alpha = 0xe8u;
        p->phase = 0;
        p->active = 1;
        if (!(state->diagnostic_flags & SMOKE_DIAG_JET_STARTED))
        {
            state->diagnostic_flags |= SMOKE_DIAG_JET_STARTED;
            SMOKE_LOG("[remote_smoke] jet started cid=%d room=%u epoch=%d bank=%d task=%x\n",
                      state->cid, state->room, state->epoch, state->bank,
                      (unsigned int)(unsigned long)state->task);
        }
        return;
    }
}

static void step_jet(AnchorRemoteSmokeState *state, float vertical_drift)
{
    int i;
    for (i = ANCHOR_REMOTE_SMOKE_SWITCH_PARTS;
         i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
    {
        AnchorRemoteSmokeParticle *p = &state->particle[i];
        if (!p->active)
            continue;
        p->vy = vertical_drift;
        p->x += p->vx;
        p->y += p->vy;
        p->z += p->vz;
        p->scale += 0.004f;
        if (p->alpha <= 0x10u)
            p->active = 0;
        else
            p->alpha -= 0x10u;
        render_particle(state, i, p->x, p->y, p->z);
    }
}

static int switch_live(const AnchorRemoteSmokeState *state)
{
    int i;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS; ++i)
        if (state->particle[i].active)
            return 1;
    return 0;
}

void anchor_remote_smoke_apply(AnchorRemoteSmokeState *state,
    void *remote_task, void *remote_object,
    const AnchorPlayerModelRemote *remote, unsigned short room)
{
    void *owned_object;
    int graphics_bank;
    int i;
    int action_edge, character_edge;
    int started_jet = 0;
    int jet_active;
    float jet_drift = -2.0f;
    if (!state || !state->initialized || !remote ||
        state->arena_generation != s_arena_generation ||
        state->owner_task != remote_task || state->owner_object != remote_object ||
        state->cid != remote->cid ||
        state->session != remote->interaction_session ||
        state->epoch != remote->player_epoch || state->room != room ||
        !remote_task || !remote_object)
        return;
    __builtin_memcpy(&owned_object, (const unsigned char *)remote_task + 0x18,
                     sizeof(owned_object));
    if (owned_object != remote_object)
        return;
    graphics_bank = D_800C7A72_C8672;
    if (graphics_bank < 0 || graphics_bank > 1)
    {
        /* No graphics bank is safe to write. Hide the existing draw objects
         * and keep the event edge and particle state for the next valid bank. */
        for (i = 0; i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
            hide_object(state->object[i]);
        return;
    }
    state->material_bank = (unsigned char)graphics_bank;

    action_edge = remote->action == ACTION_SWITCH &&
        state->last_action != ACTION_SWITCH;
    character_edge = remote->ch != state->last_ch;
    if (action_edge && state->switch_await_action)
        state->switch_await_action = 0;
    else if (action_edge && !switch_live(state) && !state->switch_pending)
    {
        state->switch_pending = 1;
        state->switch_retry = SWITCH_RETRY_FRAMES;
        state->switch_await_ch = character_edge ? 0 : SWITCH_RETRY_FRAMES;
        state->release_idle = 0;
    }
    if (character_edge)
    {
        if (state->switch_await_ch)
            state->switch_await_ch = 0;
        else if (!action_edge && !switch_live(state) && !state->switch_pending)
        {
            state->switch_pending = 1;
            state->switch_retry = SWITCH_RETRY_FRAMES;
            state->switch_await_action = SWITCH_RETRY_FRAMES;
            state->release_idle = 0;
        }
    }
    else if (state->switch_await_ch)
        --state->switch_await_ch;
    if (!action_edge && state->switch_await_action)
        --state->switch_await_action;
    if (state->switch_pending)
    {
        start_switch(state);
        if (state->switch_pending && --state->switch_retry == 0)
        {
            state->switch_pending = 0;
            state->release_idle = 1;
        }
    }
    state->last_action = remote->action;
    state->last_ch = remote->ch;
    step_switch(state);

    jet_active = jet_emission_active(remote);
    if (jet_active)
    {
        if (remote->jet_velocity_100 != ANCHOR_REMOTE_JET_SPEED_UNAVAILABLE)
            jet_drift = ((float)remote->jet_velocity_100 / 100.0f -
                         2.56410265f) * 0.5f;
        ++state->jet_tick;
        if ((state->jet_tick & 1) == 0)
        {
            spawn_jet(state, 0);
            spawn_jet(state, 1);
            started_jet = 1;
        }
    }
    else
        state->jet_tick = 0;
    step_jet(state, jet_drift);
    /* A failed allocator must not monopolize one of the two shared banks
     * while an otherwise active jetpack keeps retrying. */
    if (started_jet && !state->switch_pending && !switch_live(state))
    {
        int i;
        int jet_live = 0;
        for (i = ANCHOR_REMOTE_SMOKE_SWITCH_PARTS;
             i < ANCHOR_REMOTE_SMOKE_PARTS; ++i)
            if (state->particle[i].active)
                jet_live = 1;
        if (!jet_live)
            state->release_idle = 1;
        else
            state->release_idle = 0;
    }
}
