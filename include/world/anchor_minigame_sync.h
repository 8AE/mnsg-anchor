#ifndef ANCHOR_MINIGAME_SYNC_H
#define ANCHOR_MINIGAME_SYNC_H

#include "world/anchor_minigame_codec.h"

typedef struct AnchorMinigameAdapter {
    int game;
    void (*reset)(void);
    int (*capture)(AnchorMinigameState *state);
    int (*validate)(const AnchorMinigameState *state);
    /* Preflight the entire native graph before changing any live scalar. */
    int (*apply)(const AnchorMinigameState *state);
    void (*role)(int active, int owner, int ready);
    /* 1 consumes (or deliberately rejects) the request; 0 defers it. */
    int (*input)(const int *words, unsigned int count);
    /* Native result phase has been entered; validated local cleanup is due. */
    int (*return_pending)(void);
    void (*result)(int result);
} AnchorMinigameAdapter;

/* Register a static adapter after its root's verified initializer completes. */
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *adapter);
void anchor_minigame_sync_begin_load(void);
void anchor_minigame_sync_finish_load(void);
void anchor_minigame_sync_frame(void);
unsigned int anchor_minigame_sync_visit(void);
int anchor_minigame_sync_game(void);
/* Native initialization is adapter controlled. Gameplay callbacks consult
 * these after initialization, retaining local player physics/collision. */
int anchor_minigame_sync_native_allowed(int game);
int anchor_minigame_sync_waiting(int game);
int anchor_minigame_sync_returning(void);
int anchor_minigame_sync_result(void);
/* Last committed ready status for this bound game/visit; no bridge queries. */
unsigned int anchor_minigame_sync_local_ack(int game);
int anchor_minigame_sync_participant_current(int game, int cid);
/* Copy the committed current-game roster without querying the bridge. */
unsigned int anchor_minigame_sync_participants(int game, int *out,
                                              unsigned int capacity);

#endif
