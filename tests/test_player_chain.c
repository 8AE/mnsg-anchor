#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
int recomp_printf(const char *format, ...);

typedef void (*ChainCallback)(void *, void *);
typedef struct PointerSlot
{
    void *record;
    unsigned int offset;
    void *value;
} PointerSlot;

static PointerSlot s_pointers[80];
static unsigned int s_pointer_count;
static unsigned char s_player[0x100], s_model[0x100], s_work[0x100];
static unsigned char s_head[0x100], s_head_model[0x100];
static unsigned char s_segment[0x100], s_segment_model[0x100];
static unsigned char s_links[3][8];
static int s_wrong_callback;

static void **pointer_slot(void *record, unsigned int offset)
{
    unsigned int i;
    for (i = 0; i < s_pointer_count; ++i)
        if (s_pointers[i].record == record && s_pointers[i].offset == offset)
            return &s_pointers[i].value;
    assert(s_pointer_count < sizeof(s_pointers) / sizeof(s_pointers[0]));
    s_pointers[s_pointer_count].record = record;
    s_pointers[s_pointer_count].offset = offset;
    s_pointers[s_pointer_count].value = 0;
    return &s_pointers[s_pointer_count++].value;
}

static ChainCallback callback_for(void *record);
#define CHAIN_PTR(record, offset) (*pointer_slot((void *)(record), (offset)))
#define CHAIN_UPDATE(record) callback_for(record)
#include "../src/combat/anchor_player_chain.c"

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
float D_800C7DBC;
static int s_connected, s_loaded, s_scripted, s_epoch, s_target_count;
static int s_latches, s_actions, s_releases, s_sounds, s_heading_calls;
static int s_fail_alloc;
static AnchorPlayerHitTarget s_targets[3];
static float s_latch_xyz[3];
static unsigned int s_diagnostic_logs;
static char s_last_diagnostic[2048];

static void write_float_bits(void *destination, unsigned int bits)
{
    memcpy(destination, &bits, sizeof(bits));
}

int recomp_printf(const char *format, ...)
{
    va_list arguments;
    int count;
    va_start(arguments, format);
    count = vsnprintf(s_last_diagnostic, sizeof(s_last_diagnostic), format, arguments);
    va_end(arguments);
    ++s_diagnostic_logs;
    return count;
}

static ChainCallback callback_for(void *record)
{
    if (s_wrong_callback)
        return 0;
    if (record == s_head)
        return func_801EA0D8_5A5FE8;
    if (record == s_segment)
        return func_801EA4E8_5A63F8;
    return 0;
}

