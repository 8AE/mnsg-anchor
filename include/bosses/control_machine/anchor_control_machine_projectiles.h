#ifndef ANCHOR_CONTROL_MACHINE_PROJECTILES_H
#define ANCHOR_CONTROL_MACHINE_PROJECTILES_H

#include "bosses/control_machine/anchor_control_machine_native.h"

/* The binding visit identifies a single local health-child incarnation. */
void anchor_control_machine_projectiles_set_context(void *child,
                                                     unsigned int binding_visit,
                                                     int active, int owner,
                                                     int paused);
int anchor_control_machine_projectiles_capture(AnchorControlMachineSnapshot *out);
int anchor_control_machine_projectiles_apply(const AnchorControlMachineSnapshot *in);
void anchor_control_machine_projectiles_reset(void);

#endif
