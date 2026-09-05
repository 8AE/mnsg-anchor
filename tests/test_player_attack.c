#include "anchor_player_attack.h"
#include "anchor_player_models.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static AnchorPlayerHitTarget s_targets[ANCHOR_PLAYER_MODEL_MAX];
static int s_target_count;
static int s_sent;
static int s_send_ok;
static int s_last_cid;
static int s_last_epoch;
static AnchorCollisionVec3 s_last_center;
static int s_actor_ids[70];

int anchor_player_models_get_hit_targets(AnchorPlayerHitTarget *out, int capacity)
{
    assert(capacity >= s_target_count);
    memcpy(out, s_targets, sizeof(*out) * s_target_count);
    return s_target_count;
}

int anchor_send_player_hit(int cid, int epoch, float x, float y, float z)
{
    if (!s_send_ok)
        return 0;
    ++s_sent;
    s_last_cid = cid;
    s_last_epoch = epoch;
    s_last_center = (AnchorCollisionVec3){x, y, z};
    return 1;
}

static AnchorPlayerAttackSample sample(int actor)
{
    AnchorPlayerAttackSample result = {
        &s_actor_ids[actor], &s_actor_ids[actor], 0x900157c, 0x60010000,
        0, 1, {0, 10, 0}, 2};
    return result;
}

static void setup(void)
{
    anchor_player_attack_reset();
    s_sent = 0;
    s_send_ok = 1;
    s_target_count = 1;
    s_targets[0] = (AnchorPlayerHitTarget){17, 3, {{0, 0, 0}, 5, 20}};
    anchor_player_attack_begin_frame(1, 9);
}

static void actual_geometry_and_disabled_attack(void)
{
    AnchorPlayerAttackSample a = sample(0);
    setup();
    a.descriptor = 0; /* Idle/reset task has no native outgoing descriptor. */
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    a.descriptor = 1;
    a.radius = 0;
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    a.radius = 2;
    a.center.x = 7.01f;
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    a.center.x = 0;
    a.center.y = 22.01f;
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    a.center.y = -2.01f;
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    a.center.x = 7;
    a.center.y = 22; /* Native expanded cylinder, including flat end. */
    anchor_player_attack_observe(&a);
    assert(s_sent == 1 && s_last_cid == 17 && s_last_epoch == 3);
    assert(s_last_center.x == 7 && s_last_center.y == 22);
}

static void one_hit_per_swing_and_target(void)
{
    AnchorPlayerAttackSample a = sample(0);
    int frame;
    int sphere;
    setup();
    s_targets[1] = (AnchorPlayerHitTarget){18, 7, {{2, 0, 0}, 5, 20}};
    s_target_count = 2;
    for (frame = 0; frame < 8; ++frame)
    {
        anchor_player_attack_begin_frame(1, 9);
        a.frame = (float)frame;
        /* Several spheres, each visited for four native victim groups. */
        for (sphere = 0; sphere < 12; ++sphere)
            anchor_player_attack_observe(&a);
    }
    assert(s_sent == 2);
    a.frame = 0; /* Back-to-back combo starts another native clip. */
    anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_observe(&a);
    assert(s_sent == 4);
    a.descriptor = 2;
    anchor_player_attack_observe(&a);
    assert(s_sent == 6);
}

static void projectile_lifetime_and_rearm(void)
{
    AnchorPlayerAttackSample a = sample(1);
    setup();
    a.descriptor = 0xffffffffu;
    a.is_player = 0;
    anchor_player_attack_observe(&a);
    anchor_player_attack_begin_frame(1, 9);
    a.frame = 10;
    anchor_player_attack_observe(&a);
    anchor_player_attack_begin_frame(1, 9);
    a.frame = 0;
    a.animation += 4;
    anchor_player_attack_observe(&a);
    assert(s_sent == 1); /* Looping projectile visuals cannot do repeated damage. */
    anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_begin_frame(1, 9); /* Absent from one complete scan. */
    anchor_player_attack_observe(&a);
    assert(s_sent == 2);
}

static void transport_failure_and_epoch_changes(void)
{
    AnchorPlayerAttackSample a = sample(0);
    setup();
    s_send_ok = 0;
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    s_send_ok = 1;
    anchor_player_attack_observe(&a);
    assert(s_sent == 1);
    ++s_targets[0].epoch;
    anchor_player_attack_observe(&a);
    assert(s_sent == 2 && s_last_epoch == 4);
    anchor_player_attack_begin_frame(0, 9);
    anchor_player_attack_observe(&a);
    assert(s_sent == 2);
    anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_observe(&a);
    assert(s_sent == 3);
    anchor_player_attack_begin_frame(1, 10);
    anchor_player_attack_observe(&a);
    assert(s_sent == 4);
}

static void bounded_cache_and_invalid_spheres(void)
{
    AnchorPlayerAttackSample a;
    int i;
    setup();
    a = sample(0);
    a.radius = NAN;
    anchor_player_attack_observe(&a);
    a.radius = 2;
    a.center.x = INFINITY;
    anchor_player_attack_observe(&a);
    assert(s_sent == 0);
    for (i = 0; i < 70; ++i)
    {
        a = sample(i);
        anchor_player_attack_observe(&a);
    }
    assert(s_sent == 64);
    for (i = 0; i < 70; ++i)
    {
        a = sample(i);
        anchor_player_attack_observe(&a);
    }
    assert(s_sent == 64); /* Capacity never evicts an already hit attack. */
}

int main(void)
{
    actual_geometry_and_disabled_attack();
    one_hit_per_swing_and_target();
    projectile_lifetime_and_rearm();
    transport_failure_and_epoch_changes();
    bounded_cache_and_invalid_spheres();
    puts("player attack geometry and episode tests passed");
    return 0;
}
