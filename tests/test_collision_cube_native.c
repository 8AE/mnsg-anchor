#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ANCHOR_COLLISION_CUBE_HOST_TEST
#include "../src/combat/anchor_collision_cube.c"

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
static unsigned long long task_storage[0x80 / 8];
static unsigned long long object_storage[0x80 / 8];
static AnchorFreezeCubeCollision visible_cube;
static int connected, loaded, scripted, visual_active, epoch;

int anchor_is_connected(void) { return connected; }
int item_sync_save_is_loaded(void) { return loaded; }
int anchor_remote_collision_is_scripted(void) { return scripted; }
int anchor_player_models_get_epoch(void) { return epoch; }
int anchor_player_freeze_visual_get_cubes(AnchorFreezeCubeCollision *out,
                                          int capacity)
{
    if (!visual_active || !out || capacity <= 0)
        return 0;
    out[0] = visible_cube;
    return 1;
}
int anchor_player_freeze_visual_has_cube(int cid, int session, int target_epoch)
{
    return visual_active && cid == visible_cube.cid &&
           session == visible_cube.session &&
           target_epoch == visible_cube.epoch;
}

static void setup(void)
{
    memset(task_storage, 0, sizeof(task_storage));
    memset(object_storage, 0, sizeof(object_storage));
    memset(s_floor_scopes, 0, sizeof(s_floor_scopes));
    memset(s_ceiling_scopes, 0, sizeof(s_ceiling_scopes));
    memset(s_ground_scopes, 0, sizeof(s_ground_scopes));
    memset(s_ray_overrides, 0, sizeof(s_ray_overrides));
    s_floor_depth = s_ceiling_depth = s_ground_depth = s_ray_depth = 0;
    D_801FC604_5B8514 = task_storage;
    D_801FC60C_5B851C = object_storage;
    *(void **)((unsigned char *)task_storage + 0x18) = object_storage;
    D_800C7AB2 = 10;
    epoch = 5;
    connected = loaded = visual_active = 1;
    scripted = 0;
    visible_cube.cid = 2;
    visible_cube.session = 3;
    visible_cube.epoch = 4;
    visible_cube.cube.min = (AnchorCollisionVec3){-10.0f, 0.0f, -10.0f};
    visible_cube.cube.max = (AnchorCollisionVec3){10.0f, 20.0f, 10.0f};
}

static void floor_ray(NativeCubeRayHit *hit, float range)
{
    anchor_collision_cube_floor_begin(task_storage);
    anchor_collision_cube_ray_begin(hit, 0.0f, 30.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f, range);
    anchor_collision_cube_ray_end();
    anchor_collision_cube_floor_end();
}

static void ground_ray(NativeCubeRayHit *hit, float origin_y, float range)
{
    anchor_collision_cube_ground_begin(task_storage, object_storage);
    anchor_collision_cube_ray_begin(hit, 0.0f, origin_y, 0.0f,
                                    0.0f, -1.0f, 0.0f, range);
    anchor_collision_cube_ray_end();
    anchor_collision_cube_ground_end();
}

int main(void)
{
    NativeCubeRayHit hit;
    setup();
    memset(&hit, 0, sizeof(hit));
    hit.dynamic_object = hit.dynamic_task = 123u;
    floor_ray(&hit, 40.0f);
    assert(hit.hit == 0x7fffu && hit.delta[1] == -10.0f);
    assert(hit.normal[1] == 1.0f && hit.distance_squared == 100.0f);
    assert(hit.dynamic_object == 0 && hit.dynamic_task == 0);

    /* The real ground selector casts from high above proposed feet. A fast
     * drop can put the later floor ray below top; this earlier long ray must
     * still select the cube and let native code set grounded state. */
    memset(&hit, 0, sizeof(hit));
    ground_ray(&hit, 190.0f, 200.0f);
    assert(hit.hit == 0x7fffu && hit.delta[1] == -170.0f);
    assert(hit.normal[1] == 1.0f && hit.distance_squared == 28900.0f);
    memset(&hit, 0, sizeof(hit));
    ground_ray(&hit, 190.0f, 160.0f);
    assert(hit.hit == 0); /* Cube top is outside the native ray range. */

    memset(&hit, 0, sizeof(hit));
    hit.hit = 0x7fffu;
    hit.distance_squared = 10000.0f;
    hit.delta[1] = -100.0f;
    ground_ray(&hit, 190.0f, 200.0f);
    assert(hit.distance_squared == 10000.0f && hit.delta[1] == -100.0f);

    memset(&hit, 0, sizeof(hit));
    anchor_collision_cube_ground_begin(task_storage, object_storage);
    anchor_collision_cube_ray_begin(&hit, 0.0f, 190.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f, 200.0f);
    ++D_800C7AB2;
    anchor_collision_cube_ray_end();
    anchor_collision_cube_ground_end();
    assert(hit.hit == 0); /* Room change invalidates a pending override. */
    --D_800C7AB2;

    /* A nearer native platform or ground always wins. */
    memset(&hit, 0, sizeof(hit));
    hit.hit = 0x7fffu;
    hit.distance_squared = 25.0f;
    hit.delta[1] = -5.0f;
    floor_ray(&hit, 40.0f);
    assert(hit.distance_squared == 25.0f && hit.delta[1] == -5.0f);
    memset(&hit, 0, sizeof(hit));
    floor_ray(&hit, 5.0f);
    assert(hit.hit == 0);

    /* The selector's owner, room and epoch must still match at ray return. */
    memset(&hit, 0, sizeof(hit));
    anchor_collision_cube_floor_begin(task_storage);
    anchor_collision_cube_ray_begin(&hit, 0.0f, 30.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f, 40.0f);
    ++epoch;
    anchor_collision_cube_ray_end();
    anchor_collision_cube_floor_end();
    assert(hit.hit == 0);

    setup();
    memset(&hit, 0, sizeof(hit));
    anchor_collision_cube_ceiling_begin(task_storage);
    anchor_collision_cube_ray_begin(&hit, 0.0f, -10.0f, 0.0f,
                                    0.0f, 1.0f, 0.0f, 30.0f);
    anchor_collision_cube_ray_end();
    anchor_collision_cube_ceiling_end();
    assert(hit.hit == 0x7fffu && hit.delta[1] == 10.0f);
    assert(hit.normal[1] == -1.0f && hit.distance_squared == 100.0f);

    setup();
    visible_cube.cid = 0; /* Never collide with the local player's own ice. */
    memset(&hit, 0, sizeof(hit));
    floor_ray(&hit, 40.0f);
    assert(hit.hit == 0);
    setup();
    visual_active = 0; /* Thaw or hidden visual has no native ray response. */
    memset(&hit, 0, sizeof(hit));
    floor_ray(&hit, 40.0f);
    assert(hit.hit == 0);
    puts("cube native ray scope tests passed");
    return 0;
}