void *recomp_alloc(unsigned long size) { return s_fail_alloc ? 0 : malloc(size); }
void recomp_free(void *memory) { free(memory); }
int anchor_remote_model_pool_contains(const void *p) { return p != 0; }
int anchor_is_connected(void) { return s_connected; }
int item_sync_save_is_loaded(void) { return s_loaded; }
int anchor_remote_collision_is_scripted(void) { return s_scripted; }
int anchor_player_models_get_epoch(void) { return s_epoch; }
int anchor_player_models_capacity(void) { return 3; }
int anchor_player_models_get_hit_targets(AnchorPlayerHitTarget *out, int capacity)
{
    assert(capacity >= s_target_count);
    memcpy(out, s_targets, (unsigned int)s_target_count * sizeof(*out));
    return s_target_count;
}
int anchor_player_models_peer_is_current(int cid, int session, int epoch)
{
    int i;
    for (i = 0; i < s_target_count; ++i)
        if (s_targets[i].cid == cid && s_targets[i].session == session &&
            s_targets[i].epoch == epoch)
            return 1;
    return 0;
}
void func_801EA0D8_5A5FE8(void *task, void *object)
{ (void)task; (void)object; }
void func_801EA4E8_5A63F8(void *task, void *object)
{ (void)task; (void)object; }
unsigned short func_801CF51C_58B42C(void *player, float x, float y, float z)
{
    assert(player == s_player && isfinite(x) && isfinite(y) && isfinite(z));
    ++s_heading_calls;
    return z > 0 ? 256 : 512;
}
void func_80038BC8_397C8(unsigned short cue)
{
    assert(cue == 0x21c);
    ++s_sounds;
}
void func_801EA5E4_5A64F4(void *chain, void *player, void *target)
{
    unsigned char *object = CHAIN_PTR(target, 0x18);
    assert(chain == s_head && player == s_player && target != s_player);
    assert(target != s_head && target != s_segment);
    assert(object != s_model && object != s_head_model);
    memcpy(s_latch_xyz, object + 8, sizeof(s_latch_xyz));
    memcpy(s_head_model + 8, s_latch_xyz, sizeof(s_latch_xyz));
    *(float *)(s_model + 0xc) = s_latch_xyz[1] - 3;
    *(unsigned short *)s_work = 3;
    /* Exact native EA5E4 constants: work+4 is acceleration, while E4/E8
     * contain the horizontal speed caps used by the ordinary player physics. */
    *(unsigned int *)(s_work + 4) = 0x3fc7c1f0u;
    *(unsigned int *)(s_player + 0xe4) = 0x4195d174u;
    *(unsigned int *)(s_player + 0xe8) = 0x4195d174u;
    *(float *)(s_player + 0x88) = 1.0f;
    s_head[0x60] = 2;
    ++s_latches;
}
void func_801DACDC_596BEC(void *player, unsigned char action)
{
    assert(player == s_player);
    assert(action == 0x73 || action == 0x7a);
    if (s_player[0xcc] == 0x75 || s_player[0xcc] == 0x76)
    {
        assert(*(unsigned short *)(s_work + 0x34) == 0);
        assert(*(unsigned short *)(s_player + 0xce) == 0);
    }
    s_player[0xcc] = action;
    ++s_actions;
}
void func_801F4C20_5B0B30(void *player, unsigned char release_action)
{
    assert(player == s_player && s_head[0x60] == 3);
    assert(release_action == (s_player[0xcc] == 0x7a ? 0x7b : 0x74));
    assert(*(short *)(s_work + 0x30) != 0); /* Never relatch transient target. */
    *(short *)(s_work + 0x30) = 0;
    *(float *)(s_work + 4) = 0;
    s_head[0x65] = 1;
    s_player[0xcc] = release_action;
    ++s_releases;
}

static void reset(void)
{
    memset(s_player, 0, sizeof(s_player));
    memset(s_model, 0, sizeof(s_model));
    memset(s_work, 0, sizeof(s_work));
    memset(s_head, 0, sizeof(s_head));
    memset(s_head_model, 0, sizeof(s_head_model));
    memset(s_segment, 0, sizeof(s_segment));
    memset(s_segment_model, 0, sizeof(s_segment_model));
    memset(&s_candidate, 0, sizeof(s_candidate));
    memset(&s_active, 0, sizeof(s_active));
    s_visual_head = s_visual_owner = s_visual_object = 0;
    s_sphere_attacker = s_blocked_head = 0;
    chain_debug_reset();
    s_pointer_count = 0;
    D_801FC604_5B8514 = s_player;
    D_801FC60C_5B851C = s_model;
    D_800C7AB2 = 4;
    CHAIN_PTR(s_player, 4) = s_links[0];
    CHAIN_PTR(s_links[0], 0) = s_player;
    CHAIN_PTR(s_head, 4) = s_links[1];
    CHAIN_PTR(s_links[1], 0) = s_head;
    CHAIN_PTR(s_segment, 4) = s_links[2];
    CHAIN_PTR(s_links[2], 0) = s_segment;
    CHAIN_PTR(s_player, 0x18) = s_model;
    CHAIN_PTR(s_player, 0x5c) = s_work;
    CHAIN_PTR(s_head, 0x18) = s_head_model;
    CHAIN_PTR(s_head, 0x5c) = s_player;
    CHAIN_PTR(s_segment, 0x18) = s_segment_model;
    CHAIN_PTR(s_segment, 0x5c) = s_player;
    CHAIN_PTR(s_segment, 0x9c) = s_head;
    *(unsigned short *)(s_player + 0x3c) = 50;
    *(float *)(s_model + 0x1c) = 0.2f;
    s_player[0xcc] = 0x71;
    s_connected = s_loaded = s_epoch = s_target_count = 1;
    s_scripted = s_wrong_callback = s_fail_alloc = 0;
    s_latches = s_actions = s_releases = s_sounds = s_heading_calls = 0;
    s_diagnostic_logs = 0;
    s_last_diagnostic[0] = 0;
    s_targets[0] = (AnchorPlayerHitTarget){7, 9, {{100, 20, 0}, 10, 25}, 11};
    D_800C7DBC = 0.0f;
}

