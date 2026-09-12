#ifndef ANCHOR_IMPACT_CATALOG_H
#define ANCHOR_IMPACT_CATALOG_H
#define ANCHOR_IMPACT_PRIVATE_WORDS 24
void anchor_impact_catalog_init(void);
unsigned int anchor_impact_phase_id(unsigned int encounter, unsigned int callback);
unsigned int anchor_impact_phase_callback(unsigned int encounter, unsigned int phase);
unsigned int anchor_impact_clip_id(unsigned int encounter, unsigned int model, unsigned int file);
const void *anchor_impact_clip_data(unsigned int encounter, unsigned int clip);
unsigned int anchor_impact_private_offset(unsigned int index);
int anchor_impact_private_used(unsigned int encounter, unsigned int index);
#endif
