#ifndef ANCHOR_CONTROL_MACHINE_HUD_H
#define ANCHOR_CONTROL_MACHINE_HUD_H

/* The File_46 kind-6 battle child owns Control Machine's five HP. Both
 * accessors reject a stale child, room exit, and the native death sequence. */
void *anchor_control_machine_hud_task(void);
int anchor_control_machine_hud_health(unsigned int *health);

#endif