static void offer(void *task)
{
    AnchorPlayerAttackSample sample = {
        .task = task, .is_chain_pipe = 1, .center = {100, 25, 0}};
    anchor_player_chain_sphere_begin(task);
    anchor_player_chain_observe(&sample, &s_targets[0]);
}
static void finish_scan(void)
{
    anchor_player_chain_sphere_end();
    anchor_player_chain_head_update(s_head, s_head_model);
}
static void latch(void *task)
{
    offer(task);
    finish_scan();
    assert(s_latches == 1 && s_actions == 1 && s_sounds == 1);
    assert(s_active.head == s_head && *(short *)(s_work + 0x30) == 1);
}

static void ground_and_air_use_native_latch_and_release(void)
{
    reset();
    latch(s_head);
    assert(s_player[0xcc] == 0x73);
    assert(s_latch_xyz[0] == 100 && s_latch_xyz[1] == 23 && s_latch_xyz[2] == 0);
    assert(*(float *)(s_model + 0xc) == 20);
    assert(CHAIN_PTR(s_work, 0x88) == 0); /* No retained synthetic descriptor. */
    *(float *)(s_model + 8) = 80;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1 && s_player[0xcc] == 0x74 && !s_active.head);
    assert(*(float *)(s_work + 4) == 0);

    reset();
    s_player[0xcc] = 0x78;
    latch(s_segment);
    assert(s_player[0xcc] == 0x7a);
    s_targets[0].session++;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1 && s_player[0xcc] == 0x7b);

    reset();
    s_player[0xcc] = 0x76;
    *(unsigned short *)(s_work + 0x34) = 7;
    *(unsigned short *)(s_player + 0xce) = 8;
    latch(s_head);
    assert(s_player[0xcc] == 0x73);
    assert(*(unsigned short *)(s_work + 0x34) == 0);
    assert(*(unsigned short *)(s_player + 0xce) == 0);

    reset();
    s_player[0xcc] = 0x75;
    latch(s_head);
    assert(s_player[0xcc] == 0x73);
}

static void native_pull_controls_survive_action_entry_and_tracking(void)
{
    static const unsigned char starts[] = {0x70, 0x71, 0x75, 0x76, 0x77, 0x78};
    unsigned int i, tick;
    for (i = 0; i < sizeof(starts); ++i)
    {
        reset();
        s_player[0xcc] = starts[i];
        latch(s_head);
        for (tick = 0; tick < 5; ++tick)
        {
            assert(*(unsigned short *)s_work == 3);
            assert(*(unsigned int *)(s_work + 4) == 0x3fc7c1f0u);
            assert(*(unsigned int *)(s_player + 0xe4) == 0x4195d174u);
            assert(*(unsigned int *)(s_player + 0xe8) == 0x4195d174u);
            s_targets[0].body.position.z += 1;
            anchor_player_chain_head_update(s_head, s_head_model);
            assert(!s_releases);
        }
    }
}

/* Verified CD310 flat-surface integration, including its special 95/9A
 * neutral-input reset at CDBBC..CDC68. Normalization/flat-floor friction
 * otherwise preserve previous velocity in mode3; CE870 adds work+4 along
 * work+2, and the native speed cap is applied before CC4C0 stores B0. */
