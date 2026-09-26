#include <assert.h>
#include <stdio.h>
#include <string.h>

#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#include "../src/combat/anchor_player_cube.c"

typedef union MockTask
{
    void *align;
    unsigned char bytes[0x100];
} MockTask;

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
unsigned int D_8015C604;
static MockTask player, work, visual_task, visual_object, player_object;
static int self_id, frozen, visual_active, projected, sent_count;
static int last_op, last_target, last_carry, release_count, action_count;
static int thaw_count, impact_x100, shatter_count;
static AnchorPlayerCubeControl incoming;
static int pending_incoming, mock_hit, mock_edge_hit;
static int fail_impact, mock_ground_start;
static int rotate_count;
static unsigned short rotate_rx, rotate_ry, rotate_rz;
static float rotate_input_x, rotate_input_y, rotate_input_z;

int anchor_is_connected(void) { return 1; }
unsigned int anchor_get_client_id(void) { return (unsigned int)self_id; }
int item_sync_save_is_loaded(void) { return 1; }
int anchor_player_models_get_epoch(void) { return 5; }
int anchor_player_models_peer_is_current(int cid, int session, int epoch)
{ return cid > 0 && session == 4 && epoch == 5; }
int anchor_player_models_get_sound_position(int cid, int session, int epoch,
                                           float *x, float *y, float *z)
{
    if (!anchor_player_models_peer_is_current(cid, session, epoch))
        return 0;
    *x = 12.0f; *y = 0.0f; *z = 0.0f;
    return 1;
}
int anchor_player_freeze_active(void) { return frozen; }
void anchor_player_freeze_thaw_on_cube_impact(void)
{
    float x, y, z;
    assert(anchor_player_cube_victim_pose(&x, &y, &z));
    impact_x100 = (int)(x * 100.0f);
    ++thaw_count;
    frozen = 0;
    anchor_player_cube_victim_thaw();
}
int anchor_player_freeze_visual_get_cubes(AnchorFreezeCubeCollision *out,
                                          int capacity)
{
    if (!visual_active || capacity < 1)
        return 0;
    memset(out, 0, sizeof(*out));
    out->cid = 2; out->session = 4; out->epoch = 5;
    out->cube.min.x = -20; out->cube.max.x = 20;
    out->cube.min.y = -10; out->cube.max.y = 190;
    out->cube.min.z = -20; out->cube.max.z = 20;
    return 1;
}
int anchor_player_freeze_visual_get_native(int cid, int session, int epoch,
                                          void **task, void **object,
                                          float *scale)
{
    if (!visual_active || cid != 2 || session != 4 || epoch != 5)
        return 0;
    *task = visual_task.bytes;
    *object = visual_object.bytes;
    *scale = 1.0f;
    return 1;
}
int anchor_player_freeze_visual_has_cube(int cid, int session, int epoch)
{ return visual_active && cid == 2 && session == 4 && epoch == 5; }
int anchor_player_freeze_visual_owns_task(const void *task)
{ return task == visual_task.bytes; }
int anchor_player_freeze_visual_shatter(int cid, int session, int epoch)
{
    assert(cid == 2 && session == 4 && epoch == 5);
    ++shatter_count;
    visual_active = 0;
    return 1;
}
int anchor_send_player_cube_control(int op, int target_cid, int target_epoch,
    int carry_id, int x100, int y100, int z100, int vx100, int vy100,
    int vz100, int rx, int ry, int rz, int source_epoch)
{
    (void)target_epoch; (void)x100; (void)y100; (void)z100;
    (void)vx100; (void)vy100; (void)vz100;
    (void)rx; (void)ry; (void)rz;
    assert(source_epoch == 5);
    ++sent_count; last_op = op; last_target = target_cid;
    last_carry = carry_id;
    return !(fail_impact && op == ANCHOR_CUBE_IMPACT);
}
int anchor_poll_player_cube_control(AnchorPlayerCubeControl *out)
{
    if (!pending_incoming)
        return 0;
    *out = incoming;
    pending_incoming = 0;
    return 1;
}
void func_801E54A8_5A13B8(void *local, void *task)
{
    void *local_work = *(void **)((unsigned char *)local + 0x5c);
    *(void **)((unsigned char *)local_work + 0x8c) = task;
}
void func_801E55A0_5A14B0(void *local)
{
    void *local_work = *(void **)((unsigned char *)local + 0x5c);
    *(void **)((unsigned char *)local_work + 0x8c) = 0;
    ++release_count;
}
void func_801DACDC_596BEC(void *local, unsigned char action)
{ (void)local; (void)action; ++action_count; }
void func_80033898_34498(unsigned short rx, unsigned short ry,
                          unsigned short rz, float *x, float *y, float *z)
{
    float old_x = *x;
    ++rotate_count;
    rotate_rx = rx; rotate_ry = ry; rotate_rz = rz;
    rotate_input_x = *x; rotate_input_y = *y; rotate_input_z = *z;
    /* A quarter-turn yaw is enough to check that launch uses local forward. */
    if (ry == 256)
    {
        *x = *z;
        *z = -old_x;
    }
}
void *func_8002C9D4_2D5D4(void *out, float x, float y, float z,
                           float dx, float dy, float dz, float range)
{
    CubeQuery *query = out;
    (void)x; (void)y; (void)z; (void)range;
    if (mock_hit || (mock_ground_start && y <= 0.0f) ||
        (mock_edge_hit && x == 100.0f && y == 200.0f &&
                     z == 0.0f))
    {
        query->hit = 0x7fff;
        query->delta[0] = query->delta[1] = query->delta[2] = 0;
        query->normal[0] = -dx;
        query->normal[1] = -dy;
        query->normal[2] = -dz;
    }
    return out;
}

