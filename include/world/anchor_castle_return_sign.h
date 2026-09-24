#ifndef ANCHOR_CASTLE_RETURN_SIGN_H
#define ANCHOR_CASTLE_RETURN_SIGN_H

/* A sign-owned prompt or accepted transfer still needs the local world. */
int anchor_castle_return_sign_pending(void);

/* The castle's mod-owned sign stays local and must not enter generic NPC sync. */
int anchor_castle_return_sign_owns_task(void *task);

#endif
