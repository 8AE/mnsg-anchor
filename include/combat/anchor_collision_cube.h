#ifndef ANCHOR_COLLISION_CUBE_H
#define ANCHOR_COLLISION_CUBE_H

#include "combat/anchor_remote_collision.h"

typedef struct AnchorCollisionCube
{
    AnchorCollisionVec3 min;
    AnchorCollisionVec3 max;
} AnchorCollisionCube;

/* Sweep a playable feet cylinder against axis-aligned cube sides, including
 * rounded cylinder contact at the cube corners. This leaves floors alone. */
int anchor_collision_cube_move_sides(const AnchorCollisionBody *moving,
                                     const AnchorCollisionVec3 *target,
                                     const AnchorCollisionCube *cubes, int count,
                                     AnchorCollisionVec3 *out);
int anchor_collision_cube_side_overlaps(const AnchorCollisionBody *moving,
                                        const AnchorCollisionVec3 *position,
                                        const AnchorCollisionCube *cube);

/* Return only the horizontal travel blocked by a cube side during a bounded
 * native player step. Existing overlap, top contact and sliding do not push. */
int anchor_collision_cube_push_intent(const AnchorCollisionBody *moving,
                                      const AnchorCollisionVec3 *target,
                                      const AnchorCollisionCube *cube,
                                      float *intent_x, float *intent_z);

/* Return the exact top plane crossed by a descending feet cylinder. The
 * caller must integrate that plane with native floor selection before using
 * it to change the playable position or grounded state. */
int anchor_collision_cube_top_contact(const AnchorCollisionBody *moving,
                                      const AnchorCollisionVec3 *target,
                                      const AnchorCollisionCube *cube,
                                      float *top_y);

#endif
