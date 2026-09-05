#ifndef ANCHOR_PLAYER_ATTACK_H
#define ANCHOR_PLAYER_ATTACK_H

#include "anchor_remote_collision.h"

/* A copied native attack sphere; no task ownership or collision fields are
 * modified. The observer also serves the deterministic host-side tests. */
typedef struct AnchorPlayerAttackSample
{
    const void *task;
    const void *object;
    unsigned int descriptor;
    unsigned int animation;
    float frame;
    int is_player;
    AnchorCollisionVec3 center;
    float radius;
} AnchorPlayerAttackSample;

void anchor_player_attack_reset(void);
void anchor_player_attack_begin_frame(int enabled, int player_epoch);
void anchor_player_attack_observe(const AnchorPlayerAttackSample *sample);

#endif
