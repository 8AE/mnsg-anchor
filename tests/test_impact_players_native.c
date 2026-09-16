#include <stdio.h>
#include <stdlib.h>
#include "impact_test_pointers.h"
char *anchor_impact_players_update(int, unsigned int, unsigned int, unsigned int, const char *);
void recomp_free(void *);
#define ANCHOR_IMPACT_PLAYERS_HOST_TEST
#define IP_READ_PTR(p, o) TP(p, o)
#include "../src/anchor_impact_players.c"

static unsigned int state[0xC0], manager[0x80], local_task[0x80], local_object[0x80], local_backlink[4];
static unsigned char system_data[0x40000], resource[16];
static unsigned int tasks[64][0x80], objects[64][0x80], backlinks[64][4];
static unsigned int allocations, constructions, last_model, last_material;
static unsigned int arm_task[64], arm_object[64], hook_task[64], reel_task[64], reel_back[4];
static float last_scale;
static int owner = 1, ready = 1, resource_ready = 1;
static unsigned int test_boss = 1;
unsigned char D_8006D328_6DF28[16];
unsigned char D_8020A728_635B08[16], D_8020A7D0_635BB0[16];
void *D_8020EED0_63A2B0 = state;
unsigned char *D_8015C5C8_15D1C8 = system_data;
int anchor_impact_native_ready(void) { return ready; }
int anchor_impact_native_is_owner(void) { return owner; }
unsigned int anchor_impact_native_stage(void) { return 0x25F + test_boss; }
unsigned int anchor_impact_native_encounter(void) { return test_boss; }
unsigned int anchor_impact_native_visit(void) { return 1; }
void func_801DB200_6065E0(void) {}
void *func_800141C4_14DC4(unsigned int f) { assert(f == 0x4A8); return resource_ready ? resource : 0; }
void *func_80034E08_35A08(void *parent, void (*update)(void *, void *), unsigned short flags) {
    unsigned int i = allocations++;
    assert(parent == manager && !flags && i < 64);
    TP(tasks[i], 4) = backlinks[i]; TP(backlinks[i], 0) = tasks[i];
    TP(tasks[i], 0x0C) = (void *)(uintptr_t)update;
    return tasks[i];
}
void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                         float x, float y, float z, short rx, short ry, short rz,
                         float sx, float sy, float sz, short f8, short f9) {
    unsigned int i;
    for (i = 0; i < allocations && task != tasks[i]; ++i) {}
    assert(i < allocations && f8 == 0x4A8 && f9 == 0);
    ++constructions; last_model = model; last_material = material; last_scale = sx;
    assert(sx == sy && sy == sz);
    TP(task, 0x18) = objects[i]; TU32(objects[i], 0x2C) = model;
    TF32(objects[i], 8) = x; TF32(objects[i], 0xC) = y; TF32(objects[i], 0x10) = z;
    TU16(objects[i], 0x14) = rx; TU16(objects[i], 0x16) = ry; TU16(objects[i], 0x18) = rz;
    return objects[i];
}
char *anchor_impact_players_update(int active, unsigned int stage, unsigned int boss,
                                    unsigned int visit, const char *sample) {
    const char *value = "{\"accepted\":0,\"c\":[],\"a\":[]}";
    char *out = malloc(strlen(value) + 1);
    assert(active && stage == 0x25F + test_boss && boss == test_boss && visit == 1 && sample);
    strcpy(out, value); return out;
}
void recomp_free(void *p) { free(p); }
static void setup(void) {
    TP(state, 0x1BC) = manager; TP(state, 0) = TP(state, 0xC) = local_task;
    TP(local_task, 4) = local_backlink; TP(local_backlink, 0) = local_task;
    TP(local_task, 0x18) = local_object;
    TU32(local_object, 0x2C) = IP_CURSOR_MODEL; TU16(local_object, 0x16) = 512;
    s_manager = manager;
}
int main(int argc, char **argv) {
    if (argc > 1) test_boss = (unsigned int)atoi(argv[1]);
    AnchorImpactPlayerStatus status = {0};
    unsigned int row[10] = {2, 200, 1, 2, 0x2000, 0x2000, 1000, 600, 1, 1};
    void *retained;
    unsigned int i;
    setup();
    anchor_impact_players_set_authority(1, 1);
    status.cursor_count = 1;
    status.cursors[0][0] = 2; status.cursors[0][1] = 200; status.cursors[0][2] = 1;
    status.cursors[0][3] = 1; status.cursors[0][4] = float_bits(2);
    status.cursors[0][7] = 1000; status.cursors[0][8] = 600;
    resource_ready = 0; apply_cursors(&status); assert(allocations == 0);
    resource_ready = 1; apply_cursors(&status);
    assert(allocations == 1 && constructions == 1);
    assert(last_model == 0x4800A1E0 && last_scale == 0.2f);
    assert(last_material == (unsigned int)(uintptr_t)D_8020A728_635B08);
    retained = s_cursors[0].object;
    assert(IP_READ_U8(retained, 5) == 9 && TU16(retained, 0x16) == 600);
    assert(TF32(retained, 8) == 2 && !(IP_READ_U8(retained, 0x64) & 1));
    /* Tint uses the real mesh and preserves TEXEL0 alpha in both cycles. */
    assert(TU32(s_cursors[0].task, 0xDC) == 0xFFCC40FF);
    assert(((TU32(s_cursors[0].task, 0xE4) >> 15) & 7) == 3);
    assert(((TU32(s_cursors[0].task, 0xE4) >> 6) & 7) == 3);
    assert(((TU32(s_cursors[0].task, 0xE4) >> 9) & 7) == 1);
    assert((TU32(s_cursors[0].task, 0xE4) & 7) == 1);
    assert(TU32(s_cursors[0].task, 0xE8) == 0xB8000000);
    assert(TU32(local_object, 0x30) == 0); /* local material untouched */
    /* New cursor samples move on native ticks; repeated samples do not restart
     * interpolation. Crossing 1024 follows the short arc. */
    status.cursors[0][4] = float_bits(14);
    status.cursors[0][7] = 24;
    apply_cursors(&status); assert(TF32(retained, 8) == 2);
    impact_cursor_update(s_cursors[0].task, retained);
    assert(TF32(retained, 8) == 6 && TU16(retained, 0x14) == 1016);
    apply_cursors(&status);
    impact_cursor_update(s_cursors[0].task, retained);
    assert(TF32(retained, 8) == 10 && TU16(retained, 0x14) == 8);
    impact_cursor_update(s_cursors[0].task, retained);
    assert(TF32(retained, 8) == 14 && TU16(retained, 0x14) == 24);
    /* Pause and returning peer reuse one retained object, with no hidden leak. */
    for (i = 0; i < 50; ++i) {
        anchor_impact_players_reset(); assert(IP_READ_U8(retained, 0x64) & 1);
        apply_cursors(&status); assert(s_cursors[0].object == retained);
    }
    assert(allocations == 1 && constructions == 1);
    status.cursors[0][3] = 0; apply_cursors(&status);
    assert(IP_READ_U8(retained, 0x64) & 1);
    /* A remote press uses that cursor only inside the native interpreter. */
    anchor_impact_players_set_authority(1, 1);
    receive_controls(row); TU16(system_data, 0x3B07A) = 0; TU16(system_data, 0x3B07C) = 0;
    TF32(state, 4) = 13; TF32(state, 8) = 17;
    anchor_impact_controls_begin(0, 0);
    assert(TU16(system_data, 0x3B07C) == 0x2000);
    assert(TU16(local_object, 0x14) == 1000 && TU16(local_object, 0x16) == 600);
    anchor_impact_controls_end();
    assert(TU16(system_data, 0x3B07C) == 0 && TU16(local_object, 0x16) == 512);
    assert(TF32(state, 4) == 13 && TF32(state, 8) == 17);
    anchor_impact_controls_begin(0, 0);
    assert(TU16(system_data, 0x3B07C) == 0); anchor_impact_controls_end();
    /* A release heartbeat must not overwrite the queued press's aim. */
    row[6] = 1000; row[7] = 600; row[5] = row[4] = 0x8000;
    receive_controls(row);
    row[4] = row[5] = 0; row[6] = 20; row[7] = 520; receive_controls(row);
    TU16(system_data,0x3B07C) = 0x4000;
    anchor_impact_controls_begin(0,0);
    assert(TU16(system_data,0x3B07C) == 0x4000 && s_press_count == 1);
    assert(TU16(local_object,0x16) == 512); anchor_impact_controls_end();
    TU16(system_data,0x3B07C) = 0;
    anchor_impact_controls_begin(0,0);
    assert(TU16(system_data,0x3B07C) == 0x8000 && TU16(local_object,0x16) == 600);
    IP_WRITE_U8(state,0x141,10); /* queued native punch while an arm returns */
    anchor_impact_controls_end();
    anchor_impact_controls_begin(0,0);
    assert(TU16(system_data,0x3B07C) == 0 && TU16(local_object,0x16) == 600);
    IP_WRITE_U8(state,0x141,0); anchor_impact_controls_end();
    assert(TF32(state,4) == 13 && TF32(state,8) == 17);
    /* Distinct B presses remain distinct native combo edges, in arrival order. */
    row[4] = row[5] = 0x4000; row[7] = 610; receive_controls(row);
    row[0] = 3; row[1] = 300; row[7] = 620; receive_controls(row);
    anchor_impact_controls_begin(0,1);
    assert(TU16(system_data,0x3B07C) == 0x4000 && TU16(local_object,0x16) == 610);
    assert(TU16(system_data,0x3B094) == 0x4000); anchor_impact_controls_end();
    anchor_impact_controls_begin(0,0);
    assert(TU16(system_data,0x3B07C) == 0x4000 && TU16(local_object,0x16) == 620);
    anchor_impact_controls_end(); assert(!s_press_count);
    /* Guard's native release reader runs outside the interpreter. */
    row[4] = 3; row[5] = 0; receive_controls(row);
    anchor_impact_guard_begin(); assert((TU16(system_data,0x3B07A)&3) == 3);
    anchor_impact_guard_end(); assert(TU16(system_data,0x3B07A) == 0);
    row[4] = 0; receive_controls(row);
    anchor_impact_guard_begin(); assert(!(TU16(system_data,0x3B07A)&3)); anchor_impact_guard_end();
    row[0] = 2; row[1] = 200; row[4] = row[5] = 0x2000;
    /* A queued remote punch cannot borrow the next peer's or the physical
     * player's aim while the previous limb is returning. */
    row[0] = 2; row[1] = 200; row[4] = row[5] = 0x8000; row[7] = 600;
    receive_controls(row); anchor_impact_controls_begin(0,0);
    IP_WRITE_U8(state,0x141,10); anchor_impact_controls_end();
    row[0] = 3; row[1] = 300; row[4] = row[5] = 0x4000; row[7] = 700;
    receive_controls(row); TU16(system_data,0x3B07C) = 0x2000;
    anchor_impact_controls_begin(0,0);
    assert(s_action_aim.cid == 2 && TU16(local_object,0x16) == 600);
    assert(!TU16(system_data,0x3B07C) && s_press_count == 2);
    IP_WRITE_U8(state,0x141,0); anchor_impact_controls_end();
    assert(TU16(system_data,0x3B07C) == 0x2000);
    TU16(system_data,0x3B07C) = 0;
    anchor_impact_controls_begin(0,0);
    assert(s_action_aim.cid == 3 && TU16(local_object,0x16) == 700);
    anchor_impact_controls_end(); anchor_impact_controls_begin(0,0);
    assert(!s_action_aim.cid && TU16(system_data,0x3B07C) == 0x2000);
    anchor_impact_controls_end(); assert(!s_press_count);
    row[0] = 2; row[1] = 200;
    /* Guided-fist steering and retract have separate native pad readers. */
    row[4] = row[5] = 0x10; receive_controls(row);
    /* Native child allocation inserts before any older sibling, then the
     * scheduler runs that child AFTER the interpreter restores local aim. */
    TP(D_8006D328_6DF28,0) = local_task;
    TP(local_task,0) = 0;
    anchor_impact_controls_begin(0,0); anchor_impact_action_begin(20,0);
    anchor_impact_attack_child_begin(local_task);
    anchor_impact_attack_task_reset(arm_task);
    TP(local_task,0) = arm_task; TP(arm_task,4) = local_task;
    TP(arm_task,0x18) = arm_object; TU32(arm_task,0x94) = 0;
    anchor_impact_attack_child_end();
    anchor_impact_action_end(); anchor_impact_controls_end();
    assert(attack_aim(arm_task)->cid == 2);
    TU16(local_object,0x14) = 24; TU16(local_object,0x16) = 500;
    row[4] = row[5] = 0; row[6] = 990; row[7] = 630; receive_controls(row);
    anchor_impact_attack_task_begin(arm_task);
    assert(TU16(local_object,0x14) == 990 && TU16(local_object,0x16) == 630);
    assert(TF32(state,4) == 118*(360.0f/1024.0f));
    /* Delayed child constructor inherits the arm's source; it never looks
     * up a global last-hook player or uses the local player's cursor. */
    anchor_impact_attack_child_begin(arm_task);
    anchor_impact_attack_task_reset(hook_task);
    TP(arm_task,0) = hook_task; TP(hook_task,4) = arm_task;
    anchor_impact_attack_child_end();
    anchor_impact_attack_task_begin(local_task); /* next, unrelated camera */
    assert(TU16(local_object,0x14) == 24 && TU16(local_object,0x16) == 500);
    assert(TF32(state,4) == 13 && TF32(state,8) == 17);
    anchor_impact_attack_task_begin(hook_task);
    assert(TU16(local_object,0x14) == 990 && TU16(local_object,0x16) == 630);
    anchor_impact_attack_tasks_end();
    assert(TU16(local_object,0x14) == 24 && TU16(local_object,0x16) == 500);
    anchor_impact_attack_task_reset(hook_task); /* address reuse clears origin */
    anchor_impact_attack_task_begin(hook_task); assert(TU16(local_object,0x16) == 500);
    anchor_impact_attack_tasks_end();
    /* Taisamba's grab/release readers borrow the initiating limb's source. */
    anchor_impact_players_source_aim_begin(arm_task);
    assert(TU16(local_object,0x14) == 990 && TU16(local_object,0x16) == 630);
    anchor_impact_players_source_aim_end();
    assert(TF32(state,4) == 13 && TF32(state,8) == 17);
    row[3] = 3; row[4] = 110; row[5] = 60; row[6] = row[7] = 0; receive_controls(row);
    TU16(system_data,0x3B07E) = 9; TU16(system_data,0x3B080) = 7;
    TF32(system_data,0x3B084) = .75f; TF32(system_data,0x3B088) = .5f;
    anchor_impact_players_source_axes_begin(arm_task);
    assert(TF32(system_data,0x3B084) == .375f && TF32(system_data,0x3B088) == -.25f);
    anchor_impact_players_source_axes_end();
    assert(TF32(system_data,0x3B084) == .75f && TF32(system_data,0x3B088) == .5f);
    anchor_impact_fist_begin(arm_task,arm_object);
    assert((short)TU16(system_data,0x3B07E) == 30 && (short)TU16(system_data,0x3B080) == -20);
    anchor_impact_fist_end();
    assert(TU16(system_data,0x3B07E) == 9 && TU16(system_data,0x3B080) == 7);
    anchor_impact_retract_begin(); assert(TU16(system_data,0x3B07C)&0x10);
    anchor_impact_retract_end(); assert(!TU16(system_data,0x3B07C));
    /* Dead/disconnected controls never inherit the primary player's axis. */
    for (i = 0; i < IP_CURSOR_MAX; ++i) s_controls[i].axis_age = 10;
    anchor_impact_fist_begin(arm_task,arm_object); assert(!TU16(system_data,0x3B07E));
    anchor_impact_fist_end(); assert(TU16(system_data,0x3B07E) == 9);
    /* All three native reeling readers consume remote A/B after the action
     * interpreter has returned. Ordered mashes do not become queued punches. */
    TP(reel_task,4) = reel_back; TP(reel_back,0) = reel_task;
    TP(state,0x174) = reel_task; TU32(state,0x1E4) = 1;
    row[3] = 2;
    for (i = 0; i < 3; ++i) {
        const unsigned int callbacks[] = {0x801E9624,0x801F5F0C,0x801FED3C};
        TU32(reel_task,0xC) = callbacks[i];
        row[4] = row[5] = 0x8000; receive_controls(row);
        row[4] = row[5] = 0x4000; receive_controls(row);
        row[4] = row[5] = 0; receive_controls(row);
        assert(s_reel_count == 2 && s_press_count == 0);
        TU16(system_data,0x3B094) = 0; TF32(system_data,0x3B0A0) = 1;
        anchor_impact_controls_begin(0,0); anchor_impact_controls_end();
        reel_begin(reel_task);
        assert(TU16(system_data,0x3B094) == 0x8000 && TF32(system_data,0x3B0A0) == 0);
        /* Native meter reader: 9 per edge, 0.5 decay, no second AI execution. */
        TF32(reel_task,0xA0) = TF32(reel_task,0xA0) - .5f +
            ((TU16(system_data,0x3B094)&0xC030) ? 9 : 0);
        reel_end();
        assert(!TU16(system_data,0x3B094) && TF32(system_data,0x3B0A0) == 1);
        TU16(system_data,0x3B094) = 0x8000; reel_begin(reel_task);
        assert(TU16(system_data,0x3B094) == 0xC000 && TF32(system_data,0x3B0A0) == 1);
        reel_end(); assert(TU16(system_data,0x3B094) == 0x8000);
        TU16(system_data,0x3B094) = 0; reel_begin(reel_task);
        assert(!TU16(system_data,0x3B094) && !s_reel_count); reel_end();
    }
    assert(TF32(reel_task,0xA0) == 25.5f);
    row[4] = row[5] = 0x8000; receive_controls(row);
    owner = 0; reel_begin(reel_task); assert(!TU16(system_data,0x3B094)); reel_end(); owner = 1;
    TP(state,0x174) = 0; anchor_impact_players_tick(1);
    assert(!s_reel_count); /* latch loss discards unsatisfied mash edges */
    row[3] = 2; row[4] = row[5] = 0x2000;
    /* Follower input is exported but cannot execute a second native shot. */
    owner = 0; TU16(system_data, 0x3B07A) = TU16(system_data, 0x3B07C) = 0x2000;
    TU16(system_data, 0x3B07E) = 31;
    anchor_impact_controls_begin(0, 0);
    assert(TU16(system_data, 0x3B07A) == 0 && TU16(system_data, 0x3B07C) == 0);
    assert(TU16(system_data, 0x3B07E) == 31);
    anchor_impact_controls_end(); assert(TU16(system_data, 0x3B07C) == 0x2000);
    /* Expiry runs even on followers; promotion drops old button edges. */
    receive_controls(row);
    for (i = 0; i < 12; ++i) anchor_impact_players_tick(1);
    for (i = 0; i < IP_CURSOR_MAX; ++i) assert(!s_controls[i].cid);
    owner = 1; receive_controls(row);
    anchor_impact_players_set_authority(1, 2); anchor_impact_players_tick(1);
    for (i = 0; i < IP_CURSOR_MAX; ++i) assert(!s_controls[i].cid);
    receive_controls(row); /* old owner term */
    for (i = 0; i < IP_CURSOR_MAX; ++i) assert(!s_controls[i].cid);
    row[9] = 2; receive_controls(row);
    assert(s_controls[IP_CURSOR_MAX - 1].cid == 2);
    /* Capture the constructor's actual native velocity without replaying AI. */
    TP(local_task, 0x0C) = (void *)(uintptr_t)func_801DB200_6065E0;
    TF32(local_task, 0x70) = 1; TF32(local_task, 0x74) = 2; TF32(local_task, 0x78) = -12;
    anchor_impact_shot_constructor_begin();
    anchor_impact_shot_velocity_begin(12, 0, 0, 0, local_task);
    anchor_impact_shot_velocity_end();
    assert(s_ctor_captured && s_ctor_velocity[2] == -12);
    anchor_impact_shot_constructor_end(); assert(!s_ctor_bracket);
    /* Actual dispatcher action families all need the same lifetime binding:
     * punches, hook, extended fist, kicks, paired attacks, guards, laser, Ryo,
     * barrage and secondary-controller Ryo. No fake limb is spawned here. */
    {
        const int actions[] = {10,11,12,13,20,21,30,31,32,40,41,50,51,60,70,71,100};
        unsigned int a;
        owner = ready = 1;
        anchor_impact_players_reset(); anchor_impact_players_set_authority(1,2);
        TP(state,0x174) = 0; IP_WRITE_U8(state,0x141,0); IP_WRITE_U8(state,0x145,0);
        TP(D_8006D328_6DF28,0) = local_task;
        TU16(system_data,0x3B07A) = TU16(system_data,0x3B07C) = 0;
        TU16(system_data,0x3B092) = TU16(system_data,0x3B094) = 0;
        for (a = 0; a < sizeof(actions)/sizeof(actions[0]); ++a) {
            row[0] = 2; row[1] = 200; row[3] = 2;
            row[4] = row[5] = 0x8000; row[6] = 992; row[7] = 640; row[8] = 1; row[9] = 2;
            TP(local_task,0) = 0; TP(arm_task,0) = 0;
            receive_controls(row); anchor_impact_controls_begin(0,0);
            if (actions[a] == 100) anchor_impact_secondary_action_begin(actions[a],0);
            else anchor_impact_action_begin(actions[a],0);
            anchor_impact_attack_child_begin(local_task);
            anchor_impact_attack_task_reset(arm_task);
            TP(local_task,0) = arm_task; TP(arm_task,4) = local_task;
            anchor_impact_attack_child_end();
            if (actions[a] == 100) anchor_impact_secondary_action_end();
            else anchor_impact_action_end();
            anchor_impact_controls_end();
            assert(attack_aim(arm_task)->cid == 2);
            /* The owner moves away after construction, before the delayed
             * initializer / animation updater reads yaw, pitch or cursor. */
            TF32(state,4) = -45; TF32(state,8) = 7;
            TU16(local_object,0x14) = 20; TU16(local_object,0x16) = 384;
            anchor_impact_attack_task_begin(arm_task);
            assert(TF32(state,4) == 45 && TF32(state,8) == -11.25f);
            assert(TU16(local_object,0x14) == 992 && TU16(local_object,0x16) == 640);
            /* 9030/laser convert degrees to turns; hook/Ryo read turns
             * directly. Both produce the participant's identical direction. */
            assert((int)(TF32(state,4)*(1024.0f/360.0f))+256 == 384);
            assert((int)TU16(local_object,0x16)-256 == 384);
            anchor_impact_attack_tasks_end();
            assert(TF32(state,4) == -45 && TU16(local_object,0x16) == 384);
            owner = 0; anchor_impact_attack_task_begin(arm_task);
            assert(TF32(state,4) == -45); owner = 1;
            anchor_impact_players_reset(); /* pause: task survives, controls do not */
            assert(attack_aim(arm_task)->cid == 2 && !s_press_count);
            anchor_impact_attack_task_begin(arm_task);
            assert(TF32(state,4) == 45); anchor_impact_attack_tasks_end();
            anchor_impact_attack_task_reset(arm_task);
            anchor_impact_attack_task_begin(arm_task); assert(TF32(state,4) == -45);
        }
        /* A held-only remote guard chooses its holder even without an edge. */
        row[4] = 1; row[5] = 0; receive_controls(row);
        anchor_impact_controls_begin(0,0);
        assert(s_action_aim.cid == 2 && TF32(state,4) == 45);
        anchor_impact_controls_end();
    }
    puts("Impact cursor, retained lifecycle, controls and shot capture tests passed");
    return 0;
}

int anchor_impact_boss_is_reel_callback(unsigned int c) {return c == 0x801E9624u || c == 0x801F5F0Cu || c == 0x801FED3Cu;}
