/**
 * @file anchor_ui.c
 * @brief Anchor UI overlays for MNSG: Recompiled.
 *
 * Provides two overlay surfaces:
 *
 *  1. **Connection notification banner** – a brief popup that appears near
 *     the top of the screen every time a connection attempt is made.  The
 *     accent bar is green on success, red on failure.  It auto-dismisses
 *     after NOTIFICATION_FRAMES game frames (~5 s at 60 fps).
 *
 *  2. **Player list panel** – a semi-transparent panel hosted in debug.c's
 *     shared persistent HUD context. It lists every online player and gives
 *     transferable remotes a mouse-only flute action. The panel is refreshed
 *     periodically and hidden automatically when the client disconnects.
 *
 * The notification context captures no input. The shared HUD captures mouse
 * clicks but not keyboard/controller input, so gameplay controls remain live.
 *
 * Entry points
 * ------------
 *   anchor_ui_update()        – RECOMP_HOOK_RETURN on func_80002040_2C40
 *       drives the timer and the player-list refresh.
 */

#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "recompconfig.h"
#include "anchor.h"
#include "anchor_boss_invite_world.h"
#include "anchor_runtime.h"
#include "debug_ui.h"
#include "utils/array_utils.h"
#include "utils/json_utils.h"
#include "icon_goemon.h"
#include "icon_ebisumaru.h"
#include "icon_sasuke.h"
#include "icon_yae.h"
#include "icon_flute.h"

/* =========================================================================
   Tunables
   ========================================================================= */

/** Frames the connection notification stays on screen (~5 s @ 60 fps). */
#define NOTIFICATION_FRAMES 300

/** Frames between player-list refreshes (~1 s @ 60 fps). */
#define PLAYER_LIST_REFRESH_FRAMES 60

/** Icon image size in DP units (square). */
#define ICON_SIZE 24.0f

/** Flute image and its overlaid click target size in DP units. */
#define TRANSFER_ACTION_SIZE 24.0f

/** Bound a clicked transfer while native world gates are temporarily busy. */
#define TRANSFER_REQUEST_TIMEOUT_FRAMES 300
#define TRANSFER_RETRY_FRAMES 6

/** Last ordinary room backed by the native five-short starting-data table. */
#define TRANSFER_MAX_ROOM_ID 0x225u

/** Maximum characters in a composed player-list label. */
#define PLAYER_LABEL_BUF_LEN 192

/* =========================================================================
   Colour constants
   ========================================================================= */

static const RecompuiColor COLOR_BG = {0, 0, 0, 180};
static const RecompuiColor COLOR_BORDER = {80, 80, 80, 200};
static const RecompuiColor COLOR_WHITE = {255, 255, 255, 255};
static const RecompuiColor COLOR_DIM = {180, 180, 180, 255};
static const RecompuiColor COLOR_GREEN = {80, 200, 80, 255};
static const RecompuiColor COLOR_RED = {220, 60, 60, 255};
static const RecompuiColor COLOR_GOLD = {220, 190, 60, 255};
static const RecompuiColor COLOR_ACCENT_BG = {20, 20, 20, 200};
static const RecompuiColor COLOR_TRANSPARENT = {0, 0, 0, 0};

/* =========================================================================
   Notification context
   ========================================================================= */

static RecompuiContext s_notif_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_notif_accent = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_notif_msg = RECOMPUI_NULL_RESOURCE;
static int s_notif_timer = 0;

