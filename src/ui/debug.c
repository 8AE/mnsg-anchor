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
 *   - The Transport form accepts a hex room id plus signed x/y/z coordinates.
 *     Transport does not call the file-select callback setter; it writes the
 *     destination fields the game already consumes and then enters the engine's
 *     normal warp/load step.
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
static int DEBUG_BUTTON_ENABLED = 1;

/* Provided by item_sync.c */
void item_sync_force_flag(const char *name);
void item_sync_force_flag_val(const char *name, int val);

/* NET toggle button callback provided by anchor_connect_ui.c */
extern void anchor_connect_ui_net_btn_callback(RecompuiResource res,
                                               const RecompuiEventData *ev, void *ud);

/* Race menu start-location catalog, shared so debug teleports use the same
 * curated room/coordinate list as race setup. */
extern int anchor_race_start_location_count(void);
extern int anchor_race_get_start_location(int idx, unsigned short *room, int *x, int *y, int *z, const char **name);
extern int anchor_race_is_active(void);

/* Race-start/debug transport fields.
 *
 * Ghidra evidence:
 * - func_8000B2A0_BEA0 copies the save-block spawn fields into the active
 *   destination fields used by the next load.
 * - func_80003728_4328 writes g_system->stepw, clears stepw_end, marks the
 *   system as transitioning, and resets the step substate.
 * - stepw 12 dispatches the normal warp/load state. Once the load gate is
 *   ready it calls the same destination-field consumer used by real exits and
 *   advances the engine into the following step.
 *
 * The first transport attempt called func_8003521C_35E1C, the scheduler
 * callback setter used by file-select/race autoload. That function expects the
 * active task pointer to be valid for that specific context and crashed when
 * called from the live debug menu. This menu therefore records a destination
 * and asks the engine state machine to perform the transition instead.
 */
extern unsigned char D_8015C608_15D208[];
extern signed short D_8006B780_6C380[];
extern unsigned char D_800BCCC0_BD8C0[];
extern unsigned char *D_8015C5C8_15D1C8;
extern void func_80003728_4328(unsigned char step);

/* Save block offsets consumed by func_8000B2A0_BEA0. These are kept in the
 * same x/y/z order exposed by the debug UI and by race start locations. */
#define DEBUG_SAVE_SPAWN_ROOM 0x204
#define DEBUG_SAVE_PLAYER_ROT 0x208
#define DEBUG_SAVE_SPAWN_X 0x20A
#define DEBUG_SAVE_SPAWN_Y 0x20C
#define DEBUG_SAVE_SPAWN_Z 0x20E
#define DEBUG_SAVE_CAM_ROT 0x210

/* Active destination fields in the global game-system block. Writing these
 * lets a live in-game warp use the requested coordinates immediately, without
 * waiting for the save-spawn copy routine to run in file-select context. */
#define DEBUG_SYS_DEST_STAGE 0x3AFE0
#define DEBUG_SYS_ACTIVE_DEST_STAGE 0xADD2
#define DEBUG_SYS_DEST_PLAYER_ROT 0xAFE2
#define DEBUG_SYS_DEST_X 0xAFE4
#define DEBUG_SYS_DEST_Y 0xAFE6
#define DEBUG_SYS_DEST_Z 0xAFE8
#define DEBUG_SYS_DEST_CAM_ROT 0xAFEA

/* Engine step used by the game's own warp/load path. */
#define DEBUG_STEP_WARP 12

/* D_8006B780_6C380 is indexed as room * 5 and provides the default room
 * position and rotations. The user-entered coordinates override the default
 * position, while camera/player rotations still come from this table. */
typedef struct
{
    short posX;
    short posY;
    short posZ;
    short camRot;
    short playerRot;
} DebugStartingData;

/* Pending status-label text update (also deferred to frame hook). */
static const char *s_pending_status = 0;
static char s_transport_status[96];

/* Transport form fields and selected shared race-location entry. These are
 * declared before the transport helpers because helper code updates the UI
 * label and input text directly while the modal context is open. */
static RecompuiResource s_transport_room_input = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_transport_x_input = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_transport_y_input = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_transport_z_input = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_transport_location_label = RECOMPUI_NULL_RESOURCE;
static int s_transport_location_idx = 0;

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

