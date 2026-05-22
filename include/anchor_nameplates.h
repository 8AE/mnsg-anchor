#ifndef ANCHOR_NAMEPLATES_H
#define ANCHOR_NAMEPLATES_H

#define ANCHOR_NAMEPLATE_MAX 25

typedef struct AnchorNameplateCamera
{
    float player_x;
    float player_y;
    float player_z;
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_radius;
} AnchorNameplateCamera;

typedef struct AnchorNameplatePlayer
{
    int x;
    int y;
    int z;
    int ch;
    int same_team;
    const char *name;
} AnchorNameplatePlayer;

void anchor_nameplates_hide_slot(int slot_index);
void anchor_nameplates_set_context_visible(int visible);
int anchor_nameplates_render_slot(
    int slot_index,
    const AnchorNameplatePlayer *remote,
    const AnchorNameplateCamera *camera);

#endif