static void notif_ensure_init(void)
{
    if (s_notif_ctx != RECOMPUI_NULL_CONTEXT)
        return;

    s_notif_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_notif_ctx, 0);
    recompui_set_context_captures_mouse(s_notif_ctx, 0);
    recompui_open_context(s_notif_ctx);

    RecompuiResource root = recompui_context_root(s_notif_ctx);

    /* Outer card: centered horizontally, near the top. */
    RecompuiResource card = recompui_create_element(s_notif_ctx, root);
    recompui_set_position(card, POSITION_ABSOLUTE);
    recompui_set_left(card, 50.0f, UNIT_PERCENT);
    recompui_set_top(card, 56.0f, UNIT_DP);
    recompui_set_margin_left(card, -160.0f, UNIT_DP); /* center: half of 320 */
    recompui_set_width(card, 320.0f, UNIT_DP);
    recompui_set_background_color(card, &COLOR_BG);
    recompui_set_border_radius(card, 8.0f, UNIT_DP);
    recompui_set_border_width(card, 1.5f, UNIT_DP);
    recompui_set_border_color(card, &COLOR_BORDER);

    /* Coloured accent stripe along the top edge. */
    s_notif_accent = recompui_create_element(s_notif_ctx, card);
    recompui_set_width(s_notif_accent, 100.0f, UNIT_PERCENT);
    recompui_set_height(s_notif_accent, 5.0f, UNIT_DP);
    recompui_set_border_top_left_radius(s_notif_accent, 8.0f, UNIT_DP);
    recompui_set_border_top_right_radius(s_notif_accent, 8.0f, UNIT_DP);
    recompui_set_background_color(s_notif_accent, &COLOR_GREEN);

    /* Notification message text. */
    s_notif_msg = recompui_create_label(s_notif_ctx, card, "Connected.", LABELSTYLE_NORMAL);
    recompui_set_padding(s_notif_msg, 10.0f, UNIT_DP);
    recompui_set_color(s_notif_msg, &COLOR_WHITE);
    recompui_set_font_size(s_notif_msg, 18.0f, UNIT_DP);

    recompui_close_context(s_notif_ctx);
}

/* =========================================================================
   Player list panel in the shared HUD context
   ========================================================================= */

/* Borrowed from debug.c; this module owns only s_plist_panel and its children. */
static RecompuiContext s_plist_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_plist_panel = RECOMPUI_NULL_RESOURCE;
static int s_plist_refresh_timer = 0;
static int s_plist_visible = 0;
static int s_plist_active_row_count = 0;

/* A UI callback only latches this identity. The frame hook repeatedly resolves
 * a fresh authoritative target until native transfer gates accept or timeout. */
static unsigned int s_requested_transfer_cid = 0;
static unsigned int s_pending_transfer_cid = 0;
static int s_pending_transfer_frames = 0;
static int s_transfer_retry_timer = 0;

/* ---- Per-player icon row resources ------------------------------------ */

typedef struct
{
    RecompuiResource row;        /* outer column container (FLEX_COLUMN) */
    RecompuiResource player_row; /* inner flex-row: icon + label        */
    RecompuiResource icon;
    RecompuiResource label;
    RecompuiResource transfer_action;
    RecompuiResource flute_icon;
    RecompuiResource transfer_button;
    unsigned int cid;
    char name[128];
    int character;
    int room;
    int has_pos;
    int is_self;
    int can_transfer;
    int x;
    int y;
    int z;
} PlayerRowUI;

static PlayerRowUI *s_plist_rows;
static int s_plist_capacity;
static int s_plist_row_count;
static RecompuiResource s_plist_rows_container = RECOMPUI_NULL_RESOURCE;

/* Pre-loaded character textures: 0=Goemon, 1=Ebisumaru, 2=Sasuke, 3=Yae. */
static RecompuiTextureHandle s_char_textures[4];
static RecompuiTextureHandle s_blank_texture;
static RecompuiTextureHandle s_flute_texture;
static int s_textures_initialized = 0;

/** Load character icon textures from baked-in C arrays. */
static void plist_load_textures(void)
{
    if (s_textures_initialized)
        return;
    s_textures_initialized = 1;

    /* 1×1 fully transparent placeholder texture. */
    static const unsigned char blank_px[4] = {0, 0, 0, 0};
    s_blank_texture = recompui_create_texture_rgba32((void *)blank_px, 1, 1);

    s_char_textures[0] = recompui_create_texture_rgba32(
        (void *)icon_goemon_data, ICON_GOEMON_WIDTH, ICON_GOEMON_HEIGHT);
    s_char_textures[1] = recompui_create_texture_rgba32(
        (void *)icon_ebisumaru_data, ICON_EBISUMARU_WIDTH, ICON_EBISUMARU_HEIGHT);
    s_char_textures[2] = recompui_create_texture_rgba32(
        (void *)icon_sasuke_data, ICON_SASUKE_WIDTH, ICON_SASUKE_HEIGHT);
    s_char_textures[3] = recompui_create_texture_rgba32(
        (void *)icon_yae_data, ICON_YAE_WIDTH, ICON_YAE_HEIGHT);
    s_flute_texture = recompui_create_texture_rgba32(
        (void *)icon_flute_data, ICON_FLUTE_WIDTH, ICON_FLUTE_HEIGHT);
}