static void setup(void)
{
    memset(&player, 0, sizeof(player));
    memset(&work, 0, sizeof(work));
    memset(&visual_task, 0, sizeof(visual_task));
    memset(&visual_object, 0, sizeof(visual_object));
    memset(&player_object, 0, sizeof(player_object));
    memset(&s_victim, 0, sizeof(s_victim));
    memset(&s_carrier, 0, sizeof(s_carrier));
    D_801FC604_5B8514 = player.bytes;
    D_801FC60C_5B851C = player_object.bytes;
    *(void **)(player.bytes + 0x5c) = work.bytes;
    *(float *)(player_object.bytes + 8) = 0;
    *(float *)(player_object.bytes + 0xc) = 0;
    *(float *)(player_object.bytes + 0x10) = 0;
    *(float *)(visual_object.bytes + 8) = 0;
    *(float *)(visual_object.bytes + 0xc) = 100;
    *(float *)(visual_object.bytes + 0x10) = 0;
    player.bytes[0xcc] = 7;
    D_800C7AB2 = 10;
    D_8015C604 = 3;
    self_id = 1;
    visual_active = frozen = projected = sent_count = 0;
    release_count = action_count = thaw_count = shatter_count = mock_hit =
        mock_edge_hit = 0;
    pending_incoming = fail_impact = mock_ground_start = 0;
    rotate_count = 0;
}

static void grant_cube_to_carrier(void)
{
    visual_active = 1;
    anchor_player_cube_interact_begin(player.bytes, 0);
    anchor_player_cube_interact_end();
    memset(&incoming, 0, sizeof(incoming));
    incoming.op = ANCHOR_CUBE_GRANT;
    incoming.sender_cid = 2; incoming.target_cid = 1;
    incoming.room_id = 10; incoming.source_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = last_carry;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 2 && held_task_is_ours());
}

