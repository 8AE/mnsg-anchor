/**
 * @file anchor_nameplates.c
 * @brief Floating UI nameplates for Anchor remote players.
 */

#include "recompui.h"
#include "anchor_nameplates.h"
#include "anchor_rom_icons.h"
#include "utils/array_utils.h"

#define NAMEPLATE_ICON_SIZE 22.0f
#define NAMEPLATE_NEAR_DISTANCE_SQ 160000.0f
#define NAMEPLATE_FAR_DISTANCE_SQ 1000000.0f
#define NAMEPLATE_HIDE_DISTANCE_SQ 1690000.0f
#define NAMEPLATE_MIN_SCALE 0.45f
#define NAMEPLATE_MAX_SCALE 1.15f

/* Build-time flag for remote-player nameplates. Set to 0 to show them. */
static const int s_nameplates_disabled = 1;

static RecompuiContext s_nameplate_ctx = RECOMPUI_NULL_CONTEXT;
typedef struct NameplateSlot
{
    RecompuiResource card;
    RecompuiResource icon;
    RecompuiResource label;
} NameplateSlot;

static NameplateSlot *s_nameplates;
static int s_nameplate_capacity;
static int s_nameplate_count;
static int s_nameplate_initialized;
static int s_nameplate_ctx_visible;
static RecompuiTextureHandle
    s_nameplate_char_textures[ANCHOR_MAP_FACE_ICON_COUNT];
static RecompuiTextureHandle s_nameplate_blank_texture;
static int s_nameplate_textures_initialized;
static int s_nameplate_map_faces_ready;
static unsigned char
    s_nameplate_map_face_rgba[ANCHOR_MAP_FACE_ICONS_RGBA32_SIZE]
        __attribute__((aligned(8)));

static const RecompuiColor NAMEPLATE_TEXT = {255, 255, 255, 235};
static const RecompuiColor NAMEPLATE_TEXT_OPPONENT = {255, 72, 72, 245};
static const RecompuiColor NAMEPLATE_BG = {0, 0, 0, 150};

static void nameplates_load_textures(void)
{
    static const unsigned char blank_px[4] = {0, 0, 0, 0};
    unsigned int character;

    if (s_nameplate_textures_initialized)
        return;
    s_nameplate_textures_initialized = 1;

    s_nameplate_blank_texture =
        recompui_create_texture_rgba32((void *)blank_px, 1, 1);
    for (character = 0; character < ANCHOR_MAP_FACE_ICON_COUNT; ++character)
        s_nameplate_char_textures[character] = s_nameplate_blank_texture;

    if (anchor_rom_load_map_face_icons_rgba32(
            s_nameplate_map_face_rgba, sizeof(s_nameplate_map_face_rgba)))
    {
        for (character = 0; character < ANCHOR_MAP_FACE_ICON_COUNT;
             ++character)
        {
            s_nameplate_char_textures[character] =
                recompui_create_texture_rgba32(
                    s_nameplate_map_face_rgba +
                        character * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE,
                    ANCHOR_MAP_FACE_ICON_WIDTH,
                    ANCHOR_MAP_FACE_ICON_HEIGHT);
        }
        s_nameplate_map_faces_ready = 1;
    }
}

