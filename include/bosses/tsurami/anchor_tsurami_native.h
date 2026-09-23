#ifndef ANCHOR_TSURAMI_NATIVE_H
#define ANCHOR_TSURAMI_NATIVE_H

#define ANCHOR_TSURAMI_ROOT_WORDS 35
#define ANCHOR_TSURAMI_VISUAL_WORDS 4
#define ANCHOR_TSURAMI_MAX_PROJECTILES 32
#define ANCHOR_TSURAMI_PROJECTILE_WORDS 20

/* Pointer-free checkpoint. Floats carry IEEE754 bits. COLLIDER_XY packs two
 * halfwords; PALETTE packs task EC..EF, ANIM_STATE packs object7C and7E. */
enum AnchorTsuramiRootField {
    TSU_PHASE, TSU_TIMER, TSU_HP, TSU_HURT, TSU_CLIP, TSU_FRAME,
    TSU_ANIM_STATE, TSU_FLAGS, TSU_FLAGS2, TSU_STATUS, TSU_SPECIAL,
    TSU_X, TSU_Y, TSU_Z, TSU_RX, TSU_YAW, TSU_RZ,
    TSU_SCALE_X, TSU_SCALE_Y, TSU_SCALE_Z, TSU_VX, TSU_VY, TSU_VZ,
    TSU_COLLIDER_XY, TSU_COLLIDER_Z, TSU_SHOT_DIRECTION, TSU_PALETTE,
    TSU_SPIN_SPEED, TSU_BURST, TSU_FLIGHT_VY,
    TSU_DEST_X, TSU_DEST_Y, TSU_DEST_Z, TSU_EVENTS, TSU_FLASH
};
enum AnchorTsuramiVisualField { TSU_VISUAL_CLIP, TSU_VISUAL_FRAME,
    TSU_VISUAL_FLAGS, TSU_VISUAL_PHASE };
/* KIND 0=travelling/reflecting attack,1=expanding damage ring. FLAGS is
 * taskE8. HEALTH packs HP, recovery byte and status bit0 at bits0/8/16.
 * AI 0=04ED0,1=05B50 (reflection),2=0587C (ring). */
enum AnchorTsuramiProjectileField {
    TSU_PROJECTILE_ID, TSU_PROJECTILE_BORN, TSU_PROJECTILE_KIND,
    TSU_PROJECTILE_FLAGS, TSU_PROJECTILE_TIMER, TSU_PROJECTILE_HEALTH,
    TSU_PROJECTILE_X, TSU_PROJECTILE_Y, TSU_PROJECTILE_Z,
    TSU_PROJECTILE_YAW, TSU_PROJECTILE_VX, TSU_PROJECTILE_VY,
    TSU_PROJECTILE_VZ, TSU_PROJECTILE_SCALE_X, TSU_PROJECTILE_SCALE_Y,
    TSU_PROJECTILE_SCALE_Z, TSU_PROJECTILE_OPACITY, TSU_PROJECTILE_AI,
    TSU_PROJECTILE_PITCH, TSU_PROJECTILE_GRAVITY
};
enum AnchorTsuramiPhase { TSU_PHASE_NEUTRAL=1, TSU_PHASE_TERMINAL=33 };

typedef struct AnchorTsuramiNativeSnapshot {
    unsigned int root[ANCHOR_TSURAMI_ROOT_WORDS];
    unsigned int visual[ANCHOR_TSURAMI_VISUAL_WORDS];
    unsigned int projectile_serial, tick, projectile_count;
    unsigned int projectile[ANCHOR_TSURAMI_MAX_PROJECTILES]
                           [ANCHOR_TSURAMI_PROJECTILE_WORDS];
} AnchorTsuramiNativeSnapshot;

void anchor_tsurami_native_set_role(int active,int owner,int paused);
int anchor_tsurami_native_ready(void);
int anchor_tsurami_native_snapshot_ready(void);
void anchor_tsurami_native_finish_terminal(void);
int anchor_tsurami_native_world_paused(void);
unsigned int anchor_tsurami_native_visit(void);
int anchor_tsurami_native_capture(AnchorTsuramiNativeSnapshot *snapshot);
int anchor_tsurami_native_apply(const AnchorTsuramiNativeSnapshot *snapshot);
void anchor_tsurami_native_tick(void);
void anchor_tsurami_native_reset(void);
int anchor_tsurami_native_is_root(const void *task);
void *anchor_tsurami_native_root_task(void);
void anchor_tsurami_native_set_target(float x,float y,float z);
void anchor_tsurami_native_clear_target(void);
/* Only mode1 projectiles whose native reflection callback is still live are
 * addressable hit targets. Never send a local native pointer over the wire. */
unsigned int anchor_tsurami_native_projectile_id(const void *task);
void *anchor_tsurami_native_projectile_task(unsigned int id);
int anchor_tsurami_native_is_projectile(const void *task);
#endif