static void test_raised_carry_and_throw(void)
{
    float lift;
    setup();
    grant_cube_to_carrier();
    s_carrier.scale = 0.75f;
    lift = CUBE_CARRY_LIFT * s_carrier.scale;

    /* A native placement rewrites the object pose before each return hook. */
    anchor_player_cube_placement_begin(player.bytes, work.bytes, 1);
    *(float *)(visual_object.bytes + 0xc) = 12.0f;
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 12.0f + lift);
    anchor_player_cube_placement_begin(player.bytes, work.bytes, 1);
    *(float *)(visual_object.bytes + 0xc) = 14.0f;
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);

    /* Native no-write calls must not add a second lift to the prior pose. */
    anchor_player_cube_placement_begin(player.bytes, 0, 1);
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);
    anchor_player_cube_placement_begin(player.bytes, work.bytes, 0);
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);
    anchor_player_cube_placement_begin(player.bytes, 0, 0);
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);
    /* The lower marker helper can fail even with both arguments supplied. */
    anchor_player_cube_placement_begin(player.bytes, work.bytes, 1);
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);
    anchor_player_cube_placement_begin(player.bytes, work.bytes, 1);
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);

    /* Unrelated placement callbacks leave the cube untouched. */
    anchor_player_cube_placement_begin(work.bytes, work.bytes, 1);
    anchor_player_cube_placement_end();
    assert(*(float *)(visual_object.bytes + 0xc) == 14.0f + lift);

    *(float *)(player.bytes + 0x68) = 2.0f;
    *(float *)(player.bytes + 0x6c) = 1.0f;
    *(float *)(player.bytes + 0x70) = 4.0f;
    *(unsigned short *)(player_object.bytes + 0x14) = 12;
    *(unsigned short *)(player_object.bytes + 0x16) = 256;
    *(unsigned short *)(player_object.bytes + 0x18) = 34;
    anchor_player_cube_throw_begin(player.bytes);
    *(float *)(visual_object.bytes + 0xc) = 10.0f; /* Native launch pose. */
    func_801E55A0_5A14B0(player.bytes);
    anchor_player_cube_throw_end();
    assert(s_carrier.phase == 3);
    assert(CUBE_CARRY_LIFT == 180.0f);
    assert(*(float *)(visual_object.bytes + 0xc) == 10.0f + lift);
    assert(rotate_count == 1 && rotate_rx == 12 && rotate_ry == 256 &&
           rotate_rz == 34);
    assert(rotate_input_x == 2.0f && rotate_input_y == 6.0f &&
           rotate_input_z == 7.0f);
    assert(s_carrier.vx == 7.0f && s_carrier.vy == 6.0f &&
           s_carrier.vz == -2.0f);
    mock_ground_start = 1;
    for (int frame = 0; frame < 8; ++frame)
    {
        float old_x = *(float *)(visual_object.bytes + 8);
        float old_y = *(float *)(visual_object.bytes + 0xc);
        float old_vy = s_carrier.vy;
        anchor_player_cube_tick();
        assert(s_carrier.phase == 3 && s_carrier.throw_sent);
        assert(*(float *)(visual_object.bytes + 8) == old_x + 7.0f);
        assert(s_carrier.vy < old_vy);
        assert(*(float *)(visual_object.bytes + 0xc) >
               100.0f * s_carrier.scale);
        if (frame < 7)
            assert(*(float *)(visual_object.bytes + 0xc) > old_y);
    }
}

static void test_impact_send_failure_releases_carrier(void)
{
    setup();
    visual_active = 1;
    anchor_player_cube_interact_begin(player.bytes, 0);
    anchor_player_cube_interact_end();
    memset(&incoming, 0, sizeof(incoming));
    incoming.op = ANCHOR_CUBE_GRANT;
    incoming.sender_cid = 2; incoming.target_cid = 1;
    incoming.room_id = 10; incoming.source_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = last_carry;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 2);
    *(float *)(visual_task.bytes + 0x78) = 10;
    *(float *)(visual_task.bytes + 0x7c) = 8;
    anchor_player_cube_throw_begin(player.bytes);
    func_801E55A0_5A14B0(player.bytes);
    anchor_player_cube_throw_end();
    anchor_player_cube_tick();
    assert(s_carrier.phase == 3 && s_carrier.throw_sent);
    mock_hit = 1;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 4);
    mock_hit = 0;
    fail_impact = 1;
    visual_active = 0; /* Target may already have thawed on its own client. */
    for (int i = 0; i < CUBE_GRANT_WAIT; ++i)
        anchor_player_cube_tick();
    assert(s_carrier.phase == 0 && !s_carrier.task && !s_carrier.object);
    assert((visual_task.bytes[0x30] & 2u) == 0);
    assert(*(unsigned int *)(visual_task.bytes + 0x48) == 0);
    assert(*(void **)(visual_task.bytes + 0x34) == 0);
    assert(*(void **)(visual_task.bytes + 0x38) == 0);
    assert(last_op == ANCHOR_CUBE_IMPACT && thaw_count == 0);
    visual_active = 1;
    fail_impact = 0;
    anchor_player_cube_interact_begin(player.bytes, 0);
    anchor_player_cube_interact_end();
    assert(s_carrier.phase == 1 && last_op == ANCHOR_CUBE_REQUEST);
}

static void test_room_change_clears_impact_attack(void)
{
    setup();
    grant_cube_to_carrier();
    anchor_player_cube_throw_begin(player.bytes);
    func_801E55A0_5A14B0(player.bytes);
    anchor_player_cube_throw_end();
    mock_hit = 1;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 4 && (visual_task.bytes[0x30] & 2u));
    *(void **)(visual_task.bytes + 0x34) = work.bytes;
    *(void **)(visual_task.bytes + 0x38) = work.bytes;
    ++D_800C7AB2;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 0 && !s_carrier.task);
    assert((visual_task.bytes[0x30] & 2u) == 0);
    assert(*(unsigned int *)(visual_task.bytes + 0x48) == 0);
    assert(*(void **)(visual_task.bytes + 0x34) == 0);
    assert(*(void **)(visual_task.bytes + 0x38) == 0);
}

