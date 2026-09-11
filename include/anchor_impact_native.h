#ifndef ANCHOR_IMPACT_NATIVE_H
#define ANCHOR_IMPACT_NATIVE_H

/* Wire fields for the shared giant-robot Impact battle. All values are 32-bit
 * integers; float fields carry their IEEE754 single-precision bits so
 * checkpoints stay compact and deterministic. No pointer is serialized except
 * the overlay AI callback, which native code validates against file_13's own
 * executable range before it is installed. */
#define ANCHOR_IMPACT_ROOT_WORDS 5

enum AnchorImpactRootField
{
    /* Dedicated file_13 battle-state block (D_8020EED0_63A2B0). */
    IMP_BOSS_HP,   /* +0x60 signed int, boss health */
    IMP_AMMO,      /* +0x64 signed int, player/mech ryo ammunition */
    IMP_MECH_HP,   /* +0x68 signed int, player/mech health */
    IMP_PAUSE,     /* +0x2C0 byte, combat pause gate */
    IMP_CLOCK      /* +0x2C8 word, per-encounter battle clock */
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
int anchor_impact_native_world_paused(void);
unsigned int anchor_impact_native_visit(void);
unsigned int anchor_impact_native_encounter(void);
unsigned int anchor_impact_native_stage(void);
int anchor_impact_native_capture(AnchorImpactNativeSnapshot *snapshot);
int anchor_impact_native_apply(const AnchorImpactNativeSnapshot *snapshot);
void anchor_impact_native_tick(void);

#endif
