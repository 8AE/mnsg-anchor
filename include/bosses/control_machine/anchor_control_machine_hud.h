#ifndef ANCHOR_CONTROL_MACHINE_HUD_H
#define ANCHOR_CONTROL_MACHINE_HUD_H

/* The File_46 battle child belongs to the placed root. These raw accessors
 * retain the validated binding through HP zero and the native death phases. */
void *anchor_control_machine_bound_task(void);
void *anchor_control_machine_bound_root(void);

/* HUD display excludes the native death sequence and HP zero. */
void *anchor_control_machine_hud_task(void);
int anchor_control_machine_hud_health(unsigned int *health);

#endif
