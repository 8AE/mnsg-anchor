#include "anchor_remote_collision.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition);       \
            return 1;                                                          \
        }                                                                      \
    } while (0)

/* Analytic surfaces stand in for native static and registered dynamic
 * geometry. These mocks implement the native query record contract, while
 * tests exercise the real world-collision wrapper through its public API. */
typedef struct TestPlane
{
    int axis;
    float coordinate;
    float normal;
    float low[3];
    float high[3];
    int dynamic;
} TestPlane;

static TestPlane s_planes[16];
static int s_plane_count;
static int s_static_wall_queries;
static int s_dynamic_wall_queries;
static int s_ray_queries;

static int near_float(float actual, float expected)
{
    return fabsf(actual - expected) < 0.03f;
}

static void clear_scene(void)
{
    s_plane_count = 0;
    s_static_wall_queries = 0;
    s_dynamic_wall_queries = 0;
    s_ray_queries = 0;
}

static TestPlane *add_plane(int axis, float coordinate, float normal, int dynamic)
{
    TestPlane *plane = &s_planes[s_plane_count++];
    int i;

    plane->axis = axis;
    plane->coordinate = coordinate;
    plane->normal = normal;
    plane->dynamic = dynamic;
    for (i = 0; i < 3; ++i)
    {
        plane->low[i] = -1000000.0f;
        plane->high[i] = 1000000.0f;
    }
    return plane;
}

static void add_step(float x, float height)
{
    TestPlane *plane;

    plane = add_plane(0, x, -1.0f, 0);
    plane->low[1] = 0.0f;
    plane->high[1] = height;
    plane = add_plane(1, height, 1.0f, 0);
    plane->low[0] = x;
}

static void write_float(void *out, int offset, float value)
{
    memcpy((unsigned char *)out + offset, &value, sizeof(value));
}

static void write_hit(void *out, unsigned short hit)
{
    memcpy((unsigned char *)out + 0x38, &hit, sizeof(hit));
}

static void no_hit(void *out)
{
    int i;

    memset(out, 0xa5, 0x48);
    /* Finite, plausible poison makes a missing hit-field check observable.
     * Native queries return out even on misses; none of these fields is a
     * valid result unless the u16 at +0x38 equals 0x7fff. */
    for (i = 0; i < 3; ++i)
    {
        write_float(out, 0x18 + 4 * i, 2.0f);
        write_float(out, 0x24 + 4 * i, i == 0 ? 1.0f : 0.0f);
    }
    write_hit(out, 0);
}

void *func_8002C9D4_2D5D4(void *out, float x, float y, float z,
                         float dx, float dy, float dz, float range)
{
    float origin[3] = {x, y, z};
    float direction[3] = {dx, dy, dz};
    float nearest = range + 1.0f;
    const TestPlane *best = 0;
    int i;

    ++s_ray_queries;
    no_hit(out);
    for (i = 0; i < s_plane_count; ++i)
    {
        const TestPlane *plane = &s_planes[i];
        float distance;
        int axis;
        int inside = 1;

        if (direction[plane->axis] * plane->normal >= -0.00001f)
            continue;
        distance = (plane->coordinate - origin[plane->axis]) /
                   direction[plane->axis];
        if (distance < 0.0f || distance > range || distance >= nearest)
            continue;
        /* Native actor-ray selection starts at squared distance 2000 even
         * for a longer requested ray. The wrapper must query in segments. */
        if (plane->dynamic && distance * distance > 2000.0f)
            continue;
        for (axis = 0; axis < 3; ++axis)
        {
            float point = origin[axis] + direction[axis] * distance;
            if (axis != plane->axis &&
                (point < plane->low[axis] || point > plane->high[axis]))
                inside = 0;
        }
        if (inside)
        {
            best = plane;
            nearest = distance;
        }
    }
    if (best)
    {
        write_hit(out, 0x7fff);
        for (i = 0; i < 3; ++i)
        {
            write_float(out, 0x18 + 4 * i, direction[i] * nearest);
            write_float(out, 0x24 + 4 * i, i == best->axis ? best->normal : 0.0f);
        }
        write_float(out, 0x3c, nearest * nearest);
    }
    return out;
}