static void debug_write_save_s16(int offset, short value)
{
    *(short *)&D_8015C608_15D208[offset] = value;
}

static void debug_write_system_s16(int offset, short value)
{
    *(short *)&D_800BCCC0_BD8C0[offset] = value;
}

static int debug_parse_int(const char *s, int *out)
{
    int sign = 1;
    int value = 0;
    int saw_digit = 0;

    if (!s || !out)
        return 0;

    while (*s == ' ' || *s == '\t')
        s++;

    if (*s == '-')
    {
        sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9')
    {
        saw_digit = 1;
        value = value * 10 + (*s++ - '0');
    }

    *out = value * sign;
    return saw_digit;
}

static int debug_parse_hex_room(const char *s, unsigned short *out)
{
    unsigned int value = 0;
    int saw_digit = 0;

    if (!s || !out)
        return 0;

    while (*s == ' ' || *s == '\t')
        s++;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    while (*s)
    {
        unsigned int digit;
        if (*s >= '0' && *s <= '9')
            digit = (unsigned int)(*s - '0');
        else if (*s >= 'a' && *s <= 'f')
            digit = (unsigned int)(*s - 'a' + 10);
        else if (*s >= 'A' && *s <= 'F')
            digit = (unsigned int)(*s - 'A' + 10);
        else if (*s == ' ' || *s == '\t')
            break;
        else
            return 0;

        saw_digit = 1;
        value = (value << 4) | digit;
        if (value > 0xFFFFu)
            return 0;
        s++;
    }

    if (!saw_digit)
        return 0;

    *out = (unsigned short)value;
    return 1;
}

static short debug_clamp_s16(int value)
{
    if (value < -32768)
        return -32768;
    if (value > 32767)
        return 32767;
    return (short)value;
}

static void debug_append_char(char *dst, int *pos, int max_len, char value)
{
    if (*pos < max_len - 1)
        dst[(*pos)++] = value;
}

static void debug_append_text(char *dst, int *pos, int max_len, const char *text)
{
    while (*text && *pos < max_len - 1)
        dst[(*pos)++] = *text++;
}

static void debug_append_int(char *dst, int *pos, int max_len, int value)
{
    char tmp[12];
    int len = 0;
    unsigned int v;

    if (value < 0)
    {
        debug_append_char(dst, pos, max_len, '-');
        v = (unsigned int)(-(value + 1)) + 1u;
    }
    else
    {
        v = (unsigned int)value;
    }

    if (v == 0)
    {
        debug_append_char(dst, pos, max_len, '0');
        return;
    }

    while (v > 0 && len < (int)sizeof(tmp))
    {
        tmp[len++] = (char)('0' + (v % 10u));
        v /= 10u;
    }

    while (len > 0)
        debug_append_char(dst, pos, max_len, tmp[--len]);
}

static void debug_append_room_hex(char *dst, int *pos, int max_len, unsigned short room)
{
    static const char hex[] = "0123456789ABCDEF";
    debug_append_text(dst, pos, max_len, "0x");
    debug_append_char(dst, pos, max_len, hex[(room >> 12) & 0xF]);
    debug_append_char(dst, pos, max_len, hex[(room >> 8) & 0xF]);
    debug_append_char(dst, pos, max_len, hex[(room >> 4) & 0xF]);
    debug_append_char(dst, pos, max_len, hex[room & 0xF]);
}

static void debug_make_int_text(char *dst, int max_len, int value)
{
    int pos = 0;
    debug_append_int(dst, &pos, max_len, value);
    dst[pos] = '\0';
}

static void debug_make_room_text(char *dst, int max_len, unsigned short room)
{
    int pos = 0;
    debug_append_room_hex(dst, &pos, max_len, room);
    dst[pos] = '\0';
}

static void debug_set_transport_status(unsigned short room, int x, int y, int z)
{
    int pos = 0;
    debug_append_text(s_transport_status, &pos, (int)sizeof(s_transport_status), "Transported to ");
    debug_append_room_hex(s_transport_status, &pos, (int)sizeof(s_transport_status), room);
    debug_append_text(s_transport_status, &pos, (int)sizeof(s_transport_status), " [");
    debug_append_int(s_transport_status, &pos, (int)sizeof(s_transport_status), x);
    debug_append_text(s_transport_status, &pos, (int)sizeof(s_transport_status), ", ");
    debug_append_int(s_transport_status, &pos, (int)sizeof(s_transport_status), y);
    debug_append_text(s_transport_status, &pos, (int)sizeof(s_transport_status), ", ");
    debug_append_int(s_transport_status, &pos, (int)sizeof(s_transport_status), z);
    debug_append_char(s_transport_status, &pos, (int)sizeof(s_transport_status), ']');
    s_transport_status[pos] = '\0';
    s_pending_status = s_transport_status;
}

static void debug_apply_transport(unsigned short room, int x, int y, int z)
{
    DebugStartingData *debug_start = (DebugStartingData *)&D_8006B780_6C380[room * 5];

    /* Keep the persistent save-spawn copy in sync with the live destination.
     * This mirrors the race-start path and makes the requested location survive
     * if a later engine step re-reads spawn data during the same transition. */
    debug_write_save_s16(DEBUG_SAVE_SPAWN_ROOM, (short)room);
    debug_write_save_s16(DEBUG_SAVE_SPAWN_X, debug_clamp_s16(x));
    debug_write_save_s16(DEBUG_SAVE_SPAWN_Y, debug_clamp_s16(y));
    debug_write_save_s16(DEBUG_SAVE_SPAWN_Z, debug_clamp_s16(z));
    debug_write_save_s16(DEBUG_SAVE_CAM_ROT, debug_start->camRot);
    debug_write_save_s16(DEBUG_SAVE_PLAYER_ROT, debug_start->playerRot);

    /* The pointer-backed system field and the static system mirror are both
     * updated because different decompiled paths reference different bases.
     * Updating both matches the observed engine layout and avoids landing in
     * the new room with stale coordinates or rotation. */
    if (D_8015C5C8_15D1C8)
        *(short *)(D_8015C5C8_15D1C8 + DEBUG_SYS_DEST_STAGE) = (short)room;

    debug_write_system_s16(DEBUG_SYS_ACTIVE_DEST_STAGE, (short)room);
    debug_write_system_s16(DEBUG_SYS_DEST_PLAYER_ROT, debug_start->playerRot);
    debug_write_system_s16(DEBUG_SYS_DEST_X, debug_clamp_s16(x));
    debug_write_system_s16(DEBUG_SYS_DEST_Y, debug_clamp_s16(y));
    debug_write_system_s16(DEBUG_SYS_DEST_Z, debug_clamp_s16(z));
    debug_write_system_s16(DEBUG_SYS_DEST_CAM_ROT, debug_start->camRot);

    /* Hand off to the game's warp step. This is deliberately the last write:
     * once stepw changes, the engine can consume the destination fields on a
     * later frame. */
    func_80003728_4328(DEBUG_STEP_WARP);
}

static int debug_get_selected_location(unsigned short *room, int *x, int *y, int *z, const char **name)
{
    int count = anchor_race_start_location_count();

    if (count <= 0)
        return 0;

    if (s_transport_location_idx < 0)
        s_transport_location_idx = count - 1;
    if (s_transport_location_idx >= count)
        s_transport_location_idx = 0;

    return anchor_race_get_start_location(s_transport_location_idx, room, x, y, z, name);
}

static void debug_format_selected_location(char *dst, int max_len)
{
    unsigned short room = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    const char *name = 0;
    int pos = 0;

    if (!debug_get_selected_location(&room, &x, &y, &z, &name))
    {
        debug_append_text(dst, &pos, max_len, "No race start locations");
        dst[pos] = '\0';
        return;
    }

    debug_append_text(dst, &pos, max_len, name ? name : "Unknown");
    debug_append_text(dst, &pos, max_len, "  ");
    debug_append_room_hex(dst, &pos, max_len, room);
    debug_append_text(dst, &pos, max_len, " [");
    debug_append_int(dst, &pos, max_len, x);
    debug_append_text(dst, &pos, max_len, ", ");
    debug_append_int(dst, &pos, max_len, y);
    debug_append_text(dst, &pos, max_len, ", ");
    debug_append_int(dst, &pos, max_len, z);
    debug_append_char(dst, &pos, max_len, ']');
    dst[pos] = '\0';
}

static void debug_update_location_label(void)
{
    char text[160];

    if (s_transport_location_label == RECOMPUI_NULL_RESOURCE)
        return;

    debug_format_selected_location(text, (int)sizeof(text));
    recompui_set_text(s_transport_location_label, text);
}

static int debug_fill_transport_fields_from_location(void)
{
    unsigned short room = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    const char *name = 0;
    char room_text[8];
    char x_text[12];
    char y_text[12];
    char z_text[12];

    if (!debug_get_selected_location(&room, &x, &y, &z, &name))
        return 0;

    (void)name;
    debug_make_room_text(room_text, (int)sizeof(room_text), room);
    debug_make_int_text(x_text, (int)sizeof(x_text), x);
    debug_make_int_text(y_text, (int)sizeof(y_text), y);
    debug_make_int_text(z_text, (int)sizeof(z_text), z);

    recompui_set_input_text(s_transport_room_input, room_text);
    recompui_set_input_text(s_transport_x_input, x_text);
    recompui_set_input_text(s_transport_y_input, y_text);
    recompui_set_input_text(s_transport_z_input, z_text);

    return 1;
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
    {"eq_kunai", "Sasuke Ice Kunai", 0},
    {"eq_bazooka", "Yae Bazooka", 0},
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

    /* ── Silver Fortune Doll Pickups ────────────────────────────────── */
    {0, "Silver Fortune Dolls", 0},
    {"sd_oe_trt", "Silver Doll: Oedo Turtle", 0},
    {"sd_oe_sc", "Silver Doll: Oedo Silver Cat", 0},
    {"sd_oe_blk", "Silver Doll: Oedo Blocked", 0},
    {"sd_oe_elv", "Silver Doll: Oedo Elevator", 0},
    {"sd_gh_elv", "Silver Doll: Ghost Elevator", 0},
    {"sd_gh_crn", "Silver Doll: Ghost Crane", 0},
    {"sd_gh_can", "Silver Doll: Ghost Cannon", 0},
    {"sd_gh_spk", "Silver Doll: Ghost Big Spike", 0},
    {"sd_ft_sc", "Silver Doll: Festival Silver Cat", 0},
    {"sd_ft_rng", "Silver Doll: Festival Ring", 0},
    {"sd_ft_ik", "Silver Doll: Festival Ice Kunai", 0},
    {"sd_ft_fk", "Silver Doll: Festival Fish Kit", 0},
    {"sd_mc_gf", "Silver Doll: Musical Gold Fan", 0},
    {"sd_mc_ml", "Silver Doll: Musical Multi Lock", 0},
    {"sd_mc_ba", "Silver Doll: Musical Big Aqua", 0},
    {"sd_mc_ds", "Silver Doll: Musical Descent", 0},
    {"sd_kai_hw", "Silver Doll: Kai Highway", 0},
    {"sd_mtfuji", "Silver Doll: Mt. Fuji", 0},
    {"sd_musashi", "Silver Doll: Musashi", 0},
    {"sd_ym_up", "Silver Doll: Yamato Upper", 0},
    {"sd_ym_lo", "Silver Doll: Yamato Lower", 0},
    {"sd_ym_uw", "Silver Doll: Yamato Underwater", 0},
    {"sd_trt_rk", "Silver Doll: Turtle Rock", 0},
    {"sd_kii_aw", "Silver Doll: Kii Awaji", 0},
    {"sd_hwr", "Silver Doll: Husband Wife Rocks", 0},
    {"sd_iga_vn", "Silver Doll: Iga Vine", 0},
    {"sd_dogo", "Silver Doll: Dogo Hot Springs", 0},
    {"sd_bizen", "Silver Doll: Bizen", 0},
    {"sd_ngt_me", "Silver Doll: Nagato Mini Ebi", 0},
    {"sd_ngt_gt", "Silver Doll: Nagato Gate", 0},
    {"sd_inaba", "Silver Doll: Inaba", 0},
    {"sd_jp_sea", "Silver Doll: Japan Sea", 0},
    {"sd_ne_tun", "Silver Doll: Northeast Tunnel", 0},
    {"sd_uz_tun", "Silver Doll: Uzen Tunnel", 0},
    {"sd_oe_brm", "Silver Doll: Oedo Bridge Musashi", 0},
    {"sd_oe_pth", "Silver Doll: Oedo Path Castle", 0},
    {"sd_zz_wh", "Silver Doll: Watering Hole", 0},
    {"sd_zz_wy", "Silver Doll: Zazen Waterway", 0},
    {"sd_fp_est", "Silver Doll: Folkypoke East", 0},
    {"sd_ft_hid", "Silver Doll: Festival Hidden", 0},

    /* ── Gold Fortune Doll Pickups ──────────────────────────────────── */
    {0, "Gold Fortune Dolls", 0},
    {"gd_gh_sg", "Gold Doll: Ghost Shogi Cannon", 0},
    {"gd_ft_wg", "Gold Doll: Festival West Gold", 0},
    {"gd_mc2_gc", "Gold Doll: Musical 2 Gold Cat", 0},
    {"gd_bizen", "Gold Doll: Bizen", 0},
    {"gd_kegon", "Gold Doll: Waterfall Kegon", 0},

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

    /* ── Weapon Upgrades ─────────────────────────────────────────────── */
    {0, "Weapon Upgrades", 0},
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
static int s_pending_transport = 0;
static int s_pending_location_delta = 0;
static int s_pending_location_fill = 0;
static int s_pending_location_transport = 0;

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

/** Transport button: defer input reads and transition work to the frame hook. */
static void on_transport_clicked(RecompuiResource res,
                                 const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_transport = 1;
}

static void on_transport_location_prev_clicked(RecompuiResource res,
                                               const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_location_delta -= 1;
}

static void on_transport_location_next_clicked(RecompuiResource res,
                                               const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_location_delta += 1;
}

static void on_transport_location_fill_clicked(RecompuiResource res,
                                               const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_location_fill = 1;
}

static void on_transport_location_go_clicked(RecompuiResource res,
                                             const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type != UI_EVENT_CLICK)
        return;
    s_pending_location_transport = 1;
}

/* =========================================================================
   UI construction (called once from the frame hook)
   ========================================================================= */

#define ROW_H 68.0f     /* height of each flag row                      */
#define HDR_BAR_H 64.0f /* header bar height                            */
#define STATUS_H 48.0f  /* status bar height                            */

static void make_transport_field(RecompuiContext ctx, RecompuiResource parent,
                                 const char *label_text, const char *default_text,
                                 RecompuiResource *out)
{
    RecompuiResource field = recompui_create_element(ctx, parent);
    recompui_set_display(field, DISPLAY_FLEX);
    recompui_set_flex_direction(field, FLEX_DIRECTION_COLUMN);
    recompui_set_flex_grow(field, 1.0f);
    recompui_set_min_width(field, 120.0f, UNIT_DP);
    recompui_set_min_height(field, 64.0f, UNIT_DP);

    RecompuiResource lbl = recompui_create_label(ctx, field, label_text, LABELSTYLE_ANNOTATION);
    recompui_set_color(lbl, &C_DIM);
    recompui_set_font_size(lbl, 15.0f, UNIT_DP);
    recompui_set_margin_bottom(lbl, 8.0f, UNIT_DP);

    *out = recompui_create_textinput(ctx, field);
    recompui_set_font_size(*out, 16.0f, UNIT_DP);
    recompui_set_height(*out, 42.0f, UNIT_DP);
    recompui_set_min_height(*out, 42.0f, UNIT_DP);
    recompui_set_tab_index(*out, TAB_INDEX_AUTO);
    recompui_set_input_text(*out, default_text);
}

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
        recompui_set_min_height(hdr, 64.0f, UNIT_DP);
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

        /* ── Status bar: shows the last debug action ─────────────── */
        RecompuiResource status = recompui_create_element(s_modal_ctx, panel);
        recompui_set_display(status, DISPLAY_FLEX);
        recompui_set_flex_direction(status, FLEX_DIRECTION_ROW);
        recompui_set_align_items(status, ALIGN_ITEMS_CENTER);
        recompui_set_min_height(status, 52.0f, UNIT_DP);
        recompui_set_padding_top(status, 12.0f, UNIT_DP);
        recompui_set_padding_bottom(status, 12.0f, UNIT_DP);
        recompui_set_padding_left(status, 20.0f, UNIT_DP);
        recompui_set_padding_right(status, 20.0f, UNIT_DP);
        recompui_set_background_color(status, &C_STATUS_BG);
        recompui_set_border_bottom_width(status, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(status, &C_BORDER);

        RecompuiResource pfx = recompui_create_label(
            s_modal_ctx, status, "Last action: ", LABELSTYLE_SMALL);
        recompui_set_color(pfx, &C_DIM);
        recompui_set_font_size(pfx, 16.0f, UNIT_DP);

        s_status_label = recompui_create_label(
            s_modal_ctx, status, "\xe2\x80\x94", LABELSTYLE_SMALL); /* — */
        recompui_set_color(s_status_label, &C_TEAL);
        recompui_set_font_weight(s_status_label, 600);
        recompui_set_font_size(s_status_label, 16.0f, UNIT_DP);

        /* ── Transport form ───────────────────────────────────────── */
        RecompuiResource transport = recompui_create_element(s_modal_ctx, panel);
        recompui_set_display(transport, DISPLAY_FLEX);
        recompui_set_flex_direction(transport, FLEX_DIRECTION_COLUMN);
        recompui_set_min_height(transport, 228.0f, UNIT_DP);
        recompui_set_padding_top(transport, 18.0f, UNIT_DP);
        recompui_set_padding_bottom(transport, 18.0f, UNIT_DP);
        recompui_set_padding_left(transport, 20.0f, UNIT_DP);
        recompui_set_padding_right(transport, 20.0f, UNIT_DP);
        recompui_set_background_color(transport, &C_ROW_EVEN);
        recompui_set_border_bottom_width(transport, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(transport, &C_BORDER);
        recompui_set_gap(transport, 12.0f, UNIT_DP);

        RecompuiResource transport_title = recompui_create_label(
            s_modal_ctx, transport, "Transport", LABELSTYLE_SMALL);
        recompui_set_color(transport_title, &C_GREEN);
        recompui_set_font_weight(transport_title, 700);
        recompui_set_font_size(transport_title, 18.0f, UNIT_DP);

        RecompuiResource transport_row = recompui_create_element(s_modal_ctx, transport);
        recompui_set_display(transport_row, DISPLAY_FLEX);
        recompui_set_flex_direction(transport_row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(transport_row, ALIGN_ITEMS_FLEX_END);
        recompui_set_min_height(transport_row, 68.0f, UNIT_DP);
        recompui_set_gap(transport_row, 14.0f, UNIT_DP);

        make_transport_field(s_modal_ctx, transport_row, "Room Hex", "0x01D1", &s_transport_room_input);
        make_transport_field(s_modal_ctx, transport_row, "X", "56", &s_transport_x_input);
        make_transport_field(s_modal_ctx, transport_row, "Y", "-40", &s_transport_y_input);
        make_transport_field(s_modal_ctx, transport_row, "Z", "51", &s_transport_z_input);

        RecompuiResource transport_btn = recompui_create_button(
            s_modal_ctx, transport_row, "Transport", BUTTONSTYLE_PRIMARY);
        recompui_set_cursor(transport_btn, CURSOR_POINTER);
        recompui_set_font_size(transport_btn, 14.0f, UNIT_DP);
        recompui_set_width(transport_btn, 150.0f, UNIT_DP);
        recompui_set_height(transport_btn, 42.0f, UNIT_DP);
        recompui_set_min_height(transport_btn, 42.0f, UNIT_DP);
        recompui_set_margin_left(transport_btn, 6.0f, UNIT_DP);
        recompui_set_padding_left(transport_btn, 18.0f, UNIT_DP);
        recompui_set_padding_right(transport_btn, 18.0f, UNIT_DP);
        recompui_set_tab_index(transport_btn, TAB_INDEX_NONE);
        recompui_register_callback(transport_btn, on_transport_clicked, 0);

        RecompuiResource location_row = recompui_create_element(s_modal_ctx, transport);
        recompui_set_display(location_row, DISPLAY_FLEX);
        recompui_set_flex_direction(location_row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(location_row, ALIGN_ITEMS_CENTER);
        recompui_set_min_height(location_row, 44.0f, UNIT_DP);
        recompui_set_margin_top(location_row, 4.0f, UNIT_DP);
        recompui_set_gap(location_row, 8.0f, UNIT_DP);

        RecompuiResource prev_btn = recompui_create_button(
            s_modal_ctx, location_row, "Prev", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(prev_btn, CURSOR_POINTER);
        recompui_set_font_size(prev_btn, 13.0f, UNIT_DP);
        recompui_set_width(prev_btn, 72.0f, UNIT_DP);
        recompui_set_height(prev_btn, 36.0f, UNIT_DP);
        recompui_set_min_height(prev_btn, 36.0f, UNIT_DP);
        recompui_set_tab_index(prev_btn, TAB_INDEX_NONE);
        recompui_register_callback(prev_btn, on_transport_location_prev_clicked, 0);

        s_transport_location_label = recompui_create_label(
            s_modal_ctx, location_row, "", LABELSTYLE_SMALL);
        recompui_set_color(s_transport_location_label, &C_WHITE);
        recompui_set_flex_grow(s_transport_location_label, 1.0f);
        recompui_set_font_size(s_transport_location_label, 15.0f, UNIT_DP);
        debug_update_location_label();

        RecompuiResource next_btn = recompui_create_button(
            s_modal_ctx, location_row, "Next", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(next_btn, CURSOR_POINTER);
        recompui_set_font_size(next_btn, 13.0f, UNIT_DP);
        recompui_set_width(next_btn, 72.0f, UNIT_DP);
        recompui_set_height(next_btn, 36.0f, UNIT_DP);
        recompui_set_min_height(next_btn, 36.0f, UNIT_DP);
        recompui_set_tab_index(next_btn, TAB_INDEX_NONE);
        recompui_register_callback(next_btn, on_transport_location_next_clicked, 0);

        RecompuiResource fill_btn = recompui_create_button(
            s_modal_ctx, location_row, "Use", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(fill_btn, CURSOR_POINTER);
        recompui_set_font_size(fill_btn, 13.0f, UNIT_DP);
        recompui_set_width(fill_btn, 66.0f, UNIT_DP);
        recompui_set_height(fill_btn, 36.0f, UNIT_DP);
        recompui_set_min_height(fill_btn, 36.0f, UNIT_DP);
        recompui_set_tab_index(fill_btn, TAB_INDEX_NONE);
        recompui_register_callback(fill_btn, on_transport_location_fill_clicked, 0);

        RecompuiResource go_btn = recompui_create_button(
            s_modal_ctx, location_row, "Go", BUTTONSTYLE_PRIMARY);
        recompui_set_cursor(go_btn, CURSOR_POINTER);
        recompui_set_font_size(go_btn, 13.0f, UNIT_DP);
        recompui_set_width(go_btn, 66.0f, UNIT_DP);
        recompui_set_height(go_btn, 36.0f, UNIT_DP);
        recompui_set_min_height(go_btn, 36.0f, UNIT_DP);
        recompui_set_tab_index(go_btn, TAB_INDEX_NONE);
        recompui_register_callback(go_btn, on_transport_location_go_clicked, 0);

        /* ── Scrollable flag list ──────────────────────────────────── */
        RecompuiResource scroll = recompui_create_element(s_modal_ctx, panel);
        recompui_set_flex_grow(scroll, 1.0f);
        recompui_set_overflow_y(scroll, OVERFLOW_SCROLL);
        recompui_set_display(scroll, DISPLAY_FLEX);
        recompui_set_flex_direction(scroll, FLEX_DIRECTION_COLUMN);
        recompui_set_padding_top(scroll, 14.0f, UNIT_DP);
        recompui_set_padding_bottom(scroll, 10.0f, UNIT_DP);
        recompui_set_padding_left(scroll, 10.0f, UNIT_DP);
        recompui_set_padding_right(scroll, 10.0f, UNIT_DP);
        recompui_set_gap(scroll, 4.0f, UNIT_DP);

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
                       !anchor_race_is_active() &&
                       (recomp_get_config_u32("anchor_show_net_button") == 0);
        recompui_open_context(s_toggle_ctx);
        recompui_set_display(s_net_btn, show_net ? DISPLAY_BLOCK : DISPLAY_NONE);
        recompui_close_context(s_toggle_ctx);
    }

    /* Process pending location-selector actions from the transport section. */
    if (s_pending_location_delta || s_pending_location_fill || s_pending_location_transport)
    {
        int count = anchor_race_start_location_count();

        if (count > 0)
        {
            unsigned short room = 0;
            int x = 0;
            int y = 0;
            int z = 0;
            const char *name = 0;

            if (s_pending_location_delta)
            {
                s_transport_location_idx += s_pending_location_delta;
                while (s_transport_location_idx < 0)
                    s_transport_location_idx += count;
                while (s_transport_location_idx >= count)
                    s_transport_location_idx -= count;
            }

            recompui_open_context(s_modal_ctx);
            debug_update_location_label();
            if (s_pending_location_fill)
                debug_fill_transport_fields_from_location();
            recompui_close_context(s_modal_ctx);

            if (s_pending_location_transport &&
                debug_get_selected_location(&room, &x, &y, &z, &name))
            {
                debug_set_transport_status(room, x, y, z);
                recomp_printf("[Debug] Transport location: %s room=0x%04X xyz=(%d,%d,%d)\n",
                              name ? name : "Unknown", (unsigned int)room, x, y, z);
                if (s_modal_visible)
                {
                    recompui_hide_context(s_modal_ctx);
                    s_modal_visible = 0;
                }
                debug_apply_transport(room, x, y, z);
            }
        }
        else
        {
            s_pending_status = "No transport locations";
        }

        s_pending_location_delta = 0;
        s_pending_location_fill = 0;
        s_pending_location_transport = 0;
    }

    /* Process pending transport action from the modal button. */
    if (s_pending_transport)
    {
        char *room_str;
        char *x_str;
        char *y_str;
        char *z_str;
        unsigned short room = 0;
        int x = 0;
        int y = 0;
        int z = 0;
        int ok;

        s_pending_transport = 0;

        recompui_open_context(s_modal_ctx);
        room_str = s_transport_room_input != RECOMPUI_NULL_RESOURCE ? recompui_get_input_text(s_transport_room_input) : 0;
        x_str = s_transport_x_input != RECOMPUI_NULL_RESOURCE ? recompui_get_input_text(s_transport_x_input) : 0;
        y_str = s_transport_y_input != RECOMPUI_NULL_RESOURCE ? recompui_get_input_text(s_transport_y_input) : 0;
        z_str = s_transport_z_input != RECOMPUI_NULL_RESOURCE ? recompui_get_input_text(s_transport_z_input) : 0;
        recompui_close_context(s_modal_ctx);

        ok = debug_parse_hex_room(room_str, &room) &&
             debug_parse_int(x_str, &x) &&
             debug_parse_int(y_str, &y) &&
             debug_parse_int(z_str, &z);

        if (room_str)
            recomp_free(room_str);
        if (x_str)
            recomp_free(x_str);
        if (y_str)
            recomp_free(y_str);
        if (z_str)
            recomp_free(z_str);

        if (ok)
        {
            debug_set_transport_status(room, x, y, z);
            recomp_printf("[Debug] Transport: room=0x%04X xyz=(%d,%d,%d)\n",
                          (unsigned int)room, x, y, z);
            if (s_modal_visible)
            {
                recompui_hide_context(s_modal_ctx);
                s_modal_visible = 0;
            }
            debug_apply_transport(room, x, y, z);
        }
        else
        {
            s_pending_status = "Invalid transport input";
        }
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
