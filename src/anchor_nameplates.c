/**
 * @file anchor_nameplates.c
 * @brief Floating UI nameplates for Anchor remote players.
 */

#include "recompui.h"
#include "anchor_nameplates.h"
#include "icon_goemon.h"
#include "icon_ebisumaru.h"
#include "icon_sasuke.h"
#include "icon_yae.h"

#define NAMEPLATE_ICON_SIZE 22.0f
#define NAMEPLATE_NEAR_DISTANCE_SQ 160000.0f
#define NAMEPLATE_FAR_DISTANCE_SQ 1000000.0f
#define NAMEPLATE_HIDE_DISTANCE_SQ 1690000.0f
#define NAMEPLATE_MIN_SCALE 0.45f
#define NAMEPLATE_MAX_SCALE 1.15f

static RecompuiContext s_nameplate_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_nameplate_cards[ANCHOR_NAMEPLATE_MAX];
static RecompuiResource s_nameplate_icons[ANCHOR_NAMEPLATE_MAX];
static RecompuiResource s_nameplate_labels[ANCHOR_NAMEPLATE_MAX];
static int s_nameplate_initialized;
static int s_nameplate_ctx_visible;
static RecompuiTextureHandle s_nameplate_char_textures[4];
static RecompuiTextureHandle s_nameplate_blank_texture;
static int s_nameplate_textures_initialized;

static const RecompuiColor NAMEPLATE_TEXT = {255, 255, 255, 235};
static const RecompuiColor NAMEPLATE_TEXT_OPPONENT = {255, 72, 72, 245};
static const RecompuiColor NAMEPLATE_BG = {0, 0, 0, 150};

static void nameplates_load_textures(void)
{
    static const unsigned char blank_px[4] = {0, 0, 0, 0};

    if (s_nameplate_textures_initialized)
        return;
    s_nameplate_textures_initialized = 1;

    s_nameplate_blank_texture = recompui_create_texture_rgba32((void *)blank_px, 1, 1);
    s_nameplate_char_textures[0] = recompui_create_texture_rgba32(
        (void *)icon_goemon_data, ICON_GOEMON_WIDTH, ICON_GOEMON_HEIGHT);
    s_nameplate_char_textures[1] = recompui_create_texture_rgba32(
        (void *)icon_ebisumaru_data, ICON_EBISUMARU_WIDTH, ICON_EBISUMARU_HEIGHT);
    s_nameplate_char_textures[2] = recompui_create_texture_rgba32(
        (void *)icon_sasuke_data, ICON_SASUKE_WIDTH, ICON_SASUKE_HEIGHT);
    s_nameplate_char_textures[3] = recompui_create_texture_rgba32(
        (void *)icon_yae_data, ICON_YAE_WIDTH, ICON_YAE_HEIGHT);
}

