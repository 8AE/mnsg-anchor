#ifndef ANCHOR_NAMEPLATES_H
#define ANCHOR_NAMEPLATES_H

/* Reserve scene-lifetime, renderer-addressable plate data after stage assets
 * are loaded and before the render scratch pool consumes the remaining arena. */
void anchor_nameplates_load_resources(void);

/* Forget native handles when the player owner or stage is replaced. */
void anchor_nameplates_reset(void);

/* Hide a current peer's plate without touching an unrelated reused task. */
void anchor_nameplates_hide(int cid);

/* Called after the remote child task has finalized its displayed body pose.
 * The plate follows that exact render object and is submitted only if it is
 * visible in the current room. height is the rendered body's top above its
 * origin; the plate adds its own half-height and clearance. */
void anchor_nameplates_sync(int cid, const char *name, int same_team,
                            void *task, const void *body, unsigned short room,
                            float height);

#endif
