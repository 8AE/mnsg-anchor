#include "anchor_remote_collision.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

unsigned char D_800C7AE0;
unsigned char D_800C7AE3;

static int near(float a, float b)
{
    return fabsf(a - b) < 0.03f;
}

static AnchorCollisionBody body(float x, float y, float z, float radius, float height)
{
    AnchorCollisionBody b = {{x, y, z}, radius, height};
    return b;
}

static void fast_crossing_and_retreat(void)
{
    AnchorCollisionBody moving = body(-50, 0, 0, 5, 20);
    AnchorCollisionBody peer = body(0, 0, 0, 5, 20);
    AnchorCollisionVec3 target = {50, 0, 0};
    AnchorCollisionVec3 result;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(near(result.x, -10.02f));
    assert(near(result.y, 0) && near(result.z, 0));
    moving.position = result;
    target.x = -60;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(near(result.x, -60));
}

static void slide_and_clearance(void)
{
    AnchorCollisionBody moving = body(-20, 0, -5, 5, 20);
    AnchorCollisionBody peer = body(0, 0, 0, 5, 20);
    AnchorCollisionVec3 target = {20, 0, 5};
    AnchorCollisionVec3 result;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(result.x < 20 && result.z < 5);
    assert(result.x * result.x + result.z * result.z >= 100.0f);
    moving = body(-20, 21, 0, 5, 20);
    target = (AnchorCollisionVec3){20, 21, 0};
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(near(result.x, 20) && near(result.y, 21));
}

static void vertical_crossing(void)
{
    AnchorCollisionBody moving = body(0, 50, 0, 5, 20);
    AnchorCollisionBody peer = body(0, 0, 0, 5, 20);
    AnchorCollisionVec3 target = {0, -50, 0};
    AnchorCollisionVec3 result;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(near(result.y, 20));
    moving = body(0, -50, 0, 5, 20);
    target.y = 50;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(near(result.y, -20));
}

static void coincident_spawn_and_mini(void)
{
    AnchorCollisionBody moving = body(0, 0, 0, 5, 20);
    AnchorCollisionBody peer = body(0, 0, 0, 5, 20);
    AnchorCollisionVec3 target = {0, 0, 0};
    AnchorCollisionVec3 result;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(isfinite(result.x) && near(result.x, 10.02f));
    moving = body(-20, 0, 0, 1.625f, 4);
    target.x = 20;
    anchor_collision_move_peers(&moving, &target, &peer, 1, &result);
    assert(near(result.x, -6.645f));
}

static void nearest_peer_and_empty_list(void)
{
    AnchorCollisionBody moving = body(-50, 0, 0, 5, 20);
    AnchorCollisionBody peers[2] = {
        {{20, 0, 0}, 5, 20}, {{0, 0, 0}, 5, 20}
    };
    AnchorCollisionVec3 target = {50, 0, 0};
    AnchorCollisionVec3 result;
    anchor_collision_move_peers(&moving, &target, peers, 2, &result);
    assert(near(result.x, -10.02f));
    anchor_collision_move_peers(&moving, &target, 0, 0, &result);
    assert(near(result.x, 50));
}

static void native_script_gate(void)
{
    D_800C7AE0 = 0;
    D_800C7AE3 = 0;
    assert(!anchor_remote_collision_is_scripted());
    D_800C7AE0 = 1;
    assert(anchor_remote_collision_is_scripted());
    D_800C7AE0 = 2;
    assert(anchor_remote_collision_is_scripted());
    D_800C7AE0 = 4;
    assert(!anchor_remote_collision_is_scripted());
    D_800C7AE3 = 1;
    assert(anchor_remote_collision_is_scripted());
    D_800C7AE3 = 0;
    assert(!anchor_remote_collision_is_scripted());
}

int main(void)
{
    fast_crossing_and_retreat();
    slide_and_clearance();
    vertical_crossing();
    coincident_spawn_and_mini();
    nearest_peer_and_empty_list();
    native_script_gate();
    puts("collision math and scripted gate tests passed");
    return 0;
}
