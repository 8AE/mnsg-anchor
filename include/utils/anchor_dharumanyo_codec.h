#ifndef ANCHOR_DHARUMANYO_CODEC_H
#define ANCHOR_DHARUMANYO_CODEC_H

#include "anchor_dharumanyo_native.h"

#define ANCHOR_DHARUMANYO_STATE_JSON_SIZE 4096
#define ANCHOR_DHARUMANYO_STATUS_JSON_SIZE 8192
#define ANCHOR_DHARUMANYO_HIT_BATCH 32

typedef struct AnchorDharumanyoStatus
{
    int role;
    int owner;
    int paused;
    unsigned int revision;
    unsigned int term;
    unsigned int encounter[3];
    int has_state;
    AnchorDharumanyoNativeSnapshot state;
    int hit_count;
    int hits[ANCHOR_DHARUMANYO_HIT_BATCH][5];
} AnchorDharumanyoStatus;

int anchor_dharumanyo_state_encode(const AnchorDharumanyoNativeSnapshot *state,
                                    char *out, unsigned int capacity);
/* Strict structural decoding precedes the native field/lifecycle checks.
 * An invalid status is never partially applied to game memory. */
int anchor_dharumanyo_status_decode(const char *json,
                                     AnchorDharumanyoStatus *out);

#endif