static void test_flight_edge_midpoint_contact(void)
{
    setup();
    mock_edge_hit = 1;
    assert(sweep_cube(0.0f, 100.0f, 0.0f,
                      10.0f, 5.0f, 0.0f, 100.0f));
    mock_edge_hit = 0;
    assert(!sweep_cube(0.0f, 100.0f, 0.0f,
                       10.0f, 5.0f, 0.0f, 100.0f));
}

static void test_fast_throw_impact_without_held_pose(void)
{
    setup();
    self_id = 2;
    frozen = 1;
    memset(&incoming, 0, sizeof(incoming));
    incoming.op = ANCHOR_CUBE_REQUEST;
    incoming.sender_cid = 1; incoming.target_cid = 2;
    incoming.room_id = 10;
    incoming.source_session = 4; incoming.target_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = 9; incoming.control_seq = 10;
    incoming.x100 = 1200;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_victim.moving && s_victim.pose_ready && s_victim.x == 0.0f);
    incoming.op = ANCHOR_CUBE_THROW;
    incoming.control_seq = 11;
    incoming.x100 = 1300;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_victim.flight && s_victim.x == 13.0f);
    incoming.op = ANCHOR_CUBE_IMPACT;
    incoming.control_seq = 12;
    incoming.x100 = 1700;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(thaw_count == 1 && impact_x100 == 1700 && !s_victim.moving);
}

static void test_interact_scoping(void)
{
    setup();
    anchor_player_cube_interact_begin(player.bytes, 0);
    assert(D_8015C604 == 3);
    anchor_player_cube_interact_end();
    assert(sent_count == 0);
    visual_active = 1;
    anchor_player_cube_interact_begin(player.bytes, 0);
    assert(D_8015C604 == 0);
    anchor_player_cube_interact_end();
    assert(D_8015C604 == 3);
    assert(sent_count == 1 && last_op == ANCHOR_CUBE_REQUEST);
    assert(last_target == 2 && s_carrier.phase == 1);

    setup();
    visual_active = 1;
    anchor_player_cube_interact_begin(player.bytes, 1);
    assert(D_8015C604 == 3);
    anchor_player_cube_interact_end();
    assert(sent_count == 0);
    anchor_player_cube_interact_begin(player.bytes, 0);
    *(void **)(work.bytes + 0x8c) = visual_task.bytes;
    anchor_player_cube_interact_end();
    assert(D_8015C604 == 3 && sent_count == 0);
}

