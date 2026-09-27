#ifndef ANCHOR_PLAYER_EFFECTS_H
#define ANCHOR_PLAYER_EFFECTS_H

#include "player/anchor_player_models.h"

/* One native effect manager belongs to one current remote model task. Purple
 * uses four child records; Sudden Impact uses a main plus five satellites.
 * A room-wide cap also bounds their low-RDRAM material mirrors. */
#define ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT 4
#define ANCHOR_REMOTE_CHARGE_LIMIT 2
#define ANCHOR_REMOTE_AURA_LIMIT 2

typedef struct AnchorPlayerEffectState
{
    void *manager;
    void *owner_task;
    void *charge_object[3];
    int cid;
    int session;
    int epoch;
    int ch;
    int last_action;
    int purple_pending_action;
    unsigned char purple_retry_frames;
    int charge_bank;
    int material_bank;
    unsigned int material_generation;
    int charge_alpha;
    float charge_scale;
    unsigned short charge_spin;
    unsigned char aura_spawned;
    unsigned char aura_drop_pending;
    unsigned char aura_occupied;
    unsigned char aura_phase_seen;
    unsigned char aura_retry_frames;
    unsigned short room;
    unsigned char initialized;
} AnchorPlayerEffectState;

/* Called from the frame-end model publisher, outside the native scheduler. */
void anchor_player_effects_set_context(AnchorPlayerEffectState *state,
                                       void *remote_task,
                                       const AnchorPlayerModelRemote *remote,
                                       unsigned short room);
void anchor_player_effects_reset(AnchorPlayerEffectState *state, int owner_live);
/* Hide any in-flight visuals when the owner model cannot be bound this tick. */
void anchor_player_effects_suspend(AnchorPlayerEffectState *state);
/* Reserve native model/material resources once at stage load. */
void anchor_player_effects_load_resources(void);

/* Called from the remote model task after its displayed pose is updated. */
void anchor_player_effects_apply(AnchorPlayerEffectState *state,
                                  void *remote_task, void *remote_object,
                                  const AnchorPlayerModelRemote *remote,
                                  unsigned short room);

#endif
