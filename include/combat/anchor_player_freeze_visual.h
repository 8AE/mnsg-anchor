#ifndef ANCHOR_PLAYER_FREEZE_VISUAL_H
#define ANCHOR_PLAYER_FREEZE_VISUAL_H

#include "combat/anchor_collision_cube.h"

/* A direct-list ice model draws into the stock graphics bank. Keep the
 * simultaneous matrix/command cost bounded independently of room size. */
#define ANCHOR_FREEZE_VISUAL_MAX 8

typedef struct AnchorFreezeVisualTarget
{
    int cid, session, epoch;
    float x, y, z, scale;
} AnchorFreezeVisualTarget;

typedef struct AnchorFreezeCubeCollision
{
    int cid, session, epoch;
    AnchorCollisionCube cube;
} AnchorFreezeCubeCollision;

int anchor_player_models_get_freeze_visual_targets(
    AnchorFreezeVisualTarget *out, int capacity);
void anchor_player_freeze_visual_load_resources(void);
void anchor_player_freeze_visual_tick(void *owner);
void anchor_player_freeze_visual_reset(void);
/* Only active, owned cube displays in the current room. Shatter shards and
 * targets without an allocated visible cube never become solid. */
int anchor_player_freeze_visual_get_cubes(AnchorFreezeCubeCollision *out,
                                          int capacity);
int anchor_player_freeze_visual_has_cube(int cid, int session, int epoch);

#endif