static int nameplates_ensure_init(int needed)
{
    int i;
    RecompuiResource root;

    if (s_nameplates_disabled || needed < 0)
        return 0;
    if (!mnsg_array_reserve((void **)&s_nameplates, &s_nameplate_capacity,
                            needed, sizeof(*s_nameplates)))
        return 0;
    if (!s_nameplate_initialized)
    {
        nameplates_load_textures();
        s_nameplate_ctx = recompui_create_context();
        if (s_nameplate_ctx == RECOMPUI_NULL_CONTEXT)
            return 0;
        recompui_set_context_captures_input(s_nameplate_ctx, 0);
        recompui_set_context_captures_mouse(s_nameplate_ctx, 0);
        s_nameplate_initialized = 1;
    }
    if (needed <= s_nameplate_count)
        return 1;
    recompui_open_context(s_nameplate_ctx);

    root = recompui_context_root(s_nameplate_ctx);
    for (i = s_nameplate_count; i < needed; ++i)
    {
        s_nameplates[i].card = recompui_create_element(s_nameplate_ctx, root);
        if (s_nameplates[i].card == RECOMPUI_NULL_RESOURCE)
            break;
        recompui_set_position(s_nameplates[i].card, POSITION_ABSOLUTE);
        recompui_set_width(s_nameplates[i].card, 220.0f, UNIT_DP);
        recompui_set_margin_left(s_nameplates[i].card, -110.0f, UNIT_DP);
        recompui_set_display(s_nameplates[i].card, DISPLAY_NONE);
        recompui_set_flex_direction(s_nameplates[i].card, FLEX_DIRECTION_ROW);
        recompui_set_align_items(s_nameplates[i].card, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(s_nameplates[i].card, JUSTIFY_CONTENT_CENTER);
        recompui_set_gap(s_nameplates[i].card, 6.0f, UNIT_DP);
        recompui_set_background_color(s_nameplates[i].card, &NAMEPLATE_BG);
        recompui_set_border_radius(s_nameplates[i].card, 4.0f, UNIT_DP);
        recompui_set_padding(s_nameplates[i].card, 3.0f, UNIT_DP);

        s_nameplates[i].icon = recompui_create_imageview(
            s_nameplate_ctx, s_nameplates[i].card, s_nameplate_blank_texture);
        if (s_nameplates[i].icon == RECOMPUI_NULL_RESOURCE)
        {
            recompui_destroy_element(root, s_nameplates[i].card);
            break;
        }
        recompui_set_width(s_nameplates[i].icon, NAMEPLATE_ICON_SIZE, UNIT_DP);
        recompui_set_height(s_nameplates[i].icon, NAMEPLATE_ICON_SIZE, UNIT_DP);

        s_nameplates[i].label = recompui_create_label(s_nameplate_ctx, s_nameplates[i].card, "", LABELSTYLE_ANNOTATION);
        if (s_nameplates[i].label == RECOMPUI_NULL_RESOURCE)
        {
            recompui_destroy_element(root, s_nameplates[i].card);
            break;
        }
        recompui_set_text_align(s_nameplates[i].label, TEXT_ALIGN_CENTER);
        recompui_set_font_size(s_nameplates[i].label, 20.0f, UNIT_DP);
        recompui_set_font_weight(s_nameplates[i].label, 700);
        recompui_set_color(s_nameplates[i].label, &NAMEPLATE_TEXT);
        ++s_nameplate_count;
    }

    recompui_close_context(s_nameplate_ctx);
    return s_nameplate_count >= needed;
}

static float clamp_dp(float value, float lo, float hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static float nameplate_scale_from_distance_sq(float distance_sq)
{
    float t;

    if (distance_sq <= NAMEPLATE_NEAR_DISTANCE_SQ)
        return NAMEPLATE_MAX_SCALE;
    if (distance_sq >= NAMEPLATE_FAR_DISTANCE_SQ)
        return NAMEPLATE_MIN_SCALE;

    t = (distance_sq - NAMEPLATE_NEAR_DISTANCE_SQ) /
        (NAMEPLATE_FAR_DISTANCE_SQ - NAMEPLATE_NEAR_DISTANCE_SQ);
    return NAMEPLATE_MAX_SCALE + (NAMEPLATE_MIN_SCALE - NAMEPLATE_MAX_SCALE) * t;
}

void anchor_nameplates_hide_slot(int slot_index)
{
    if (s_nameplates_disabled)
        return;

    if (slot_index < 0 || slot_index >= s_nameplate_count)
        return;

    if (s_nameplates[slot_index].card != RECOMPUI_NULL_RESOURCE)
    {
        recompui_open_context(s_nameplate_ctx);
        recompui_set_display(s_nameplates[slot_index].card, DISPLAY_NONE);
        recompui_close_context(s_nameplate_ctx);
    }
}

void anchor_nameplates_set_context_visible(int visible)
{
    if (s_nameplates_disabled)
        return;

    if (!nameplates_ensure_init(0))
        return;
    if (visible)
    {
        if (!s_nameplate_ctx_visible)
        {
            recompui_show_context(s_nameplate_ctx);
            s_nameplate_ctx_visible = 1;
        }
    }
    else if (s_nameplate_ctx_visible)
    {
        recompui_hide_context(s_nameplate_ctx);
        s_nameplate_ctx_visible = 0;
    }
}

int anchor_nameplates_render_slot(
    int slot_index,
    const AnchorNameplatePlayer *remote,
    const AnchorNameplateCamera *camera)
{
    const float half_width = 960.0f;
    const float half_height = 540.0f;
    const float focal_x = 900.0f;
    const float focal_y = 620.0f;
    const float label_height = 55.0f;
    float eye_x;
    float eye_y;
    float eye_z;
    float radius;
    float forward_x;
    float forward_z;
    float right_x;
    float right_z;
    float rel_x;
    float rel_y;
    float rel_z;
    float local_dx;
    float local_dy;
    float local_dz;
    float distance_sq;
    float depth;
    float vertical_depth;
    float side;
    float screen_x;
    float screen_y;
    float label_scale;
    float card_width;
    float icon_size;
    float font_size;

    if (s_nameplates_disabled)
        return 0;

    if (slot_index < 0 || slot_index == 0x7fffffff || !remote || !camera ||
        !nameplates_ensure_init(slot_index + 1))
    {
        anchor_nameplates_hide_slot(slot_index);
        return 0;
    }

    local_dx = remote->x - camera->player_x;
    local_dy = remote->y - camera->player_y;
    local_dz = remote->z - camera->player_z;
    distance_sq = local_dx * local_dx + local_dy * local_dy + local_dz * local_dz;
    if (distance_sq > NAMEPLATE_HIDE_DISTANCE_SQ)
    {
        anchor_nameplates_hide_slot(slot_index);
        return 0;
    }

    radius = camera->camera_radius;
    if (radius < 1.0f)
        radius = 1.0f;

    eye_x = camera->player_x + camera->camera_x;
    eye_y = camera->player_y + camera->camera_y;
    eye_z = camera->player_z + camera->camera_z;

    forward_x = -camera->camera_x / radius;
    forward_z = -camera->camera_z / radius;
    right_x = -forward_z;
    right_z = forward_x;

    rel_x = remote->x - eye_x;
    rel_y = (remote->y + label_height) - eye_y;
    rel_z = remote->z - eye_z;
    depth = rel_x * forward_x + rel_z * forward_z;

    if (depth < 20.0f)
    {
        anchor_nameplates_hide_slot(slot_index);
        return 0;
    }

    label_scale = nameplate_scale_from_distance_sq(distance_sq);
    card_width = 220.0f * label_scale;
    icon_size = NAMEPLATE_ICON_SIZE * label_scale;
    font_size = 20.0f * label_scale;

    side = rel_x * right_x + rel_z * right_z;
    vertical_depth = depth + 260.0f;
    screen_x = clamp_dp(half_width + (side / depth) * focal_x, 80.0f, 1840.0f);
    screen_y = clamp_dp(half_height - (rel_y / vertical_depth) * focal_y, 60.0f, 980.0f);

    recompui_open_context(s_nameplate_ctx);
    recompui_set_width(s_nameplates[slot_index].card, card_width, UNIT_DP);
    recompui_set_margin_left(s_nameplates[slot_index].card, -(card_width * 0.5f), UNIT_DP);
    recompui_set_gap(s_nameplates[slot_index].card, 6.0f * label_scale, UNIT_DP);
    recompui_set_padding(s_nameplates[slot_index].card, 3.0f * label_scale, UNIT_DP);
    recompui_set_border_radius(s_nameplates[slot_index].card, 4.0f * label_scale, UNIT_DP);
    recompui_set_width(s_nameplates[slot_index].icon, icon_size, UNIT_DP);
    recompui_set_height(s_nameplates[slot_index].icon, icon_size, UNIT_DP);
    recompui_set_imageview_texture(
        s_nameplates[slot_index].icon,
        (s_nameplate_map_faces_ready && remote->ch >= 0 &&
         remote->ch < (int)ANCHOR_MAP_FACE_ICON_COUNT)
            ? s_nameplate_char_textures[remote->ch]
            : s_nameplate_blank_texture);
    recompui_set_text(s_nameplates[slot_index].label, remote->name ? remote->name : "");
    recompui_set_font_size(s_nameplates[slot_index].label, font_size, UNIT_DP);
    recompui_set_color(s_nameplates[slot_index].label, remote->same_team ? &NAMEPLATE_TEXT : &NAMEPLATE_TEXT_OPPONENT);
    recompui_set_left(s_nameplates[slot_index].card, screen_x, UNIT_DP);
    recompui_set_top(s_nameplates[slot_index].card, screen_y, UNIT_DP);
    recompui_set_display(s_nameplates[slot_index].card, DISPLAY_FLEX);
    recompui_close_context(s_nameplate_ctx);
    return 1;
}
