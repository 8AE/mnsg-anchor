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
#include "recompconfig.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"
#include "anchor_runtime.h"
#include "anchor_flag_catalog.h"

/* Set to 1 to show the DBG button in the bottom-right corner. */
static int DEBUG_BUTTON_ENABLED = 0;

/* Provided by item_sync.c */
void item_sync_force_flag(const char *name);
void item_sync_force_flag_val(const char *name, int val);

/* NET toggle button callback provided by anchor_connect_ui.c */
extern void anchor_connect_ui_net_btn_callback(RecompuiResource res,
                                               const RecompuiEventData *ev, void *ud);

static int debug_text_equals(const char *a, const char *b)
{
    int i = 0;

    if (!a || !b)
        return 0;

    while (a[i] && b[i])
    {
        if (a[i] != b[i])
            return 0;
        i++;
    }

    return a[i] == b[i];
}

/* =========================================================================
   Flag / item entry table
   =========================================================================
   Entries with key == NULL are non-interactive section headers; all others
   are clickable rows backed by a flag in item_sync.c's sync tables.
   ========================================================================= */

static const AnchorFlagEntry s_entries[] = {
    /* ── Characters ─────────────────────────────────────────────────── */
    {0, "Characters", 0},
    {"chr_goemon", "Goemon", 0},
    {"chr_ebisu", "Ebisumaru", 0},
    {"chr_sasuke", "Sasuke", 0},
    {"chr_yae", "Yae", 0},

    /* ── Equipment ───────────────────────────────────────────────────── */
    {0, "Equipment", 0},
    {"eq_chain", "Goemon Chain Pipe", 0},
    {"eq_hammer", "Ebisumaru Meat Hammer", 0},
    {"eq_firecrk", "Sasuke Bomb", 0},
    {"eq_flute", "Yae Flute (Item)", 0},
    {"eq_camera", "Windup Camera", 0},
    {"eq_kunai", "Ebisumaru Camera", 0},
    {"eq_bazooka", "Sasuke Ice Kunai", 0},
    {"eq_fire_ryo", "Fire Ryo", 0},

    /* ── Abilities ───────────────────────────────────────────────────── */
    {0, "Abilities", 0},
    {"ab_impact", "Sudden Impact", 0},
    {"ab_mini_ebi", "Mini Ebisu", 0},
    {"ab_jetpack", "Jetpack", 0},
    {"ab_mermaid", "Mermaid", 0},

    /* ── Quest Items ─────────────────────────────────────────────────── */
    {0, "Quest Items", 0},
    {"ki_triton", "Triton Shell (Item)", 0},
    {"ki_superps", "Super Pass (Item)", 0},
    {"ki_achilles", "Achilles Heel", 0},
    {"ki_traindoor", "Key (Gold, Silver or Diamond)", 0},
    {"ki_cucumber", "Cucumber", 0},
    {"ki_map_jpn", "Map of Japan", 0},

    /* ── Warp Points ─────────────────────────────────────────────────── */
    {0, "Warp Points", 0},
    {"wp_goemon_h", "Goemon's House", 0},
    {"wp_kai_hwy", "Kai Highway", 0},
    {"wp_oedo", "Oedo Castle", 0},
    {"wp_zazen", "Zazen Town", 0},
    {"wp_kii_cafe", "Kii Coffee Shop", 0},
    {"wp_folkypoke", "Folkypoke Village", 0},
    {"wp_kompira", "Kompira Mountain", 0},
    {"wp_iyo_tea", "Iyo Tea House", 0},
    {"wp_ghost", "Ghost Toys Castle", 0},
    {"wp_izumo_tea", "Izumo Tea House", 0},
    {"wp_festival", "Festival Temple", 0},
    {"wp_fest_vill", "Festival Village", 0},
    {"wp_witch", "Witch's Hut", 0},

    /* ── Miracle Items ───────────────────────────────────────────────── */
    {0, "Miracle Items", 0},
    {"mi_star", "Miracle Star", 0},
    {"mi_moon", "Miracle Moon", 0},
    {"mi_flower", "Miracle Flower", 0},
    {"mi_snow", "Miracle Snow", 0},

    /* ── Boss Defeats ────────────────────────────────────────────────── */
    {0, "Boss Defeats", 0},
    {"fl_dharmanyo", "Dharmanyo Defeated", 0},
    {"fl_thaisamba", "Thaisamba Defeated", 0},
    {"fl_tsurami", "Tsurami Defeated", 0},
    {"fl_benkei", "Benkei Defeated", 0},
    {"fl_congo", "Congo Defeated Reward Spawn", 0},

    /* ── Character & Ability Flags ───────────────────────────────────── */
    {0, "Character & Ability Flags", 0},
    {"fl_sasuke_rc", "Sasuke Recruited", 0},
    {"fl_mini_ebi", "Mini Ebismaru Obtained", 0},
    {"fl_yae_rc", "Yae Recurited", 0},
    {"fl_s_impact", "Suddent Impact", 0},
    {"fl_mermaid", "Mermaid Magic", 0},
    {"fl_superjmp", "Jetpack", 0},
    {"fl_mi_snow", "Miracle Snow (Flag)", 0},

    /* ── Quest Flags ─────────────────────────────────────────────────── */
    {0, "Quest Flags", 0},
    {"fl_superpass", "Aquired Lords Super Pass", 0},
    {"fl_sp_gate", "Bridge Guards Opened Gate Already", 0},
    {"fl_gym_key", "Jump Gym Key", 0},
    {"fl_chain", "Chain Pipe (Flag)", 0},
    {"fl_fire_ryo", "Medal of Flames (Flag)", 0},
    {"fl_crane_on", "Crane Power On", 0},
    {"fl_map_jpn", "Map of Japan (Flag)", 0},
    {"fl_bat_sas", "Sasuke Battery(Flag)", 0},

    /* ── Dungeon Keys: Oedo Castle ───────────────────────────────────── */
    {0, "Keys: Oedo Castle", 0},
    {"ky_s_oc_tile", "OC: Falling Platforms Silver Key", 0},
    {"ky_s_oc_1f", "OC: Silver Key Room Near Congos Hand", 0},
    {"ky_g_oc_1f", "OC: Gold Key(1F)", 0},
    {"ky_s_oc_cp", "OC: Silver (Chain Pipe)", 0},
    {"ky_s_oc_crsh", "OC: Floor Panel Room Silver Key", 0},
    {"ky_s_oc_2f", "OC: First Fight Silver Key", 0},

    /* ── Dungeon Keys: Ghost Toys Castle ─────────────────────────────── */
    {0, "Keys: Ghost Toys Castle", 0},
    {"ky_s_gt_flwr", "Ghost Toys Ghost and Bean fight room Silver Key", 0},
    {"ky_s_gt_crn", "Ghost Toys Picture floor room Gold Key", 0},
    {"ky_s_gt_inv", "Festival Temple Boss Fight Boss Beaten", 0},
    {"ky_s_gt_spin", "Festival Temple Miracle Item Received", 0},
    {"ky_s_gt_dar", "Ghost Toys Flower Pot Room Silver Key", 0},
    {"ky_g_gt_ff", "Ghost Toys Crane Game Room Silver Key", 0},
    {"ky_d_gt_sc", "GTC: Diamond (2F Spike Cannon)", 0},
    {"ky_s_gt_bil", "Get Camera", 0},

    /* ── Dungeon Keys: Festival Temple Castle ────────────────────────── */
    {0, "Keys: Festival Temple", 0},
    {"ky_g_ft_hot", "FTC: Gold Key", 0},
    {"ky_s_ft_ring", "FTC: Silver Key", 0},

    /* ── Dungeon Keys: Gourmet Submarine ─────────────────────────────── */
    {0, "Keys: Gourmet Submarine", 0},
    {"ky_s_gs_baz", "GS: Silver (2F Bazooka)", 0},
    {"ky_g_gs_jet", "GS: Gold Key (2F Jetpack)", 0},
    {"ky_s_gs_lava", "GS: Silver Key (2F Lava)", 0},
    {"ky_s_gs_uw", "GS: Silver Key (2F Underwater)", 0},
    {"ky_s_gs_swd", "GS: Silver Key (3F Sword)", 0},
    {"ky_d_gs_inv", "GS: Diamond Key (3F Invisible)", 0},
    {"ky_s_gs_sus", "GS: Silver Key (3F Sushi)", 0},

    /* ── Dungeon Keys: Musical Castle ────────────────────────────────── */
    {0, "Keys: Musical Castle", 0},
    {"ky_g_mc_fan", "Gorgeous Musical Castle Spinning Block Platforming Diamond Key", 0},
    {"ky_s_mc_tall", "Gorgeous Musical Castle Raising Platform Diamond Key", 0},
    {"ky_g_mc_hj", "Gorgeous Musical Castle Conveyor Belt Room Switch Hit", 0},
    {"ky_g_mc_mini", "MC: Gold (1F Mini)", 0},
    {"ky_d_mc_cube", "Gorgeous Musical Castle Fan Room Gold Key", 0},
    {"ky_d_mc2", "Gorgeous Musical Castle First Climb Silver Key", 0},

    /* ── Fish Counts ─────────────────────────────────────────────────── */
    {0, "Fish Counts", 0},
    {"fish_red", "Red Fish", 0},
    {"fish_yellow", "Yellow Fish", 0},
    {"fish_blue", "Blue Fish", 0},

    /* ── Fishing Quest Flags ─────────────────────────────────────────── */
    {0, "Fishing Quest Flags", 0},
    {"fl_fish_r_on", "Looking for Red Fish", 0},
    {"fl_fish_y_on", "Looking for Yello Fish", 0},
    {"fl_fish_b_on", "Looking for Blue Fish", 0},
    {"fl_fish_r_mx", "Red Fish: Max Caught", 0},
    {"fl_fish_y_mx", "Yellow Fish: Max Caught", 0},
    {"fl_fish_b_mx", "Blue Fish: Max Caught", 0},

    /* ── Cat Eyes Shop ───────────────────────────────────────────────── */
    {0, "Cat Eyes Shop", 0},
    {"fl_ce_dharma", "Bought Dharma", 0},
    {"fl_ce_notice", "Bought Notice Board", 0},
    {"fl_ce_doll", "Bought Doll", 0},

    /* ── World / Story Events ────────────────────────────────────────── */
    {0, "World Events", 0},
    {"fl_koryuta", "Freed Koryuta the Dragon", 0},
    {"fl_outerspace", "Went to Outer Space", 0},
    {"fl_to_space", "Going to Outer Space", 0},
    {"fl_baron_iga", "Met Baron in Iga", 0},
    {"fl_mokubei", "Mokubei Can Upgrade Weapons", 0},
    {"fl_wiseman", "Met Ghost of Wise Man", 0},
    {"fl_witch_np", "Spoke to Witch (But Didn't Pay)", 0},
    {"fl_kyushu", "Kyushu Disappeared", 0},
    {"fl_dragon_fp", "Riding Dragon from Folkypoke", 0},
    {"fl_sas_res", "Resurrect Sasuke (Plasma tip)", 0},

    /* ── Benkei / Ushiwaka Quest Chain ───────────────────────────────── */
    {0, "Benkei / Ushiwaka", 0},
    {"fl_met_benkei", "Meet Benkei", 0},
    {"fl_ushi_ben", "Ushiwaka: Can Talk Benkei", 0},
    {"fl_ushi_id", "Asked Ushiwaka Who Benkei Is ", 0},
    {"fl_ushi_gt", "Ushiwaka Went to Golden Temple", 0},

    /* ── Kihachi Food Quest ───────────────────────────────────────────── */
    {0, "Kihachi Food Quest", 0},
    {"fl_kihachi_b", "Heard about Kihachi from Benkei", 0},
    {"fl_kihachi_q", "Looking for Kihachi's Favorite Food", 0},
    {"fl_kihachi_f", "Learned Kihachi's Food", 0},
    {"fl_kihachi_h1", "Kihachi Will Give Favorite Food Hint", 0},
    {"fl_kihachi_h2", "Kihachi Gave Favorite Food Hint", 0},

    /* ── Folkypoke / Priest Quest Chain ──────────────────────────────── */
    {0, "Folkypoke Chain", 0},
    {"fl_priest_son", "Spoke to Priest's Son", 0},
    {"fl_folkypoke", "Arrived at Folkypoke Tourist Center", 0},
    {"fl_inaba_bat", "Heard about Inaba Desert Battery", 0},
    {"fl_dragon_p", "Heard about Dragon Problem", 0},
    {"fl_tourist_g", "Received Gratitude from Tourist Center", 0},
    {"fl_dancin", "Spoke to Dancin Alnite", 0},

    /* ── Tenements Cousins Quest ─────────────────────────────────────── */
    {0, "Tenements Cousins", 0},
    {"fl_cous1", "Goemon Cousins #1", 0},
    {"fl_cous2", "Goemon Cousins #2", 0},
    {"fl_cous3", "Goemon Cousins #3", 0},
    {"fl_cous4", "Goemon Cousins #4", 0},

    /* ── Zazen Old Woman Quest ───────────────────────────────────────── */
    {0, "Zazen Old Woman", 0},
    {"fl_zazen_bef", "Met Old Woman in Zazen Before Dharmanyo", 0},
    {"fl_zazen_aft", "Met Old Woman in Zazen After Dharmanyo", 0},
    {"fl_zazen_ba", "Met Old Woman in Zazen Before And Afte", 0},
    {"fl_zazen_rwd", "Zazen Woman Reward", 0},

    /* ── NPC Meeting Cutscenes ───────────────────────────────────────── */
    {0, "NPC Meetings", 0},
    {"cs_oldman_oe", "Old Man in South Oedo", 0},
    {"cs_omitsu", "Spoke to Omitsu about UFO", 0},
    {"cs_omitsu_f", "Omitsu Fan", 0},
    {"cs_zazen_dw", "Heard about Zazen Dwarf", 0},
    {"cs_kompira", "Spoke to Kompira Priest", 0},
    {"cs_tourist_c", "Tourist Center Cutscene", 0},
    {"cs_baron_wm", "Baron / Wise Man Meeting", 0},

    /* ── Post-Boss Cutscenes ─────────────────────────────────────────── */
    {0, "Post-Boss Cutscenes", 0},
    {"cs_dhrm_1", "After Dharmanyo #1", 0},
    {"cs_dhrm_2", "After Dharmanyo #2", 0},
    {"cs_dhrm_3", "After Dharmanyo #3", 0},
    {"cs_dhrm_4", "After Dharmanyo #4", 0},
    {"cs_tsurami", "After Tsurami", 0},

    /* ── Gorgeous Stage / Sogen ──────────────────────────────────────── */
    {0, "Gorgeous Stage", 0},
    {"cs_gorge_1", "Gorgeous Stage Cutscene #1", 0},
    {"cs_gorge_2", "Gorgeous Stage Cutscene #2", 0},
    {"cs_sogen_l", "Sogen Girl Imitated Lily", 0},
    {"cs_sogen_d", "Sogen Girl Imitated Dancin", 0},

    /* ── Witch Cutscene Chain ────────────────────────────────────────── */
    {0, "Witch Cutscenes", 0},
    {"cs_witch_1", "Witch Cutscene #1", 0},
    {"cs_witch_2", "Witch Cutscene #2", 0},
    {"cs_witch_3", "Witch Cutscene #3", 0},
    {"cs_witch_4", "Witch Cutscene #4", 0},
    {"cs_witch_5", "Witch Cutscene #5", 0},
    {"cs_witch_6", "Witch Cutscene #6", 0},

    /* ── Weapon Upgrades ─────────────────────────────────────────────── */ {0, "Weapon Upgrades", 0},
    {"wpn_goemon", "Goemon Weapons (Silver)", 1},
    {"wpn_goemon", "Goemon Weapons (Gold)", 2},
    {"wpn_ebisu", "Ebisumaru Weapons (Silver)", 1},
    {"wpn_ebisu", "Ebisumaru Weapons (Gold)", 2},
    {"wpn_sasuke", "Sasuke Weapons (Silver)", 1},
    {"wpn_sasuke", "Sasuke Weapons (Gold)", 2},
    {"wpn_yae", "Yae Weapons (Silver)", 1},
    {"wpn_yae", "Yae Weapons (Gold)", 2},
    {"fl_gold_wpn", "Gold Weapon Flag", 0},
};

