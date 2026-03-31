/**
 * @file anchor_connect_ui.c
 * @brief Anchor Multiplayer connection setup screen.
 *
 * Provides two persistent UI contexts:
 *
 *  1. **Connection modal** – opened when the player clicks the NET button.
 *     Players fill in server address, port, room ID, player name and team,
 *     then click "Connect" or "Skip / Play Offline" to dismiss it.
 *
 *  2. **"NET" toggle button** – a small button fixed to the bottom-left
 *     corner that reopens the connection modal at any time during gameplay,
 *     allowing players to disconnect or change settings.
 *
 * Both contexts are created lazily on the very first game frame via a
 * RECOMP_HOOK_RETURN on func_80002040_2C40 (the same hook used by debug.c
 * and anchor_ui.c).  The callback/frame-hook pattern from debug.c is used
 * throughout: callbacks only set flags, never call context API functions
 * directly; the frame hook drains those flags on the next frame.
 */

#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "recompconfig.h"
#include "anchor.h"

/* =========================================================================
   Colour palette
   ========================================================================= */

static const RecompuiColor CC_BG = {10, 10, 15, 242};
static const RecompuiColor CC_BORDER = {60, 60, 80, 210};
static const RecompuiColor CC_TITLE_BG = {14, 14, 22, 255};
static const RecompuiColor CC_OVERLAY = {0, 0, 0, 160};
static const RecompuiColor CC_WHITE = {255, 255, 255, 255};
static const RecompuiColor CC_DIM = {165, 165, 175, 255};
static const RecompuiColor CC_TEAL = {0, 178, 158, 255};
static const RecompuiColor CC_GREEN = {55, 175, 75, 255};
static const RecompuiColor CC_RED = {210, 60, 60, 255};
static const RecompuiColor CC_STATUS = {12, 12, 20, 255};
static const RecompuiColor CC_TOGGLE = {10, 10, 18, 210};
static const RecompuiColor CC_FIELDS_LOCK = {10, 10, 15, 155}; /* overlay when connected */

/* =========================================================================
   UI state
   ========================================================================= */

/* Small always-visible "NET" toggle button. */
/* Button is hosted inside debug.c's toggle context to share Z-order. */

/* Full-screen connection-setup modal. */
static RecompuiContext s_modal_ctx = RECOMPUI_NULL_CONTEXT;

static int s_initialized = 0;   /* one-time lazy init guard */
static int s_modal_visible = 0; /* current show/hide state of the modal */

/* Text-input field handles (read when Connect is pressed). */
static RecompuiResource s_in_host = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_port = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_room = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_name = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_team = RECOMPUI_NULL_RESOURCE;

/* Dynamic label / button handles updated at runtime. */
static RecompuiResource s_status_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_disconnect_btn = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_connect_btn = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_skip_btn = RECOMPUI_NULL_RESOURCE;
/* Semi-transparent overlay that covers the input fields when connected,
 * preventing the user from editing them.                                */
static RecompuiResource s_fields_overlay = RECOMPUI_NULL_RESOURCE;

/* Deferred actions set inside callbacks, consumed by the frame hook.
 * Context API calls are forbidden inside callbacks; they are made here. */
static int s_pending_open = 0;
static int s_pending_close = 0;
static int s_pending_connect = 0;
static int s_pending_disconnect = 0;

/* Countdown for the one-shot startup open.  Set to 1 after init so the
 * modal is shown on the frame AFTER debug.c's toggle context has been
 * raised — preventing the toggle context (captures_mouse=1) from landing
 * above the modal and blocking all input.  0 = already fired / inactive. */
static int s_startup_open_delay = 0;

/* Pending status text and its colour indication (1=green / 0=red). */
static const char *s_pending_status = 0;
static int s_pending_status_ok = 0;

/* =========================================================================
   Utility: integer → decimal string (no stdlib required)
   ========================================================================= */

