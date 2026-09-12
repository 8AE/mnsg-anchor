#ifndef ANCHOR_IMPACT_PLAYERS_CODEC_H
#define ANCHOR_IMPACT_PLAYERS_CODEC_H

#define ANCHOR_IMPACT_PLAYER_PEERS 16
#define ANCHOR_IMPACT_PLAYER_BATCH 16
#define ANCHOR_IMPACT_PLAYER_JSON_SIZE 4096
#define ANCHOR_IMPACT_PLAYER_STATUS_SIZE 8192

typedef struct AnchorImpactPlayerSample
{
    /* visibility, native reticle XYZ float bits, three u16 rotations. */
    unsigned int cursor[7];
    unsigned int attacks[ANCHOR_IMPACT_PLAYER_BATCH][8];
    unsigned int attack_count;
} AnchorImpactPlayerSample;

typedef struct AnchorImpactPlayerStatus
{
    unsigned int accepted;
    /* Identity words are opaque transport session/visit tokens. */
    unsigned int cursors[ANCHOR_IMPACT_PLAYER_PEERS][10];
    unsigned int attacks[ANCHOR_IMPACT_PLAYER_BATCH][10];
    unsigned int cursor_count, attack_count;
} AnchorImpactPlayerStatus;

int anchor_impact_players_encode(const AnchorImpactPlayerSample *sample,
                                char *json, unsigned int capacity);
int anchor_impact_players_decode(const char *json, AnchorImpactPlayerStatus *status);

#endif