#define NUM_ENTRIES ((int)(sizeof(s_entries) / sizeof(s_entries[0])))

int anchor_flag_catalog_count(void)
{
    return NUM_ENTRIES;
}

const AnchorFlagEntry *anchor_flag_catalog_get(int index)
{
    if (index < 0 || index >= NUM_ENTRIES)
        return 0;
    return &s_entries[index];
}

const char *anchor_flag_catalog_find_display(const char *key)
{
    int i;

    if (!key)
        return 0;

    for (i = 0; i < NUM_ENTRIES; i++)
    {
        if (s_entries[i].key && debug_text_equals(s_entries[i].key, key))
            return s_entries[i].display;
    }

    return 0;
}

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

/* Handle to the NET button so we can show/hide it based on config. */
static RecompuiResource s_net_btn = RECOMPUI_NULL_RESOURCE;
/* Handle to the DBG button so we can apply DEBUG_BUTTON_ENABLED at init. */
static RecompuiResource s_dbg_btn = RECOMPUI_NULL_RESOURCE;

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

    /* Apply locally + broadcast to all players including this one.
     * Use the entry's force_val when non-zero (e.g. tier-2 vs tier-3 weapons). */
    if (s_entries[idx].force_val != 0)
        item_sync_force_flag_val(s_entries[idx].key, s_entries[idx].force_val);
    else
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

        /* Button placed directly as an absolute child of root.
         * A full-screen wrapper would intercept clicks for other contexts. */
        s_dbg_btn = recompui_create_button(
            s_toggle_ctx, root, "DBG", BUTTONSTYLE_SECONDARY);
        RecompuiResource btn = s_dbg_btn;
        recompui_set_position(btn, POSITION_ABSOLUTE);
        recompui_set_bottom(btn, 12.0f, UNIT_DP); /* bottom-right */
        recompui_set_right(btn, 12.0f, UNIT_DP);
        recompui_set_font_size(btn, 13.0f, UNIT_DP);
        recompui_set_background_color(btn, &C_TOGGLE_BG);
        recompui_set_border_color(btn, &C_BORDER);
        recompui_set_cursor(btn, CURSOR_POINTER);
        recompui_set_tab_index(btn, TAB_INDEX_NONE);
        recompui_register_callback(btn, on_toggle_clicked, 0);
        recompui_set_display(btn, DEBUG_BUTTON_ENABLED ? DISPLAY_BLOCK : DISPLAY_NONE);

        /* NET button shares this context so both buttons are at the same Z-level. */
        s_net_btn = recompui_create_button(
            s_toggle_ctx, root, "NET", BUTTONSTYLE_SECONDARY);
        RecompuiResource net_btn = s_net_btn;
        recompui_set_position(net_btn, POSITION_ABSOLUTE);
        recompui_set_bottom(net_btn, 12.0f, UNIT_DP); /* bottom-left */
        recompui_set_left(net_btn, 12.0f, UNIT_DP);
        recompui_set_font_size(net_btn, 13.0f, UNIT_DP);
        recompui_set_cursor(net_btn, CURSOR_POINTER);
        recompui_set_tab_index(net_btn, TAB_INDEX_NONE);
        recompui_register_callback(net_btn, anchor_connect_ui_net_btn_callback, 0);
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
        recompui_set_font_size(close_btn, 14.0f, UNIT_DP);
        recompui_set_padding_top(close_btn, 5.0f, UNIT_DP);
        recompui_set_padding_bottom(close_btn, 5.0f, UNIT_DP);
        recompui_set_padding_left(close_btn, 12.0f, UNIT_DP);
        recompui_set_padding_right(close_btn, 12.0f, UNIT_DP);
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
                recompui_set_display(sec, DISPLAY_FLEX);
                recompui_set_flex_direction(sec, FLEX_DIRECTION_ROW);
                recompui_set_align_items(sec, ALIGN_ITEMS_CENTER);
                recompui_set_background_color(sec, &C_SEC_BG);
                recompui_set_padding_left(sec, 16.0f, UNIT_DP);
                recompui_set_padding_right(sec, 16.0f, UNIT_DP);
                recompui_set_padding_top(sec, 8.0f, UNIT_DP);
                recompui_set_padding_bottom(sec, 8.0f, UNIT_DP);
                recompui_set_min_height(sec, 40.0f, UNIT_DP);
                recompui_set_border_radius(sec, 4.0f, UNIT_DP);
                recompui_set_margin_top(sec, 14.0f, UNIT_DP);

                RecompuiResource sec_lbl = recompui_create_label(
                    s_modal_ctx, sec, s_entries[i].display, LABELSTYLE_SMALL);
                recompui_set_color(sec_lbl, &C_GREEN);
                recompui_set_font_weight(sec_lbl, 700);
                recompui_set_font_size(sec_lbl, 20.0f, UNIT_DP);

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
                recompui_set_font_size(force_btn, 14.0f, UNIT_DP);
                recompui_set_padding_top(force_btn, 5.0f, UNIT_DP);
                recompui_set_padding_bottom(force_btn, 5.0f, UNIT_DP);
                recompui_set_padding_left(force_btn, 14.0f, UNIT_DP);
                recompui_set_padding_right(force_btn, 14.0f, UNIT_DP);
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
   Public utility
   ========================================================================= */