static void *depenetrate_walls(void *out, float x, float low_y,
                               float high_y, float z, float radius, int dynamic)
{
    float position[3] = {x, 0.0f, z};
    float original[3] = {x, 0.0f, z};
    const TestPlane *last = 0;
    int i;

    no_hit(out);
    for (i = 0; i < s_plane_count; ++i)
    {
        const TestPlane *plane = &s_planes[i];
        float distance;
        int other;

        if (plane->axis == 1 || plane->dynamic != dynamic)
            continue;
        if ((low_y < plane->low[1] || low_y > plane->high[1]) &&
            (high_y < plane->low[1] || high_y > plane->high[1]))
            continue;
        other = plane->axis == 0 ? 2 : 0;
        if (position[other] < plane->low[other] || position[other] > plane->high[other])
            continue;
        distance = (position[plane->axis] - plane->coordinate) * plane->normal;
        if (distance >= radius || distance < -radius)
            continue;
        position[plane->axis] += (radius - distance) * plane->normal;
        last = plane;
    }
    if (last)
    {
        write_hit(out, 0x7fff);
        for (i = 0; i < 3; ++i)
        {
            write_float(out, 0x18 + 4 * i, position[i] - original[i]);
            write_float(out, 0x24 + 4 * i, i == last->axis ? last->normal : 0.0f);
        }
    }
    return out;
}

void *func_8002EB10_2F710(void *out, float x, float low_y,
                         float high_y, float z, float radius)
{
    s_static_wall_queries++;
    return depenetrate_walls(out, x, low_y, high_y, z, radius, 0);
}

void *func_80030730_31330(void *out, float x, float low_y,
                         float high_y, float z, float radius)
{
    s_dynamic_wall_queries++;
    return depenetrate_walls(out, x, low_y, high_y, z, radius, 1);
}

static int test_no_hit_outputs_never_replace_the_target(void)
{
    AnchorCollisionVec3 from = {100.0f, 50.0f, -200.0f};
    AnchorCollisionVec3 target = {220.0f, 125.0f, -400.0f};
    AnchorCollisionVec3 out = {999.0f, 999.0f, 999.0f};

    clear_scene();
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(near_float(out.x, target.x));
    CHECK(near_float(out.y, target.y));
    CHECK(near_float(out.z, target.z));
    CHECK(s_static_wall_queries > 0);
    CHECK(s_dynamic_wall_queries > 0);
    return 0;
}

