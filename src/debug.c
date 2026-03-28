/**
 * @file debug.c
 * @brief Anchor debug menu – force any tracked flag/item for all players.
 *
 * Opens a scrollable modal panel listing every item and flag tracked by
 * item_sync.c.  Clicking "Force" for an entry:
 *   1. Writes the value unconditionally into the local save file.
 *   2. Broadcasts it via anchor_send_flag with add_to_queue=1 so all
 *      connected teammates receive it immediately and offline teammates
 *      receive it when they reconnect.
 *
 * Usage
 * -----
 *   - A small "DBG" button is shown in the bottom-right corner of the screen.
 *   - Click it to open or close the debug panel.
 *   - Click "✕" inside the panel to close it.
 *   - The panel captures input while open so game controls are suspended.
 *
 * The panel is initialised lazily on the first game frame (RECOMP_HOOK_RETURN
 * on func_80002040_2C40) so it is safe to add this file to any build that
 * already includes anchor.c and item_sync.c.
 */

#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"

/* Provided by item_sync.c – applies a flag locally and broadcasts it. */
void item_sync_force_flag(const char *name);

/* =========================================================================
   Flag / item entry table
   =========================================================================
   Entries with key == NULL are non-interactive section headers; all others
   are clickable rows backed by a flag in item_sync.c's sync tables.
   ========================================================================= */

typedef struct
{
    const char *key;     /* NULL → section header; otherwise the anchor flag name */
    const char *display; /* Section title, or human-readable item/flag name       */
} DebugEntry;

