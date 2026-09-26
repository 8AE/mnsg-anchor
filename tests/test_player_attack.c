#include "combat/anchor_player_attack.h"
#include "player/anchor_player_models.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AnchorPlayerHitTarget s_targets[256];
static int s_fail_alloc;
static int s_target_count;
static int s_sent;
static int s_send_ok;
static int s_last_cid;
static int s_last_epoch;
static int s_last_damage;
static AnchorCollisionVec3 s_last_center;
static int s_actor_ids[140];

void *recomp_alloc(unsigned long size)
{
    return s_fail_alloc ? 0 : malloc(size);
}

void recomp_free(void *memory)
{
    free(memory);
}

int anchor_player_models_capacity(void)
{
    return 256;
}

int anchor_player_models_get_hit_targets(AnchorPlayerHitTarget *out, int capacity)
{
    assert(capacity >= s_target_count);
    memcpy(out, s_targets, sizeof(*out) * s_target_count);
    return s_target_count;
}

int anchor_send_player_hit(int cid, int epoch, float x, float y, float z,
                           int kind, int damage)
{
    assert(kind == 0);
    if (!s_send_ok)
        return 0;
    ++s_sent;
    s_last_cid = cid;
    s_last_epoch = epoch;
    s_last_damage = damage;
    s_last_center = (AnchorCollisionVec3){x, y, z};
    return 1;
}

static AnchorPlayerAttackSample sample(int actor)
{
    AnchorPlayerAttackSample result = {
        &s_actor_ids[actor], &s_actor_ids[actor], 0x900157c, 0x60010000,
        0, 1, {0, 10, 0}, 2};
    result.damage = 1;
    return result;
}

static void setup(void)
{
    anchor_player_attack_reset();
    s_sent = 0;
    s_last_damage = 0;
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
    a.is_projectile = 1;
    assert(anchor_player_attack_observe(&a) == 1);
    anchor_player_attack_begin_frame(1, 9);
    a.frame = 10;
    assert(anchor_player_attack_observe(&a) == 1);
    anchor_player_attack_begin_frame(1, 9);
    a.frame = 0;
    a.animation += 4;
    assert(anchor_player_attack_observe(&a) == 1);
    assert(s_sent == 1); /* Looping projectile visuals cannot do repeated damage. */
    anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_begin_frame(1, 9); /* A paused shot stays consumed. */
    assert(anchor_player_attack_observe(&a) == 1);
    assert(s_sent == 1);
    anchor_player_attack_forget_task(a.task); /* Native task was recycled. */
    anchor_player_attack_observe(&a);
    assert(s_sent == 2);
}

static void projectile_stops_at_first_remote_player(void)
{
    AnchorPlayerAttackSample a = sample(1);
    setup();
    a.is_player = 0;
    a.is_projectile = 1;
    s_targets[1] = (AnchorPlayerHitTarget){18, 7, {{0, 0, 0}, 5, 20}};
    s_target_count = 2;
    assert(anchor_player_attack_observe(&a) == 1);
    assert(s_sent == 1);
    assert(anchor_player_attack_observe(&a) == 1);
    assert(s_sent == 1);
    anchor_player_attack_begin_frame(1, 9);
    a.frame = 10;
    assert(anchor_player_attack_observe(&a) == 1);
    assert(s_sent == 1);
}

