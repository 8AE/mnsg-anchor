#ifndef ANCHOR_PROJECTILE_CAPTURE_H
#define ANCHOR_PROJECTILE_CAPTURE_H

#include "anchor_projectiles.h"

/* Entry-side activation check, before native initialization advances state. */
int anchor_projectile_capture_first_update(const void *task);

/* Read an initialized native task/display record into one spawn event.
 * The caller owns pointer lifetime, owner and session validation. Records
 * must be aligned and provide native layouts through offsets 0x74/0x65.
 * A zero return leaves the output unusable; the queue replaces placeholder
 * id 1 after a successful read. This helper does not call the game. */
int anchor_projectile_capture_fields(const void *task, const void *object,
                                      AnchorProjectileSpawn *out);

#endif