static void append_char_limited(char *dst, int *pos, int max_len, char value)
{
    if (*pos < max_len - 1)
        dst[(*pos)++] = value;
}

static void append_text_limited(char *dst, int *pos, int max_len, const char *text)
{
    while (*text && *pos < max_len - 1)
        dst[(*pos)++] = *text++;
}

static void append_int_limited(char *dst, int *pos, int max_len, int value)
{
    char tmp[12];
    int len = 0;
    unsigned int v;

    if (value < 0)
    {
        append_char_limited(dst, pos, max_len, '-');
        v = (unsigned int)(-(value + 1)) + 1u;
    }
    else
    {
        v = (unsigned int)value;
    }

    if (v == 0)
    {
        append_char_limited(dst, pos, max_len, '0');
        return;
    }

    while (v > 0 && len < (int)sizeof(tmp))
    {
        tmp[len++] = (char)('0' + (v % 10u));
        v /= 10u;
    }

    while (len > 0)
        append_char_limited(dst, pos, max_len, tmp[--len]);
}

static int plist_int_field(const char *object, const char *key, int fallback)
{
    int value;
    return mnsg_json_get_s32(object, key, &value) ? value : fallback;
}

static unsigned int plist_u32_field(const char *object, const char *key,
                                    unsigned int fallback)
{
    unsigned int value;
    return mnsg_json_get_u32(object, key, &value) ? value : fallback;
}

static void plist_on_transfer_clicked(RecompuiResource resource,
                                      const RecompuiEventData *event,
                                      void *userdata)
{
    unsigned long index = (unsigned long)userdata;
    PlayerRowUI *row;

    (void)resource;
    if (event->type != UI_EVENT_CLICK ||
        index >= (unsigned long)s_plist_active_row_count)
        return;

    row = &s_plist_rows[index];
    if (!row->cid || row->is_self || !row->can_transfer)
        return;

    /* The dynamic row array may move on a later refresh, so latch only the
     * stable network identity. The frame hook owns every other side effect. */
    s_requested_transfer_cid = row->cid;
}

static int plist_ensure_init(void)
{
    RecompuiResource root;

    if (s_plist_panel != RECOMPUI_NULL_RESOURCE)
        return 1;

    /* anchor_ui_update precedes debug_ui_frame_hook in link order on the first
     * frame. Retry instead of creating a second mouse-capturing context. */
    s_plist_ctx = debug_ui_hud_context();
    root = debug_ui_hud_root();
    if (s_plist_ctx == RECOMPUI_NULL_CONTEXT ||
        root == RECOMPUI_NULL_RESOURCE)
        return 0;

    /* Load icon textures (GPU resources, not bound to any context). */
    plist_load_textures();

    recompui_open_context(s_plist_ctx);

    /* Panel: fixed to the top-left corner. */
    s_plist_panel = recompui_create_element(s_plist_ctx, root);
    if (s_plist_panel == RECOMPUI_NULL_RESOURCE)
    {
        recompui_close_context(s_plist_ctx);
        s_plist_ctx = RECOMPUI_NULL_CONTEXT;
        return 0;
    }
    recompui_set_position(s_plist_panel, POSITION_ABSOLUTE);
    recompui_set_left(s_plist_panel, 12.0f, UNIT_DP);
    recompui_set_top(s_plist_panel, 12.0f, UNIT_DP);
    /* Let the widest row determine the panel width. With an absolute left
     * position and no right edge, RmlUi resolves auto as shrink-to-fit. */
    recompui_set_width_auto(s_plist_panel);
    recompui_set_background_color(s_plist_panel, &COLOR_BG);
    recompui_set_border_radius(s_plist_panel, 6.0f, UNIT_DP);
    recompui_set_border_width(s_plist_panel, 1.0f, UNIT_DP);
    recompui_set_border_color(s_plist_panel, &COLOR_BORDER);
    recompui_set_padding(s_plist_panel, 8.0f, UNIT_DP);
    recompui_set_flex_direction(s_plist_panel, FLEX_DIRECTION_COLUMN);
    recompui_set_display(s_plist_panel, DISPLAY_NONE);

    /* "Online Players" title. */
    RecompuiResource title = recompui_create_label(
        s_plist_ctx, s_plist_panel, "Online Players", LABELSTYLE_SMALL);
    recompui_set_color(title, &COLOR_GOLD);
    recompui_set_font_weight(title, 700);
    recompui_set_margin_bottom(title, 2.0f, UNIT_DP);

    /* Thin divider. */
    RecompuiResource divider = recompui_create_element(s_plist_ctx, s_plist_panel);
    /* Its default auto width stretches within the flex column after the panel's
     * intrinsic width is known; a percentage would distort shrink-to-fit. */
    recompui_set_height(divider, 1.0f, UNIT_DP);
    recompui_set_background_color(divider, &COLOR_BORDER);
    recompui_set_margin_bottom(divider, 4.0f, UNIT_DP);

    /* Rows container: vertical flex column holding per-player rows. */
    s_plist_rows_container = recompui_create_element(s_plist_ctx, s_plist_panel);
    recompui_set_display(s_plist_rows_container, DISPLAY_FLEX);
    recompui_set_flex_direction(s_plist_rows_container, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_plist_rows_container, 4.0f, UNIT_DP);
    recompui_set_max_height(s_plist_rows_container, 850.0f, UNIT_DP);
    recompui_set_overflow_y(s_plist_rows_container, OVERFLOW_AUTO);

    recompui_close_context(s_plist_ctx);
    return 1;
}