static const DebugEntry s_entries[] = {
    /* ── Characters ─────────────────────────────────────────────────── */
    {0, "Characters"},
    {"chr_goemon", "Goemon"},
    {"chr_ebisu", "Ebisumaru"},
    {"chr_sasuke", "Sasuke"},
    {"chr_yae", "Yae"},

    /* ── Equipment ───────────────────────────────────────────────────── */
    {0, "Equipment"},
    {"eq_chain", "Chain Pipe"},
    {"eq_hammer", "Meat Hammer"},
    {"eq_firecrk", "Firecracker Bomb"},
    {"eq_flute", "Flute"},
    {"eq_camera", "Wind-up Camera"},
    {"eq_kunai", "Ice Kunai"},
    {"eq_bazooka", "Bazooka"},
    {"eq_fire_ryo", "Medal of Flames"},

    /* ── Abilities ───────────────────────────────────────────────────── */
    {0, "Abilities"},
    {"ab_impact", "Sudden Impact"},
    {"ab_mini_ebi", "Mini Ebisumaru"},
    {"ab_jetpack", "Jetpack"},
    {"ab_mermaid", "Mermaid Magic"},

    /* ── Quest Items ─────────────────────────────────────────────────── */
    {0, "Quest Items"},
    {"ki_triton", "Triton Shell"},
    {"ki_superps", "Super Pass"},
    {"ki_achilles", "Achilles' Heel"},
    {"ki_cucumber", "Quality Cucumber"},
    {"ki_map_jpn", "Map of Japan"},

    /* ── Miracle Items ───────────────────────────────────────────────── */
    {0, "Miracle Items"},
    {"mi_star", "Miracle Star"},
    {"mi_moon", "Miracle Moon"},
    {"mi_flower", "Miracle Flower"},
    {"mi_snow", "Miracle Snow"},

    /* ── Boss Defeats ────────────────────────────────────────────────── */
    {0, "Boss Defeats"},
    {"fl_dharmanyo", "Dharmanyo Defeated"},
    {"fl_thaisamba", "Thaisamba Defeated"},
    {"fl_tsurami", "Tsurami Defeated"},
    {"fl_benkei", "Benkei Defeated"},
    {"fl_congo", "Congo Defeated"},

    /* ── Character & Ability Flags ───────────────────────────────────── */
    {0, "Character & Ability Flags"},
    {"fl_sasuke_rc", "Sasuke Recruited"},
    {"fl_mini_ebi", "Mini Ebisumaru (Flag)"},
    {"fl_yae_rc", "Yae Recruited"},
    {"fl_s_impact", "Sudden Impact (Flag)"},
    {"fl_mermaid", "Mermaid Magic (Flag)"},
    {"fl_superjmp", "Jetpack (Flag)"},
    {"fl_mi_snow", "Miracle Snow (Flag)"},

    /* ── Quest Flags ─────────────────────────────────────────────────── */
    {0, "Quest Flags"},
    {"fl_superpass", "Super Pass (Flag)"},
    {"fl_sp_gate", "Super Pass Gate"},
    {"fl_gym_key", "Jump Gym Key"},
    {"fl_chain", "Chain Pipe (Flag)"},
    {"fl_fire_ryo", "Medal of Flames (Flag)"},
    {"fl_crane_on", "Crane Power On"},
    {"fl_map_jpn", "Map of Japan (Flag)"},
    {"fl_bat_sas", "Sasuke Battery"},

    /* ── Dungeon Keys: Oedo Castle ───────────────────────────────────── */
    {0, "Keys: Oedo Castle"},
    {"ky_s_oc_tile", "OC: Silver (1F Tile)"},
    {"ky_s_oc_1f", "OC: Silver (1F)"},
    {"ky_g_oc_1f", "OC: Gold (1F)"},
    {"ky_s_oc_cp", "OC: Silver (Chain Pipe)"},
    {"ky_s_oc_crsh", "OC: Silver (2F Crusher)"},
    {"ky_s_oc_2f", "OC: Silver (2F)"},

    /* ── Dungeon Keys: Ghost Toys Castle ─────────────────────────────── */
    {0, "Keys: Ghost Toys Castle"},
    {"ky_s_gt_flwr", "GTC: Silver (1F Flower)"},
    {"ky_s_gt_crn", "GTC: Silver (1F Crane)"},
    {"ky_s_gt_inv", "GTC: Silver (1F Invisible)"},
    {"ky_s_gt_spin", "GTC: Silver (2F Spinning)"},
    {"ky_s_gt_dar", "GTC: Silver (2F Darumanyo)"},
    {"ky_g_gt_ff", "GTC: Gold (2F False Floor)"},
    {"ky_d_gt_sc", "GTC: Diamond (2F Spike Cannon)"},
    {"ky_s_gt_bil", "GTC: Silver (2F Billiards)"},

    /* ── Dungeon Keys: Festival Temple Castle ────────────────────────── */
    {0, "Keys: Festival Temple"},
    {"ky_g_ft_hot", "FTC: Gold (Hot)"},
    {"ky_s_ft_ring", "FTC: Silver (Ring)"},

    /* ── Dungeon Keys: Gourmet Submarine ─────────────────────────────── */
    {0, "Keys: Gourmet Submarine"},
    {"ky_s_gs_baz", "GS: Silver (2F Bazooka)"},
    {"ky_g_gs_jet", "GS: Gold (2F Jetpack)"},
    {"ky_s_gs_lava", "GS: Silver (2F Lava)"},
    {"ky_s_gs_uw", "GS: Silver (2F Underwater)"},
    {"ky_s_gs_swd", "GS: Silver (3F Sword)"},
    {"ky_d_gs_inv", "GS: Diamond (3F Invisible)"},
    {"ky_s_gs_sus", "GS: Silver (3F Sushi)"},

    /* ── Dungeon Keys: Musical Castle ────────────────────────────────── */
    {0, "Keys: Musical Castle"},
    {"ky_g_mc_fan", "MC: Gold (1F Fan)"},
    {"ky_s_mc_tall", "MC: Silver (1F Tall)"},
    {"ky_g_mc_hj", "MC: Gold (1F High Jump)"},
    {"ky_g_mc_mini", "MC: Gold (1F Mini)"},
    {"ky_d_mc_cube", "MC: Diamond (1F Cube)"},
    {"ky_d_mc2", "MC: Diamond (2F)"},
};

