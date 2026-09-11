#ifndef ANCHOR_IMPACT_CODEC_H
#define ANCHOR_IMPACT_CODEC_H

#include "anchor_impact_native.h"

#define ANCHOR_IMPACT_STATE_JSON_SIZE 2048
#define ANCHOR_IMPACT_STATUS_JSON_SIZE 8192
#define ANCHOR_IMPACT_HIT_BATCH 32

typedef struct AnchorImpactStatus
{
    int role;
    int owner;
    int paused;
    unsigned int revision;
    unsigned int term;
    unsigned int encounter[3];
    int has_state;
    AnchorImpactNativeSnapshot state;
    int hit_count;
    int hits[ANCHOR_IMPACT_HIT_BATCH][5];
} AnchorImpactStatus;

int anchor_impact_state_encode(const AnchorImpactNativeSnapshot *state,
                               char *out, unsigned int capacity);
/* Strict structural decoding precedes the native field/lifecycle checks. An
 * invalid status is never partially applied to game memory. */
int anchor_impact_status_decode(const char *json, AnchorImpactStatus *out);

#endif