static void plist_set_visible(int visible)
{
    visible = visible != 0;
    if (s_plist_ctx == RECOMPUI_NULL_CONTEXT ||
        s_plist_panel == RECOMPUI_NULL_RESOURCE)
    {
        if (!visible)
            s_plist_visible = 0;
        return;
    }
    if (visible == s_plist_visible)
        return;

    recompui_open_context(s_plist_ctx);
    recompui_set_display(s_plist_panel,
                         visible ? DISPLAY_FLEX : DISPLAY_NONE);
    recompui_close_context(s_plist_ctx);
    s_plist_visible = visible;
}

static int plist_ensure_rows(int needed)
{
    int i;
    if (!mnsg_array_reserve((void **)&s_plist_rows, &s_plist_capacity,
                            needed, sizeof(*s_plist_rows)))
        return 0;
    recompui_open_context(s_plist_ctx);
    for (i = s_plist_row_count; i < needed; ++i)
    {
        /* ── Outer slot: column layout, hidden until assigned. ─────── */
        s_plist_rows[i].row = recompui_create_element(
            s_plist_ctx, s_plist_rows_container);
        if (s_plist_rows[i].row == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_display(s_plist_rows[i].row, DISPLAY_NONE);
        recompui_set_flex_direction(s_plist_rows[i].row, FLEX_DIRECTION_COLUMN);

        /* ── Player row: flex-row with icon and label. ─────────────── */
        s_plist_rows[i].player_row = recompui_create_element(
            s_plist_ctx, s_plist_rows[i].row);
        if (s_plist_rows[i].player_row == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_display(s_plist_rows[i].player_row, DISPLAY_FLEX);
        recompui_set_flex_direction(s_plist_rows[i].player_row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(s_plist_rows[i].player_row, ALIGN_ITEMS_CENTER);
        recompui_set_gap(s_plist_rows[i].player_row, 6.0f, UNIT_DP);
        recompui_set_padding_top(s_plist_rows[i].player_row, 2.0f, UNIT_DP);

        /* Character icon image view. */
        s_plist_rows[i].icon = recompui_create_imageview(
            s_plist_ctx, s_plist_rows[i].player_row, s_blank_texture);
        if (s_plist_rows[i].icon == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_width(s_plist_rows[i].icon, ICON_SIZE, UNIT_DP);
        recompui_set_height(s_plist_rows[i].icon, ICON_SIZE, UNIT_DP);
        recompui_set_flex_shrink(s_plist_rows[i].icon, 0.0f);

        /* Player name + location label. */
        s_plist_rows[i].label = recompui_create_label(
            s_plist_ctx, s_plist_rows[i].player_row, "", LABELSTYLE_SMALL);
        if (s_plist_rows[i].label == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_color(s_plist_rows[i].label, &COLOR_DIM);
        /* Preserve the label's intrinsic one-line width and keep the action
         * immediately after its text instead of pushing it to a fixed edge. */
        recompui_set_width_auto(s_plist_rows[i].label);
        recompui_set_flex_grow(s_plist_rows[i].label, 0.0f);
        recompui_set_flex_shrink(s_plist_rows[i].label, 0.0f);
        recompui_set_flex_basis_auto(s_plist_rows[i].label);

        /* RecompUI buttons are text-only and cannot own an image child. Put
         * the exact flute texture and a later-created real button in the same
         * positioned wrapper so the transparent button is the hit target. */
        s_plist_rows[i].transfer_action = recompui_create_element(
            s_plist_ctx, s_plist_rows[i].player_row);
        if (s_plist_rows[i].transfer_action == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_display(s_plist_rows[i].transfer_action, DISPLAY_NONE);
        recompui_set_position(s_plist_rows[i].transfer_action, POSITION_RELATIVE);
        recompui_set_width(s_plist_rows[i].transfer_action,
                           TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_min_width(s_plist_rows[i].transfer_action,
                               TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_height(s_plist_rows[i].transfer_action,
                            TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_min_height(s_plist_rows[i].transfer_action,
                                TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_flex_shrink(s_plist_rows[i].transfer_action, 0.0f);

        s_plist_rows[i].flute_icon = recompui_create_imageview(
            s_plist_ctx, s_plist_rows[i].transfer_action, s_flute_texture);
        if (s_plist_rows[i].flute_icon == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_position(s_plist_rows[i].flute_icon, POSITION_ABSOLUTE);
        recompui_set_left(s_plist_rows[i].flute_icon, 0.0f, UNIT_DP);
        recompui_set_top(s_plist_rows[i].flute_icon, 0.0f, UNIT_DP);
        recompui_set_width(s_plist_rows[i].flute_icon,
                           TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_height(s_plist_rows[i].flute_icon,
                            TRANSFER_ACTION_SIZE, UNIT_DP);

        s_plist_rows[i].transfer_button = recompui_create_button(
            s_plist_ctx, s_plist_rows[i].transfer_action, "",
            BUTTONSTYLE_SECONDARY);
        if (s_plist_rows[i].transfer_button == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_position(s_plist_rows[i].transfer_button,
                              POSITION_ABSOLUTE);
        recompui_set_left(s_plist_rows[i].transfer_button, 0.0f, UNIT_DP);
        recompui_set_top(s_plist_rows[i].transfer_button, 0.0f, UNIT_DP);
        recompui_set_width(s_plist_rows[i].transfer_button,
                           TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_min_width(s_plist_rows[i].transfer_button,
                               TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_height(s_plist_rows[i].transfer_button,
                            TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_min_height(s_plist_rows[i].transfer_button,
                                TRANSFER_ACTION_SIZE, UNIT_DP);
        recompui_set_padding(s_plist_rows[i].transfer_button, 0.0f, UNIT_DP);
        recompui_set_border_width(s_plist_rows[i].transfer_button,
                                  0.0f, UNIT_DP);
        recompui_set_background_color(s_plist_rows[i].transfer_button,
                                      &COLOR_TRANSPARENT);
        recompui_set_color(s_plist_rows[i].transfer_button,
                           &COLOR_TRANSPARENT);
        recompui_set_cursor(s_plist_rows[i].transfer_button, CURSOR_POINTER);
        recompui_set_tab_index(s_plist_rows[i].transfer_button, TAB_INDEX_NONE);
        recompui_register_callback(s_plist_rows[i].transfer_button,
                                   plist_on_transfer_clicked,
                                   (void *)(unsigned long)i);
        ++s_plist_row_count;
    }

    recompui_close_context(s_plist_ctx);
    return 1;
failed:
    if (s_plist_rows[i].row != RECOMPUI_NULL_RESOURCE)
        recompui_destroy_element(s_plist_rows_container, s_plist_rows[i].row);
    recompui_close_context(s_plist_ctx);
    return 0;
}

static void plist_clear_pending_transfer(void)
{
    s_requested_transfer_cid = 0;
    s_pending_transfer_cid = 0;
    s_pending_transfer_frames = 0;
    s_transfer_retry_timer = 0;
}

static int plist_parse_transfer_target(const char *json,
                                       unsigned int requested_cid,
                                       unsigned short *room,
                                       short *x, short *y, short *z)
{
    unsigned int parsed_cid;
    unsigned short parsed_room;
    int parsed_x;
    int parsed_y;
    int parsed_z;

    if (!json || !room || !x || !y || !z || !requested_cid ||
        !mnsg_json_get_u32(json, "cid", &parsed_cid) ||
        parsed_cid != requested_cid ||
        !mnsg_json_get_u16(json, "room", &parsed_room) ||
        parsed_room > TRANSFER_MAX_ROOM_ID ||
        !mnsg_json_get_s32(json, "x", &parsed_x) ||
        !mnsg_json_get_s32(json, "y", &parsed_y) ||
        !mnsg_json_get_s32(json, "z", &parsed_z) ||
        parsed_x < -32768 || parsed_x > 32767 ||
        parsed_y < -32768 || parsed_y > 32767 ||
        parsed_z < -32768 || parsed_z > 32767)
        return 0;

    *room = parsed_room;
    *x = (short)parsed_x;
    *y = (short)parsed_y;
    *z = (short)parsed_z;
    return 1;
}

/* Return non-zero once the native transition accepts the request. A rejected
 * target or temporarily busy native world is retried with fresh Python state. */
static int plist_update_pending_transfer(void)
{
    unsigned int cid;
    unsigned short room;
    short x;
    short y;
    short z;
    char *json;
    int valid;

    if (s_requested_transfer_cid)
    {
        s_pending_transfer_cid = s_requested_transfer_cid;
        s_requested_transfer_cid = 0;
        s_pending_transfer_frames = TRANSFER_REQUEST_TIMEOUT_FRAMES;
        s_transfer_retry_timer = 0;
    }

    cid = s_pending_transfer_cid;
    if (!cid)
        return 0;
    if (s_pending_transfer_frames <= 0)
    {
        recomp_printf("[Anchor] Player transfer timed out for client %u\n", cid);
        plist_clear_pending_transfer();
        return 0;
    }
    --s_pending_transfer_frames;

    if (s_transfer_retry_timer > 0)
    {
        --s_transfer_retry_timer;
        return 0;
    }
    s_transfer_retry_timer = TRANSFER_RETRY_FRAMES;

    json = anchor_get_transfer_target_json(cid);
    valid = plist_parse_transfer_target(json, cid, &room, &x, &y, &z);
    if (json)
        recomp_free(json);
    if (!valid)
        return 0;

    if (!anchor_boss_invite_world_transfer_to(room, x, y, z))
        return 0;

    recomp_printf("[Anchor] Transferring to client %u: room=0x%04X xyz=(%d,%d,%d)\n",
                  cid, (unsigned int)room, (int)x, (int)y, (int)z);
    plist_clear_pending_transfer();
    return 1;
}

/* =========================================================================
   Per-frame game hook
   ========================================================================= */

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_ui_update(void)
{
    char *info_json;
    char *cursor;
    char *object;
    char *end;
    int row_count;
    int required;
    int show_room_hex;
    int show_positions;
    unsigned int local_cid;

    /* The first call precedes debug.c's initialization. Retry every frame
     * until its shared mouse-capturing HUD context is available. */
    (void)plist_ensure_init();

    /* ----- Notification countdown ---------------------------------------- */
    if (s_notif_timer > 0)
    {
        s_notif_timer--;
        if (s_notif_timer == 0 && s_notif_ctx != RECOMPUI_NULL_CONTEXT)
            recompui_hide_context(s_notif_ctx);
    }

    /* Debug owns shared-HUD show/hide. This module only toggles its panel. */
    if (!anchor_startup_menu_is_complete())
    {
        plist_set_visible(0);
        plist_clear_pending_transfer();
        s_plist_active_row_count = 0;
        s_plist_refresh_timer = 0;
        return;
    }

    /* ----- Player list maintenance --------------------------------------- */
    if (!anchor_is_connected())
    {
        plist_set_visible(0);
        plist_clear_pending_transfer();
        s_plist_active_row_count = 0;
        s_plist_refresh_timer = 0;
        return;
    }

    if (s_plist_panel == RECOMPUI_NULL_RESOURCE)
        return;

    /* Run before the refresh throttle so a click is handled promptly. The
     * resolver and native transfer helper independently revalidate every gate. */
    if (plist_update_pending_transfer())
        return;

    /* Throttle refresh to once per PLAYER_LIST_REFRESH_FRAMES frames. */
    if (s_plist_refresh_timer > 0)
    {
        s_plist_refresh_timer--;
        return;
    }
    s_plist_refresh_timer = PLAYER_LIST_REFRESH_FRAMES;

    /* cid/self bind each reusable UI slot to its current player. ct is a
     * presentation hint only; a click always resolves a fresh target again. */
    info_json = anchor_get_player_info_json();
    if (!info_json)
        return;

    /* Allocate every roster entry, then bound all field reads to its own
     * object (names may contain braces). */
    row_count = 0;
    required = 0;
    cursor = info_json;
    while (mnsg_json_next_object(&cursor, &end))
    {
        if (required == 0x7fffffff)
        {
            recomp_free(info_json);
            return;
        }
        ++required;
    }
    if (!plist_ensure_rows(required))
    {
        recomp_free(info_json);
        return;
    }
    cursor = info_json;
    while ((object = mnsg_json_next_object(&cursor, &end)) != 0)
    {
        PlayerRowUI *row = &s_plist_rows[row_count++];
        char saved = *end;
        *end = 0;
        row->name[0] = 0;
        mnsg_json_copy_display_string(object, "n", row->name, sizeof(row->name));
        row->cid = plist_u32_field(object, "cid", 0);
        row->is_self = plist_int_field(object, "self", 0) != 0;
        row->can_transfer = plist_int_field(object, "ct", 0) != 0;
        row->character = plist_int_field(object, "c", -1);
        row->room = plist_int_field(object, "r", -1);
        row->has_pos = plist_int_field(object, "hp", 0);
        row->x = plist_int_field(object, "x", 0);
        row->y = plist_int_field(object, "y", 0);
        row->z = plist_int_field(object, "z", 0);
        *end = saved;
    }

    s_plist_active_row_count = row_count;
    recomp_free(info_json);

    /* Read config options once per refresh. */
    show_room_hex = (recomp_get_config_u32("anchor_show_room_hex") == 0);
    show_positions = (recomp_get_config_u32("anchor_show_player_positions") == 0);
    local_cid = anchor_get_client_id();

    static const char s_hex_chars[] = "0123456789ABCDEF";
    static char label_buf[PLAYER_LABEL_BUF_LEN];

    recompui_open_context(s_plist_ctx);
    for (int i = 0; i < row_count; i++)
    {
        /* Show the outer slot (column wrapper). */
        recompui_set_display(s_plist_rows[i].row, DISPLAY_FLEX);

        /* Character icon. */
        int ci = s_plist_rows[i].character;
        RecompuiTextureHandle tex =
            (ci >= 0 && ci < 4) ? s_char_textures[ci] : s_blank_texture;
        recompui_set_imageview_texture(s_plist_rows[i].icon, tex);

        /* Hide the action for our row and for remotes whose latest state is
         * already known not to satisfy transfer-target freshness/safety. */
        recompui_set_display(
            s_plist_rows[i].transfer_action,
            s_plist_rows[i].cid && !s_plist_rows[i].is_self &&
                    s_plist_rows[i].cid != local_cid &&
                    s_plist_rows[i].can_transfer
                ? DISPLAY_BLOCK
                : DISPLAY_NONE);

        /* Player label, optionally with room and position details appended. */
        {
            int len = 0;
            const char *src = s_plist_rows[i].name;
            while (*src && len < 127)
                label_buf[len++] = *src++;

            if (show_room_hex && s_plist_rows[i].room >= 0)
            {
                append_text_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, " (0x");
                unsigned int rid = (unsigned int)s_plist_rows[i].room;
                append_char_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_hex_chars[(rid >> 12) & 0xF]);
                append_char_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_hex_chars[(rid >> 8) & 0xF]);
                append_char_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_hex_chars[(rid >> 4) & 0xF]);
                append_char_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_hex_chars[rid & 0xF]);
                append_char_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, ')');
            }

            if (show_positions && s_plist_rows[i].has_pos)
            {
                append_text_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, " [");
                append_int_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_plist_rows[i].x);
                append_text_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, ", ");
                append_int_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_plist_rows[i].y);
                append_text_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, ", ");
                append_int_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, s_plist_rows[i].z);
                append_char_limited(label_buf, &len, PLAYER_LABEL_BUF_LEN, ']');
            }

            label_buf[len] = '\0';
            recompui_set_text(s_plist_rows[i].label, label_buf);
        }
    }
    /* Hide any slots beyond the current player count. */
    for (int i = row_count; i < s_plist_row_count; i++)
        recompui_set_display(s_plist_rows[i].row, DISPLAY_NONE);
    recompui_close_context(s_plist_ctx);

    plist_set_visible(1);
}