static void native_flat_pull_physics(void)
{
    float vx = *(float *)(s_player + 0x68);
    float vz = *(float *)(s_player + 0x70);
    float acceleration = *(float *)(s_work + 4);
    float cap = *(float *)(s_player + 0xe4);
    float magnitude;
    unsigned short floor = *(unsigned short *)(s_player + 0x98);
    unsigned short mode = *(unsigned short *)s_work;
    if ((floor == 0x95 || floor == 0x9a) &&
        ((double)D_800C7DBC < 0.1 || ((mode & 2u) && (double)acceleration < 0.1)))
        vx = vz = 0.0f;
    if (*(unsigned short *)(s_work + 2) == 256)
        vz += acceleration;
    else
        vx += acceleration;
    magnitude = sqrtf(vx * vx + vz * vz);
    if (magnitude > cap)
    {
        vx *= cap / magnitude;
        vz *= cap / magnitude;
        magnitude = cap;
    }
    *(float *)(s_player + 0x68) = vx;
    *(float *)(s_player + 0x70) = vz;
    *(float *)(s_work + 0xb0) = magnitude;
    *(float *)(s_model + 8) += vx;
    *(float *)(s_model + 0x10) += vz;
    s_work[0x18] = 2;
}

static void native_neutral_floor_accumulates_exact_native_pull_speed(void)
{
    static const unsigned short floors[] = {0x95, 0x9a, 0, 1};
    static const unsigned char starts[] = {0x71, 0x76, 0x78};
    unsigned int floor_index, action_index, tick;
    float acceleration, cap;
    reset();
    s_targets[0].body.position.x = 1000;
    latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    acceleration = *(float *)(s_work + 4);
    for (tick = 0; tick < 12; ++tick)
        native_flat_pull_physics();
    assert(*(float *)(s_work + 0xb0) == acceleration); /* Logged old-code crawl. */

    for (floor_index = 0; floor_index < sizeof(floors) / sizeof(floors[0]); ++floor_index)
    {
        for (action_index = 0; action_index < sizeof(starts); ++action_index)
        {
            reset();
            s_player[0xcc] = starts[action_index];
            s_targets[0].body.position.x = 1000;
            latch(s_head);
            *(unsigned short *)(s_player + 0x98) = floors[floor_index];
            acceleration = *(float *)(s_work + 4);
            cap = *(float *)(s_player + 0xe4);
            for (tick = 1; tick <= 15; ++tick)
            {
                float expected = (float)tick * acceleration;
                if (expected > cap) expected = cap;
#if DEBUG_BUTTON_ENABLED
                anchor_player_chain_movement_diagnostic_begin(s_player);
#endif
                anchor_player_chain_physics_begin(s_player);
                native_flat_pull_physics();
                anchor_player_chain_physics_end();
#if DEBUG_BUTTON_ENABLED
                anchor_player_chain_movement_diagnostic_end();
#endif
                assert(D_800C7DBC == 0.0f);
                assert(fabsf(*(float *)(s_work + 0xb0) - expected) < 0.0001f);
                assert(*(float *)(s_work + 4) == acceleration);
                assert(*(float *)(s_player + 0xe4) == cap);
                anchor_player_chain_head_update(s_head, s_head_model);
                assert(s_active.head && !s_releases);
            }
            assert(fabsf(*(float *)(s_work + 0xb0) - 18.7272720337f) < 0.0001f);
            s_targets[0].body.position.z = 100;
            anchor_player_chain_head_update(s_head, s_head_model);
            assert(*(unsigned short *)(s_work + 2) == 256);
            anchor_player_chain_physics_begin(s_player);
            native_flat_pull_physics();
            anchor_player_chain_physics_end();
            assert(*(float *)(s_player + 0x70) > 0.0f && D_800C7DBC == 0.0f);
        }
    }
}

