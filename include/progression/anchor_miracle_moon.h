#ifndef ANCHOR_MIRACLE_MOON_H
#define ANCHOR_MIRACLE_MOON_H

/* Call only for a received, completed Moon pickup. The inventory grant occurs
 * earlier than native completion, while the local dialogue still owns input. */
void anchor_miracle_moon_remote_completed(unsigned short current_room);
void anchor_miracle_moon_update_room(unsigned short current_room);
void anchor_miracle_moon_reset(void);
/* Includes a pickup selected by the idle actor earlier in this frame. Defer
 * remote completion flag 0xA4 while this is true; it also drives the camera
 * and exit door, so it must follow the local scenario's input release. */
int anchor_miracle_moon_local_pickup_active(void);

#endif
