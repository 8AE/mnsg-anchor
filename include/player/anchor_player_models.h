#ifndef ANCHOR_PLAYER_MODELS_H
#define ANCHOR_PLAYER_MODELS_H

#include "combat/anchor_remote_collision.h"
#include "player/alternative_ebisumaru/anchor_alternative_model.h"

#define ANCHOR_APPEARANCE_SUDDEN_IMPACT (1 << 0)
#define ANCHOR_APPEARANCE_MINI_EBISUMARU (1 << 1)
#define ANCHOR_APPEARANCE_HURT_RECOVERY (1 << 2)
/* Alternative/fundoshi Ebisumaru opening-cutscene skin. */
#define ANCHOR_APPEARANCE_ALTERNATIVE_EBISUMARU (1 << 3)

/* An alternative appearance change replaces both segment bases. */
static inline int anchor_player_model_remote_rebind_required(
    int bound_ch, int bound_action, int bound_sudden_impact, int bound_alternative,
    int ch, int action, int sudden_impact, int alternative)
{
    return bound_ch != ch || bound_action != action ||
           bound_sudden_impact != sudden_impact || bound_alternative != alternative;
}

/* The private segment-9 buffer retains the complete 0x124 broad file at the
 * original offsets. The resident 0x4D9 opening file is appended at +0x18000;
 * its display-list references are rebased into segment 9. A private copy of
 * each active 0x127 action replaces only the body's display references with
 * alternative mesh references. The native action header, joint tree, frame and
 * expression data keep their playable values. */
#define ANCHOR_ALTERNATIVE_SIZE_FALLBACK ANCHOR_ALTERNATIVE_RESOURCE_BYTES
#define ANCHOR_CLOTHED_ANIM_CONTEXT 0xc01fc680u

/* Bounding size of a resident scene resource, or 0 when unknown. Shared with
 * the skin module so the alternative file size uses the same registry walk. */
unsigned int resident_resource_size(const unsigned char *base);

/* --- Low-RDRAM alternative render-data pool -------------------------------
 *
 * The private broad copy is both geometry and texture data, so every byte a
 * display-list command resolves must be directly addressable by RT64. The
 * Goemon64Recomp build enables the extended opcode but leaves
 * gEXSetRDRAMExtended(...,1) disabled, so an address at or above 0x80800000
 * is truncated/aliased and corrupts the display list (eventually nulling
 * RT64's hleGBI). recomp_alloc returns heap addresses around 0x81xxxxxx, so
 * the copy cannot live there. These buffers are instead carved from the stock
 * scene-registry arena below 0x80800000, exactly like the remote face arena,
 * and handed out by a per-stage bump allocator.
 *
 * Reserve the pool at stage load after file 0x4D9 is resident; returns 1 when
 * the pool is usable. */
int anchor_player_models_reserve_alternative_pool(void);
/* Bump-allocate one render-data buffer of `size` bytes from the low-RDRAM
 * pool. Returns a valid low RDRAM pointer, or 0 when the pool was not
 * reserved, the request exceeds the fixed per-buffer stride, or it is
 * exhausted. */
unsigned char *anchor_player_models_alternative_pool_alloc(unsigned int size);

/* One shared relocated mesh per stage. */
#define ANCHOR_ALTERNATIVE_POOL_CAP 0x80000u
#define ANCHOR_ALTERNATIVE_POOL_MAX_BUFFERS 1

/* Private broad + opening mesh base, or 0 when the asset cannot be prepared. */
unsigned int anchor_player_models_alternative_broad_base(void);
/* Private playable action slice with alternative body displays. Returns its
 * rebased segment-8 base, or zero if this action cannot safely use the skin. */
unsigned int anchor_player_models_alternative_action_base(int action,
                                                          unsigned int model_ptr);


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
/* True for an active remote slot currently bound to the alternative mesh graft. */
int anchor_player_models_is_alternative_object(const void *object);
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
/* Fill every currently eligible player (local + visible same-team remotes)
 * with its collision-body world position. Returns the number written (up to
 * capacity) so a caller can choose the nearest target per actor. */
int anchor_player_models_get_boss_targets(AnchorBossTarget *out, int capacity);

#endif
