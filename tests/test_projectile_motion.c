#include "anchor_projectile_motion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static AnchorProjectileSpawn shot(int kind)
{
    AnchorProjectileSpawn spawn = {
        .id = 1, .kind = kind, .x100 = 100, .y100 = 1000, .z100 = 300,
        .vx100 = 100, .vy100 = 200, .vz100 = 300, .scale100000 = 10000,
    };
    return spawn;
}

static void near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.0001f);
}

static void at(AnchorCollisionVec3 actual, float x, float y, float z)
{
    near(actual.x, x);
    near(actual.y, y);
    near(actual.z, z);
}

static void test_bomb_ballistics_growth_and_fade(void)
{
    int kind;
    for (kind = 0x0e; kind <= 0x10; ++kind)
    {
        AnchorProjectileSpawn spawn = shot(kind);
        AnchorProjectileMotion motion;
        AnchorCollisionVec3 target, impact = {8, 0, 9};
        int i;
        assert(anchor_projectile_motion_init(&motion, &spawn));
        assert(motion.ttl == 90);
        at(motion.position, 1, 10, 3);
        near(motion.scale, 0.1f);
        anchor_projectile_motion_target(&motion, &target);
        at(target, 2, 11.333333f, 6);
        at(motion.position, 1, 10, 3);
        anchor_projectile_motion_step(&motion, 0);
        at(motion.position, 2, 11.333333f, 6);
        near(motion.velocity.y, 1.333333f);
        anchor_projectile_motion_step(&motion, &impact);
        assert(motion.phase == ANCHOR_SHOT_GROW && motion.phase_age == 0);
        at(motion.position, 8, 0, 9);
        near(motion.scale, 0.025f);
        anchor_projectile_motion_target(&motion, &target);
        at(target, 8, 0, 9);
        anchor_projectile_motion_step(&motion, 0);
        near(motion.scale, 0.0259f);
        for (i = 1; i < 7; ++i)
            anchor_projectile_motion_step(&motion, 0);
        assert(motion.phase == ANCHOR_SHOT_FADE && motion.alpha == 255);
        near(motion.scale, 0.15f);
        anchor_projectile_motion_step(&motion, 0);
        assert(motion.alpha == 231 && motion.gray == 116 && motion.alive);
        near(motion.scale, 0.152f);
        for (i = 1; i < 10; ++i)
            anchor_projectile_motion_step(&motion, 0);
        assert(motion.alpha == 15 && motion.alive);
        anchor_projectile_motion_step(&motion, 0);
        assert(motion.alpha == 0 && !motion.alive);
        at(motion.position, 8, 0, 9);
    }
}

static void test_kunai_constant_motion_and_blue_fade(void)
{
    int kind;
    for (kind = 0x1a; kind <= 0x1b; ++kind)
    {
        AnchorProjectileSpawn spawn = shot(kind);
        AnchorProjectileMotion motion;
        AnchorCollisionVec3 impact = {20, 30, 40};
        int i;
        assert(anchor_projectile_motion_init(&motion, &spawn));
        for (i = 0; i < 58; ++i)
            anchor_projectile_motion_step(&motion, 0);
        assert(motion.alive && motion.ttl == 1);
        at(motion.position, 59, 126, 177);
        anchor_projectile_motion_step(&motion, 0);
        assert(!motion.alive);
        at(motion.position, 59, 126, 177); /* TTL expiry skips the move. */
        assert(anchor_projectile_motion_init(&motion, &spawn));
        anchor_projectile_motion_step(&motion, &impact);
        assert(motion.phase == ANCHOR_SHOT_IMPACT && motion.alpha == 255);
        near(motion.scale, 0.15f);
        for (i = 0; i < 21; ++i)
            anchor_projectile_motion_step(&motion, 0);
        assert(motion.alive && motion.alpha == 3);
        at(motion.position, 20, 30, 40);
        anchor_projectile_motion_step(&motion, 0);
        assert(!motion.alive && motion.alpha == 0);
    }
}

static void test_coin_ttl_and_native_clip_completion(void)
{
    AnchorProjectileSpawn spawn = shot(1);
    AnchorProjectileMotion motion;
    AnchorCollisionVec3 impact = {4, 5, 6};
    int i;
    assert(anchor_projectile_motion_init(&motion, &spawn));
    for (i = 0; i < 118; ++i)
        anchor_projectile_motion_step(&motion, 0);
    assert(motion.alive && motion.ttl == 1);
    at(motion.position, 119, 246, 357);
    anchor_projectile_motion_step(&motion, 0);
    assert(!motion.alive);
    at(motion.position, 119, 246, 357);
    assert(anchor_projectile_motion_init(&motion, &spawn));
    anchor_projectile_motion_step(&motion, &impact);
    motion.clip_frames = 10;
    for (i = 0; i < 5; ++i)
        anchor_projectile_motion_step(&motion, 0);
    assert(motion.alive && motion.frame == 7.5f);
    anchor_projectile_motion_step(&motion, 0);
    assert(!motion.alive && motion.frame == 9.0f);
    assert(anchor_projectile_motion_init(&motion, &spawn));
    anchor_projectile_motion_step(&motion, &impact);
    for (i = 0; i < 30; ++i)
        anchor_projectile_motion_step(&motion, 0);
    assert(!motion.alive); /* Missing frame metadata still has a finite life. */
}

