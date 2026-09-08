/**
 * @file anchor_ui.c
 * @brief Anchor UI overlays for MNSG: Recompiled.
 *
 * Provides two persistent overlay contexts:
 *
 *  1. **Connection notification banner** – a brief popup that appears near
 *     the top of the screen every time a connection attempt is made.  The
 *     accent bar is green on success, red on failure.  It auto-dismisses
 *     after NOTIFICATION_FRAMES game frames (~5 s at 60 fps).
 *
 *  2. **Player list panel** – a semi-transparent panel anchored to the
 *     top-left corner that lists the names of every player currently in the
 *     Anchor room.  It is refreshed once per second while connected, and
 *     hidden automatically when the client disconnects.
 *
 * Both contexts are created lazily on first use and never capture keyboard
 * or mouse input so they never interfere with gameplay.
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
#include "anchor_runtime.h"
#include "utils/array_utils.h"
#include "utils/json_utils.h"
#include "icon_goemon.h"
#include "icon_ebisumaru.h"
#include "icon_sasuke.h"
#include "icon_yae.h"

/* Provided by debug.c – re-raises the toggle-button context to the top of
 * the recompui Z-stack after any other persistent context is shown.         */
extern void debug_ui_bump_toggle_ctx(void);

/* =========================================================================
   Tunables
   ========================================================================= */

/** Frames the connection notification stays on screen (~5 s @ 60 fps). */
#define NOTIFICATION_FRAMES 300

/** Frames between player-list refreshes (~1 s @ 60 fps). */
#define PLAYER_LIST_REFRESH_FRAMES 60

/** Icon image size in DP units (square). */
#define ICON_SIZE 24.0f

/** Width of the player-list panel in DP units. */
#define PANEL_WIDTH 300.0f

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
   Player list context
   ========================================================================= */

static RecompuiContext s_plist_ctx = RECOMPUI_NULL_CONTEXT;
static int s_plist_refresh_timer = 0;
static int s_plist_visible = 0;

/* ---- Per-player icon row resources ------------------------------------ */

typedef struct
{
    RecompuiResource row;        /* outer column container (FLEX_COLUMN) */
    RecompuiResource player_row; /* inner flex-row: icon + label        */
    RecompuiResource icon;
    RecompuiResource label;
    char name[128];
    int character;
    int room;
    int has_pos;
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

static void plist_ensure_init(void)
{
    if (s_plist_ctx != RECOMPUI_NULL_CONTEXT)
        return;

    /* Load icon textures (GPU resources, not bound to any context). */
    plist_load_textures();

    s_plist_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_plist_ctx, 0);
    recompui_set_context_captures_mouse(s_plist_ctx, 0);
    recompui_open_context(s_plist_ctx);

    RecompuiResource root = recompui_context_root(s_plist_ctx);

    /* Panel: fixed to the top-left corner. */
    RecompuiResource panel = recompui_create_element(s_plist_ctx, root);
    recompui_set_position(panel, POSITION_ABSOLUTE);
    recompui_set_left(panel, 12.0f, UNIT_DP);
    recompui_set_top(panel, 12.0f, UNIT_DP);
    recompui_set_width(panel, PANEL_WIDTH, UNIT_DP);
    recompui_set_background_color(panel, &COLOR_BG);
    recompui_set_border_radius(panel, 6.0f, UNIT_DP);
    recompui_set_border_width(panel, 1.0f, UNIT_DP);
    recompui_set_border_color(panel, &COLOR_BORDER);
    recompui_set_padding(panel, 8.0f, UNIT_DP);
    recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);
    recompui_set_display(panel, DISPLAY_FLEX);

    /* "Online Players" title. */
    RecompuiResource title = recompui_create_label(
        s_plist_ctx, panel, "Online Players", LABELSTYLE_SMALL);
    recompui_set_color(title, &COLOR_GOLD);
    recompui_set_font_weight(title, 700);
    recompui_set_margin_bottom(title, 2.0f, UNIT_DP);

    /* Thin divider. */
    RecompuiResource divider = recompui_create_element(s_plist_ctx, panel);
    recompui_set_width(divider, 100.0f, UNIT_PERCENT);
    recompui_set_height(divider, 1.0f, UNIT_DP);
    recompui_set_background_color(divider, &COLOR_BORDER);
    recompui_set_margin_bottom(divider, 4.0f, UNIT_DP);

    /* Rows container: vertical flex column holding per-player rows. */
    s_plist_rows_container = recompui_create_element(s_plist_ctx, panel);
    recompui_set_display(s_plist_rows_container, DISPLAY_FLEX);
    recompui_set_flex_direction(s_plist_rows_container, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_plist_rows_container, 4.0f, UNIT_DP);
    recompui_set_max_height(s_plist_rows_container, 850.0f, UNIT_DP);
    recompui_set_overflow_y(s_plist_rows_container, OVERFLOW_AUTO);

    recompui_close_context(s_plist_ctx);
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

        /* Player name + location label. */
        s_plist_rows[i].label = recompui_create_label(
            s_plist_ctx, s_plist_rows[i].player_row, "", LABELSTYLE_SMALL);
        if (s_plist_rows[i].label == RECOMPUI_NULL_RESOURCE)
            goto failed;
        recompui_set_color(s_plist_rows[i].label, &COLOR_DIM);
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