#define NUM_ENTRIES ((int)(sizeof(s_entries) / sizeof(s_entries[0])))

/* =========================================================================
   Colours
   ========================================================================= */

static const RecompuiColor C_BG = {8, 8, 8, 235};
static const RecompuiColor C_BORDER = {70, 70, 70, 200};
static const RecompuiColor C_TITLE_BG = {18, 18, 18, 255};
static const RecompuiColor C_STATUS_BG = {12, 12, 12, 255};
static const RecompuiColor C_SEC_BG = {25, 55, 25, 255};
static const RecompuiColor C_ROW_EVEN = {16, 16, 16, 255};
static const RecompuiColor C_ROW_ODD = {22, 22, 22, 255};
static const RecompuiColor C_WHITE = {255, 255, 255, 255};
static const RecompuiColor C_DIM = {165, 165, 165, 255};
static const RecompuiColor C_GOLD = {220, 190, 60, 255};
static const RecompuiColor C_GREEN = {55, 175, 75, 255};
static const RecompuiColor C_TEAL = {0, 178, 158, 255};
static const RecompuiColor C_TOGGLE_BG = {10, 10, 10, 195};
static const RecompuiColor C_OVERLAY = {0, 0, 0, 145};

/* =========================================================================
   UI state
   ========================================================================= */

/* Small toggle button context – always visible, never captures input. */
static RecompuiContext s_toggle_ctx = RECOMPUI_NULL_CONTEXT;

/* Full modal context – shown while the menu is open. */
static RecompuiContext s_modal_ctx = RECOMPUI_NULL_CONTEXT;

/* Label updated after every Force click to show what was last forced. */
static RecompuiResource s_status_label = RECOMPUI_NULL_RESOURCE;

static int s_initialized = 0;    /* guard for one-time lazy initialisation */
static int s_toggle_visible = 0; /* 1 after the toggle button is shown    */
static int s_modal_visible = 0;  /* tracks actual shown/hidden state      */

/* Pending actions set by callbacks; consumed by the frame hook.
 * Callbacks must NEVER call show/hide/set_captures directly –
 * those are only safe outside of a callback (i.e. from the frame hook).  */
static int s_pending_open = 0;
static int s_pending_close = 0;

/* Pending status-label text update (also deferred to frame hook).        */
static const char *s_pending_status = 0;

/* =========================================================================
   Callbacks  (flag-setters only – no context API calls here)
   ========================================================================= */

/** Toggle button: request open or close depending on current state. */
static void on_toggle_clicked(RecompuiResource res,
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

/** Close button inside the modal header. */
static void on_close_clicked(RecompuiResource res,
                             const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_close = 1;
}

/**
 * "Force" button for a flag row.
 *
 * userdata is the entry index cast to void*.  Calls item_sync_force_flag
 * which writes the value to the local save and broadcasts via Anchor.
 */
static void on_force_clicked(RecompuiResource res,
                             const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type != UI_EVENT_CLICK)
        return;
    unsigned long idx = (unsigned long)ud;
    if (idx >= (unsigned long)NUM_ENTRIES || !s_entries[idx].key)
        return;

    /* Apply locally + broadcast to all players including this one. */
    item_sync_force_flag(s_entries[idx].key);

    /* Defer the status-label update: opening a context inside a callback
     * would trigger "attempted to open a UI context without closing another".
     * The frame hook picks this up on the next frame instead.              */
    s_pending_status = s_entries[idx].display;

    recomp_printf("[Debug] Forced: %s (%s)\n",
                  s_entries[idx].key, s_entries[idx].display);
}

/* =========================================================================
   UI construction (called once from the frame hook)
   ========================================================================= */

#define ROW_H 68.0f     /* height of each flag row                      */
#define HDR_BAR_H 64.0f /* header bar height                            */
#define STATUS_H 48.0f  /* status bar height                            */

