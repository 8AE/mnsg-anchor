#ifndef ANCHOR_MINIGAME_CODEC_H
#define ANCHOR_MINIGAME_CODEC_H

#define ANCHOR_MINIGAME_MAX_WORDS 32
#define ANCHOR_MINIGAME_MAX_ACTOR_WORDS 40
#define ANCHOR_MINIGAME_MAX_ACTORS 64
#define ANCHOR_MINIGAME_MAX_PARTICIPANTS 64
#define ANCHOR_MINIGAME_MAX_INPUTS 32
#define ANCHOR_MINIGAME_INPUT_WORDS 8
#define ANCHOR_MINIGAME_RECEIVED_INPUT_WORDS 11
#define ANCHOR_MINIGAME_JSON_SIZE 32768

/* Words are typed by each game's adapter. Pointers and callbacks never belong
 * in these rows. Actor rows begin with kind, stable identity, generation. */
typedef struct AnchorMinigameState {
    int ready, running, edge, result;
    unsigned int controller_count, actor_count, input_count, input_ack_count;
    /* Coordinator-owned acknowledgment of an atomically adopted checkpoint. */
    unsigned int adopted_generation;
    /* Bridge-only accepted receipt, never emitted in an outbound graph. */
    unsigned int local_ack;
    /* Bridge-only complete SYNC checkpoint, retained across omitted ACTION rows. */
    unsigned int full_tick;
    /* Bridge-only authoritative status tick, including fresh ACTION controllers. */
    unsigned int source_tick;
    int controller[ANCHOR_MINIGAME_MAX_WORDS];
    unsigned int actor_words[ANCHOR_MINIGAME_MAX_ACTORS];
    int actors[ANCHOR_MINIGAME_MAX_ACTORS][ANCHOR_MINIGAME_MAX_ACTOR_WORDS];
    unsigned int input_words[ANCHOR_MINIGAME_MAX_INPUTS];
    int inputs[ANCHOR_MINIGAME_MAX_INPUTS][ANCHOR_MINIGAME_INPUT_WORDS];
    /* Sender cid, interaction session, local visit, accepted sequence. */
    int input_ack[ANCHOR_MINIGAME_MAX_INPUTS][4];
} AnchorMinigameState;

typedef struct AnchorMinigameStatus {
    int ready, owner, is_owner, result, takeover, has_state;
    unsigned int token[4], generation, tick, input_count;
    unsigned int local_ack, full_tick, participant_count;
    int participants[ANCHOR_MINIGAME_MAX_PARTICIPANTS];
    AnchorMinigameState state;
    unsigned int input_words[ANCHOR_MINIGAME_MAX_INPUTS];
    int inputs[ANCHOR_MINIGAME_MAX_INPUTS][ANCHOR_MINIGAME_RECEIVED_INPUT_WORDS];
} AnchorMinigameStatus;

/* Volatile stores avoid lowering large clears to an unavailable libc memset. */
void anchor_minigame_zero(void *memory, unsigned int size);
int anchor_minigame_state_valid(const AnchorMinigameState *state);
int anchor_minigame_state_encode(const AnchorMinigameState *state,
                                char *out, unsigned int capacity);
/* Failure does not change out. Semantic adapter validation is still required. */
int anchor_minigame_status_decode(const char *json, AnchorMinigameStatus *out);

#endif
