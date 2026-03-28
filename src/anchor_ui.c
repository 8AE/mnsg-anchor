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
 *   anchor_ui_show_notification(msg, is_success)  – call from anchor.c after
 *       a connect/disconnect attempt.
 *   anchor_ui_update()        – RECOMP_HOOK_RETURN on func_80002040_2C40
 *       drives the timer and the player-list refresh.
 */

#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"
#include "icon_goemon.h"
#include "icon_ebisumaru.h"
#include "icon_sasuke.h"
#include "icon_yae.h"

/* Current scene/room ID – written by the game engine each frame. */
extern unsigned short D_800C7AB2;

/*
 * Current character index – written by the character cycler every time the
 * player presses L/R to switch characters (func_801DD50C_59941C patch).
 *
 * In the rando's character_cycler.c:
 *   u32 *other_ptr = (u32 *)0x8015C5D8;  // D_8015C5D8_15D1D8
 *   other_ptr[1] = index & 0xFF;         // → 0x8015C5DC
 *
 * Values: 0=Goemon  1=Ebisumaru  2=Sasuke  3=Yae
 */
#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

static const char *const s_char_names[4] = {
    "Goemon", "Ebisumaru", "Sasuke", "Yae"};

/*
 * Player background-world (CLS_BG_W) pointer.
 *
 * D_801FC60C_5B851C holds a CLS_BG_W* at runtime.  The struct layout
 * (from MNSGRecompRando include/common_structs.h):
 *
 *   CLS_W   header;      // offset 0x00, size 0x08 (next ptr + kind/pri)
 *   VEC3F_W position;    // offset 0x08: float x, float y, float z
 *
 * So position.x = *(float*)(ptr + 0x08), .y at +0x0C, .z at +0x10.
 * The pointer may be NULL between scenes; always null-check before use.
 */
extern void *D_801FC60C_5B851C;

/* =========================================================================
   Tunables
   ========================================================================= */

/** Frames the connection notification stays on screen (~5 s @ 60 fps). */
#define NOTIFICATION_FRAMES 300

/** Frames between player-list refreshes (~1 s @ 60 fps). */
#define PLAYER_LIST_REFRESH_FRAMES 60

/** Maximum player rows shown in the list panel. */
#define MAX_DISPLAY_PLAYERS 16

/** Icon image size in DP units (square). */
#define ICON_SIZE 24.0f

/** Width of the player-list panel in DP units. */
#define PANEL_WIDTH 300.0f

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

/* Last room ID we sent to Python; 0xFFFF = "not sent yet". */
static unsigned short s_prev_room_id = 0xFFFF;

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
    RecompuiResource row;   /* flex-row container                       */
    RecompuiResource icon;  /* imageview showing the character icon      */
    RecompuiResource label; /* text label with "Name - Location"         */
} PlayerRowUI;

static PlayerRowUI s_plist_rows[MAX_DISPLAY_PLAYERS];
static RecompuiResource s_plist_rows_container = RECOMPUI_NULL_RESOURCE;

/* Pre-loaded character textures: index matches s_char_names[]. */
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

    /* Pre-allocate MAX_DISPLAY_PLAYERS rows; all start hidden. */
    for (int i = 0; i < MAX_DISPLAY_PLAYERS; i++)
    {
        /* Row: flex row, items vertically centred, small horizontal gap. */
        s_plist_rows[i].row = recompui_create_element(
            s_plist_ctx, s_plist_rows_container);
        recompui_set_display(s_plist_rows[i].row, DISPLAY_NONE);
        recompui_set_flex_direction(s_plist_rows[i].row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(s_plist_rows[i].row, ALIGN_ITEMS_CENTER);
        recompui_set_gap(s_plist_rows[i].row, 6.0f, UNIT_DP);

        /* Character icon image view. */
        s_plist_rows[i].icon = recompui_create_imageview(
            s_plist_ctx, s_plist_rows[i].row, s_blank_texture);
        recompui_set_width(s_plist_rows[i].icon, ICON_SIZE, UNIT_DP);
        recompui_set_height(s_plist_rows[i].icon, ICON_SIZE, UNIT_DP);

        /* Player name + location label. */
        s_plist_rows[i].label = recompui_create_label(
            s_plist_ctx, s_plist_rows[i].row, "", LABELSTYLE_SMALL);
        recompui_set_color(s_plist_rows[i].label, &COLOR_DIM);
    }

    recompui_close_context(s_plist_ctx);
}

/* =========================================================================
   Public API
   ========================================================================= */

/**
 * @brief Show a connection notification banner.
 *
 * @param msg        UTF-8 message string to display.  Pass NULL for a default.
 * @param is_success Non-zero => green (connected); zero => red (failed).
 */
