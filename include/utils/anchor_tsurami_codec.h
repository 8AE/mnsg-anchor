#ifndef ANCHOR_TSURAMI_CODEC_H
#define ANCHOR_TSURAMI_CODEC_H

#include "anchor_tsurami_native.h"

#define ANCHOR_TSURAMI_STATE_JSON_SIZE 7680
#define ANCHOR_TSURAMI_STATUS_JSON_SIZE 12288
#define ANCHOR_TSURAMI_HIT_BATCH 32

typedef struct AnchorTsuramiStatus
{
    int role;
    int owner;
    int paused;
    unsigned int revision;
    unsigned int term;
    unsigned int encounter[3];
    int has_state;
    AnchorTsuramiNativeSnapshot state;
    int hit_count;
    int hits[ANCHOR_TSURAMI_HIT_BATCH][6];
} AnchorTsuramiStatus;

int anchor_tsurami_state_encode(const AnchorTsuramiNativeSnapshot *state,
                                    char *out, unsigned int capacity);
/* Strict structural decoding precedes the native field/lifecycle checks.
 * An invalid status is never partially applied to game memory. */
int anchor_tsurami_status_decode(const char *json,
                                     AnchorTsuramiStatus *out);

#endif