static void neutral_floor_scope_is_local_nested_and_restores_exact_input(void)
{
    unsigned int bits, i;
    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    anchor_player_chain_physics_begin(s_player);
    assert(D_800C7DBC == *(float *)(s_work + 4));
    anchor_player_chain_physics_begin(s_segment);
    assert(D_800C7DBC == 0.0f); /* Nested unrelated movement sees physical input. */
    anchor_player_chain_physics_end();
    assert(D_800C7DBC == *(float *)(s_work + 4));
    D_801FC604_5B8514 = 0;
    anchor_player_chain_physics_end();
    assert(D_800C7DBC == 0.0f); /* Owner changes cannot leak temporary input. */

    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x95;
    for (i = 0; i < 10; ++i)
        anchor_player_chain_physics_begin(s_player);
    assert(D_800C7DBC == 0.0f); /* Defensive overflow exposes raw input. */
    for (i = 0; i < 10; ++i)
        anchor_player_chain_physics_end();
    assert(D_800C7DBC == 0.0f);

    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    write_float_bits(&D_800C7DBC, 0x80000000u);
    anchor_player_chain_physics_begin(s_player);
    assert(D_800C7DBC == *(float *)(s_work + 4));
    anchor_player_chain_task_reinitialized(s_head);
    anchor_player_chain_physics_end();
    memcpy(&bits, &D_800C7DBC, sizeof(bits));
    assert(bits == 0x80000000u); /* Preserve even the sign bit of zero. */

    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    write_float_bits(&D_800C7DBC, 0x7fc01234u);
    anchor_player_chain_physics_begin(s_player);
    anchor_player_chain_physics_end();
    memcpy(&bits, &D_800C7DBC, sizeof(bits));
    assert(bits == 0x7fc01234u);
}

static void neutral_floor_scope_rejects_non_peer_or_ineligible_movement(void)
{
    unsigned int case_index, tick;
    for (case_index = 0; case_index < 10; ++case_index)
    {
        reset(); latch(s_head);
        *(unsigned short *)(s_player + 0x98) = 0x9a;
        switch (case_index)
        {
            case 0: s_active.head = 0; break; /* Ordinary native block pull. */
            case 1: s_player[0xcc] = 0x74; break;
            case 2: s_player[0x61] = 1; break;
            case 3: s_work[0x69] = 1; break;
            case 4: s_targets[0].epoch++; break;
            case 5: s_targets[0].session++; break;
            case 6: s_epoch++; break;
            case 7: s_head[0x60] = 3; break;
            case 8: *(unsigned short *)s_work = 1; break;
            case 9: write_float_bits(s_work + 4, 0x7f800000u); break;
        }
        anchor_player_chain_physics_begin(s_player);
        assert(D_800C7DBC == 0.0f);
        anchor_player_chain_physics_end();
        assert(D_800C7DBC == 0.0f);
    }
    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    D_800C7DBC = 0.5f;
    anchor_player_chain_physics_begin(s_player);
    assert(D_800C7DBC == 0.5f);
    anchor_player_chain_physics_end();
    assert(D_800C7DBC == 0.5f);
    reset(); latch(s_head);
    s_active.head = 0; /* Same native movement fields, ordinary block ownership. */
    for (tick = 0; tick < 15; ++tick)
    {
        anchor_player_chain_physics_begin(s_player);
        assert(D_800C7DBC == 0.0f);
        native_flat_pull_physics();
        anchor_player_chain_physics_end();
    }
    assert(fabsf(*(float *)(s_work + 0xb0) - 18.7272720337f) < 0.0001f);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    anchor_player_chain_physics_begin(s_player);
    native_flat_pull_physics();
    anchor_player_chain_physics_end();
    assert(*(float *)(s_work + 0xb0) == *(float *)(s_work + 4));

    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    D_800C7DBC = 0.0f;
    finish_pull();
    anchor_player_chain_physics_begin(s_player);
    assert(D_800C7DBC == 0.0f);
    anchor_player_chain_physics_end();
}