static void test_grant_native_throw(void)
{
    setup();
    visual_active = 1;
    anchor_player_cube_interact_begin(player.bytes, 0);
    anchor_player_cube_interact_end();
    memset(&incoming, 0, sizeof(incoming));
    incoming.op = ANCHOR_CUBE_GRANT;
    incoming.sender_cid = 2; incoming.target_cid = 1;
    incoming.room_id = 10; incoming.source_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = last_carry;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 2 && held_task_is_ours());
    assert(action_count == 1);
    *(float *)(visual_task.bytes + 0x78) = 10;
    *(float *)(visual_task.bytes + 0x7c) = 8;
    anchor_player_cube_throw_begin(player.bytes);
    func_801E55A0_5A14B0(player.bytes);
    anchor_player_cube_throw_end();
    assert(s_carrier.phase == 3 && !s_carrier.throw_sent);
    anchor_player_cube_tick();
    assert(s_carrier.throw_sent && last_op == ANCHOR_CUBE_THROW);
    mock_hit = 1;
    anchor_player_cube_tick();
    assert(s_carrier.phase == 4);
    assert(shatter_count == 1);
    assert(s_carrier.impact_attack_frames == CUBE_IMPACT_ATTACK_FRAMES);
    assert((visual_task.bytes[0x30] & 2u) != 0);
    assert(*(unsigned int *)(visual_task.bytes + 0x48) == 0xffffffffu);
    assert(*(unsigned short *)(visual_task.bytes + 0x4c) == 0x1cu);
    assert(*(unsigned short *)(visual_task.bytes + 0x4e) == 75u);
    assert(*(void **)(visual_task.bytes + 0x5c) == 0);
    anchor_player_cube_tick();
    assert(last_op == ANCHOR_CUBE_IMPACT && s_carrier.impact_sent);
    assert(s_carrier.impact_attack_frames == CUBE_IMPACT_ATTACK_FRAMES - 1);
    assert((visual_task.bytes[0x30] & 2u) != 0);
    for (int i = 1; i < CUBE_IMPACT_ATTACK_FRAMES; ++i)
    {
        *(void **)(visual_task.bytes + 0x34) = work.bytes;
        *(void **)(visual_task.bytes + 0x38) = work.bytes;
        anchor_player_cube_tick();
    }
    assert(s_carrier.impact_attack_frames == 0);
    assert((visual_task.bytes[0x30] & 2u) == 0);
    assert(*(unsigned int *)(visual_task.bytes + 0x48) == 0);
    assert(*(void **)(visual_task.bytes + 0x34) == 0);
    assert(*(void **)(visual_task.bytes + 0x38) == 0);

    setup();
    visual_active = 1;
    anchor_player_cube_interact_begin(player.bytes, 0);
    anchor_player_cube_interact_end();
    memset(&incoming, 0, sizeof(incoming));
    incoming.op = ANCHOR_CUBE_GRANT;
    incoming.sender_cid = 2; incoming.target_cid = 1;
    incoming.room_id = 10; incoming.source_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = last_carry;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(held_task_is_ours());
    incoming.op = ANCHOR_CUBE_CANCEL;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(!s_carrier.phase && !held_task_is_ours());
    assert(release_count == 1);
}

static void test_victim_lock_impact_cancel(void)
{
    setup();
    self_id = 2;
    frozen = 1;
    memset(&incoming, 0, sizeof(incoming));
    incoming.op = ANCHOR_CUBE_REQUEST;
    incoming.sender_cid = 1; incoming.target_cid = 2;
    incoming.room_id = 10;
    incoming.source_session = 4; incoming.target_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = 9; incoming.control_seq = 10;
    incoming.x100 = 1200;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_victim.moving && last_op == ANCHOR_CUBE_GRANT);
    assert(last_target == 1);
    incoming.op = ANCHOR_CUBE_POSE; incoming.control_seq = 11;
    incoming.x100 = 1300;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_victim.pose_ready && s_victim.x == 13.0f);
    incoming.op = ANCHOR_CUBE_THROW; incoming.control_seq = 12;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(s_victim.flight);
    incoming.op = ANCHOR_CUBE_IMPACT; incoming.control_seq = 13;
    incoming.x100 = 1700;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(thaw_count == 1 && impact_x100 == 1700);
    assert(!s_victim.moving);

    setup(); self_id = 2; frozen = 1;
    incoming.op = ANCHOR_CUBE_REQUEST;
    incoming.sender_cid = 1; incoming.target_cid = 2;
    incoming.room_id = 10; incoming.source_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = 9; incoming.control_seq = 10;
    incoming.x100 = 1200;
    pending_incoming = 1;
    anchor_player_cube_tick();
    incoming.op = ANCHOR_CUBE_CANCEL; incoming.control_seq = 11;
    pending_incoming = 1;
    anchor_player_cube_tick();
    assert(!s_victim.moving && frozen && thaw_count == 0);

    setup(); self_id = 2; frozen = 1;
    incoming.op = ANCHOR_CUBE_REQUEST;
    incoming.sender_cid = 1; incoming.target_cid = 2;
    incoming.room_id = 10; incoming.source_session = 4;
    incoming.source_epoch = incoming.target_epoch = 5;
    incoming.carry_id = 9; incoming.control_seq = 10;
    incoming.x100 = 1200;
    pending_incoming = 1;
    anchor_player_cube_tick();
    for (int i = 0; i < CUBE_LEASE; ++i)
        anchor_player_cube_tick();
    assert(!s_victim.moving && frozen && thaw_count == 0);
    assert(last_op == ANCHOR_CUBE_CANCEL && last_target == 1);
}

int main(void)
{
    test_raised_carry_and_throw();
    test_interact_scoping();
    test_grant_native_throw();
    test_victim_lock_impact_cancel();
    test_impact_send_failure_releases_carrier();
    test_room_change_clears_impact_attack();
    test_flight_edge_midpoint_contact();
    test_fast_throw_impact_without_held_pose();
    puts("frozen player cube carry tests passed");
    return 0;
}