static int test_wall_collision_preserves_tangential_motion(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {100.0f, 0.0f, 25.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    add_plane(1, 0.0f, 1.0f, 0);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(out.x <= 43.001f);
    CHECK(out.x > 42.8f);
    CHECK(near_float(out.y, 0.0f));
    CHECK(near_float(out.z, 25.0f));
    return 0;
}

static int test_fast_sample_cannot_tunnel_through_a_thin_wall(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {20000.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(out.x <= 43.001f);
    CHECK(out.x > 42.8f);
    return 0;
}

static int test_distant_dynamic_wall_obeys_the_native_query_distance_limit(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {300.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, 150.0f, -1.0f, 1);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(out.x <= 143.001f);
    CHECK(out.x > 142.8f);
    return 0;
}

static int test_extreme_target_bounds_query_work_without_teleporting(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {20000.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(isfinite(out.x));
    CHECK(near_float(out.x, 512.0f));
    CHECK(near_float(out.y, 0.0f));
    CHECK(near_float(out.z, 0.0f));
    return 0;
}

static int test_floor_ceiling_and_standing_preserve_body_clearance(void)
{
    AnchorCollisionVec3 from = {0.0f, 80.0f, 0.0f};
    AnchorCollisionVec3 target = {30.0f, -80.0f, 20.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(1, 0.0f, 1.0f, 0);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(out.y >= -0.001f);
    CHECK(near_float(out.y, 0.0f));
    CHECK(near_float(out.x, 30.0f));
    CHECK(near_float(out.z, 20.0f));
    from = out;
    anchor_collision_world_move(&from, &from, 0.1f, &out);
    CHECK(near_float(out.y, 0.0f));

    add_plane(1, 40.0f, -1.0f, 0);
    target = from;
    target.y = 100.0f;
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(out.y <= 21.501f);
    CHECK(out.y > 21.4f);
    return 0;
}

static int test_airborne_motion_does_not_snap_down_to_the_floor(void)
{
    AnchorCollisionVec3 from = {0.0f, 2.0f, 0.0f};
    AnchorCollisionVec3 target = {10.0f, 2.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(1, 0.0f, 1.0f, 0);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(near_float(out.x, 10.0f));
    CHECK(near_float(out.y, 2.0f));
    return 0;
}

static int test_small_step_can_be_climbed(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {70.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;
    int height;

    for (height = 2; height <= 4; height += 2)
    {
        clear_scene();
        add_plane(1, 0.0f, 1.0f, 0);
        add_step(50.0f, (float)height);
        anchor_collision_world_move(&from, &target, 0.1f, &out);
        CHECK(near_float(out.x, target.x));
        CHECK(near_float(out.y, (float)height));
    }
    return 0;
}

static int test_step_above_wall_clearance_blocks_horizontal_movement(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {70.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(1, 0.0f, 1.0f, 0);
    add_step(50.0f, 6.0f);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    CHECK(out.x <= 43.001f);
    CHECK(out.x > 42.8f);
    CHECK(near_float(out.y, 0.0f));
    return 0;
}

static int test_step_without_headroom_cannot_embed_the_body_in_geometry(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {70.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(1, 0.0f, 1.0f, 0);
    add_plane(1, 19.0f, -1.0f, 0);
    add_step(50.0f, 2.0f);
    anchor_collision_world_move(&from, &target, 0.1f, &out);
    if (out.x >= 50.0f)
        fprintf(stderr, "step without headroom resolved to (%f, %f, %f)\n",
                out.x, out.y, out.z);
    CHECK(out.x < 50.0f);
    CHECK(out.y >= -0.001f);
    CHECK(out.y + 18.5f <= 19.001f);
    return 0;
}

static int test_moving_object_pushes_a_stationary_remote_out(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;
    TestPlane *moving_wall;

    clear_scene();
    moving_wall = add_plane(0, 50.0f, -1.0f, 1);
    anchor_collision_world_move(&from, &from, 0.1f, &out);
    CHECK(near_float(out.x, 0.0f));
    moving_wall->coordinate = 5.0f;
    anchor_collision_world_move(&from, &from, 0.1f, &out);
    CHECK(out.x <= -2.0f + 0.001f);
    CHECK(out.x > -2.03f);
    CHECK(near_float(out.y, 0.0f));
    CHECK(near_float(out.z, 0.0f));
    return 0;
}

static int test_mini_scale_changes_wall_and_ceiling_clearance(void)
{
    AnchorCollisionVec3 from = {0.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 target = {100.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 normal;
    AnchorCollisionVec3 mini;

    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    anchor_collision_world_move(&from, &target, 0.1f, &normal);
    anchor_collision_world_move(&from, &target, 0.025f, &mini);
    CHECK(near_float(normal.x, 43.0f));
    CHECK(near_float(mini.x, 48.25f));

    clear_scene();
    add_plane(1, 40.0f, -1.0f, 0);
    target.x = 0.0f;
    target.y = 100.0f;
    anchor_collision_world_move(&from, &target, 0.1f, &normal);
    anchor_collision_world_move(&from, &target, 0.025f, &mini);
    CHECK(near_float(normal.y, 21.5f));
    CHECK(near_float(mini.y, 35.375f));
    return 0;
}

static int bodies_separate(const AnchorCollisionBody *moving,
                          AnchorCollisionVec3 position,
                          const AnchorCollisionBody *peer)
{
    float dx = position.x - peer->position.x;
    float dz = position.z - peer->position.z;
    float radius = moving->radius + peer->radius;
    if (position.y + moving->height <= peer->position.y ||
        position.y >= peer->position.y + peer->height)
        return 1;
    return dx * dx + dz * dz >= radius * radius - 0.001f;
}

static int test_shared_contact_resolver_separates_coincident_players_beside_wall(void)
{
    AnchorCollisionBody moving = {{43.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peer = moving;
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    CHECK(anchor_collision_move_body(&moving, &moving.position, 0.1f,
                                      &peer, 1, &out));
    CHECK(out.x <= 43.001f);
    CHECK(bodies_separate(&moving, out, &peer));
    CHECK(near_float(out.y, 0.0f));
    return 0;
}

static int test_shared_contact_resolver_stops_local_approach_to_remote_beside_wall(void)
{
    AnchorCollisionBody local = {{0.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody remote = {{43.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionVec3 target = {100.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    CHECK(anchor_collision_move_body(&local, &target, 0.1f,
                                      &remote, 1, &out));
    CHECK(out.x <= 29.001f);
    CHECK(out.x > 28.8f);
    CHECK(bodies_separate(&local, out, &remote));
    return 0;
}

static int test_spawn_escape_keeps_a_third_player_as_a_swept_obstacle(void)
{
    AnchorCollisionBody moving = {{43.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peers[2] = {
        {{43.0f, 0.0f, 0.0f}, 7.0f, 18.5f},
        {{20.0f, 0.0f, 0.0f}, 7.0f, 18.5f},
    };
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    CHECK(anchor_collision_move_body(&moving, &moving.position, 0.1f,
                                      peers, 2, &out));
    CHECK(out.x <= 43.001f);
    CHECK(bodies_separate(&moving, out, &peers[0]));
    CHECK(bodies_separate(&moving, out, &peers[1]));
    CHECK(out.x > peers[1].position.x);
    return 0;
}

static int test_fully_blocked_spawn_reports_failure_without_an_invalid_collider(void)
{
    AnchorCollisionBody moving = {{0.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peer = moving;
    AnchorCollisionVec3 out;

    clear_scene();
    add_plane(0, -8.0f, 1.0f, 0);
    add_plane(0, 8.0f, -1.0f, 0);
    add_plane(2, -8.0f, 1.0f, 0);
    add_plane(2, 8.0f, -1.0f, 0);
    CHECK(!anchor_collision_move_body(&moving, &moving.position, 0.1f,
                                       &peer, 1, &out));
    CHECK(near_float(out.x, moving.position.x));
    CHECK(near_float(out.y, moving.position.y));
    CHECK(near_float(out.z, moving.position.z));
    return 0;
}

static int test_normal_contact_free_motion_uses_only_one_world_resolution(void)
{
    AnchorCollisionBody moving = {{0.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peer = {{100.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionVec3 target = {10.0f, 0.0f, 5.0f};
    AnchorCollisionVec3 out;
    int static_queries;
    int dynamic_queries;

    clear_scene();
    anchor_collision_world_move(&moving.position, &target, 0.1f, &out);
    static_queries = s_static_wall_queries;
    dynamic_queries = s_dynamic_wall_queries;
    clear_scene();
    CHECK(anchor_collision_move_body(&moving, &target, 0.1f, &peer, 1, &out));
    CHECK(near_float(out.x, target.x));
    CHECK(near_float(out.y, target.y));
    CHECK(near_float(out.z, target.z));
    CHECK(s_static_wall_queries == static_queries);
    CHECK(s_dynamic_wall_queries == dynamic_queries);
    return 0;
}

static int test_player_pressure_moves_recipient_until_world_or_peer_blocks(void)
{
    AnchorCollisionBody recipient = {{14.02f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peers[2] = {
        {{0.0f, 0.0f, 0.0f}, 7.0f, 18.5f},
        {{36.0f, 0.0f, 0.0f}, 7.0f, 18.5f},
    };
    int i;
    clear_scene();
    add_plane(0, 30.0f, -1.0f, 0);
    for (i = 0; i < 20; ++i)
    {
        AnchorCollisionVec3 push = anchor_collision_push(&recipient, &peers[0], 2.0f, 0.0f);
        AnchorCollisionVec3 target = recipient.position;
        AnchorCollisionVec3 out;
        target.x += push.x;
        CHECK(anchor_collision_move_body(&recipient, &target, 0.1f, peers, 1, &out));
        recipient.position = out;
        CHECK(recipient.position.x <= 23.001f);
        CHECK(bodies_separate(&recipient, out, &peers[0]));
        /* Next movement snapshot preserves the recipient's new position;
         * the source can approach the now displaced body and push again. */
        peers[0].position.x = recipient.position.x - 14.02f;
    }
    CHECK(recipient.position.x > 22.8f);
    clear_scene();
    recipient.position.x = 14.02f;
    peers[0].position.x = 0.0f;
    for (i = 0; i < 20; ++i)
    {
        AnchorCollisionVec3 push = anchor_collision_push(&recipient, &peers[0], 2.0f, 0.0f);
        AnchorCollisionVec3 target = recipient.position;
        AnchorCollisionVec3 out;
        target.x += push.x;
        CHECK(anchor_collision_move_body(&recipient, &target, 0.1f, peers, 2, &out));
        recipient.position = out;
        peers[0].position.x = recipient.position.x - 14.02f;
        CHECK(bodies_separate(&recipient, out, &peers[1]));
    }
    CHECK(recipient.position.x > 21.8f && recipient.position.x <= 22.001f);
    return 0;
}

static void fill_distant_peers(AnchorCollisionBody *peers, int count)
{
    int i;
    for (i = 0; i < count; ++i)
    {
        peers[i].position = (AnchorCollisionVec3){1000.0f + 20.0f * i,
                                                 0.0f, 1000.0f};
        peers[i].radius = 7.0f;
        peers[i].height = 18.5f;
    }
}

static int test_large_roster_sweeps_the_last_peer_and_preserves_free_motion(void)
{
    AnchorCollisionBody moving = {{0.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peers[128];
    AnchorCollisionVec3 target = {100.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;
    fill_distant_peers(peers, 128);
    clear_scene();
    CHECK(anchor_collision_move_body(&moving, &target, 0.1f, peers, 128, &out));
    CHECK(near_float(out.x, 100.0f));

    /* Only a peer beyond the former limit blocks this fast crossing. The
     * target and origin both lie outside that peer, so endpoint-only checks
     * would allow tunneling. World resolution must retain the same contact. */
    peers[127] = (AnchorCollisionBody){{43.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    add_plane(0, 50.0f, -1.0f, 0);
    CHECK(anchor_collision_move_body(&moving, &target, 0.1f, peers, 128, &out));
    CHECK(out.x <= 29.001f && out.x > 28.8f);
    CHECK(bodies_separate(&moving, out, &peers[127]));
    return 0;
}

static int test_large_roster_spawn_escape_keeps_late_peers_swept(void)
{
    AnchorCollisionBody moving = {{43.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody peers[128];
    AnchorCollisionVec3 out;
    AnchorCollisionVec3 compact;
    int i;
    fill_distant_peers(peers, 128);
    peers[126] = moving;
    peers[127] = (AnchorCollisionBody){{20.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    clear_scene();
    add_plane(0, 50.0f, -1.0f, 0);
    CHECK(anchor_collision_move_body(&moving, &moving.position, 0.1f,
                                      peers + 126, 2, &compact));
    CHECK(anchor_collision_move_body(&moving, &moving.position, 0.1f,
                                      peers, 128, &out));
    CHECK(out.x == compact.x && out.y == compact.y && out.z == compact.z);
    CHECK(out.x <= 43.001f && out.x > peers[127].position.x);
    for (i = 0; i < 128; ++i)
        CHECK(bodies_separate(&moving, out, &peers[i]));

    /* All peers can also overlap the origin: the filtered path is empty,
     * but every one still participates in final clearance validation. */
    for (i = 0; i < 128; ++i)
        peers[i] = moving;
    CHECK(anchor_collision_move_body(&moving, &moving.position, 0.1f,
                                      peers, 128, &out));
    CHECK(out.x <= 43.001f);
    for (i = 0; i < 128; ++i)
        CHECK(bodies_separate(&moving, out, &peers[i]));
    return 0;
}

static int test_remote_actor_contact_ignores_elevator_and_room_meshes(void)
{
    AnchorCollisionBody remote = {{0.0f, 0.0f, 0.0f}, 7.0f, 18.5f};
    AnchorCollisionBody enemy = {{80.0f, 60.0f, 0.0f}, 10.0f, 25.0f};
    AnchorCollisionVec3 target = {40.0f, 60.0f, 0.0f}, out;
    clear_scene();
    add_plane(1, 25.0f, -1.0f, 1); /* Elevator underside. */
    add_plane(0, 20.0f, -1.0f, 0); /* Receiving client's room geometry. */
    CHECK(anchor_collision_move_actors(&remote, &target, 0, 0, &out));
    CHECK(out.x == target.x && out.y == target.y && out.z == target.z);
    CHECK(!s_ray_queries && !s_static_wall_queries && !s_dynamic_wall_queries);

    /* Once the packet reaches the arena, a live enemy/player body still
     * blocks the remote even though meshes remain excluded. */
    remote.position = out;
    target.x = 120.0f;
    CHECK(anchor_collision_move_actors(&remote, &target, &enemy, 1, &out));
    CHECK(out.x > 62.9f && out.x < 63.01f && out.y == target.y);
    CHECK(bodies_separate(&remote, out, &enemy));
    CHECK(!s_ray_queries && !s_static_wall_queries && !s_dynamic_wall_queries);

    /* A stationary platform cannot displace a remote display body. */
    remote.position = (AnchorCollisionVec3){19.0f, 7.0f, 0.0f};
    CHECK(anchor_collision_move_actors(&remote, &remote.position, 0, 0, &out));
    CHECK(out.x == 19.0f && out.y == 7.0f);
    CHECK(!s_ray_queries && !s_static_wall_queries && !s_dynamic_wall_queries);
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_no_hit_outputs_never_replace_the_target();
    failures += test_wall_collision_preserves_tangential_motion();
    failures += test_fast_sample_cannot_tunnel_through_a_thin_wall();
    failures += test_distant_dynamic_wall_obeys_the_native_query_distance_limit();
    failures += test_extreme_target_bounds_query_work_without_teleporting();
    failures += test_floor_ceiling_and_standing_preserve_body_clearance();
    failures += test_airborne_motion_does_not_snap_down_to_the_floor();
    failures += test_small_step_can_be_climbed();
    failures += test_step_above_wall_clearance_blocks_horizontal_movement();
    failures += test_step_without_headroom_cannot_embed_the_body_in_geometry();
    failures += test_moving_object_pushes_a_stationary_remote_out();
    failures += test_mini_scale_changes_wall_and_ceiling_clearance();
    failures += test_shared_contact_resolver_separates_coincident_players_beside_wall();
    failures += test_shared_contact_resolver_stops_local_approach_to_remote_beside_wall();
    failures += test_spawn_escape_keeps_a_third_player_as_a_swept_obstacle();
    failures += test_fully_blocked_spawn_reports_failure_without_an_invalid_collider();
    failures += test_normal_contact_free_motion_uses_only_one_world_resolution();
    failures += test_player_pressure_moves_recipient_until_world_or_peer_blocks();
    failures += test_large_roster_sweeps_the_last_peer_and_preserves_free_motion();
    failures += test_large_roster_spawn_escape_keeps_late_peers_swept();
    failures += test_remote_actor_contact_ignores_elevator_and_room_meshes();
    return failures != 0;
}
