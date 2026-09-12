#ifndef ANCHOR_IMPACT_VISUAL_CODEC_H
#define ANCHOR_IMPACT_VISUAL_CODEC_H
#define ANCHOR_IMPACT_VISUAL_MAX 64u
#define ANCHOR_IMPACT_VISUAL_WORDS 29u
#define ANCHOR_IMPACT_VISUAL_JSON 22000u
typedef struct AnchorImpactVisualFrame {
    unsigned int count;
    unsigned int rows[ANCHOR_IMPACT_VISUAL_MAX][ANCHOR_IMPACT_VISUAL_WORDS];
} AnchorImpactVisualFrame;
int anchor_impact_visual_encode(const AnchorImpactVisualFrame *, char *, unsigned int);
int anchor_impact_visual_decode(const char *, AnchorImpactVisualFrame *);
int anchor_impact_visual_row_valid(const unsigned int *);
#endif
