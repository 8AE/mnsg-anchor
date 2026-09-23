#ifndef ANCHOR_DHARUMANYO_NATIVE_H
#define ANCHOR_DHARUMANYO_NATIVE_H

#define ANCHOR_DHARUMANYO_ROOT_WORDS 29
#define ANCHOR_DHARUMANYO_CARRIER_WORDS 9
#define ANCHOR_DHARUMANYO_MAX_PROJECTILES 16
#define ANCHOR_DHARUMANYO_PROJECTILE_WORDS 11

/* All wire values are 32-bit integers. Float fields carry their IEEE754
 * single-precision bits so checkpoints remain compact and deterministic.
 * DHAR_PRIVATE_D0 packs the attack selector in bits 0..7 and the projectile
 * burst counter in bits 8..15. DHAR_PRIVATE_D4 and DHAR_PRIVATE_E8 contain
 * only their native low halfword. DHAR_COLLIDER_XY packs the native +0x3c
 * extent in bits 0..15 and +0x3e in bits 16..31. No pointer is serialized. */
enum AnchorDharumanyoRootField
{
    DHAR_PHASE,
    DHAR_TIMER,
    DHAR_CLIP,
    DHAR_FRAME,
    DHAR_ANIM_STATE,
    DHAR_FLAGS,
    DHAR_FLAGS2,
    DHAR_PHASE_FLAGS,
    DHAR_PRIVATE_D0,
    DHAR_PRIVATE_D4,
    DHAR_PRIVATE_D8,
    DHAR_PRIVATE_E0,
    DHAR_PRIVATE_E4,
    DHAR_PRIVATE_E8,
    DHAR_PRIVATE_EC,
    DHAR_X,
    DHAR_Y,
    DHAR_Z,
    DHAR_RX,
    DHAR_YAW,
    DHAR_RZ,
    DHAR_SCALE_X,
    DHAR_SCALE_Y,
    DHAR_SCALE_Z,
    DHAR_VX,
    DHAR_VY,
    DHAR_VZ,
    DHAR_COLLIDER_XY,
    DHAR_COLLIDER_Z
};

enum AnchorDharumanyoCarrierField
{
    DHAR_CARRIER_LIVES,
    DHAR_CARRIER_HURT,
    DHAR_CARRIER_YAW,
    DHAR_CARRIER_X,
    DHAR_CARRIER_Y,
    DHAR_CARRIER_Z,
    DHAR_CARRIER_SCALE_X,
    DHAR_CARRIER_SCALE_Y,
    DHAR_CARRIER_SCALE_Z
};

enum AnchorDharumanyoProjectileField
{
    DHAR_PROJECTILE_ID,
    DHAR_PROJECTILE_BORN,
    DHAR_PROJECTILE_VARIANT,
    DHAR_PROJECTILE_X,
    DHAR_PROJECTILE_Y,
    DHAR_PROJECTILE_Z,
    DHAR_PROJECTILE_YAW,
    DHAR_PROJECTILE_DEST_X,
    DHAR_PROJECTILE_DEST_Z,
    DHAR_PROJECTILE_VY,
    DHAR_PROJECTILE_TIMER
};

enum AnchorDharumanyoPhase
{
    DHAR_PHASE_NEUTRAL = 1,
    DHAR_PHASE_TERMINAL = 32
};

typedef struct AnchorDharumanyoNativeSnapshot
{
    unsigned int root[ANCHOR_DHARUMANYO_ROOT_WORDS];
    unsigned int carrier[ANCHOR_DHARUMANYO_CARRIER_WORDS];
    unsigned int projectile_serial;
    unsigned int tick;
    unsigned int projectile_count;
    unsigned int projectile[ANCHOR_DHARUMANYO_MAX_PROJECTILES]
                           [ANCHOR_DHARUMANYO_PROJECTILE_WORDS];
} AnchorDharumanyoNativeSnapshot;

void anchor_dharumanyo_native_set_role(int active, int owner, int paused);
int anchor_dharumanyo_native_ready(void);
/* Remains true for the retained terminal checkpoint after the combat actors
 * have been removed, until the native reward controller reaches completion. */
int anchor_dharumanyo_native_snapshot_ready(void);
void anchor_dharumanyo_native_finish_terminal(void);
int anchor_dharumanyo_native_world_paused(void);
unsigned int anchor_dharumanyo_native_visit(void);
int anchor_dharumanyo_native_capture(AnchorDharumanyoNativeSnapshot *snapshot);
/* Checkpoints are copied into one bounded native-pre queue. Capture remains
 * unavailable until the queued state is applied. Terminal checkpoints start
 * the existing verified native last-hit path instead of seeking death AIs. */
int anchor_dharumanyo_native_apply(
    const AnchorDharumanyoNativeSnapshot *snapshot);
void anchor_dharumanyo_native_tick(void);
void anchor_dharumanyo_native_reset(void);
int anchor_dharumanyo_native_is_root(const void *task);
void *anchor_dharumanyo_native_root_task(void);
void anchor_dharumanyo_native_set_target(float x, float y, float z);
void anchor_dharumanyo_native_clear_target(void);

#endif
