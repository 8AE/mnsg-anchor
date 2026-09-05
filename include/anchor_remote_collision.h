#ifndef ANCHOR_REMOTE_COLLISION_H
#define ANCHOR_REMOTE_COLLISION_H

typedef struct AnchorCollisionVec3
{
    float x;
    float y;
    float z;
} AnchorCollisionVec3;

typedef struct AnchorCollisionBody
{
    AnchorCollisionVec3 position; /* feet, in world units */
    float radius;
    float height;
} AnchorCollisionBody;

#define ANCHOR_COLLISION_MAX_PEERS 26

int anchor_remote_collision_is_scripted(void);

/* A peer's native movement before solid player contact, in world units per
 * tick. Only approaching horizontal contact contributes pressure. The
 * recipient resolves the returned displacement against its own world. */
AnchorCollisionVec3 anchor_collision_push(const AnchorCollisionBody *recipient,
                                         const AnchorCollisionBody *peer,
                                         float drive_x, float drive_z);

/* Native room and moving-object geometry, without player action callbacks. */
void anchor_collision_world_move(const AnchorCollisionVec3 *from,
                                 const AnchorCollisionVec3 *target,
                                 float scale, AnchorCollisionVec3 *out);

/* Move a cylinder against stationary cylinders, retaining tangential motion.
 * This also separates coincident spawns deterministically. */
void anchor_collision_move_peers(const AnchorCollisionBody *moving,
                                 const AnchorCollisionVec3 *target,
                                 const AnchorCollisionBody *obstacles,
                                 int count, AnchorCollisionVec3 *out);

/* Resolve world and peer contacts together. Success guarantees no remaining
 * peer overlap after world resolution. An overlapping spawn may escape via
 * one of eight bounded horizontal candidates; all other peers remain swept
 * obstacles. On failure out retains from and no new collider should activate.
 * count must be between zero and ANCHOR_COLLISION_MAX_PEERS. */
int anchor_collision_move_body(const AnchorCollisionBody *moving,
                               const AnchorCollisionVec3 *target, float scale,
                               const AnchorCollisionBody *peers, int count,
                               AnchorCollisionVec3 *out);

#endif
