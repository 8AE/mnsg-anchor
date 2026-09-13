#ifndef ANCHOR_IMPACT_BOSS_H
#define ANCHOR_IMPACT_BOSS_H
#include "anchor_impact_native.h"

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

#endif
