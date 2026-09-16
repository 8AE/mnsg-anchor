/* USA file_13 root callback and animation allowlists. Phase IDs never carry
 * executable addresses over the wire. Only callbacks passed to the native
 * callback setters within each boss root state machine are catalogued.
 * Evidence: docs/impact-sync.md. Initialize imports only with file_13 live. */
#include "utils/anchor_impact_catalog.h"
#include "anchor_impact_boss.h"
#define DISABLED 0x00800000u
static const unsigned short offsets[24] = {0x70, 0x74, 0x78, 0x7c, 0x80, 0x84, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4, 0xb0, 0xb4, 0xb8, 0xcc, 0xd0, 0xd4, 0xd8, 0xdc, 0xe0, 0xe4, 0xe8, 0xec};
void anchor_impact_catalog_init(void) {}
unsigned int anchor_impact_phase_id(unsigned int encounter, unsigned int callback) {
    unsigned int i;
    if (encounter < 1 || encounter > 4) return 0;
    for (i = 1; i < ANCHOR_IMPACT_PHASES; ++i) {
        unsigned int candidate = anchor_impact_phase_callback(encounter,i);
        if (candidate && (callback & ~DISABLED) == (candidate & ~DISABLED)) return i;
    }
    return 0;
}
unsigned int anchor_impact_phase_callback(unsigned int encounter, unsigned int phase) {
    if (encounter < 1 || encounter > 4 || phase >= ANCHOR_IMPACT_PHASES) return 0;
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(encounter);
    return (unsigned int)(unsigned long)(p ? p->phases[phase] : 0);
}
unsigned int anchor_impact_clip_id(unsigned int encounter, unsigned int model, unsigned int file) {
    unsigned int i;
    if (encounter < 1 || encounter > 4) return 0;
    for (i = 1; i < 17; ++i) {
        const unsigned int *clip = anchor_impact_clip_data(encounter,i);
        if (clip && clip[0] == model && (clip[1] & 0xffffu) == file) return i;
    }
    return 0;
}
const void *anchor_impact_clip_data(unsigned int encounter, unsigned int clip) {
    if (encounter < 1 || encounter > 4 || clip >= 17) return 0;
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(encounter);
    return p ? p->clips[clip] : 0;
}
unsigned int anchor_impact_private_offset(unsigned int index) {
    return index < 24 ? offsets[index] : 0;
}
int anchor_impact_private_used(unsigned int encounter, unsigned int index) {
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(encounter);
    return encounter >= 1 && encounter <= 4 && index < 24 &&
           ((p ? p->private_mask : 0) & (1u << index)) != 0;
}