static void debug_init_ui(void)
{
    int i;

    /* ── Toggle button (bottom-right corner of the screen) ──────────── */

    s_toggle_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_toggle_ctx, 0);
    recompui_set_context_captures_mouse(s_toggle_ctx, 1); /* must be 1 for button clicks to register */

    recompui_open_context(s_toggle_ctx);
    {
        RecompuiResource root = recompui_context_root(s_toggle_ctx);

        /* Full-screen wrapper so right/bottom absolute positioning works. */
        RecompuiResource screen = recompui_create_element(s_toggle_ctx, root);
        recompui_set_position(screen, POSITION_ABSOLUTE);
        recompui_set_left(screen, 0.0f, UNIT_PERCENT);
        recompui_set_top(screen, 0.0f, UNIT_PERCENT);
        recompui_set_width(screen, 100.0f, UNIT_PERCENT);
        recompui_set_height(screen, 100.0f, UNIT_PERCENT);

        RecompuiResource btn = recompui_create_button(
            s_toggle_ctx, screen, "DBG", BUTTONSTYLE_SECONDARY);
        recompui_set_position(btn, POSITION_ABSOLUTE);
        recompui_set_right(btn, 12.0f, UNIT_DP);
        recompui_set_bottom(btn, 12.0f, UNIT_DP);
        recompui_set_font_size(btn, 13.0f, UNIT_DP);
        recompui_set_background_color(btn, &C_TOGGLE_BG);
        recompui_set_border_color(btn, &C_BORDER);
        recompui_set_cursor(btn, CURSOR_POINTER);
        recompui_set_tab_index(btn, TAB_INDEX_NONE);
        recompui_register_callback(btn, on_toggle_clicked, 0);
    }
    recompui_close_context(s_toggle_ctx);
    /* show is deferred to the frame hook via s_toggle_visible flag */

    /* ── Modal panel (hidden at startup) ─────────────────────────────── */

    s_modal_ctx = recompui_create_context();
    /* Captures are fixed permanently – never changed after creation.
     * show/hide controls the actual open/closed state.                  */
    recompui_set_context_captures_input(s_modal_ctx, 1);
    recompui_set_context_captures_mouse(s_modal_ctx, 1);

    recompui_open_context(s_modal_ctx);
    {
        RecompuiResource root = recompui_context_root(s_modal_ctx);

        /* ── Full-screen translucent overlay (dims the game behind the panel) */
        RecompuiResource overlay = recompui_create_element(s_modal_ctx, root);
        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &C_OVERLAY);

        /* ── Centred panel ──────────────────────────────────────────── */
        RecompuiResource panel = recompui_create_element(s_modal_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 5.0f, UNIT_PERCENT);
        recompui_set_top(panel, 4.0f, UNIT_PERCENT);
        recompui_set_width(panel, 90.0f, UNIT_PERCENT);
        recompui_set_height(panel, 92.0f, UNIT_PERCENT);
        recompui_set_background_color(panel, &C_BG);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &C_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        /* ── Header row: title + close button ──────────────────────── */
        RecompuiResource hdr = recompui_create_element(s_modal_ctx, panel);
        recompui_set_display(hdr, DISPLAY_FLEX);
        recompui_set_flex_direction(hdr, FLEX_DIRECTION_ROW);
        recompui_set_align_items(hdr, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(hdr, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_padding_left(hdr, 20.0f, UNIT_DP);
        recompui_set_padding_right(hdr, 12.0f, UNIT_DP);
        recompui_set_padding_top(hdr, 14.0f, UNIT_DP);
        recompui_set_padding_bottom(hdr, 14.0f, UNIT_DP);
        recompui_set_background_color(hdr, &C_TITLE_BG);
        recompui_set_border_top_left_radius(hdr, 8.0f, UNIT_DP);
        recompui_set_border_top_right_radius(hdr, 8.0f, UNIT_DP);
        recompui_set_border_bottom_width(hdr, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(hdr, &C_BORDER);

        RecompuiResource title = recompui_create_label(
            s_modal_ctx, hdr, "Debug: Force Flag", LABELSTYLE_NORMAL);
        recompui_set_color(title, &C_GOLD);
        recompui_set_font_weight(title, 700);
        recompui_set_font_size(title, 22.0f, UNIT_DP);

        RecompuiResource close_btn = recompui_create_button(
            s_modal_ctx, hdr, "X", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(close_btn, CURSOR_POINTER);
        recompui_set_font_size(close_btn, 18.0f, UNIT_DP);
        recompui_set_tab_index(close_btn, TAB_INDEX_NONE);
        recompui_register_callback(close_btn, on_close_clicked, 0);

        /* ── Status bar: shows the last forced flag name ──────────── */
        RecompuiResource status = recompui_create_element(s_modal_ctx, panel);
        recompui_set_display(status, DISPLAY_FLEX);
        recompui_set_flex_direction(status, FLEX_DIRECTION_ROW);
        recompui_set_align_items(status, ALIGN_ITEMS_CENTER);
        recompui_set_padding_top(status, 10.0f, UNIT_DP);
        recompui_set_padding_bottom(status, 10.0f, UNIT_DP);
        recompui_set_padding_left(status, 16.0f, UNIT_DP);
        recompui_set_padding_right(status, 16.0f, UNIT_DP);
        recompui_set_background_color(status, &C_STATUS_BG);
        recompui_set_border_bottom_width(status, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(status, &C_BORDER);

        RecompuiResource pfx = recompui_create_label(
            s_modal_ctx, status, "Last forced: ", LABELSTYLE_SMALL);
        recompui_set_color(pfx, &C_DIM);
        recompui_set_font_size(pfx, 16.0f, UNIT_DP);

        s_status_label = recompui_create_label(
            s_modal_ctx, status, "\xe2\x80\x94", LABELSTYLE_SMALL); /* — */
        recompui_set_color(s_status_label, &C_TEAL);
        recompui_set_font_weight(s_status_label, 600);
        recompui_set_font_size(s_status_label, 16.0f, UNIT_DP);

        /* ── Scrollable flag list ──────────────────────────────────── */
        RecompuiResource scroll = recompui_create_element(s_modal_ctx, panel);
        recompui_set_flex_grow(scroll, 1.0f);
        recompui_set_overflow_y(scroll, OVERFLOW_SCROLL);
        recompui_set_display(scroll, DISPLAY_FLEX);
        recompui_set_flex_direction(scroll, FLEX_DIRECTION_COLUMN);
        recompui_set_padding(scroll, 10.0f, UNIT_DP);
        recompui_set_gap(scroll, 3.0f, UNIT_DP);

        /* Build flag rows.  Section headers are plain styled labels;
         * flag rows are flex rows with a text label and a Force button. */
        int row_parity = 0;

        for (i = 0; i < NUM_ENTRIES; ++i)
        {
            if (s_entries[i].key == 0)
            {
                /* ── Section header ─────────────────────────────────── */
                RecompuiResource sec = recompui_create_element(
                    s_modal_ctx, scroll);
                recompui_set_background_color(sec, &C_SEC_BG);
                recompui_set_padding_left(sec, 16.0f, UNIT_DP);
                recompui_set_padding_right(sec, 16.0f, UNIT_DP);
                recompui_set_padding_top(sec, 10.0f, UNIT_DP);
                recompui_set_padding_bottom(sec, 10.0f, UNIT_DP);
                recompui_set_border_radius(sec, 4.0f, UNIT_DP);
                recompui_set_margin_top(sec, 14.0f, UNIT_DP);

                RecompuiResource sec_lbl = recompui_create_label(
                    s_modal_ctx, sec, s_entries[i].display, LABELSTYLE_SMALL);
                recompui_set_color(sec_lbl, &C_GREEN);
                recompui_set_font_weight(sec_lbl, 700);
                recompui_set_font_size(sec_lbl, 18.0f, UNIT_DP);

                row_parity = 0; /* reset stripe for each new section */
            }
            else
            {
                /* ── Flag row ───────────────────────────────────────── */
                const RecompuiColor *bg =
                    (row_parity & 1) ? &C_ROW_ODD : &C_ROW_EVEN;
                row_parity++;

                RecompuiResource row = recompui_create_element(
                    s_modal_ctx, scroll);
                recompui_set_display(row, DISPLAY_FLEX);
                recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
                recompui_set_align_items(row, ALIGN_ITEMS_CENTER);
                recompui_set_justify_content(row, JUSTIFY_CONTENT_SPACE_BETWEEN);
                recompui_set_min_height(row, 52.0f, UNIT_DP);
                recompui_set_padding_left(row, 16.0f, UNIT_DP);
                recompui_set_padding_right(row, 12.0f, UNIT_DP);
                recompui_set_background_color(row, bg);
                recompui_set_border_radius(row, 3.0f, UNIT_DP);

                /* Flag display name (takes up remaining width). */
                RecompuiResource name_lbl = recompui_create_label(
                    s_modal_ctx, row, s_entries[i].display, LABELSTYLE_SMALL);
                recompui_set_color(name_lbl, &C_WHITE);
                recompui_set_flex_grow(name_lbl, 1.0f);
                recompui_set_font_size(name_lbl, 18.0f, UNIT_DP);

                /* Flag key shown in dimmer text to the right of the name. */
                RecompuiResource key_lbl = recompui_create_label(
                    s_modal_ctx, row, s_entries[i].key, LABELSTYLE_ANNOTATION);
                recompui_set_color(key_lbl, &C_DIM);
                recompui_set_margin_right(key_lbl, 14.0f, UNIT_DP);
                recompui_set_font_size(key_lbl, 16.0f, UNIT_DP);

                /* Force button.  userdata = entry index so the callback
                 * can resolve the key and display strings. */
                RecompuiResource force_btn = recompui_create_button(
                    s_modal_ctx, row, "Force", BUTTONSTYLE_PRIMARY);
                recompui_set_cursor(force_btn, CURSOR_POINTER);
                recompui_set_font_size(force_btn, 16.0f, UNIT_DP);
                recompui_set_tab_index(force_btn, TAB_INDEX_NONE);
                recompui_register_callback(
                    force_btn, on_force_clicked, (void *)(unsigned long)i);
            }
        }
    }
    recompui_close_context(s_modal_ctx);
    /* Modal stays hidden until the toggle button is clicked. */
}

/* =========================================================================
   Per-frame hook – lazy one-time initialisation
   ========================================================================= */

/**
 * Hooked on the game's main per-frame tick alongside anchor_ui.c and
 * item_sync.c.  Builds the UI on the very first frame, then becomes a no-op.
 */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void debug_ui_frame_hook(void)
{
    if (!s_initialized)
    {
        s_initialized = 1;
        debug_init_ui();
        return;
    }

    /* Show the toggle button once at startup. */
    if (!s_toggle_visible)
    {
        recompui_show_context(s_toggle_ctx);
        s_toggle_visible = 1;
    }

    /* Process pending open/close actions from callbacks. */
    if (s_pending_open)
    {
        s_pending_open = 0;
        if (!s_modal_visible)
        {
            recompui_show_context(s_modal_ctx);
            s_modal_visible = 1;
        }
    }
    if (s_pending_close)
    {
        s_pending_close = 0;
        if (s_modal_visible)
        {
            recompui_hide_context(s_modal_ctx);
            s_modal_visible = 0;
        }
    }

    /* Flush pending status-label text update. */
    if (s_pending_status && s_status_label != RECOMPUI_NULL_RESOURCE)
    {
        recompui_open_context(s_modal_ctx);
        recompui_set_text(s_status_label, s_pending_status);
        recompui_close_context(s_modal_ctx);
        s_pending_status = 0;
    }
}