static void no_op_physics_scopes_preserve_other_hooks_input_changes(void)
{
    unsigned int case_index, i, bits;
    void *task;
    for (case_index = 0; case_index < 5; ++case_index)
    {
        reset(); latch(s_head);
        task = s_player;
        *(unsigned short *)(s_player + 0x98) = 0x9a;
        switch (case_index)
        {
            case 0: s_active.head = 0; break; /* Native block pull. */
            case 1: s_player[0xcc] = 0x74; break; /* Ineligible release action. */
            case 2: task = s_segment; break; /* Unrelated task. */
            case 3: D_800C7DBC = 0.5f; break; /* Non-dead physical input. */
            case 4: *(unsigned short *)(s_player + 0x98) = 1; break;
        }
        anchor_player_chain_physics_begin(task);
        write_float_bits(&D_800C7DBC, 0x3ec00000u);
        anchor_player_chain_physics_end();
        memcpy(&bits, &D_800C7DBC, sizeof(bits));
        assert(bits == 0x3ec00000u);
    }
    reset();
    for (i = 0; i < 10; ++i)
        anchor_player_chain_physics_begin(s_segment);
    D_800C7DBC = 0.375f;
    for (i = 0; i < 10; ++i)
        anchor_player_chain_physics_end();
    assert(D_800C7DBC == 0.375f); /* No-op overflow also owns no restoration. */

    reset(); latch(s_head);
    *(unsigned short *)(s_player + 0x98) = 0x9a;
    anchor_player_chain_physics_begin(s_player);
    anchor_player_chain_physics_begin(s_segment);
    assert(D_800C7DBC == 0.0f);
    D_800C7DBC = 0.375f;
    anchor_player_chain_physics_end();
    assert(D_800C7DBC == *(float *)(s_work + 4)); /* Shielded scope restores outer. */
    anchor_player_chain_physics_end();
    assert(D_800C7DBC == 0.0f);
}

static void real_native_contacts_keep_precedence(void)
{
    reset(); offer(s_head);
    CHAIN_PTR(s_head, 0x34) = s_work;
    finish_scan();
    assert(!s_latches && !s_actions);
    assert(CHAIN_PTR(s_head, 0x34) == s_work);
    reset(); offer(s_segment);
    CHAIN_PTR(s_segment, 0x38) = s_work;
    finish_scan();
    assert(!s_latches);
    reset(); offer(s_head);
    CHAIN_PTR(s_work, 0x88) = s_segment;
    finish_scan();
    assert(!s_latches && CHAIN_PTR(s_work, 0x88) == s_segment);
}

static void all_native_victim_groups_finish_before_commit(void)
{
    reset();
    offer(s_head);
    anchor_player_chain_sphere_end();
    assert(!s_latches); /* The first victim group cannot decide a grapple. */
    anchor_player_chain_sphere_begin(s_segment);
    CHAIN_PTR(s_segment, 0x34) = s_work; /* Later native enemy group. */
    anchor_player_chain_sphere_end();
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(!s_latches && !s_active.head);

    reset();
    anchor_player_chain_sphere_begin(s_segment);
    CHAIN_PTR(s_segment, 0x34) = s_work;
    anchor_player_chain_sphere_end();
    CHAIN_PTR(s_segment, 0x34) = 0; /* A segment update may clear its slot. */
    offer(s_head);
    finish_scan();
    assert(!s_latches); /* An earlier native hit also keeps precedence. */

    reset();
    offer(s_head);
    anchor_player_chain_sphere_end();
    anchor_player_chain_sphere_begin(s_player); /* Unrelated actor group. */
    anchor_player_chain_sphere_end();
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_latches == 1); /* An unrelated sphere cannot erase B's contact. */
}

static void only_current_local_chain_tasks_are_classified(void)
{
    reset();
    assert(anchor_player_chain_is_native(s_head));
    assert(anchor_player_chain_is_native(s_segment));
    assert(!anchor_player_chain_is_native(s_player));
    s_wrong_callback = 1;
    assert(!anchor_player_chain_is_native(s_head));
    s_wrong_callback = 0;
    CHAIN_PTR(s_head, 0x5c) = s_segment;
    assert(!anchor_player_chain_is_native(s_head));
    assert(!anchor_player_chain_is_native(s_segment));
    reset(); s_player[0x60] = 2;
    assert(anchor_player_chain_is_native(s_head));
    assert(anchor_player_chain_is_native(s_segment));
    offer(s_head);
    finish_scan();
    assert(!s_latches); /* Pending native teardown cannot turn into damage. */
}

static void stale_contacts_and_target_lifetimes_are_rejected(void)
{
    reset(); offer(s_head); s_targets[0].session++;
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); s_targets[0].epoch++;
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); ++s_epoch;
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); ++D_800C7AB2;
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); s_target_count = 0;
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); s_scripted = 1;
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); anchor_player_chain_task_reinitialized(s_head);
    finish_scan(); assert(!s_latches);
    reset(); offer(s_head); s_head[0x60] = 1; /* Terrain rejected extension. */
    finish_scan(); assert(!s_latches);
    reset(); s_head[0x60] = 1; offer(s_head);
    finish_scan(); assert(!s_latches);
}