/**
 * Re-raise the toggle-button context to the top of the recompui Z-order.
 *
 * recompui_show_context() bumps a context to the top of the Z-stack every
 * time it is called.  Whenever any other context is shown after s_toggle_ctx
 * (e.g. the player-list panel in anchor_ui.c), the toggle buttons end up
 * below it and stop receiving mouse events.  Call this function after
 * showing any persistent HUD context to restore correct ordering.
 */
void debug_ui_bump_toggle_ctx(void)
{
    /* recompui_show_context errors if the context is already visible, so we
     * must hide it first before re-showing to move it to the top of the
     * Z-stack.  Only do this once the toggle is actually visible.          */
    if (s_initialized && s_toggle_visible && s_toggle_ctx != RECOMPUI_NULL_CONTEXT)
    {
        recompui_hide_context(s_toggle_ctx);
        recompui_show_context(s_toggle_ctx);
    }
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

    if (!anchor_startup_menu_is_complete())
    {
        if (s_toggle_visible)
        {
            recompui_hide_context(s_toggle_ctx);
            s_toggle_visible = 0;
        }
        return;
    }

    /* Show the toggle button once after the startup menu is finished. */
    if (!s_toggle_visible)
    {
        recompui_show_context(s_toggle_ctx);
        s_toggle_visible = 1;
    }

    /* Show or hide the NET button based on config. */
    if (s_net_btn != RECOMPUI_NULL_RESOURCE)
    {
        int show_net = anchor_startup_menu_is_complete() &&
                       (recomp_get_config_u32("anchor_show_net_button") == 0);
        recompui_open_context(s_toggle_ctx);
        recompui_set_display(s_net_btn, show_net ? DISPLAY_BLOCK : DISPLAY_NONE);
        recompui_close_context(s_toggle_ctx);
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