/* =========================================================================
   Per-frame game hook
   ========================================================================= */

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_ui_update(void)
{
    /* ----- One-time eager context creation --------------------------------
     * plist_ctx must be created BEFORE debug.c creates its toggle context so
     * that the toggle context sits at a higher Z-order.  recompui places
     * later-created contexts on top; if plist_ctx were created lazily (after
     * the toggle context) its root would be above the DBG/NET buttons and
     * block their mouse events even with captures_mouse = 0.
     * anchor_ui_update runs before debug_ui_frame_hook every frame (link
     * order is alphabetical), so calling plist_ensure_init() here on the
     * very first frame guarantees the correct stacking order.              */
    {
        static int s_ctx_created = 0;
        if (!s_ctx_created)
        {
            s_ctx_created = 1;
            plist_ensure_init();
        }
    }

    /* ----- Notification countdown ---------------------------------------- */
    if (s_notif_timer > 0)
    {
        s_notif_timer--;
        if (s_notif_timer == 0 && s_notif_ctx != RECOMPUI_NULL_CONTEXT)
            recompui_hide_context(s_notif_ctx);
    }

    /* Startup/race configuration screens are full-screen interactive modals.
     * Keep the player list out of the UI stack until the game has actually
     * launched so RT64 and startup menus retain control. */
    if (!anchor_startup_menu_is_complete())
    {
        if (s_plist_visible && s_plist_ctx != RECOMPUI_NULL_CONTEXT)
        {
            recompui_hide_context(s_plist_ctx);
            s_plist_visible = 0;
        }
        s_plist_refresh_timer = 0;
        return;
    }

    /* ----- Player list maintenance --------------------------------------- */
    if (!anchor_is_connected())
    {
        if (s_plist_visible && s_plist_ctx != RECOMPUI_NULL_CONTEXT)
        {
            recompui_hide_context(s_plist_ctx);
            s_plist_visible = 0;
        }
        s_plist_refresh_timer = 0;
        return;
    }

    /* Ensure context exists the first time we are connected. */
    plist_ensure_init();

    /* Throttle refresh to once per PLAYER_LIST_REFRESH_FRAMES frames. */
    if (s_plist_refresh_timer > 0)
    {
        s_plist_refresh_timer--;
        return;
    }
    s_plist_refresh_timer = PLAYER_LIST_REFRESH_FRAMES;

    /* Fetch structured player info from Python:
     * [{"n":"Name - Location","c":0,"r":165,"hp":1,"x":10,"y":20,"z":30}, ...]
     * where "c" is the character index (0=Goemon..3=Yae, -1=unknown). */
    char *info_json = anchor_get_player_info_json();
    if (!info_json)
        return;

    /* Allocate every roster entry, then bound all field reads to its own
     * object (names may contain braces). */
    int row_count = 0;
    int required = 0;
    char *cursor = info_json;
    char *object;
    char *end;
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
        row->character = plist_int_field(object, "c", -1);
        row->room = plist_int_field(object, "r", -1);
        row->has_pos = plist_int_field(object, "hp", 0);
        row->x = plist_int_field(object, "x", 0);
        row->y = plist_int_field(object, "y", 0);
        row->z = plist_int_field(object, "z", 0);
        *end = saved;
    }

    recomp_free(info_json);

    /* Read config options once per refresh. */
    int show_room_hex = (recomp_get_config_u32("anchor_show_room_hex") == 0);
    int show_positions = (recomp_get_config_u32("anchor_show_player_positions") == 0);

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

    /* Show the panel the first time we have data (also handles reconnect
     * after a disconnect, since s_plist_visible is cleared on hide).        */
    if (!s_plist_visible)
    {
        recompui_show_context(s_plist_ctx);
        s_plist_visible = 1;
        /* recompui_show_context bumps s_plist_ctx to the top of the Z-stack;
         * immediately re-raise the toggle-button context so DBG/NET buttons
         * are always above the player-list panel and remain clickable.      */
        debug_ui_bump_toggle_ctx();
    }
}