void anchor_ui_show_notification(const char *msg, int is_success)
{
    notif_ensure_init();

    recompui_open_context(s_notif_ctx);

    /* Set accent colour: green for success, red for failure. */
    recompui_set_background_color(s_notif_accent, is_success ? &COLOR_GREEN : &COLOR_RED);

    /* Set message text. */
    if (msg && msg[0])
        recompui_set_text(s_notif_msg, msg);
    else
        recompui_set_text(s_notif_msg, is_success ? "Connected to Anchor." : "Anchor connection failed.");

    recompui_close_context(s_notif_ctx);

    recompui_show_context(s_notif_ctx);
    s_notif_timer = NOTIFICATION_FRAMES;
}

/* =========================================================================
   Per-frame game hook
   ========================================================================= */

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_ui_update(void)
{
    /* ----- Room-ID tracking (runs every frame, even when disconnected) ---- */
    {
        unsigned short cur_room = D_800C7AB2;
        if (cur_room != s_prev_room_id)
        {
            s_prev_room_id = cur_room;
            anchor_set_local_room((unsigned int)cur_room);
        }
    }

    /* ----- Notification countdown ---------------------------------------- */
    if (s_notif_timer > 0)
    {
        s_notif_timer--;
        if (s_notif_timer == 0 && s_notif_ctx != RECOMPUI_NULL_CONTEXT)
            recompui_hide_context(s_notif_ctx);
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

    /* ---- Send our world position to teammates ----------------------------- */
    {
        void *bg_w = D_801FC60C_5B851C;
        if (bg_w)
        {
            int px = (int)*(float *)((char *)bg_w + 0x08);
            int py = (int)*(float *)((char *)bg_w + 0x0C);
            int pz = (int)*(float *)((char *)bg_w + 0x10);
            anchor_set_position(px, py, pz);
        }
    }

    /* ---- Broadcast currently selected character to teammates ------------- */
    {
        unsigned int char_idx = *CURRENT_CHAR_PTR & 0x3;
        anchor_set_character(s_char_names[char_idx]);
    }

    /* Fetch structured player info from Python:
     * [{"n":"Name - Location","c":0}, ...]
     * where "c" is the character index (0=Goemon..3=Yae, -1=unknown). */
    char *info_json = anchor_get_player_info_json();
    if (!info_json)
        return;

    /* Parse info_json into local row data arrays (no JSON library needed:
     * the format is fixed and machine-generated). */
    static char row_name_buf[MAX_DISPLAY_PLAYERS][128];
    static int row_char_idx[MAX_DISPLAY_PLAYERS];
    int row_count = 0;
    const char *p = info_json;

    while (*p && row_count < MAX_DISPLAY_PLAYERS)
    {
        /* Locate the "n":" key. */
        while (*p && !(*p == '"' && *(p + 1) == 'n' && *(p + 2) == '"' && *(p + 3) == ':' && *(p + 4) == '"'))
            p++;
        if (!*p)
            break;
        p += 5; /* skip "n":" */

        /* Read name string until the closing quote (honour \" escapes). */
        int n = 0;
        while (*p && *p != '"' && n < (int)sizeof(row_name_buf[0]) - 1)
        {
            if (*p == '\\' && *(p + 1))
                p++; /* skip escape prefix */
            row_name_buf[row_count][n++] = *p++;
        }
        row_name_buf[row_count][n] = '\0';
        if (*p == '"')
            p++;

        /* Locate the "c": key. */
        while (*p && !(*p == '"' && *(p + 1) == 'c' && *(p + 2) == '"' && *(p + 3) == ':'))
            p++;
        if (!*p)
            break;
        p += 4; /* skip "c": */

        /* Read integer value (may be negative for -1). */
        int sign = 1;
        if (*p == '-')
        {
            sign = -1;
            p++;
        }
        int cidx = 0;
        while (*p >= '0' && *p <= '9')
            cidx = cidx * 10 + (*p++ - '0');
        row_char_idx[row_count] = cidx * sign;

        row_count++;
    }

    recomp_free(info_json);

    /* Update the pre-allocated player rows in the panel. */
    recompui_open_context(s_plist_ctx);
    for (int i = 0; i < row_count; i++)
    {
        recompui_set_display(s_plist_rows[i].row, DISPLAY_FLEX);
        int ci = row_char_idx[i];
        RecompuiTextureHandle tex =
            (ci >= 0 && ci < 4) ? s_char_textures[ci] : s_blank_texture;
        recompui_set_imageview_texture(s_plist_rows[i].icon, tex);
        recompui_set_text(s_plist_rows[i].label, row_name_buf[i]);
    }
    /* Hide any rows beyond the current player count. */
    for (int i = row_count; i < MAX_DISPLAY_PLAYERS; i++)
        recompui_set_display(s_plist_rows[i].row, DISPLAY_NONE);
    recompui_close_context(s_plist_ctx);

    /* Show the panel the first time we have data. */
    if (!s_plist_visible)
    {
        recompui_show_context(s_plist_ctx);
        s_plist_visible = 1;
    }
}
