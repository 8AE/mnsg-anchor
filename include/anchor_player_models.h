#ifndef ANCHOR_PLAYER_MODELS_H
#define ANCHOR_PLAYER_MODELS_H

#define ANCHOR_PLAYER_MODEL_MAX 25
#define ANCHOR_APPEARANCE_SUDDEN_IMPACT (1 << 0)
#define ANCHOR_APPEARANCE_MINI_EBISUMARU (1 << 1)

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
    int same_team;
} AnchorPlayerModelRemote;

void anchor_player_models_update(const AnchorPlayerModelRemote *remotes, int count,
                                 void *render_parent_task);
void anchor_player_models_load_resources(void);
void anchor_player_models_reset(void);

#endif
