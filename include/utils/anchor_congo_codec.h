#ifndef ANCHOR_CONGO_CODEC_H
#define ANCHOR_CONGO_CODEC_H

#include "anchor_congo_native.h"

#define ANCHOR_CONGO_STATE_JSON_SIZE 4096
#define ANCHOR_CONGO_STATUS_JSON_SIZE 8192
#define ANCHOR_CONGO_HIT_BATCH 32

typedef struct AnchorCongoStatus
{
    int role;
    int owner;
    int paused;
    unsigned int revision;
    unsigned int term;
    unsigned int encounter[3];
    int has_state;
    AnchorCongoNativeSnapshot state;
    int hit_count;
    int hits[ANCHOR_CONGO_HIT_BATCH][5];
} AnchorCongoStatus;

int anchor_congo_state_encode(const AnchorCongoNativeSnapshot *state,
                               char *out, unsigned int capacity);
/* Strict structural decoding precedes the native field/lifecycle checks.
 * An invalid status is never partially applied to game memory. */
int anchor_congo_status_decode(const char *json, AnchorCongoStatus *out);

#endif
