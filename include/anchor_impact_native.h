#ifndef ANCHOR_IMPACT_NATIVE_H
#define ANCHOR_IMPACT_NATIVE_H

/* Wire fields for the shared giant-robot Impact battle. All values are 32-bit
 * integers; float fields carry their IEEE754 single-precision bits so
 * checkpoints stay compact and deterministic. No callback pointer is ever
 * serialized: callbacks and animation records use per-boss allowlisted IDs. */
#include "utils/anchor_impact_catalog.h"
#define ANCHOR_IMPACT_MECH_OBJECTS 10
#define ANCHOR_IMPACT_POSE_WORDS 11

enum AnchorImpactRootField
{
    /* Dedicated file_13 battle-state block (D_8020EED0_63A2B0). */
    IMP_BOSS_HP,   /* +0x60 signed int, boss health */
    IMP_AMMO,      /* +0x64 signed int, player/mech ryo ammunition */
    IMP_MECH_HP,   /* +0x68 signed int, player/mech health */
    IMP_PAUSE,     /* +0x2C0 byte, combat pause gate */
    IMP_CLOCK,     /* +0x2C8 word, per-encounter battle clock */
    /* Root model object (task+0x18), mirrored for visible alignment. */
    IMP_POS_X,     /* +0x08 float bits */
    IMP_POS_Y,     /* +0x0C float bits */
    IMP_POS_Z,     /* +0x10 float bits */
    IMP_ROT_X,     /* +0x14 unsigned short */
    IMP_ROT_Y,     /* +0x16 unsigned short */
    IMP_ROT_Z,     /* +0x18 unsigned short */
    IMP_SCALE_X,   /* +0x1C float bits */
    IMP_SCALE_Y,   /* +0x20 float bits */
    IMP_SCALE_Z,   /* +0x24 float bits */
    IMP_FRAME,     /* +0x28 float bits, animation frame */
    IMP_PHASE,
    IMP_CLIP,
    IMP_FLAGS,
    IMP_COLLIDER_X, IMP_COLLIDER_Y, IMP_COLLIDER_Z,
    IMP_MODEL_FLAGS, IMP_MODE,
    IMP_PRIVATE,
    IMP_MECH_MASK = IMP_PRIVATE + ANCHOR_IMPACT_PRIVATE_WORDS,
    IMP_MECH_POSES,
    /* Camera position, target, FOV and aim remain entirely local. */
    IMP_AUX_KIND = IMP_MECH_POSES + ANCHOR_IMPACT_MECH_OBJECTS * ANCHOR_IMPACT_POSE_WORDS,
    IMP_AUX_DATA,
    ANCHOR_IMPACT_ROOT_WORDS = IMP_AUX_DATA + 6
};

typedef struct AnchorImpactNativeSnapshot
{
    unsigned int root[ANCHOR_IMPACT_ROOT_WORDS];
    unsigned int encounter;
    unsigned int stage;
} AnchorImpactNativeSnapshot;

void anchor_impact_native_set_role(int active, int owner, int paused);
void anchor_impact_native_reset(void);
int anchor_impact_native_snapshot_ready(void);
int anchor_impact_native_ready(void);
int anchor_impact_native_root_live(void);
int anchor_impact_native_is_owner(void);
int anchor_impact_native_world_paused(void);
unsigned int anchor_impact_native_visit(void);
unsigned int anchor_impact_native_encounter(void);
unsigned int anchor_impact_native_stage(void);
int anchor_impact_native_capture(AnchorImpactNativeSnapshot *snapshot);
int anchor_impact_native_apply(const AnchorImpactNativeSnapshot *snapshot);
void anchor_impact_native_tick(void);

#endif
