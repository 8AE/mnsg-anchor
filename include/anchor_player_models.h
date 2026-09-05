#ifndef ANCHOR_PLAYER_MODELS_H
#define ANCHOR_PLAYER_MODELS_H

#include "anchor_remote_collision.h"

#define ANCHOR_PLAYER_MODEL_MAX 25
#define ANCHOR_APPEARANCE_SUDDEN_IMPACT (1 << 0)
#define ANCHOR_APPEARANCE_MINI_EBISUMARU (1 << 1)
#define ANCHOR_APPEARANCE_HURT_RECOVERY (1 << 2)

typedef struct AnchorPlayerModelRemote
{
    int cid;
    int ch;
    float x;
    float y;
    float z;
    int vx;
    int vy;
    int vz;
    int seq;
    int action;
    int anim_frame_100;
    int anim_frame_count_100;
    int anim_step_100;
    int has_anim_step;
    int motion_phase_frames;
    int new_motion_sample;
    int rot_x;
    int rot_y;
    int rot_z;
    int appearance_flags;
    int collision_disabled;
    int drive_x;
    int drive_z;
    int player_epoch;
    int interaction_session;
    int same_team;
} AnchorPlayerModelRemote;

typedef struct AnchorPlayerHitTarget
{
    int cid;
    int epoch;
    AnchorCollisionBody body;
} AnchorPlayerHitTarget;

void anchor_player_models_update(const AnchorPlayerModelRemote *remotes, int count,
                                 void *render_parent_task);
void anchor_player_models_load_resources(void);
void anchor_player_models_reset(void);
int anchor_player_models_get_position(int cid, float *x, float *y, float *z);
int anchor_player_models_get_hit_targets(AnchorPlayerHitTarget *out, int capacity);
void anchor_player_models_get_drive(int *x, int *z);
int anchor_player_models_get_epoch(void);
int anchor_player_models_peer_is_current(int cid, int session, int epoch);

#endif
