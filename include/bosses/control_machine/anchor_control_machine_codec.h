#ifndef ANCHOR_CONTROL_MACHINE_CODEC_H
#define ANCHOR_CONTROL_MACHINE_CODEC_H

#include "bosses/control_machine/anchor_control_machine_native.h"

#define ANCHOR_CONTROL_MACHINE_STATE_JSON_SIZE 4096
#define ANCHOR_CONTROL_MACHINE_STATUS_JSON_SIZE 12288
#define ANCHOR_CONTROL_MACHINE_HIT_BATCH 32

typedef struct AnchorControlMachineStatus {
    int role;
    int owner;
    int paused;
    unsigned int term;
    unsigned int revision;
    unsigned int encounter[3];
    int has_state;
    AnchorControlMachineSnapshot state;
    int has_preview;
    AnchorControlMachineSnapshot preview;
    int hit_count;
    int hits[ANCHOR_CONTROL_MACHINE_HIT_BATCH][5];
} AnchorControlMachineStatus;

int anchor_control_machine_state_encode(const AnchorControlMachineSnapshot *state,
                                        char *out, unsigned int capacity);
int anchor_control_machine_status_decode(const char *json,
                                         AnchorControlMachineStatus *out);

#endif
