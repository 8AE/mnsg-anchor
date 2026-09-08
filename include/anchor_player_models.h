#ifndef ANCHOR_PLAYER_MODELS_H
#define ANCHOR_PLAYER_MODELS_H

#include "anchor_remote_collision.h"

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

typedef struct AnchorBossTarget
{
    int cid;
    float x;
    float y;
    float z;
} AnchorBossTarget;

void anchor_player_models_update(const AnchorPlayerModelRemote *remotes, int count,
                                 void *render_parent_task);
void anchor_player_models_load_resources(void);
void anchor_player_models_reset(void);
int anchor_player_models_get_position(int cid, float *x, float *y, float *z);
/* Resolve a current peer's audible position without requiring gameplay
 * collision, which is intentionally absent during scripted movement. */
int anchor_player_models_get_sound_position(int cid, int session, int epoch,
                                            float *x, float *y, float *z);
/* Accept the real playable task and directly owned native effect tasks, but
 * never Anchor's render-only remote-model task. This performs native-only
 * linkage/ownership checks and is safe to call from the final audio hook. */
int anchor_player_models_is_local_sound_task(const void *task);
int anchor_player_models_capacity(void);
int anchor_player_models_is_remote_object(const void *object);
const void *anchor_player_models_resolve_render_address(const void *object,
    unsigned int encoded, unsigned int bytes);
int anchor_player_models_get_hit_targets(AnchorPlayerHitTarget *out, int capacity);
void anchor_player_models_get_drive(int *x, int *z);
int anchor_player_models_get_epoch(void);
/* Native-only snapshot for hooks that must not enter the Python bridge. The
 * frame publisher refreshes the authoritative epoch before consumers use it. */
int anchor_player_models_peek_epoch(void);
/* Cached scripted-control state paired with peek_epoch(). This does not run
 * collision/cutscene detection and is therefore safe inside native hooks. */
int anchor_player_models_peek_scripted(void);
int anchor_player_models_peer_is_current(int cid, int session, int epoch);
/* Retain an eligible target, or rotate through every current same-team player
 * in client-ID order. This scans the dynamic roster without a player limit. */
int anchor_player_models_get_boss_target(int current_cid, int rotate,
                                         AnchorBossTarget *out);

#endif