static void test_yae_acceleration_clamp_and_impact_clip(void)
{
    int kind;
    for (kind = 0x17; kind <= 0x19; ++kind)
    {
        AnchorProjectileSpawn spawn = shot(kind);
        AnchorProjectileMotion motion;
        AnchorCollisionVec3 target, impact = {2, 3, 4};
        int i;
        spawn.vx100 = 100;
        spawn.vy100 = spawn.vz100 = 0;
        assert(anchor_projectile_motion_init(&motion, &spawn));
        anchor_projectile_motion_target(&motion, &target);
        at(target, 3, 10, 3);
        for (i = 0; i < 6; ++i)
            anchor_projectile_motion_step(&motion, 0);
        at(motion.velocity, 6, 0, 0);
        at(motion.position, 27, 10, 3);
        anchor_projectile_motion_step(&motion, &impact);
        assert(motion.phase == ANCHOR_SHOT_IMPACT);
        near(motion.scale, 0.3f);
        motion.clip_frames = 10;
        for (i = 0; i < 8; ++i)
            anchor_projectile_motion_step(&motion, 0);
        assert(motion.alive && motion.frame == 8);
        anchor_projectile_motion_step(&motion, 0);
        assert(!motion.alive && motion.frame == 9);
        spawn.vx100 = 300;
        spawn.vy100 = 400;
        assert(anchor_projectile_motion_init(&motion, &spawn));
        for (i = 0; i < 3; ++i)
            anchor_projectile_motion_step(&motion, 0);
        at(motion.velocity, 3.6f, 4.8f, 0);
    }
}

static void test_charged_reversal_and_impact_return(void)
{
    AnchorProjectileSpawn spawn = shot(2);
    AnchorProjectileMotion motion;
    AnchorCollisionVec3 impact = {30, 10, 3};
    int i;
    spawn.vx100 = 500; /* Captured velocity is 5/6 of the launch velocity. */
    spawn.vy100 = spawn.vz100 = 0;
    assert(anchor_projectile_motion_init(&motion, &spawn));
    for (i = 0; i < 5; ++i)
        anchor_projectile_motion_step(&motion, 0);
    assert(motion.phase == ANCHOR_SHOT_RETURN);
    at(motion.velocity, 0, 0, 0);
    at(motion.position, 11, 10, 3);
    anchor_projectile_motion_step(&motion, 0);
    at(motion.velocity, -1, 0, 0);
    at(motion.position, 10, 10, 3);
    assert(anchor_projectile_motion_init(&motion, &spawn));
    anchor_projectile_motion_step(&motion, 0);
    anchor_projectile_motion_step(&motion, &impact);
    assert(motion.phase == ANCHOR_SHOT_RETURN && motion.ttl == 3);
    at(motion.velocity, -3, 0, 0);
    anchor_projectile_motion_step(&motion, 0);
    at(motion.position, 26, 10, 3);
    anchor_projectile_motion_step(&motion, 0);
    at(motion.position, 21, 10, 3);
    anchor_projectile_motion_step(&motion, 0);
    assert(!motion.alive);
    at(motion.position, 21, 10, 3);
}

static void test_camera_flash_and_invalid_inputs(void)
{
    AnchorProjectileMotion motion, unchanged;
    AnchorProjectileSpawn spawn = shot(0x0c);
    AnchorCollisionVec3 target;
    int kind, i;
    for (kind = 0x0c; kind <= 0x0d; ++kind)
    {
        spawn.kind = kind;
        assert(anchor_projectile_motion_init(&motion, &spawn));
        assert(motion.phase == ANCHOR_SHOT_FLASH);
        anchor_projectile_motion_target(&motion, &target);
        at(target, 1, 10, 3);
        for (i = 0; i < 11; ++i)
            anchor_projectile_motion_step(&motion, 0);
        assert(!motion.alive && motion.alpha == 0);
    }
    memset(&motion, 0x5a, sizeof(motion));
    unchanged = motion;
    spawn.kind = 3;
    assert(!anchor_projectile_motion_init(&motion, &spawn));
    assert(memcmp(&motion, &unchanged, sizeof(motion)) == 0);
    assert(!anchor_projectile_kind_supported(0));
    assert(!anchor_projectile_kind_supported(0x11));
    assert(!anchor_projectile_kind_supported(0x16));
    assert(!anchor_projectile_kind_supported(0x1c));
    assert(!anchor_projectile_kind_supported(255));
    spawn.kind = 1;
    spawn.scale100000 = 0;
    assert(!anchor_projectile_motion_init(&motion, &spawn));
    spawn.scale100000 = 1000001;
    assert(!anchor_projectile_motion_init(&motion, &spawn));
    assert(!anchor_projectile_motion_init(0, &spawn));
    assert(!anchor_projectile_motion_init(&motion, 0));
}

int main(void)
{
    test_bomb_ballistics_growth_and_fade();
    test_kunai_constant_motion_and_blue_fade();
    test_coin_ttl_and_native_clip_completion();
    test_yae_acceleration_clamp_and_impact_clip();
    test_charged_reversal_and_impact_return();
    test_camera_flash_and_invalid_inputs();
    puts("Projectile motion tests passed");
    return 0;
}
