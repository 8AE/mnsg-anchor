#ifndef ANCHOR_COLLISION_ACTORS_H
#define ANCHOR_COLLISION_ACTORS_H

#include "anchor_remote_collision.h"

/* Append live native enemy contact bodies to a mod-owned growable array.
 * Returns the new count, or -1 if allocation failed. No native task or hit
 * record is modified. The caller must reacquire *bodies after growth. */
int anchor_collision_append_enemies(AnchorCollisionBody **bodies, int *capacity,
                                    int count);

#endif
