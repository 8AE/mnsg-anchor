#ifndef ANCHOR_CONGO_NATIVE_H
#define ANCHOR_CONGO_NATIVE_H

#define ANCHOR_CONGO_ROOT_WORDS 24
#define ANCHOR_CONGO_PARTS 6
#define ANCHOR_CONGO_PART_WORDS 4
#define ANCHOR_CONGO_MAX_FLAMES 32
#define ANCHOR_CONGO_FLAME_WORDS 7

/* Integer wire values only. Float fields contain IEEE754 single bits. */
enum AnchorCongoRootField {
    CONGO_PHASE, CONGO_TIMER, CONGO_HP, CONGO_HURT,
    CONGO_FLAGS, CONGO_FLAGS2, CONGO_STATUS, CONGO_SPECIAL,
    CONGO_X, CONGO_Y, CONGO_Z, CONGO_RX, CONGO_YAW, CONGO_RZ,
    CONGO_SCALE_X, CONGO_SCALE_Y, CONGO_SCALE_Z,
    CONGO_VX, CONGO_VY, CONGO_VZ, CONGO_SOUND_COOLDOWN,
    CONGO_RADIUS, CONGO_HEIGHT, CONGO_COLLIDER_Y
};
enum AnchorCongoPartField { CONGO_CLIP, CONGO_FRAME, CONGO_PART_FLAGS, CONGO_PART_HIDDEN };
enum AnchorCongoFlameField { CONGO_FLAME_ID, CONGO_FLAME_BORN, CONGO_FLAME_VARIANT,
    CONGO_FLAME_X, CONGO_FLAME_Y, CONGO_FLAME_Z, CONGO_FLAME_YAW };
typedef struct AnchorCongoNativeSnapshot {
    unsigned int root[ANCHOR_CONGO_ROOT_WORDS];
    unsigned int part[ANCHOR_CONGO_PARTS][ANCHOR_CONGO_PART_WORDS];
    unsigned int spin_serial;
    unsigned int tick;
    unsigned int flame_count;
    unsigned int flame[ANCHOR_CONGO_MAX_FLAMES][ANCHOR_CONGO_FLAME_WORDS];
} AnchorCongoNativeSnapshot;

void anchor_congo_native_set_role(int active, int owner, int paused);
int anchor_congo_native_ready(void);
int anchor_congo_native_world_paused(void);
unsigned int anchor_congo_native_visit(void);
int anchor_congo_native_capture(AnchorCongoNativeSnapshot *snapshot);
/* Accepts the latest checkpoint into one bounded native-pre queue. Capture
 * returns zero until it is applied, preventing publication/damage before
 * adoption. Zero rejects invalid/unready input. */
int anchor_congo_native_apply(const AnchorCongoNativeSnapshot *snapshot);
/* Call once after the scheduler, before bridge capture/apply. */
void anchor_congo_native_tick(void);
void anchor_congo_native_reset(void);
int anchor_congo_native_is_root(const void *task);
void *anchor_congo_native_root_task(void);
void anchor_congo_native_set_target(float x, float y, float z);
void anchor_congo_native_clear_target(void);

#endif
