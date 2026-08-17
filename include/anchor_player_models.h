#ifndef ANCHOR_PLAYER_MODELS_H
#define ANCHOR_PLAYER_MODELS_H

#define ANCHOR_PLAYER_MODEL_MAX 25
#define ANCHOR_APPEARANCE_SUDDEN_IMPACT (1 << 0)
#define ANCHOR_APPEARANCE_MINI_EBISUMARU (1 << 1)

typedef struct AnchorPlayerModelRemote
{
    int cid;
    int ch;
    int x;
    int y;
    int z;
    int vx;
    int vy;
    int vz;
    int seq;
    int action;
    int anim_frame_100;
    int anim_frame_count_100;
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
