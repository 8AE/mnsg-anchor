#include "combat/anchor_collision_cube.h"

#include <assert.h>
#include <stdio.h>

static int near(float a, float b)
{
    float difference = a - b;
    return difference > -0.03f && difference < 0.03f;
}

int main(void)
{
    const AnchorCollisionCube cube = {{-5.0f, 0.0f, -5.0f},
                                      {5.0f, 10.0f, 5.0f}};
    AnchorCollisionBody body = {{-20.0f, 0.0f, 0.0f}, 1.0f, 5.0f};
    AnchorCollisionVec3 target = {20.0f, 0.0f, 0.0f};
    AnchorCollisionVec3 out;
    float top = -1.0f;

    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -6.02f) && near(out.z, 0.0f));
    assert(!anchor_collision_cube_side_overlaps(&body, &out, &cube));

    /* Fast travel cannot tunnel through a cube side. */
    body.position.x = -1000.0f;
    target.x = 1000.0f;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -6.02f));

    /* A diagonal reaches the rounded cylinder/cube corner, not the larger
     * square that an inflated-AABB-only sweep would create. */
    body.position.x = body.position.z = -20.0f;
    target.x = target.z = 0.0f;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -5.72f) && near(out.z, -5.72f));
    assert(!anchor_collision_cube_side_overlaps(&body, &out, &cube));

    /* Tangential travel survives side contact. */
    body.position.x = -10.0f;
    body.position.z = 0.0f;
    target.x = 0.0f;
    target.z = 4.0f;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -6.02f) && near(out.z, 4.0f));

    /* An overlapping spawn separates deterministically instead of trapping
     * the player in the display-only cube. */
    body.position.x = body.position.z = 0.0f;
    target = body.position;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -6.02f) && near(out.z, 0.0f));

    body.position.y = 12.0f;
    target.y = 12.0f;
    body.position.x = -20.0f;
    target.x = 20.0f;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, 20.0f));
    body.position.y = -6.0f;
    target.y = -6.0f;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, 20.0f));

    body.position.x = target.x = 0.0f;
    body.position.y = 15.0f;
    target.y = 5.0f;
    assert(anchor_collision_cube_top_contact(&body, &target, &cube, &top));
    assert(top == 10.0f);
    /* Side geometry leaves Y to the native ground selector. */
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(out.y == 5.0f);
    /* A fast descent begins above the top and ends well below the 2.1-unit
     * native floor-ray origin offset. The side sweep must not eject it while
     * the native floor selector resolves the swept top crossing. */
    body.position = (AnchorCollisionVec3){1.0f, 15.0f, 0.0f};
    target = (AnchorCollisionVec3){2.0f, 5.0f, 0.0f};
    assert(anchor_collision_cube_top_contact(&body, &target, &cube, &top));
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, 2.0f) && near(out.z, 0.0f));
    body.position.y = 5.0f;
    assert(!anchor_collision_cube_top_contact(&body, &target, &cube, &top));

    /* Native floor contact can leave feet just below the top plane. A player
     * standing or walking there must not be ejected toward a side. */
    body.position = (AnchorCollisionVec3){0.0f, 9.99f, 0.0f};
    target = body.position;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, 0.0f) && near(out.z, 0.0f));
    target.x = 2.0f;
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, 2.0f) && near(out.z, 0.0f));
    assert(!anchor_collision_cube_side_overlaps(&body, &target, &cube));

    /* A body clearly below the upper plane is still blocked by the sides. */
    body.position = (AnchorCollisionVec3){0.0f, 9.97f, 0.0f};
    target = body.position;
    assert(anchor_collision_cube_side_overlaps(&body, &target, &cube));
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -6.02f) && near(out.z, 0.0f));
    body.position = (AnchorCollisionVec3){-20.0f, 9.0f, 0.0f};
    target = (AnchorCollisionVec3){20.0f, 9.0f, 0.0f};
    assert(anchor_collision_cube_move_sides(&body, &target, &cube, 1, &out));
    assert(near(out.x, -6.02f));

    body.position = (AnchorCollisionVec3){-20.0f, 0.0f, 0.0f};
    target = (AnchorCollisionVec3){20.0f, 0.0f, 0.0f};
    assert(anchor_collision_cube_move_sides(&body, &target, 0, 0, &out));
    assert(out.x == 20.0f); /* Thaw or absent visual restores free travel. */
    puts("cube side and top contact tests passed");
    return 0;
}