static void moving_target_steers_and_invalid_target_releases(void)
{
    reset(); latch(s_head);
    s_targets[0].body.position.z = 50;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(!s_releases && s_heading_calls == 1);
    assert(*(float *)(s_head_model + 0x10) == 50);
    assert(*(unsigned short *)(s_work + 2) == 256);
    assert(*(unsigned short *)(s_player + 0x94) == 256);
    s_target_count = 0;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1 && !s_active.head);

    reset(); latch(s_head); s_targets[0].body.position.y += 9;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1);
    reset(); latch(s_head); s_connected = 0;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1);
    reset(); latch(s_head); s_scripted = 1;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1);
}

static void blocked_pull_is_bounded_and_hurt_action_is_preserved(void)
{
    unsigned int i;
    reset(); latch(s_head);
    for (i = 0; i < CHAIN_PULL_TICK_LIMIT; ++i)
        anchor_player_chain_head_update(s_head, s_head_model);
    assert(!s_releases);
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(s_releases == 1);
    reset(); latch(s_head);
    s_player[0xcc] = 0x1d; /* Native damage has already chosen hurt. */
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(!s_releases && s_player[0xcc] == 0x1d && !s_active.head);
    reset(); latch(s_head); anchor_player_chain_task_reinitialized(s_player);
    assert(!s_active.head && !s_visual_head);
}

static void snapshot_covers_native_block_chains_without_retained_targets(void)
{
    int out[4];
    reset();
    *(float *)(s_head_model + 8) = 12.25f;
    *(float *)(s_head_model + 0xc) = -3.5f;
    *(float *)(s_head_model + 0x10) = 9;
    anchor_player_chain_head_update(s_head, s_head_model);
    assert(!s_active.head && anchor_player_chain_visual_snapshot(out));
    assert(out[0] == 1 && out[1] == 1225 && out[2] == -350 && out[3] == 900);
    s_head[0x60] = 2;
    assert(anchor_player_chain_visual_snapshot(out) && out[0] == 3);
    write_float_bits(s_head_model + 8, 0x7fc00000u);
    assert(!anchor_player_chain_visual_snapshot(out));
    *(float *)(s_head_model + 8) = 1;
    s_head[0x65] = 1;
    assert(!anchor_player_chain_visual_snapshot(out));
    s_head[0x65] = 0;
    s_player[0xcc] = 0;
    assert(!anchor_player_chain_visual_snapshot(out));
    reset();
    anchor_player_chain_head_update(s_head, s_head_model);
    anchor_player_chain_task_reinitialized(s_head);
    assert(!anchor_player_chain_visual_snapshot(out));
}