/** Write the decimal representation of @p value into @p out (max 10 chars + NUL). */
static void int_to_str(int value, char *out)
{
    char tmp[12];
    int len = 0;
    if (value == 0)
    {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    if (value < 0)
    {
        *out++ = '-';
        value = -value;
    }
    while (value)
    {
        tmp[len++] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = len - 1; i >= 0; i--)
        *out++ = tmp[i];
    *out = '\0';
}

/** Parse a decimal string to int; returns @p fallback on empty/invalid input. */
static int str_to_int(const char *s, int fallback)
{
    if (!s || !*s)
        return fallback;
    int val = 0, neg = 0;
    if (*s == '-')
    {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

/* =========================================================================
   Callbacks  (flag-setters only – no context API calls inside)
   ========================================================================= */

/* Public: called by debug.c's toggle-context NET button callback. */
void anchor_connect_ui_net_btn_callback(RecompuiResource res,
                                        const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    if (s_modal_visible)
        s_pending_close = 1;
    else
        s_pending_open = 1;
}

static void on_close_clicked(RecompuiResource res,
                             const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_close = 1;
}

static void on_connect_clicked(RecompuiResource res,
                               const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_connect = 1;
}

static void on_skip_clicked(RecompuiResource res,
                            const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_close = 1;
}

static void on_disconnect_clicked(RecompuiResource res,
                                  const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_disconnect = 1;
}

/* =========================================================================
   UI construction  (called once from the frame hook)
   ========================================================================= */

/** Build a labeled text-input row inside @p parent and store the input in *@p out. */
static void make_field(RecompuiContext ctx, RecompuiResource parent,
                       const char *label_text, const char *default_text,
                       RecompuiResource *out)
{
    RecompuiResource lbl = recompui_create_label(ctx, parent, label_text, LABELSTYLE_SMALL);
    recompui_set_color(lbl, &CC_DIM);
    recompui_set_font_size(lbl, 16.0f, UNIT_DP);
    recompui_set_margin_bottom(lbl, -4.0f, UNIT_DP);

    *out = recompui_create_textinput(ctx, parent);
    recompui_set_font_size(*out, 18.0f, UNIT_DP);
    recompui_set_max_width(*out, 100.0f, UNIT_PERCENT);
    recompui_set_input_text(*out, default_text);
}

static void connect_ui_init(void)
{
    /* ── Read mod-config defaults for input pre-population ─────────── */
    char *cfg_host = recomp_get_config_string("anchor_host");
    char *cfg_room = recomp_get_config_string("anchor_room_id");
    char *cfg_name = recomp_get_config_string("anchor_player_name");
    char *cfg_team = recomp_get_config_string("anchor_team_id");

    int cfg_port = (int)recomp_get_config_double("anchor_port");
    if (cfg_port <= 0)
        cfg_port = 43383;

    static char port_str[8];
    int_to_str(cfg_port, port_str);

    /* ── Connection-setup modal ─────────────────────────────────────── */
    s_modal_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_modal_ctx, 1);
    recompui_set_context_captures_mouse(s_modal_ctx, 1);

    recompui_open_context(s_modal_ctx);
    {
        /* Follow the same two-sibling pattern used throughout this codebase
         * (see debug.c): the root has no styling; an absolutely-positioned
         * overlay element dims the scene; a second absolutely-positioned
         * panel element holds all the interactive content.  Using the root
         * itself as the overlay (flex container) causes the recompui layer
         * to swallow mouse events before they reach the children.           */
        RecompuiResource root = recompui_context_root(s_modal_ctx);

        /* ── Full-screen dim overlay (sibling of the panel) ─────────── */
        RecompuiResource overlay = recompui_create_element(s_modal_ctx, root);
        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &CC_OVERLAY);

        /* ── Panel: absolute-centred, 520 dp wide ────────────────────── */
        RecompuiResource panel = recompui_create_element(s_modal_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 50.0f, UNIT_PERCENT);
        recompui_set_top(panel, 15.0f, UNIT_PERCENT);
        recompui_set_margin_left(panel, -260.0f, UNIT_DP); /* centre horizontally */
        recompui_set_width(panel, 520.0f, UNIT_DP);
        recompui_set_height_auto(panel);
        recompui_set_background_color(panel, &CC_BG);
        recompui_set_border_radius(panel, 10.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &CC_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        /* ── Teal accent stripe at the top ──────────────────────────── */
        RecompuiResource accent = recompui_create_element(s_modal_ctx, panel);
        recompui_set_width(accent, 100.0f, UNIT_PERCENT);
        recompui_set_height(accent, 5.0f, UNIT_DP);
        recompui_set_border_top_left_radius(accent, 10.0f, UNIT_DP);
        recompui_set_border_top_right_radius(accent, 10.0f, UNIT_DP);
        recompui_set_background_color(accent, &CC_TEAL);

        /* ── Header: title + close button ───────────────────────────── */
        RecompuiResource hdr = recompui_create_element(s_modal_ctx, panel);
        recompui_set_display(hdr, DISPLAY_FLEX);
        recompui_set_flex_direction(hdr, FLEX_DIRECTION_ROW);
        recompui_set_align_items(hdr, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(hdr, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_padding_left(hdr, 20.0f, UNIT_DP);
        recompui_set_padding_right(hdr, 14.0f, UNIT_DP);
        recompui_set_padding_top(hdr, 16.0f, UNIT_DP);
        recompui_set_padding_bottom(hdr, 16.0f, UNIT_DP);
        recompui_set_background_color(hdr, &CC_TITLE_BG);
        recompui_set_border_bottom_width(hdr, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(hdr, &CC_BORDER);

        RecompuiResource title = recompui_create_label(
            s_modal_ctx, hdr, "Anchor Multiplayer Setup", LABELSTYLE_NORMAL);
        recompui_set_color(title, &CC_TEAL);
        recompui_set_font_weight(title, 700);
        recompui_set_font_size(title, 22.0f, UNIT_DP);

        RecompuiResource close_btn = recompui_create_button(
            s_modal_ctx, hdr, "X", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(close_btn, CURSOR_POINTER);
        recompui_set_font_size(close_btn, 18.0f, UNIT_DP);
        recompui_set_tab_index(close_btn, TAB_INDEX_AUTO);
        recompui_register_callback(close_btn, on_close_clicked, 0);

        /* ── Body: use outer padding + column flex ───────────────────── */
        RecompuiResource body = recompui_create_element(s_modal_ctx, panel);
        recompui_set_display(body, DISPLAY_FLEX);
        recompui_set_flex_direction(body, FLEX_DIRECTION_COLUMN);
        recompui_set_padding(body, 20.0f, UNIT_DP);
        recompui_set_gap(body, 12.0f, UNIT_DP);

        /* ── Description text ───────────────────────────────────────── */
        RecompuiResource desc = recompui_create_label(
            s_modal_ctx, body,
            "Connect to an Anchor server to sync items & flags with teammates.\n"
            "Leave fields at their defaults or enter your own server details.",
            LABELSTYLE_SMALL);
        recompui_set_color(desc, &CC_DIM);
        recompui_set_font_size(desc, 15.0f, UNIT_DP);

        /* ── Fields wrapper: relative-positioned so the lock overlay works ── */
        RecompuiResource fields_wrap = recompui_create_element(s_modal_ctx, body);
        recompui_set_display(fields_wrap, DISPLAY_FLEX);
        recompui_set_flex_direction(fields_wrap, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(fields_wrap, 12.0f, UNIT_DP);
        recompui_set_position(fields_wrap, POSITION_RELATIVE);

        /* ── Two-column row for Address (left) and Port (right) ──────── */
        RecompuiResource addr_row = recompui_create_element(s_modal_ctx, fields_wrap);
        recompui_set_display(addr_row, DISPLAY_FLEX);
        recompui_set_flex_direction(addr_row, FLEX_DIRECTION_ROW);
        recompui_set_gap(addr_row, 12.0f, UNIT_DP);
        recompui_set_align_items(addr_row, ALIGN_ITEMS_FLEX_END);

        /* Address column (grows to fill available space). */
        RecompuiResource addr_col = recompui_create_element(s_modal_ctx, addr_row);
        recompui_set_display(addr_col, DISPLAY_FLEX);
        recompui_set_flex_direction(addr_col, FLEX_DIRECTION_COLUMN);
        recompui_set_flex_grow(addr_col, 1.0f);
        recompui_set_gap(addr_col, 4.0f, UNIT_DP);
        make_field(s_modal_ctx, addr_col, "Server Address",
                   (cfg_host && cfg_host[0]) ? cfg_host : "localhost",
                   &s_in_host);

        /* Port column (fixed 100 dp). */
        RecompuiResource port_col = recompui_create_element(s_modal_ctx, addr_row);
        recompui_set_display(port_col, DISPLAY_FLEX);
        recompui_set_flex_direction(port_col, FLEX_DIRECTION_COLUMN);
        recompui_set_width(port_col, 100.0f, UNIT_DP);
        recompui_set_gap(port_col, 4.0f, UNIT_DP);
        make_field(s_modal_ctx, port_col, "Port", port_str, &s_in_port);

        /* ── Remaining full-width fields ────────────────────────────── */
        RecompuiResource room_col = recompui_create_element(s_modal_ctx, fields_wrap);
        recompui_set_display(room_col, DISPLAY_FLEX);
        recompui_set_flex_direction(room_col, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(room_col, 4.0f, UNIT_DP);
        make_field(s_modal_ctx, room_col, "Room ID",
                   (cfg_room && cfg_room[0]) ? cfg_room : "mnsg-recomp",
                   &s_in_room);

        /* Two-column row: Player Name (left) + Team ID (right). */
        RecompuiResource nt_row = recompui_create_element(s_modal_ctx, fields_wrap);
        recompui_set_display(nt_row, DISPLAY_FLEX);
        recompui_set_flex_direction(nt_row, FLEX_DIRECTION_ROW);
        recompui_set_gap(nt_row, 12.0f, UNIT_DP);
        recompui_set_align_items(nt_row, ALIGN_ITEMS_FLEX_END);

        RecompuiResource name_col = recompui_create_element(s_modal_ctx, nt_row);
        recompui_set_display(name_col, DISPLAY_FLEX);
        recompui_set_flex_direction(name_col, FLEX_DIRECTION_COLUMN);
        recompui_set_flex_grow(name_col, 1.0f);
        recompui_set_gap(name_col, 4.0f, UNIT_DP);
        make_field(s_modal_ctx, name_col, "Player Name",
                   (cfg_name && cfg_name[0]) ? cfg_name : "Player",
                   &s_in_name);

        RecompuiResource team_col = recompui_create_element(s_modal_ctx, nt_row);
        recompui_set_display(team_col, DISPLAY_FLEX);
        recompui_set_flex_direction(team_col, FLEX_DIRECTION_COLUMN);
        recompui_set_flex_grow(team_col, 1.0f);
        recompui_set_gap(team_col, 4.0f, UNIT_DP);
        make_field(s_modal_ctx, team_col, "Team ID",
                   (cfg_team && cfg_team[0]) ? cfg_team : "default",
                   &s_in_team);

        /* ── Lock overlay: covers all fields when connected ─────────── */
        s_fields_overlay = recompui_create_element(s_modal_ctx, fields_wrap);
        recompui_set_position(s_fields_overlay, POSITION_ABSOLUTE);
        recompui_set_left(s_fields_overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(s_fields_overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(s_fields_overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(s_fields_overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(s_fields_overlay, &CC_FIELDS_LOCK);
        recompui_set_display(s_fields_overlay, DISPLAY_NONE);

        /* ── Status bar ─────────────────────────────────────────────── */
        RecompuiResource status_row = recompui_create_element(s_modal_ctx, body);
        recompui_set_display(status_row, DISPLAY_FLEX);
        recompui_set_align_items(status_row, ALIGN_ITEMS_CENTER);
        recompui_set_padding_left(status_row, 12.0f, UNIT_DP);
        recompui_set_padding_right(status_row, 12.0f, UNIT_DP);
        recompui_set_padding_top(status_row, 10.0f, UNIT_DP);
        recompui_set_padding_bottom(status_row, 10.0f, UNIT_DP);
        recompui_set_background_color(status_row, &CC_STATUS);
        recompui_set_border_radius(status_row, 5.0f, UNIT_DP);
        recompui_set_min_height(status_row, 44.0f, UNIT_DP);

        s_status_lbl = recompui_create_label(
            s_modal_ctx, status_row,
            "Fill in the settings above and click Connect, or click Skip to play offline.",
            LABELSTYLE_SMALL);
        recompui_set_color(s_status_lbl, &CC_DIM);
        recompui_set_font_size(s_status_lbl, 15.0f, UNIT_DP);

        /* ── Button row ─────────────────────────────────────────────── */
        RecompuiResource btn_row = recompui_create_element(s_modal_ctx, body);
        recompui_set_display(btn_row, DISPLAY_FLEX);
        recompui_set_flex_direction(btn_row, FLEX_DIRECTION_ROW);
        recompui_set_gap(btn_row, 10.0f, UNIT_DP);
        recompui_set_justify_content(btn_row, JUSTIFY_CONTENT_FLEX_END);

        /* Skip button (secondary, rightmost but before Connect). */
        s_skip_btn = recompui_create_button(
            s_modal_ctx, btn_row, "Skip / Play Offline", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(s_skip_btn, CURSOR_POINTER);
        recompui_set_font_size(s_skip_btn, 16.0f, UNIT_DP);
        recompui_set_tab_index(s_skip_btn, TAB_INDEX_AUTO);
        recompui_register_callback(s_skip_btn, on_skip_clicked, 0);

        /* Disconnect button (hidden until connected). */
        s_disconnect_btn = recompui_create_button(
            s_modal_ctx, btn_row, "Disconnect", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(s_disconnect_btn, CURSOR_POINTER);
        recompui_set_font_size(s_disconnect_btn, 16.0f, UNIT_DP);
        recompui_set_tab_index(s_disconnect_btn, TAB_INDEX_AUTO);
        recompui_set_display(s_disconnect_btn, DISPLAY_NONE);
        recompui_register_callback(s_disconnect_btn, on_disconnect_clicked, 0);

        /* Connect button (primary, rightmost). Hidden when connected. */
        s_connect_btn = recompui_create_button(
            s_modal_ctx, btn_row, "Connect", BUTTONSTYLE_PRIMARY);
        recompui_set_cursor(s_connect_btn, CURSOR_POINTER);
        recompui_set_font_size(s_connect_btn, 16.0f, UNIT_DP);
        recompui_set_tab_index(s_connect_btn, TAB_INDEX_AUTO);
        recompui_register_callback(s_connect_btn, on_connect_clicked, 0);
    }
    recompui_close_context(s_modal_ctx);

    /* Free config strings (must use recomp_free_config_string). */
    if (cfg_host)
        recomp_free_config_string(cfg_host);
    if (cfg_room)
        recomp_free_config_string(cfg_room);
    if (cfg_name)
        recomp_free_config_string(cfg_name);
    if (cfg_team)
        recomp_free_config_string(cfg_team);
}

/* =========================================================================
   Internal: update modal's dynamic state (status label + disconnect button)
   Must only be called from the frame hook (not inside a callback).
   ========================================================================= */

static void apply_status_update(void)
{
    if (!s_pending_status || !s_modal_visible)
    {
        s_pending_status = 0;
        return;
    }

    recompui_open_context(s_modal_ctx);

    /* Update status text and colour. */
    recompui_set_text(s_status_lbl, s_pending_status);
    recompui_set_color(s_status_lbl,
                       s_pending_status_ok ? &CC_GREEN : &CC_RED);

    /* Sync button visibility with current connection state. */
    int connected = anchor_is_connected();
    recompui_set_display(s_connect_btn, connected ? DISPLAY_NONE : DISPLAY_BLOCK);
    recompui_set_display(s_skip_btn, connected ? DISPLAY_NONE : DISPLAY_BLOCK);
    recompui_set_display(s_disconnect_btn, connected ? DISPLAY_BLOCK : DISPLAY_NONE);
    recompui_set_display(s_fields_overlay, connected ? DISPLAY_BLOCK : DISPLAY_NONE);

    recompui_close_context(s_modal_ctx);
    s_pending_status = 0;
}

/* =========================================================================
   Per-frame hook
   ========================================================================= */

/**
 * Hooked on the game's main per-frame tick.  On the very first frame it
 * builds all UI and shows the connection modal; thereafter it drains the
 * pending-action flags produced by the button callbacks.
 */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_connect_ui_frame_hook(void)
{
    /* ── One-time lazy initialisation (frame 1 only) ────────────────── */
    if (!s_initialized)
    {
        s_initialized = 1;
        connect_ui_init();
        /* Delay the startup open by one frame so debug.c's toggle context
         * (captures_mouse=1) is already shown before the modal appears.
         * Both hooks fire on the same frame; anchor runs before debug
         * (alphabetical), so without this delay the toggle ends up above
         * the modal and blocks all input.                                */
        s_startup_open_delay = 1;
        return;
    }

    /* ── Startup open (fires the frame after debug.c shows toggle_ctx) ─ */
    if (s_startup_open_delay > 0)
    {
        s_startup_open_delay--;
        if (s_startup_open_delay == 0)
        {
            /* Set the flag and return – the modal will be shown NEXT frame,
             * after debug_ui_frame_hook has already raised s_toggle_ctx.
             * Processing it this frame would still race with the toggle show
             * and leave toggle above the modal (blocking all input).       */
            s_pending_open = 1;
            return;
        }
    }

    /* ── Connect action ─────────────────────────────────────────────── */
    if (s_pending_connect)
    {
        s_pending_connect = 0;

        /* Read all input fields in one open/close block. */
        recompui_open_context(s_modal_ctx);
        char *host_str = recompui_get_input_text(s_in_host);
        char *port_str = recompui_get_input_text(s_in_port);
        char *room_str = recompui_get_input_text(s_in_room);
        char *name_str = recompui_get_input_text(s_in_name);
        char *team_str = recompui_get_input_text(s_in_team);
        recompui_close_context(s_modal_ctx);

        int port = str_to_int(port_str, 43383);
        if (port <= 0 || port > 65535)
            port = 43383;

        /* Disconnect first if already connected. */
        if (anchor_is_connected())
            anchor_disconnect();

        int ok = anchor_connect(
            (host_str && host_str[0]) ? host_str : "anchor.hm64.org",
            port,
            (room_str && room_str[0]) ? room_str : "mnsg-recomp",
            (name_str && name_str[0]) ? name_str : "Player",
            0, /* client_id = 0 → let the server assign one */
            (team_str && team_str[0]) ? team_str : "default");

        // int ok = 1;
        if (ok)
        {
            /* Close the modal next frame exactly like Skip/X does. */
            s_pending_close = 1;
        }
        else
        {
            s_pending_status = "Connection failed \xe2\x80\x93 check address / port and try again.";
            s_pending_status_ok = 0;
        }

        if (host_str)
            recomp_free(host_str);
        if (port_str)
            recomp_free(port_str);
        if (room_str)
            recomp_free(room_str);
        if (name_str)
            recomp_free(name_str);
        if (team_str)
            recomp_free(team_str);
    }

    /* ── Disconnect action ──────────────────────────────────────────── */
    if (s_pending_disconnect)
    {
        s_pending_disconnect = 0;
        anchor_disconnect();
        s_pending_status =
            "Disconnected. Update settings and reconnect, or close to play offline.";
        s_pending_status_ok = 0;
        /* Restore the Connect button / remove the field lock immediately
         * since the modal stays visible after disconnecting.             */
        recompui_open_context(s_modal_ctx);
        recompui_set_display(s_connect_btn, DISPLAY_BLOCK);
        recompui_set_display(s_skip_btn, DISPLAY_BLOCK);
        recompui_set_display(s_disconnect_btn, DISPLAY_NONE);
        recompui_set_display(s_fields_overlay, DISPLAY_NONE);
        recompui_close_context(s_modal_ctx);
    }

    /* ── Open modal ─────────────────────────────────────────────────── */
    if (s_pending_open)
    {
        s_pending_open = 0;
        if (!s_modal_visible)
        {
            /* Refresh UI state to match current connection state. */
            int connected = anchor_is_connected();
            recompui_open_context(s_modal_ctx);
            /* Connect button only shown when not connected; Disconnect
             * button only shown when connected.                          */
            recompui_set_display(s_connect_btn,
                                 connected ? DISPLAY_NONE : DISPLAY_BLOCK);
            recompui_set_display(s_skip_btn,
                                 connected ? DISPLAY_NONE : DISPLAY_BLOCK);
            recompui_set_display(s_disconnect_btn,
                                 connected ? DISPLAY_BLOCK : DISPLAY_NONE);
            /* Lock overlay blocks editing of fields while connected.    */
            recompui_set_display(s_fields_overlay,
                                 connected ? DISPLAY_BLOCK : DISPLAY_NONE);
            if (connected)
            {
                recompui_set_text(s_status_lbl,
                                  "Currently connected. Disconnect to change settings.");
                recompui_set_color(s_status_lbl, &CC_GREEN);
            }
            else
            {
                recompui_set_text(s_status_lbl,
                                  "Not connected. Update settings and click Connect, or close to stay offline.");
                recompui_set_color(s_status_lbl, &CC_DIM);
            }
            recompui_close_context(s_modal_ctx);

            recompui_show_context(s_modal_ctx);
            s_modal_visible = 1;
        }
    }

    /* ── Close modal ────────────────────────────────────────────────── */
    if (s_pending_close)
    {
        s_pending_close = 0;
        if (s_modal_visible)
        {
            recompui_hide_context(s_modal_ctx);
            // recompui_close_context(s_modal_ctx);

            s_modal_visible = 0;
        }
    }

    /* ── Flush status-label update from connect/disconnect ──────────── */
    apply_status_update();
}
