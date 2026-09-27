#ifndef ANCHOR_REMOTE_SMOKE_H
#define ANCHOR_REMOTE_SMOKE_H

#include "player/anchor_player_models.h"

#define ANCHOR_REMOTE_SMOKE_SWITCH_PARTS 6
#define ANCHOR_REMOTE_SMOKE_JET_PARTS 14
#define ANCHOR_REMOTE_SMOKE_PARTS \
    (ANCHOR_REMOTE_SMOKE_SWITCH_PARTS + ANCHOR_REMOTE_SMOKE_JET_PARTS)
#define ANCHOR_REMOTE_SMOKE_BANK_LIMIT 2

typedef struct AnchorRemoteSmokeParticle
{
    float x, y, z;
    float vx, vy, vz;
    float scale;
    unsigned char alpha;
    unsigned char phase;
    unsigned char active;
} AnchorRemoteSmokeParticle;

typedef struct AnchorRemoteSmokeState
{
    void *owner_task;
    void *owner_object;
    void *task;
    void *object[ANCHOR_REMOTE_SMOKE_PARTS];
    AnchorRemoteSmokeParticle particle[ANCHOR_REMOTE_SMOKE_PARTS];
    int cid;
    int session;
    int epoch;
    int last_action;
    int last_ch;
    int jet_tick;
    int bank;
    unsigned int arena_generation;
    unsigned short room;
    unsigned char initialized;
    unsigned char switch_pending;
    unsigned char switch_retry;
    unsigned char switch_await_ch;
    unsigned char switch_await_action;
    unsigned char release_idle;
    unsigned char suspended;
    unsigned char material_bank;
    unsigned char diagnostic_flags;
    unsigned char diagnostic_switch_starts;
} AnchorRemoteSmokeState;

/* Stage-load return hook only; reserves a fixed low-RDRAM material arena. */
void anchor_remote_smoke_load_resources(void);
/* Frame-end owner/context publication, outside native scheduler callbacks. */
void anchor_remote_smoke_set_context(AnchorRemoteSmokeState *state,
    void *remote_task, void *remote_object,
    const AnchorPlayerModelRemote *remote, unsigned short room);
/* Called after the remote model pose is updated by its native task. */
void anchor_remote_smoke_apply(AnchorRemoteSmokeState *state,
    void *remote_task, void *remote_object,
    const AnchorPlayerModelRemote *remote, unsigned short room);
/* owner_live permits deletion of this module's owned child task. */
void anchor_remote_smoke_reset(AnchorRemoteSmokeState *state, int owner_live);
void anchor_remote_smoke_suspend(AnchorRemoteSmokeState *state);

#endif
