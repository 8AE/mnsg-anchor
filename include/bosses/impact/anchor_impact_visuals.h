#ifndef ANCHOR_IMPACT_VISUALS_H
#define ANCHOR_IMPACT_VISUALS_H
/* Restore native visibility before capture/update; project only at render time. */
void anchor_impact_visuals_begin_frame(void);
void anchor_impact_visuals_tick(int active);
void anchor_impact_visuals_render(void);
int anchor_impact_visuals_active(void);
#endif