static void nonprojectile_helper_remains_multitarget(void)
{
    AnchorPlayerAttackSample a = sample(1);
    setup();
    a.is_player = 0;
    a.is_projectile = 0;
    s_targets[1] = (AnchorPlayerHitTarget){18, 7, {{0, 0, 0}, 5, 20}};
    s_target_count = 2;
    assert(anchor_player_attack_observe(&a) == 0);
    assert(s_sent == 2);
    assert(anchor_player_attack_observe(&a) == 0);
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

static void damage_is_forwarded_once_per_attack_episode(void)
{
    AnchorPlayerAttackSample a = sample(0);
    setup();
    a.damage = 4;
    anchor_player_attack_observe(&a);
    assert(s_sent == 1 && s_last_damage == 4);
    a.damage = 2;
    anchor_player_attack_observe(&a);
    assert(s_sent == 1); /* Same episode cannot be repriced or resent. */
    a.frame = -1;
    anchor_player_attack_observe(&a);
    assert(s_sent == 2 && s_last_damage == 2);
    a.damage = 0;
    anchor_player_attack_observe(&a);
    a.damage = 5;
    anchor_player_attack_observe(&a);
    assert(s_sent == 2);
}

static void native_attack_kinds_match_damage_table(void)
{
    assert(anchor_player_attack_damage_for_kind(0x15) == 1);
    assert(anchor_player_attack_damage_for_kind(0x16) == 2);
    assert(anchor_player_attack_damage_for_kind(0x17) == 4);
    assert(anchor_player_attack_damage_for_kind(0x19) == 1);
    assert(anchor_player_attack_damage_for_kind(0x1f) == 1);
    assert(anchor_player_attack_damage_for_kind(0x1b) == 3);
    assert(anchor_player_attack_damage_for_kind(0x23) == 2);
    assert(anchor_player_attack_damage_for_kind(0x24) == 4);
    assert(anchor_player_attack_damage_for_kind(0x22) == 8);
    assert(anchor_player_attack_damage_for_kind(0x1a) == 1);
}

static void cube_splash_hits_each_player_once_for_gold_damage(void)
{
    AnchorPlayerAttackSample a = sample(1);
    setup();
    a.is_player = 0;
    a.is_impact_splash = 1;
    a.episode_id = 40;
    a.splash_occupant_cid = 17;
    a.splash_occupant_epoch = 3;
    a.damage = 4;
    s_targets[1] = (AnchorPlayerHitTarget){18, 7, {{0, 0, 0}, 5, 20}};
    s_target_count = 2;
    assert(anchor_player_attack_observe(&a) == 0);
    assert(s_sent == 1 && s_last_cid == 18 && s_last_damage == 4);
    anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_begin_frame(1, 9); /* A scan gap cannot rearm impact. */
    a.frame = -1;
    a.animation++; /* Native object clip can change during impact. */
    assert(anchor_player_attack_observe(&a) == 0);
    assert(s_sent == 1);
    s_targets[0].epoch = 4; /* Same CID, different life is eligible. */
    assert(anchor_player_attack_observe(&a) == 0);
    assert(s_sent == 2 && s_last_cid == 17 && s_last_epoch == 4);
    s_targets[0].epoch = 3;
    a.episode_id = 41; /* A later throw reuses the visual task. */
    assert(anchor_player_attack_observe(&a) == 0);
    assert(s_sent == 3 && s_last_cid == 18 && s_last_damage == 4);
}

static void ordinary_attack_can_hit_cube_occupant(void)
{
    AnchorPlayerAttackSample a = sample(0);
    setup();
    a.splash_occupant_cid = 17;
    a.splash_occupant_epoch = 3;
    anchor_player_attack_observe(&a);
    assert(s_sent == 1 && s_last_cid == 17 && s_last_epoch == 3);
}

static void stale_cube_splash_episode_releases_its_hit_history(void)
{
    AnchorPlayerAttackSample a = sample(1);
    int frame;
    setup();
    a.is_impact_splash = 1;
    a.episode_id = 51;
    a.splash_occupant_cid = 18;
    a.splash_occupant_epoch = 7;
    a.damage = 4;
    anchor_player_attack_observe(&a);
    assert(s_sent == 1);
    for (frame = 0; frame < 16; ++frame)
        anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_observe(&a);
    assert(s_sent == 1); /* A brief gap retains the per-target hit. */
    for (frame = 0; frame < 17; ++frame)
        anchor_player_attack_begin_frame(1, 9);
    anchor_player_attack_observe(&a);
    assert(s_sent == 2 && s_last_damage == 4);
}

static void growing_cache_and_invalid_spheres(void)
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
    for (i = 0; i < 140; ++i)
    {
        a = sample(i);
        anchor_player_attack_observe(&a);
    }
    assert(s_sent == 140);
    for (i = 0; i < 140; ++i)
    {
        a = sample(i);
        anchor_player_attack_observe(&a);
    }
    assert(s_sent == 140); /* Capacity never evicts an already hit attack. */
}

static void growing_roster_and_failed_dedup_reserve(void)
{
    AnchorPlayerAttackSample a = sample(0);
    int i;
    setup();
    s_target_count = 100;
    for (i = 0; i < 200; ++i)
        s_targets[i] = (AnchorPlayerHitTarget){i + 1, 3, {{0, 0, 0}, 5, 20}};
    anchor_player_attack_observe(&a);
    assert(s_sent == 100);
    anchor_player_attack_observe(&a);
    assert(s_sent == 100); /* No truncation or duplicate damage after slot25. */
    s_target_count = 200;
    s_fail_alloc = 1;
    anchor_player_attack_observe(&a);
    assert(s_sent == 128); /* Existing capacity works; unsaved hits never send. */
    anchor_player_attack_observe(&a);
    assert(s_sent == 128);
    s_fail_alloc = 0;
    anchor_player_attack_observe(&a);
    assert(s_sent == 200); /* Retry sends only the previously unsaved hits. */
    anchor_player_attack_observe(&a);
    assert(s_sent == 200);
    ++s_targets[199].epoch;
    anchor_player_attack_observe(&a);
    assert(s_sent == 201 && s_last_cid == 200 && s_last_epoch == 4);
    s_target_count = 1;
    anchor_player_attack_observe(&a);
    assert(s_sent == 201); /* Shrinking the roster retains attack dedup. */
}

int main(void)
{
    actual_geometry_and_disabled_attack();
    one_hit_per_swing_and_target();
    projectile_lifetime_and_rearm();
    projectile_stops_at_first_remote_player();
    nonprojectile_helper_remains_multitarget();
    transport_failure_and_epoch_changes();
    damage_is_forwarded_once_per_attack_episode();
    native_attack_kinds_match_damage_table();
    cube_splash_hits_each_player_once_for_gold_damage();
    ordinary_attack_can_hit_cube_occupant();
    stale_cube_splash_episode_releases_its_hit_history();
    growing_cache_and_invalid_spheres();
    growing_roster_and_failed_dedup_reserve();
    puts("player attack geometry and episode tests passed");
    return 0;
}