#if DEBUG_BUTTON_ENABLED
static void diagnostics_pair_local_movement_and_reject_stale_returns(void)
{
    unsigned int logs;
    reset();
    latch(s_head);
    assert(s_diagnostic_logs == 1);
    assert(strstr(s_last_diagnostic, "[chain_pull] peer commit tick=0"));
    assert(strstr(s_last_diagnostic, "acc1000=1560 cap1000=18727,18727"));
    s_diagnostic_logs = 0;
    s_player[0x61] = 2;
    s_work[0x69] = 3;
    *(float *)(s_player + 0xa4) = -0.75f;
    D_800C7DBC = 0.05f;
    anchor_player_chain_movement_diagnostic_begin(s_player);
    assert(s_diagnostic_logs == 1 && strstr(s_last_diagnostic, "before tick=1"));
    assert(strstr(s_last_diagnostic, "state61=2 lock69=3 gravity1000=-750"));
    assert(strstr(s_last_diagnostic, "input1000=50"));
    *(float *)(s_model + 8) += 1.5f;
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == 2 && strstr(s_last_diagnostic, "after tick=1"));
    assert(strstr(s_last_diagnostic, "normal1000=0,1000,0"));
    assert(strstr(s_last_diagnostic, "delta100=150,0,0"));
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == 2); /* A return cannot consume a sample twice. */
    anchor_player_chain_movement_diagnostic_begin(s_segment);
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == 2); /* No reads/logging for unrelated tasks. */

    anchor_player_chain_movement_diagnostic_begin(s_player);
    logs = s_diagnostic_logs;
    ++s_epoch;
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == logs && !s_movement_diagnostic.owner.head);

    reset(); latch(s_head);
    anchor_player_chain_movement_diagnostic_begin(s_player);
    logs = s_diagnostic_logs;
    D_801FC60C_5B851C = 0; /* Old model cannot be read after owner rebinding. */
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == logs && !s_movement_diagnostic.owner.head);

    reset(); latch(s_head);
    anchor_player_chain_movement_diagnostic_begin(s_player);
    logs = s_diagnostic_logs;
    anchor_player_chain_task_reinitialized(s_head);
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == logs && !s_movement_diagnostic.pending);

    reset(); latch(s_head);
    write_float_bits(s_work + 4, 0x7fc00000u);
    write_float_bits(s_work + 0xb0, 0x7f800000u);
    anchor_player_chain_movement_diagnostic_begin(s_player);
    assert(strstr(s_last_diagnostic, "acc1000=2147483647"));
    assert(strstr(s_last_diagnostic, "b0_1000=2147483647"));
    *(float *)(s_model + 8) = -12.5f;
    anchor_player_chain_movement_diagnostic_end();
    assert(strstr(s_last_diagnostic, "xyz100=-1250,2000,0"));
    assert(strstr(s_last_diagnostic, "delta100=-1250,0,0"));
}

static void diagnostics_are_bounded_and_include_native_block_comparison(void)
{
    unsigned int tick;
    reset(); latch(s_head);
    s_diagnostic_logs = 0;
    for (tick = 0; tick < 240; ++tick)
    {
        anchor_player_chain_movement_diagnostic_begin(s_player);
        anchor_player_chain_movement_diagnostic_end();
    }
    assert(s_diagnostic_logs == 36); /* 12 initial + six sparse pairs, then stop. */
    finish_pull();
    assert(s_diagnostic_logs == 37 && strstr(s_last_diagnostic, "peer release"));
    assert(strstr(s_last_diagnostic, "action=74 phase=3"));
    anchor_player_chain_movement_diagnostic_begin(s_player);
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == 37 && !s_movement_diagnostic.owner.head);

    reset();
    s_player[0xcc] = 0x73;
    s_head[0x60] = 2;
    anchor_player_chain_head_update(s_head, s_head_model);
    anchor_player_chain_movement_diagnostic_begin(s_player);
    anchor_player_chain_movement_diagnostic_end();
    assert(s_diagnostic_logs == 2);
    assert(strstr(s_last_diagnostic, "[chain_pull] block after tick=1"));
}
#endif

int main(void)
{
    ground_and_air_use_native_latch_and_release();
    native_pull_controls_survive_action_entry_and_tracking();
    native_neutral_floor_accumulates_exact_native_pull_speed();
    neutral_floor_scope_is_local_nested_and_restores_exact_input();
    neutral_floor_scope_rejects_non_peer_or_ineligible_movement();
    no_op_physics_scopes_preserve_other_hooks_input_changes();
    real_native_contacts_keep_precedence();
    all_native_victim_groups_finish_before_commit();
    only_current_local_chain_tasks_are_classified();
    stale_contacts_and_target_lifetimes_are_rejected();
    moving_target_steers_and_invalid_target_releases();
    blocked_pull_is_bounded_and_hurt_action_is_preserved();
    snapshot_covers_native_block_chains_without_retained_targets();
#if DEBUG_BUTTON_ENABLED
    diagnostics_pair_local_movement_and_reject_stale_returns();
    diagnostics_are_bounded_and_include_native_block_comparison();
    puts("player chain debug movement pairing, lifetime and budget tests passed");
#else
    assert(s_diagnostic_logs == 0);
#endif
    puts("player chain native latch, priority, lifetime, steering and release tests passed");
    return 0;
}
