#ifndef ANCHOR_IMPACT_BOSS_H
#define ANCHOR_IMPACT_BOSS_H
#include "bosses/impact/anchor_impact_native.h"

#define ANCHOR_IMPACT_PHASES 144
#define ANCHOR_IMPACT_CLIPS 17
typedef void (*AnchorImpactCallback)(void *, void *);

/* Native addresses are resolved locally with file_13 loaded. Boss modules
 * describe their AI; the common checkpoint engine owns transport and timing. */
typedef struct AnchorImpactBossProfile {
    AnchorImpactCallback phases[ANCHOR_IMPACT_PHASES];
    const unsigned int *clips[ANCHOR_IMPACT_CLIPS];
    unsigned int task_id;
    unsigned int private_mask;
    AnchorImpactCallback reel;
    const unsigned short *sound_cues;
    unsigned int sound_cue_count;
    void (*capture)(void *state, void *task, unsigned int *root);
    int (*validate)(const unsigned int *root);
    void (*apply_lifecycle)(void *state, const unsigned int *root);
    void (*apply)(void *state, void *task, const unsigned int *root);
} AnchorImpactBossProfile;

const AnchorImpactBossProfile *anchor_impact_kashiwagi_profile(void);
const AnchorImpactBossProfile *anchor_impact_taisamba_profile(void);
const AnchorImpactBossProfile *anchor_impact_balberra_profile(void);
const AnchorImpactBossProfile *anchor_impact_detoile_profile(void);
const AnchorImpactBossProfile *anchor_impact_boss_profile(unsigned int encounter);
void anchor_impact_boss_capture(unsigned int encounter, void *state, void *task, unsigned int *root);
int anchor_impact_boss_validate(unsigned int encounter, const unsigned int *root);
void anchor_impact_boss_apply(unsigned int encounter, void *state, void *task, const unsigned int *root);
void anchor_impact_boss_apply_lifecycle(unsigned int encounter, void *state, const unsigned int *root);
int anchor_impact_native_bind(void *task, unsigned int encounter);

/* Shared native accessors also support the 32-bit-pointer host fixtures. */
#ifndef IB_PTR
#define IB_PTR(p,o) (*(void **)((unsigned char *)(p)+(o)))
#endif
#define IB_U8(p,o) (*(unsigned char *)((unsigned char *)(p)+(o)))
#define IB_U16(p,o) (*(unsigned short *)((unsigned char *)(p)+(o)))
#define IB_U32(p,o) (*(unsigned int *)((unsigned char *)(p)+(o)))
#define IB_F32(p,o) (*(float *)((unsigned char *)(p)+(o)))
int anchor_impact_boss_is_reel_callback(unsigned int callback);
int anchor_impact_boss_sound_valid(unsigned int encounter, unsigned int cue);
int anchor_impact_boss_pointer_valid(const void *pointer);

/* Pointer-free common state for the final two bosses. The local camera,
 * cinematic script byte and native task links never enter these records. */
enum {
    IB_WORLD = IMP_BOSS_DATA, IB_ASCEND = IB_WORLD+6, IB_REEL,
    IB_PLAYER_ACTION, IB_GRAB_TIMER, IB_LATCH, IB_DEFEATED,
    IB_COLLISION_KIND, IB_COLLISION_MASK, IB_ATTACK_KIND, IB_COMMON_END
};
void anchor_impact_boss_common_capture(void *state, void *task, unsigned int *root);
int anchor_impact_boss_common_validate(const unsigned int *root);
void anchor_impact_boss_common_apply(void *state, void *task, const unsigned int *root);
void anchor_impact_boss_common_lifecycle(void *state, const unsigned int *root);
void anchor_impact_boss_aux_capture(unsigned int *root);
void anchor_impact_boss_aux_apply(const unsigned int *root);
int anchor_impact_boss_finite(unsigned int word);
unsigned int anchor_impact_boss_children(void *root, void **children, unsigned int capacity);
/* valid, position XYZ, velocity XYZ, yaw; local read-only callback inputs. */
typedef struct AnchorImpactCarryView {
    unsigned int task[0x80/4], object[0x30/4];
} AnchorImpactCarryView;
void anchor_impact_boss_carry_capture(void *carrier, unsigned int *words);
int anchor_impact_boss_carry_validate(const unsigned int *words);
void *anchor_impact_boss_carry_view(AnchorImpactCarryView *view, const unsigned int *words);

#endif
