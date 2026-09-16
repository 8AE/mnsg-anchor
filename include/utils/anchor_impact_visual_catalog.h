#ifndef ANCHOR_IMPACT_VISUAL_CATALOG_H
#define ANCHOR_IMPACT_VISUAL_CATALOG_H
typedef struct AnchorImpactVisualRecipe { unsigned int model, file; } AnchorImpactVisualRecipe;
void anchor_impact_visual_catalog_init(void);
unsigned int anchor_impact_visual_recipe_id(unsigned int model, unsigned int file);
const AnchorImpactVisualRecipe *anchor_impact_visual_recipe(unsigned int id);
unsigned int anchor_impact_visual_material_id(unsigned int pointer);
void *anchor_impact_visual_material(unsigned int id);
#endif