static void nameplates_ensure_init(void)
{
    int i;
    RecompuiResource root;

    if (s_nameplate_initialized)
        return;
    s_nameplate_initialized = 1;

    nameplates_load_textures();

    s_nameplate_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_nameplate_ctx, 0);
    recompui_set_context_captures_mouse(s_nameplate_ctx, 0);
    recompui_open_context(s_nameplate_ctx);

    root = recompui_context_root(s_nameplate_ctx);
    for (i = 0; i < ANCHOR_NAMEPLATE_MAX; ++i)
    {
        s_nameplate_cards[i] = recompui_create_element(s_nameplate_ctx, root);
        recompui_set_position(s_nameplate_cards[i], POSITION_ABSOLUTE);
        recompui_set_width(s_nameplate_cards[i], 220.0f, UNIT_DP);
        recompui_set_margin_left(s_nameplate_cards[i], -110.0f, UNIT_DP);
        recompui_set_display(s_nameplate_cards[i], DISPLAY_NONE);
        recompui_set_flex_direction(s_nameplate_cards[i], FLEX_DIRECTION_ROW);
        recompui_set_align_items(s_nameplate_cards[i], ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(s_nameplate_cards[i], JUSTIFY_CONTENT_CENTER);
        recompui_set_gap(s_nameplate_cards[i], 6.0f, UNIT_DP);
        recompui_set_background_color(s_nameplate_cards[i], &NAMEPLATE_BG);
        recompui_set_border_radius(s_nameplate_cards[i], 4.0f, UNIT_DP);
        recompui_set_padding(s_nameplate_cards[i], 3.0f, UNIT_DP);

        s_nameplate_icons[i] = recompui_create_imageview(
            s_nameplate_ctx, s_nameplate_cards[i], s_nameplate_blank_texture);
        recompui_set_width(s_nameplate_icons[i], NAMEPLATE_ICON_SIZE, UNIT_DP);
        recompui_set_height(s_nameplate_icons[i], NAMEPLATE_ICON_SIZE, UNIT_DP);

        s_nameplate_labels[i] = recompui_create_label(s_nameplate_ctx, s_nameplate_cards[i], "", LABELSTYLE_ANNOTATION);
        recompui_set_text_align(s_nameplate_labels[i], TEXT_ALIGN_CENTER);
        recompui_set_font_size(s_nameplate_labels[i], 20.0f, UNIT_DP);
        recompui_set_font_weight(s_nameplate_labels[i], 700);
        recompui_set_color(s_nameplate_labels[i], &NAMEPLATE_TEXT);
    }

    recompui_close_context(s_nameplate_ctx);
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
    if (slot_index < 0 || slot_index >= ANCHOR_NAMEPLATE_MAX)
        return;

    nameplates_ensure_init();
    if (s_nameplate_cards[slot_index] != RECOMPUI_NULL_RESOURCE)
    {
        recompui_open_context(s_nameplate_ctx);
        recompui_set_display(s_nameplate_cards[slot_index], DISPLAY_NONE);
        recompui_close_context(s_nameplate_ctx);
    }
}

void anchor_nameplates_set_context_visible(int visible)
{
    nameplates_ensure_init();
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

    nameplates_ensure_init();
    if (slot_index < 0 || slot_index >= ANCHOR_NAMEPLATE_MAX ||
        !remote || !camera ||
        s_nameplate_cards[slot_index] == RECOMPUI_NULL_RESOURCE)
    {
        anchor_nameplates_hide_slot(slot_index);
        return 0;
    }

    local_dx = (float)remote->x - camera->player_x;
    local_dy = (float)remote->y - camera->player_y;
    local_dz = (float)remote->z - camera->player_z;
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

    rel_x = (float)remote->x - eye_x;
    rel_y = ((float)remote->y + label_height) - eye_y;
    rel_z = (float)remote->z - eye_z;
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
    recompui_set_width(s_nameplate_cards[slot_index], card_width, UNIT_DP);
    recompui_set_margin_left(s_nameplate_cards[slot_index], -(card_width * 0.5f), UNIT_DP);
    recompui_set_gap(s_nameplate_cards[slot_index], 6.0f * label_scale, UNIT_DP);
    recompui_set_padding(s_nameplate_cards[slot_index], 3.0f * label_scale, UNIT_DP);
    recompui_set_border_radius(s_nameplate_cards[slot_index], 4.0f * label_scale, UNIT_DP);
    recompui_set_width(s_nameplate_icons[slot_index], icon_size, UNIT_DP);
    recompui_set_height(s_nameplate_icons[slot_index], icon_size, UNIT_DP);
    recompui_set_imageview_texture(
        s_nameplate_icons[slot_index],
        (remote->ch >= 0 && remote->ch < 4) ? s_nameplate_char_textures[remote->ch] : s_nameplate_blank_texture);
    recompui_set_text(s_nameplate_labels[slot_index], remote->name ? remote->name : "");
    recompui_set_font_size(s_nameplate_labels[slot_index], font_size, UNIT_DP);
    recompui_set_color(s_nameplate_labels[slot_index], remote->same_team ? &NAMEPLATE_TEXT : &NAMEPLATE_TEXT_OPPONENT);
    recompui_set_left(s_nameplate_cards[slot_index], screen_x, UNIT_DP);
    recompui_set_top(s_nameplate_cards[slot_index], screen_y, UNIT_DP);
    recompui_set_display(s_nameplate_cards[slot_index], DISPLAY_FLEX);
    recompui_close_context(s_nameplate_ctx);
    return 1;
}
