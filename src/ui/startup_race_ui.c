#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"
#include "anchor_runtime.h"
#include "anchor_flag_catalog.h"

void item_sync_force_flag(const char *name);
void item_sync_force_flag_val(const char *name, int val);
int item_sync_save_is_loaded(void);
int item_sync_write_local_flag_val(const char *name, int val);
void anchor_set_current_character_if_needed(void);

/* D_8015C608_15D208:
 * Primary save-data block. func_8000B640_C240 clears/seeds this block for a
 * fresh file, func_8000B5D0_C1D0 mirrors it into backup/runtime save memory,
 * and func_8000B2A0_BEA0 later reads spawn fields from it. Race startup writes
 * selected characters, room, rotation, and spawn coordinates here before those
 * original game routines consume the values. */
extern unsigned char D_8015C608_15D208[];

/* D_8006B780_6C380:
 * Room-indexed starting-position table used by the game's debug/start paths.
 * Each room entry is five signed shorts: posX, posY, posZ, camera rotation,
 * and player rotation. The race UI uses this to fill the rotation fields for a
 * selected start location while preserving the user's configured xyz values. */
extern signed short D_8006B780_6C380[];

/* func_8000B640_C240:
 * Fresh-save initializer. Ghidra shows it clearing D_8015C608 for 0x304 bytes,
 * seeding default health/items/character flags, setting Goemon's house as the
 * default room, and mirroring the block to D_8015C910. Direct race launch calls
 * this first so all race writes start from a valid new-save layout. */
extern void func_8000B640_C240(void);

/* func_8000B5D0_C1D0:
 * Marks the initialized save as started/loaded. The game clears a system field,
 * copies D_8015C66C.. into D_8015C5D8, mirrors D_8015C608 into D_8015C910, and
 * sets D_8015C5D8 to 1. Race flags are safe to apply after this point because
 * item_sync_save_is_loaded() can observe live save memory. */
extern void func_8000B5D0_C1D0(void);

/* func_8000607C_6C7C:
 * Engine destination writer. It fills g_system destination_stage, destination
 * player xyz, camera rotation, player rotation, and two related load fields.
 * Direct race launch calls it in addition to writing save spawn fields so the
 * scene loader and save reader agree on the target location. */
extern void func_8000607C_6C7C(unsigned short room, short x, short y, short z,
                               short camera_rot, short player_rot,
                               short field90, int field91);

/* func_80003478_4078:
 * Gameplay-control/UI state initializer. It advances g_system + 0x3ADDE,
 * clears g_system + 0x3ADDF, and resets the substate at 0x3ADE0/0x3ADE1.
 * Direct race launch runs it after the render reset so normal gameplay UI state
 * is re-entered instead of staying in the menu/startup state. */
extern void func_80003478_4078(void);

/* func_80004240_4E40:
 * Clears the 16 small text/UI slots beginning at g_system + 0x3B168. The game
 * calls this during start/transition setup before rebuilding on-screen UI. */
extern void func_80004240_4E40(void);

/* func_8000B364_BF64:
 * Stage/scene-load setup. It copies destination_stage into the active stage
 * fields, computes the region/substage with func_8000B3E4, starts scene-load
 * bookkeeping, and resets some transition counters. */
extern void func_8000B364_BF64(void);

/* func_8000383C_443C:
 * Render/texture buffer reset. It calls the broader render init path and clears
 * the graphics/texture buffer used by the next scene. This fixed the overbright
 * texture/contrast issue introduced by jumping directly into warp step 12. */
extern void func_8000383C_443C(void);

/* func_8003521C_35E1C:
 * Scheduler callback setter. It stores a function pointer in the active task's
 * +0x0c callback slot. The old file-select autoload path used this handoff
 * after creating the no-diary save; direct race launch preserves it so the
 * normal gameplay callback path is installed without showing file select. */
extern void func_8003521C_35E1C(void *func_ptr);

/* func_801CD890_660740:
 * Gameplay/load callback installed by func_8003521C_35E1C. Ghidra's function
 * boundaries around this overlay are imperfect, but this symbol is the callback
 * pointer used by the original file-select flow. */
extern void func_801CD890_660740(void);

/* D_8015C5C8_15D1C8:
 * Runtime pointer to g_system. Most decompiled game code uses this pointer, so
 * direct race launch writes transition fields through it when available. */
extern unsigned char *D_8015C5C8_15D1C8;

/* D_800BCCC0_BD8C0:
 * Static g_system storage. Some paths and symbols refer to the static block
 * directly, so race transport mirrors destination and diary-state writes here
 * as well as through D_8015C5C8_15D1C8. */
extern unsigned char D_800BCCC0_BD8C0[];

/* func_80003728_4328:
 * Engine step switcher. Ghidra shows this writes g_system->stepw, clears
 * stepw_end, marks the system as transitioning, and resets the step substate.
 * Direct race launch uses step 12 after recreating the missing no-diary setup
 * state; using step 7 from the recomp UI path falls into the title/demo flow. */
extern void func_80003728_4328(unsigned char step);

#define RACE_MAX_FLAGS 320
#define RACE_MAX_CATEGORIES 64
#define RACE_MAX_LOCATIONS 96
#define RACE_TEXT_LEN 96
#define RACE_CONFIG_CODE_LEN 96
#define RACE_CONFIG_VERSION 2
#define RACE_CONFIG_MIN_VERSION 1
#define RACE_CONFIG_DATA_MAX (1 + 2 + 1 + 1 + 2 + ((RACE_MAX_FLAGS + 7) / 8) + 4)
#define RACE_CHALLENGE_DAMAGE_SYNC 1
#define RACE_CHALLENGE_NO_HIT 2
#define RACE_CHALLENGE_ONE_LIFE 4
#define RACE_CHALLENGE_ENEMY_MULT_SHIFT 3
#define RACE_CHALLENGE_ENEMY_MULT_MASK 0x18
#define RACE_CHALLENGE_RYO_SYNC 0x20
#define SAVE_SPAWN_ROOM 0x204
#define SAVE_PLAYER_ROT 0x208
#define SAVE_SPAWN_X 0x20A
#define SAVE_SPAWN_Z 0x20C
#define SAVE_SPAWN_Y 0x20E
#define SAVE_CAM_ROT 0x210
#define RACE_SYS_DEST_STAGE 0x3AFE0
#define RACE_SYS_ACTIVE_DEST_STAGE 0xADD2
#define RACE_SYS_DEST_PLAYER_ROT 0xAFE2
#define RACE_SYS_DEST_X 0xAFE4
#define RACE_SYS_DEST_Y 0xAFE6
#define RACE_SYS_DEST_Z 0xAFE8
#define RACE_SYS_DEST_CAM_ROT 0xAFEA
#define RACE_SYS_ADVENTURE_DIARY_SLOT 0x3B040
#define RACE_SYS_LOAD_FROM_START 0x3AE16
#define RACE_STEP_WARP 12
#define SAVE_CHARACTER_BASE 0x94
#define RACE_CHARACTER_ENFORCE_FRAMES 180

typedef struct
{
    unsigned short room;
    short x;
    short y;
    short z;
    const char *name;
} RaceStartLocation;

typedef struct
{
    short posX;
    short posY;
    short posZ;
    short camRot;
    short playerRot;
} RaceDebugStartingData;

static const RaceStartLocation s_start_locations[] = {
    {0x1D1, 56, -40, 51, "Goemon's House"},
    {0x161, -97, 0, -190, "Oedo Town Housing"},
    {0x15F, 86, 0, -349, "Oedo Town Shopping District"},
    {0x15E, -440, 0, -47, "Oedo Town Super Pass Bridge"},
    {0x160, 423, 7, -251, "Oedo Town Shrine Area"},
    {0x162, -238, 8, 228, "Oedo Town Nihon Bashi Bridge"},
    {0x163, 57, -14, 208, "Oedo Town Outside Kai Highway"},
    {0x164, -350, 7, 93, "Oedo Castle Main Gate"},
    {0x165, 101, 7, 109, "Stairs to Oedo Castle"},
    {0x166, -143, 147, 264, "Oedo Castle Entrance"},
    {0x167, 3, 0, 153, "Zazen Town Entrance"},
    {0x168, 0, 27, 90, "Zazen Town Bridge"},
    {0x169, 61, 8, 231, "Zazen Town Main Town"},
    {0x16A, 5, 27, 97, "Zazen Town Watering Hole"},
    {0x16C, 60, 7, 221, "Zazen Town Back"},
    {0x16D, -106, 7, 2, "Zazen Town Golden Temple"},
    {0x16F, -226, 7, 0, "Zazen Town Mt Nyoigatake Sidewalk"},
    {0x170, -224, 140, -1, "Zazen Town Mt Nyoigatake Fire Shrine"},
    {0x172, -62, 6, -91, "Zazen Town Duck Creek Upstream"},
    {0x173, -40, 6, -133, "Zazen Town Duck Creek Ushiwaka"},
    {0x171, 102, 7, 0, "Zazen Town Benkei Bridge"},
    {0x16B, -269, -49, -118, "Zazen Town Duck Creek Jump Area"},
    {0x16E, 282, 7, 57, "Zazen Town Bizen Bridge"},
    {0x130, -10, 56, -976, "Musashi Beach"},
    {0x131, -377, -48, -430, "Musashi Tunnel"},
    {0x14F, -577, -80, 37, "Tunnel to Northeast 1"},
    {0x150, -577, -106, 400, "Tunnel to Northeast 2"},
    {0x132, -2975, -293, -311, "Iga"},
    {0x151, 328, -150, 302, "Mutsu Crossroads"},
    {0x152, -547, -24, 27, "Uzen Tunnel"},
    {0x154, 29, -319, -535, "Waterfall of Kegon"},
    {0x179, -372, 70, -129, "Festival Village Entrance"},
    {0x17A, -3, 0, -385, "Festival Village Shopping District"},
    {0x17B, 224, 0, 0, "Festival Village Stage"},
    {0x17C, -176, 70, 87, "Festival Village Rear"},
    {0x14B, 441, -299, 758, "Mt Fear"},
    {0x14D, -242, 200, -816, "Shoreline"},
    {0x14C, 0, -46, -576, "Ugo Stone Circle"},
    {0x178, 74, 98, 179, "Folkypoke Village Entrance"},
    {0x175, -62, 0, 214, "Folkypoke Village Hay Farm"},
    {0x176, -125, -14, 90, "Folkypoke Village Housing"},
    {0x177, 172, 98, -66, "Folkypoke Village Shopping District"},
    {0x1B8, 142, -70, 77, "Oedo Tourist Center - Awa Branch"},
    {0x13A, 491, -33, -39, "Tosa Fields"},
    {0x13B, -29, 0, 723, "Tosa Bridge"},
    {0x1B5, 48, -50, 35, "Iyo Coffee Shop"},
    {0x141, -451, 16, 185, "Iyo Hills"},
    {0x1B1, 48, -50, 35, "Kai's Coffee Shop"},
    {0x12C, 770, 139, -1355, "Kai Highway"},
    {0x12D, -46, -439, 1299, "Mt Fuji (Bottom)"},
    {0x12E, 173, 90, 144, "Mt Fuji Crater"},
    {0x12F, 180, -131, 69, "Mt Fuji (Peak)"},
    {0x1D2, 56, -40, 51, "Mt Fuji Salesman Room"},
    {0x133, -167, 351, -1226, "Yamato Shrine Exterior"},
    {0x136, 609, -87, -183, "Yamato Bamboo Forest"},
    {0x137, -595, -96, 108, "Turtle Stone"},
    {0x134, 51, -330, 202, "Yamato Shrine Interior"},
    {0x1B3, 48, -50, 35, "Kii's Coffee Shop"},
    {0x138, 1021, 68, 6, "Kii Awaji Island"},
    {0x1B9, 142, -70, 77, "Oedo Tourist Center - Awaji Island Branch"},
    {0x13C, 9, -86, 282, "Kompira Mountain First Block"},
    {0x13D, 13, -155, 400, "Kompira Mountain Second Block"},
    {0x1B4, 48, -50, 35, "Kompiras Coffee Shop"},
    {0x13E, 5, 52, -143, "Kompira Mountain Third Block"},
    {0x13F, 0, -313, 763, "Kompira Mountain Fourth Block"},
    {0x140, -2, -44, 189, "Kompira Mountain Grounds"},
    {0x143, 593, 78, 0, "Kurashiki"},
    {0x144, -880, 52, 901, "Nagato"},
    {0x145, 791, -20, -792, "Hagi"},
    {0x146, -98, 159, -1152, "Akiyoshidai"},
    {0x147, -881, -24, -361, "Shuhodo"},
    {0x153, -443, 33, -236, "Gateway Viewpoint"},
    {0x1B6, 48, -50, 35, "Izumo Coffee Shop"},
    {0x148, -1386, 0, 467, "Izumo"},
    {0x149, -27, -62, -467, "Lake with a Large Tree"},
    {0x14A, 618, -170, -1039, "Inaba Dune"},
};

static const int s_start_location_count =
    (int)(sizeof(s_start_locations) / sizeof(s_start_locations[0]));

int anchor_race_start_location_count(void)
{
    return s_start_location_count;
}

int anchor_race_get_start_location(int idx, unsigned short *room, int *x, int *y, int *z, const char **name)
{
    const RaceStartLocation *loc;

    if (idx < 0 || idx >= s_start_location_count)
        return 0;

    loc = &s_start_locations[idx];
    if (room)
        *room = loc->room;
    if (x)
        *x = loc->x;
    if (y)
        *y = loc->y;
    if (z)
        *z = loc->z;
    if (name)
        *name = loc->name;

    return 1;
}

static const RecompuiColor R_BG = {8, 8, 12, 230};
static const RecompuiColor R_PANEL = {14, 14, 22, 255};
static const RecompuiColor R_HEAD = {20, 17, 27, 255};
static const RecompuiColor R_CARD = {18, 18, 27, 255};
static const RecompuiColor R_CARD_ALT = {23, 23, 34, 255};
static const RecompuiColor R_BORDER = {72, 62, 96, 225};
static const RecompuiColor R_WHITE = {248, 248, 252, 255};
static const RecompuiColor R_DIM = {170, 172, 182, 255};
static const RecompuiColor R_PURPLE = {168, 104, 255, 255};
static const RecompuiColor R_GREEN = {70, 190, 90, 255};
static const RecompuiColor R_RED = {220, 68, 68, 255};

static RecompuiContext s_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiContext s_result_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_setup_view = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_goal_view = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_location_view = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_challenges_view = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_import_input = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_category_views[RACE_MAX_CATEGORIES];
static RecompuiResource s_goal_category_views[RACE_MAX_CATEGORIES];
static RecompuiResource s_goal_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_location_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_damage_sync_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_damage_sync_detail_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_no_hit_detail_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_one_life_detail_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_ryo_sync_detail_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_enemy_multiplier_detail_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_live_config_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_status_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_result_title_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_result_team_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_result_players_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_goal_btns[RACE_MAX_FLAGS];
static RecompuiResource s_start_btns[RACE_MAX_FLAGS];
static RecompuiResource s_location_btns[RACE_MAX_LOCATIONS];
static RecompuiResource s_category_count_lbls[RACE_MAX_CATEGORIES];
static RecompuiResource s_goal_category_count_lbls[RACE_MAX_CATEGORIES];
static char s_goal_btn_text[RACE_MAX_FLAGS][RACE_TEXT_LEN];
static char s_start_btn_text[RACE_MAX_FLAGS][RACE_TEXT_LEN];
static char s_location_btn_text[RACE_MAX_LOCATIONS][RACE_TEXT_LEN];
static char s_config_code[RACE_CONFIG_CODE_LEN];
static char s_category_count_text[RACE_MAX_CATEGORIES][32];
static char s_goal_category_count_text[RACE_MAX_CATEGORIES][32];
static char s_result_team_text[128];
static char s_result_players_text[512];
static char s_config_json[8192];
static char s_challenges_summary[64];
static unsigned char s_start_selected[RACE_MAX_FLAGS];
static const char *s_category_names[RACE_MAX_CATEGORIES];
static int s_category_start[RACE_MAX_CATEGORIES];
static int s_category_end[RACE_MAX_CATEGORIES];

static int s_ui_built = 0;
static int s_initialized = 0;
static int s_visible = 0;
static int s_pending_open = 0;
static int s_pending_back = 0;
static int s_pending_choose_goal = 0;
static int s_pending_choose_location = 0;
static int s_pending_choose_challenges = 0;
static int s_pending_paste_config = 0;
static int s_pending_import_config = 0;
static int s_pending_export_config = 0;
static int s_pending_goal_done = 0;
static int s_pending_location_done = 0;
static int s_pending_challenges_done = 0;
static int s_pending_toggle_damage_sync = 0;
static int s_pending_toggle_no_hit = 0;
static int s_pending_toggle_one_life = 0;
static int s_pending_toggle_ryo_sync = 0;
static int s_pending_cycle_enemy_multiplier = 0;
static int s_pending_category_idx = -1;
static int s_pending_goal_category_idx = -1;
static int s_pending_goal_idx = -1;
static int s_pending_location_idx = -1;
static int s_pending_toggle_idx = -1;
static int s_goal_idx = -1;
static int s_location_idx = 0;
static int s_damage_sync_enabled = 0;
static int s_no_hit_enabled = 0;
static int s_one_life_enabled = 0;
static int s_ryo_sync_enabled = 0;
static int s_enemy_multiplier = 1;
static int s_flag_count = 0;
static int s_category_count = 0;
static int s_active_category = -1;
static int s_active_goal_category = -1;
static int s_screen = 0;
static int s_race_started = 0;
static int s_race_finished = 0;
static int s_result_visible = 0;
static int s_race_apply_pending = 0;
static int s_race_character_enforce_frames = 0;
static int s_race_pending_count = 0;
static const char *s_race_pending_keys[RACE_MAX_FLAGS];
static int s_race_pending_values[RACE_MAX_FLAGS];

static void update_category_count(int cat_idx);
static void update_goal_category_count(int cat_idx);
static void update_start_button(int idx);
static void update_location_buttons(int old_idx, int new_idx);
static void update_goal_buttons(int old_idx, int new_idx);
static void update_location_label(void);
static void update_goal_label(void);
static void update_damage_sync_label(void);
static void refresh_live_config_code(void);
static void apply_pending_race_flags(void);

static void write_save_s16(int offset, short value)
{
    *(short *)&D_8015C608_15D208[offset] = value;
}

static void write_save_s32(int offset, int value)
{
    *(int *)&D_8015C608_15D208[offset] = value;
}

static void write_system_s16(int offset, short value)
{
    *(short *)&D_800BCCC0_BD8C0[offset] = value;
}

static void write_system_s32(int offset, int value)
{
    *(int *)&D_800BCCC0_BD8C0[offset] = value;
}

static int race_key_equals(const char *a, const char *b)
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

static int character_index_for_key(const char *key)
{
    if (race_key_equals(key, "chr_goemon"))
        return 0;
    if (race_key_equals(key, "chr_ebisu"))
        return 1;
    if (race_key_equals(key, "chr_sasuke"))
        return 2;
    if (race_key_equals(key, "chr_yae"))
        return 3;
    return -1;
}

static void apply_race_character_selection(int update_sync_cache)
{
    int i;

    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        int character_index;
        int enabled;

        if (!entry || !entry->key)
            continue;

        character_index = character_index_for_key(entry->key);
        if (character_index < 0)
            continue;

        enabled = s_start_selected[i] ? 1 : 0;
        if (update_sync_cache)
            item_sync_write_local_flag_val(entry->key, enabled);
        else
            write_save_s32(SAVE_CHARACTER_BASE + character_index * 4, enabled);
    }
}

static int race_has_start_character(void)
{
    int i;

    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (entry && entry->key && character_index_for_key(entry->key) >= 0 && s_start_selected[i])
            return 1;
    }

    return 0;
}

static void apply_race_start_location(void)
{
    const RaceStartLocation *loc;
    RaceDebugStartingData *debug_start;

    if (s_location_idx < 0 || s_location_idx >= s_start_location_count)
        s_location_idx = 0;

    loc = &s_start_locations[s_location_idx];
    debug_start = (RaceDebugStartingData *)&D_8006B780_6C380[loc->room * 5];

    write_save_s16(SAVE_SPAWN_ROOM, (short)loc->room);
    write_save_s16(SAVE_SPAWN_X, loc->x);
    write_save_s16(SAVE_SPAWN_Z, loc->y);
    write_save_s16(SAVE_SPAWN_Y, loc->z);
    write_save_s16(SAVE_CAM_ROT, debug_start->camRot);
    write_save_s16(SAVE_PLAYER_ROT, debug_start->playerRot);

    func_8000607C_6C7C(loc->room, loc->x, loc->y, loc->z,
                       debug_start->camRot, debug_start->playerRot, 0, 0);

    if (D_8015C5C8_15D1C8)
        *(short *)(D_8015C5C8_15D1C8 + RACE_SYS_DEST_STAGE) = (short)loc->room;

    write_system_s16(RACE_SYS_ACTIVE_DEST_STAGE, (short)loc->room);
    write_system_s16(RACE_SYS_DEST_PLAYER_ROT, debug_start->playerRot);
    write_system_s16(RACE_SYS_DEST_X, loc->x);
    write_system_s16(RACE_SYS_DEST_Y, loc->y);
    write_system_s16(RACE_SYS_DEST_Z, loc->z);
    write_system_s16(RACE_SYS_DEST_CAM_ROT, debug_start->camRot);
}

static void setup_race_scene_load(void)
{
    /*
     * Mirrors the tail of the game's No Adventure Diary start function after
     * it has initialized save data and destination fields. The previous direct
     * launcher jumped straight to the generic warp step, which skipped render
     * and scene setup normally performed by this path.
     *
     * The original step-7 path sets g_system + 0x3AE16 while it is still in the
     * start sequence and clears it later before normal gameplay. Direct race
     * start uses warp step 12, so leave that field clear; otherwise gameplay UI
     * processing that depends on normal gameplay state stays disabled.
     */
    if (D_8015C5C8_15D1C8)
        *(short *)(D_8015C5C8_15D1C8 + RACE_SYS_LOAD_FROM_START) = 0;

    func_80004240_4E40();
    func_8000B364_BF64();
    func_8000383C_443C();
    func_80003478_4078();

    if (D_8015C5C8_15D1C8)
        *(short *)(D_8015C5C8_15D1C8 + RACE_SYS_LOAD_FROM_START) = 0;
}

static void launch_race_save_directly(void)
{
    /*
     * Start a fresh save without entering the game's file-select screen. This
     * replaces the old func_801CE1F0 file-select hook path:
     *
     * 1. func_8000B640 initializes the save block.
     * 2. Race character/location writes seed that new save before it is marked
     *    loaded.
     * 3. func_8000B5D0 mirrors save/control state into the runtime save state.
     * 4. g_system + 0x3B040 is forced to -1, matching the old file-select
     *    autoload path and keeping the race in No Adventure Diary mode instead
     *    of binding it to save slot one.
     * 5. Race flags are applied through item_sync now that save memory is live.
     * 6. The same scene/render setup used by the game's No Adventure Diary path
     *    is run before entering the normal warp/load step. Step 7 is only safe
     *    from inside the game's own start callback; from the recomp UI path it
     *    falls back into the title/demo sequence.
     * 7. The gameplay callback is installed explicitly, matching the old
     *    file-select autoload path and restoring in-game HUD/pause/minimap
     *    processing after the direct scene handoff.
     */
    func_8000B640_C240();
    apply_race_character_selection(0);
    apply_race_start_location();
    func_8000B5D0_C1D0();
    if (D_8015C5C8_15D1C8)
        *(int *)(D_8015C5C8_15D1C8 + RACE_SYS_ADVENTURE_DIARY_SLOT) = -1;
    write_system_s32(RACE_SYS_ADVENTURE_DIARY_SLOT, -1);
    apply_race_character_selection(0);
    apply_race_start_location();
    apply_pending_race_flags();
    anchor_set_current_character_if_needed();
    setup_race_scene_load();
    func_80003728_4328(RACE_STEP_WARP);
    func_8003521C_35E1C(func_801CD890_660740);
    recomp_printf("[Race] Started race save directly from lobby.\n");
}

static void apply_pending_race_flags(void)
{
    int i;

    if (!s_race_apply_pending)
        return;

    if (!item_sync_save_is_loaded())
        return;

    for (i = 0; i < s_race_pending_count; i++)
    {
        if (s_race_pending_keys[i])
            item_sync_write_local_flag_val(s_race_pending_keys[i], s_race_pending_values[i]);
    }
    apply_race_character_selection(1);
    apply_race_start_location();
    anchor_set_current_character_if_needed();

    s_race_apply_pending = 0;
    s_race_pending_count = 0;
    recomp_printf("[Race] Applied configured starting flags and start location to loaded save.\n");
}

static void copy_text(char *dst, const char *src, int max_len)
{
    int i = 0;
    if (!dst || max_len <= 0)
        return;
    if (!src)
        src = "";
    while (i < max_len - 1 && src[i])
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void make_toggle_text(char *dst, const char *prefix, const char *label)
{
    int i = 0;
    int j = 0;
    while (i < RACE_TEXT_LEN - 1 && prefix && prefix[j])
        dst[i++] = prefix[j++];
    j = 0;
    while (i < RACE_TEXT_LEN - 1 && label && label[j])
        dst[i++] = label[j++];
    dst[i] = '\0';
}

static int text_equals(const char *a, const char *b)
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

static int is_default_start_flag(const AnchorFlagEntry *entry)
{
    if (!entry || !entry->key)
        return 0;

    return text_equals(entry->key, "chr_goemon") ||
           text_equals(entry->key, "chr_ebisu");
}

static int find_flag_index_by_key(const char *key)
{
    int i;

    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (entry && entry->key && race_key_equals(entry->key, key))
            return i;
    }

    return -1;
}

static void set_status(const char *text, int ok)
{
    if (s_status_lbl == RECOMPUI_NULL_RESOURCE)
        return;

    recompui_open_context(s_ctx);
    recompui_set_text(s_status_lbl, text);
    recompui_set_color(s_status_lbl, ok ? &R_GREEN : &R_RED);
    recompui_close_context(s_ctx);
}

static void append_uint(char *dst, int *pos, int max_len, int value)
{
    char tmp[12];
    int len = 0;
    int i;

    if (value == 0)
    {
        if (*pos < max_len - 1)
            dst[(*pos)++] = '0';
        return;
    }

    while (value && len < (int)sizeof(tmp))
    {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (i = len - 1; i >= 0 && *pos < max_len - 1; i--)
        dst[(*pos)++] = tmp[i];
}

static void append_char_limited(char *dst, int *pos, int max_len, char value)
{
    if (*pos < max_len - 1)
        dst[(*pos)++] = value;
}

static void append_text_limited(char *dst, int *pos, int max_len, const char *text)
{
    int i = 0;
    if (!text)
        return;
    while (text[i] && *pos < max_len - 1)
        dst[(*pos)++] = text[i++];
}

static const char *find_text(const char *text, const char *needle)
{
    int i;
    int j;

    if (!text || !needle || !needle[0])
        return 0;

    for (i = 0; text[i]; i++)
    {
        for (j = 0; needle[j] && text[i + j] == needle[j]; j++)
            ;
        if (!needle[j])
            return &text[i];
    }

    return 0;
}

static int parse_json_int_after(const char *json, const char *key, int fallback)
{
    const char *p = find_text(json, key);
    int value = 0;
    int found = 0;

    if (!p)
        return fallback;
    while (*p && *p != ':')
        p++;
    if (*p == ':')
        p++;
    while (*p == ' ')
        p++;
    while (*p >= '0' && *p <= '9')
    {
        value = value * 10 + (*p - '0');
        found = 1;
        p++;
    }

    return found ? value : fallback;
}

static int parse_json_string_after(const char *json, const char *key, char *out, int out_len)
{
    const char *p = find_text(json, key);
    int i = 0;

    if (!out || out_len <= 0)
        return 0;
    out[0] = '\0';
    if (!p)
        return 0;
    while (*p && *p != ':')
        p++;
    if (*p == ':')
        p++;
    while (*p && *p != '"')
        p++;
    if (*p != '"')
        return 0;
    p++;
    while (*p && *p != '"' && i < out_len - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

static unsigned int race_config_hash(const unsigned char *data, int len)
{
    unsigned int hash = 2166136261u;
    int i;

    for (i = 0; i < len; i++)
    {
        hash ^= (unsigned int)data[i];
        hash *= 16777619u;
    }

    return hash;
}

static int race_challenge_bits(void)
{
    int bits = 0;
    if (s_damage_sync_enabled)
        bits |= RACE_CHALLENGE_DAMAGE_SYNC;
    if (s_no_hit_enabled)
        bits |= RACE_CHALLENGE_NO_HIT;
    if (s_one_life_enabled)
        bits |= RACE_CHALLENGE_ONE_LIFE;
    if (s_ryo_sync_enabled)
        bits |= RACE_CHALLENGE_RYO_SYNC;
    bits |= (s_enemy_multiplier - 1) << RACE_CHALLENGE_ENEMY_MULT_SHIFT;
    return bits;
}

static void apply_race_challenge_runtime(void)
{
    anchor_runtime_set_damage_sync_enabled(s_damage_sync_enabled);
    anchor_runtime_set_no_hit_enabled(s_no_hit_enabled);
    anchor_runtime_set_one_life_enabled(s_one_life_enabled);
    anchor_runtime_set_ryo_sync_enabled(s_ryo_sync_enabled);
    anchor_runtime_set_enemy_multiplier(s_enemy_multiplier);
}

static int base64url_value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '-')
        return 62;
    if (c == '_')
        return 63;
    return -1;
}

static void base64url_encode(const unsigned char *data, int len, char *out, int out_len)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    int i = 0;
    int pos = 0;

    while (i < len && pos < out_len - 1)
    {
        unsigned int value = ((unsigned int)data[i]) << 16;
        int remaining = len - i;
        if (remaining > 1)
            value |= ((unsigned int)data[i + 1]) << 8;
        if (remaining > 2)
            value |= (unsigned int)data[i + 2];

        out[pos++] = alphabet[(value >> 18) & 63];
        if (pos < out_len - 1)
            out[pos++] = alphabet[(value >> 12) & 63];
        if (pos < out_len - 1 && remaining > 1)
            out[pos++] = alphabet[(value >> 6) & 63];
        if (pos < out_len - 1 && remaining > 2)
            out[pos++] = alphabet[value & 63];

        i += 3;
    }

    out[pos] = '\0';
}

static int base64url_decode(const char *text, unsigned char *out, int out_len)
{
    int vals[4];
    int val_count = 0;
    int pos = 0;

    while (text && *text)
    {
        int v;

        if (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
        {
            text++;
            continue;
        }

        v = base64url_value(*text++);
        if (v < 0)
            return -1;

        vals[val_count++] = v;
        if (val_count == 4)
        {
            if (pos + 3 > out_len)
                return -1;
            out[pos++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
            out[pos++] = (unsigned char)((vals[1] << 4) | (vals[2] >> 2));
            out[pos++] = (unsigned char)((vals[2] << 6) | vals[3]);
            val_count = 0;
        }
    }

    if (val_count == 1)
        return -1;
    if (val_count == 2)
    {
        if (pos + 1 > out_len)
            return -1;
        out[pos++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
    }
    else if (val_count == 3)
    {
        if (pos + 2 > out_len)
            return -1;
        out[pos++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
        out[pos++] = (unsigned char)((vals[1] << 4) | (vals[2] >> 2));
    }

    return pos;
}

static const char *race_code_payload_start(const char *code)
{
    if (!code)
        return 0;
    while (*code == ' ' || *code == '\t' || *code == '\n' || *code == '\r')
        code++;
    if (code[0] != 'M' || code[1] != 'R' || code[2] != '1' || code[3] != '.')
        return 0;
    return code + 4;
}

static void build_race_config_code(char *out, int out_len)
{
    unsigned char data[RACE_CONFIG_DATA_MAX];
    char encoded[RACE_CONFIG_CODE_LEN];
    int flag_bytes = (s_flag_count + 7) / 8;
    int pos = 0;
    int i;
    int text_pos = 0;
    unsigned int hash;

    if (!out || out_len <= 0)
        return;

    data[pos++] = RACE_CONFIG_VERSION;
    data[pos++] = (unsigned char)((s_goal_idx >> 8) & 0xFF);
    data[pos++] = (unsigned char)(s_goal_idx & 0xFF);
    data[pos++] = (unsigned char)(s_location_idx & 0xFF);
    data[pos++] = (unsigned char)race_challenge_bits();
    data[pos++] = (unsigned char)((s_flag_count >> 8) & 0xFF);
    data[pos++] = (unsigned char)(s_flag_count & 0xFF);

    for (i = 0; i < flag_bytes; i++)
        data[pos++] = 0;
    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        if (s_start_selected[i])
            data[7 + (i / 8)] |= (unsigned char)(1 << (i & 7));
    }

    hash = race_config_hash(data, pos);
    data[pos++] = (unsigned char)((hash >> 24) & 0xFF);
    data[pos++] = (unsigned char)((hash >> 16) & 0xFF);
    data[pos++] = (unsigned char)((hash >> 8) & 0xFF);
    data[pos++] = (unsigned char)(hash & 0xFF);

    copy_text(out, "MR1.", out_len);
    text_pos = 4;
    base64url_encode(data, pos, encoded, (int)sizeof(encoded));
    append_text_limited(out, &text_pos, out_len, encoded);
    out[text_pos] = '\0';
}

static int apply_race_config_code(const char *code)
{
    unsigned char data[RACE_CONFIG_DATA_MAX];
    unsigned char selected[RACE_MAX_FLAGS];
    const char *payload = race_code_payload_start(code);
    int len;
    int goal;
    int loc;
    int flag_count;
    int flag_bytes;
    int expected_len;
    int flag_offset;
    int challenge_bits = race_challenge_bits();
    int old_goal = s_goal_idx;
    int old_location = s_location_idx;
    int i;
    unsigned int stored_hash;
    unsigned int actual_hash;

    if (!payload)
        return 0;

    len = base64url_decode(payload, data, (int)sizeof(data));
    if (len < 10)
        return 0;

    if (data[0] < RACE_CONFIG_MIN_VERSION || data[0] > RACE_CONFIG_VERSION)
        return 0;

    goal = ((int)data[1] << 8) | (int)data[2];
    loc = (int)data[3];
    if (data[0] >= 2)
    {
        challenge_bits = data[4];
        flag_count = ((int)data[5] << 8) | (int)data[6];
        flag_offset = 7;
    }
    else
    {
        flag_count = ((int)data[4] << 8) | (int)data[5];
        flag_offset = 6;
    }
    if (flag_count != s_flag_count || flag_count < 0 || flag_count > RACE_MAX_FLAGS)
        return 0;

    flag_bytes = (flag_count + 7) / 8;
    expected_len = flag_offset + flag_bytes + 4;
    if (len != expected_len)
        return 0;

    stored_hash = ((unsigned int)data[len - 4] << 24) |
                  ((unsigned int)data[len - 3] << 16) |
                  ((unsigned int)data[len - 2] << 8) |
                  (unsigned int)data[len - 1];
    actual_hash = race_config_hash(data, len - 4);
    if (stored_hash != actual_hash)
        return 0;

    if (goal < 0 || goal >= s_flag_count || goal >= RACE_MAX_FLAGS ||
        !anchor_flag_catalog_get(goal) || !anchor_flag_catalog_get(goal)->key)
        return 0;
    if (loc < 0 || loc >= s_start_location_count)
        return 0;

    for (i = 0; i < RACE_MAX_FLAGS; i++)
        selected[i] = 0;
    for (i = 0; i < flag_count; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if ((data[flag_offset + (i / 8)] & (1 << (i & 7))) && entry && entry->key)
            selected[i] = 1;
    }

    s_goal_idx = goal;
    s_location_idx = loc;
    s_damage_sync_enabled = (challenge_bits & RACE_CHALLENGE_DAMAGE_SYNC) ? 1 : 0;
    s_no_hit_enabled = (challenge_bits & RACE_CHALLENGE_NO_HIT) ? 1 : 0;
    s_one_life_enabled = (challenge_bits & RACE_CHALLENGE_ONE_LIFE) ? 1 : 0;
    s_ryo_sync_enabled = (challenge_bits & RACE_CHALLENGE_RYO_SYNC) ? 1 : 0;
    s_enemy_multiplier =
        ((challenge_bits & RACE_CHALLENGE_ENEMY_MULT_MASK) >> RACE_CHALLENGE_ENEMY_MULT_SHIFT) + 1;
    if (s_enemy_multiplier < 1 || s_enemy_multiplier > 3)
        s_enemy_multiplier = 1;
    apply_race_challenge_runtime();
    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
        s_start_selected[i] = selected[i];

    if (s_ui_built)
    {
        for (i = 0; i < s_category_count; i++)
        {
            update_category_count(i);
            update_goal_category_count(i);
        }
        for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
            update_start_button(i);
        update_goal_buttons(old_goal, s_goal_idx);
        update_location_buttons(old_location, s_location_idx);
        update_goal_label();
        update_location_label();
        update_damage_sync_label();
    }

    return 1;
}

static void refresh_live_config_code(void)
{
    if (s_live_config_lbl == RECOMPUI_NULL_RESOURCE)
        return;

    build_race_config_code(s_config_code, (int)sizeof(s_config_code));
    recompui_open_context(s_ctx);
    recompui_set_text(s_live_config_lbl, s_config_code);
    recompui_close_context(s_ctx);
}

static void race_result_ui_init(void)
{
    if (s_result_ctx != RECOMPUI_NULL_CONTEXT)
        return;

    s_result_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_result_ctx, 1);
    recompui_set_context_captures_mouse(s_result_ctx, 1);

    recompui_open_context(s_result_ctx);
    {
        RecompuiResource root = recompui_context_root(s_result_ctx);
        RecompuiResource overlay = recompui_create_element(s_result_ctx, root);
        RecompuiResource panel;
        RecompuiResource prompt;

        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &R_BG);

        panel = recompui_create_element(s_result_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 50.0f, UNIT_PERCENT);
        recompui_set_top(panel, 16.0f, UNIT_PERCENT);
        recompui_set_margin_left(panel, -280.0f, UNIT_DP);
        recompui_set_width(panel, 560.0f, UNIT_DP);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(panel, 14.0f, UNIT_DP);
        recompui_set_padding(panel, 28.0f, UNIT_DP);
        recompui_set_background_color(panel, &R_PANEL);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &R_BORDER);

        s_result_title_lbl = recompui_create_label(s_result_ctx, panel, "Race Complete", LABELSTYLE_NORMAL);
        recompui_set_color(s_result_title_lbl, &R_PURPLE);
        recompui_set_font_weight(s_result_title_lbl, 800);
        recompui_set_font_size(s_result_title_lbl, 30.0f, UNIT_DP);
        recompui_set_text_align(s_result_title_lbl, TEXT_ALIGN_CENTER);

        s_result_team_lbl = recompui_create_label(s_result_ctx, panel, "", LABELSTYLE_NORMAL);
        recompui_set_color(s_result_team_lbl, &R_WHITE);
        recompui_set_font_weight(s_result_team_lbl, 700);
        recompui_set_font_size(s_result_team_lbl, 20.0f, UNIT_DP);
        recompui_set_text_align(s_result_team_lbl, TEXT_ALIGN_CENTER);

        s_result_players_lbl = recompui_create_label(s_result_ctx, panel, "", LABELSTYLE_SMALL);
        recompui_set_color(s_result_players_lbl, &R_DIM);
        recompui_set_font_size(s_result_players_lbl, 16.0f, UNIT_DP);
        recompui_set_text_align(s_result_players_lbl, TEXT_ALIGN_CENTER);

        prompt = recompui_create_label(
            s_result_ctx, panel,
            "Please restart the game client to return to a fresh race state.",
            LABELSTYLE_SMALL);
        recompui_set_color(prompt, &R_GREEN);
        recompui_set_font_weight(prompt, 700);
        recompui_set_font_size(prompt, 16.0f, UNIT_DP);
        recompui_set_text_align(prompt, TEXT_ALIGN_CENTER);
    }
    recompui_close_context(s_result_ctx);
}

static int local_team_matches(const char *team)
{
    int same = 0;
    char *local_team;

    if (!team || !team[0])
        return 0;

    local_team = anchor_get_team_id();
    if (local_team)
    {
        same = text_equals(local_team, team);
        recomp_free(local_team);
    }

    return same;
}

static void show_race_result(int victory, const char *team, const char *players)
{
    int pos;

    race_result_ui_init();

    copy_text(s_result_team_text, "Winning Team: ", (int)sizeof(s_result_team_text));
    pos = 13;
    append_text_limited(s_result_team_text, &pos, (int)sizeof(s_result_team_text),
                        (team && team[0]) ? team : "default");
    s_result_team_text[pos] = '\0';

    copy_text(s_result_players_text, "Players: ", (int)sizeof(s_result_players_text));
    pos = 9;
    append_text_limited(s_result_players_text, &pos, (int)sizeof(s_result_players_text),
                        (players && players[0]) ? players : "Player");
    s_result_players_text[pos] = '\0';

    recompui_open_context(s_result_ctx);
    recompui_set_text(s_result_title_lbl, victory ? "Victory!" : "Defeated");
    recompui_set_color(s_result_title_lbl, victory ? &R_GREEN : &R_RED);
    recompui_set_text(s_result_team_lbl, s_result_team_text);
    recompui_set_text(s_result_players_lbl, s_result_players_text);
    recompui_close_context(s_result_ctx);

    if (!s_result_visible)
    {
        recompui_show_context(s_result_ctx);
        s_result_visible = 1;
    }
}

void anchor_race_on_forced_disconnect(void)
{
    race_result_ui_init();

    recompui_open_context(s_result_ctx);
    recompui_set_text(s_result_title_lbl, "Race Disconnected");
    recompui_set_color(s_result_title_lbl, &R_RED);
    recompui_set_text(s_result_team_lbl, "The server connection was lost.");
    recompui_set_text(s_result_players_lbl, "Quit out of the application before starting another race.");
    recompui_close_context(s_result_ctx);

    if (!s_result_visible)
    {
        recompui_show_context(s_result_ctx);
        s_result_visible = 1;
    }
}

static void update_category_count(int cat_idx)
{
    int selected = 0;
    int total = 0;
    int pos = 0;
    int i;

    if (cat_idx < 0 || cat_idx >= s_category_count)
        return;

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry || !entry->key)
            continue;
        total++;
        if (i >= 0 && i < RACE_MAX_FLAGS && s_start_selected[i])
            selected++;
    }

    s_category_count_text[cat_idx][pos++] = '[';
    append_uint(s_category_count_text[cat_idx], &pos, 32, selected);
    if (pos < 31)
        s_category_count_text[cat_idx][pos++] = '/';
    append_uint(s_category_count_text[cat_idx], &pos, 32, total);
    if (pos < 31)
        s_category_count_text[cat_idx][pos++] = ']';
    s_category_count_text[cat_idx][pos] = '\0';

    recompui_open_context(s_ctx);
    if (s_category_count_lbls[cat_idx] != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_category_count_lbls[cat_idx], s_category_count_text[cat_idx]);
    recompui_close_context(s_ctx);
}

static int category_for_flag(int flag_idx)
{
    int i;
    for (i = 0; i < s_category_count; i++)
    {
        if (flag_idx >= s_category_start[i] && flag_idx < s_category_end[i])
            return i;
    }
    return -1;
}

static void update_goal_category_count(int cat_idx)
{
    int total = 0;
    int pos = 0;
    int i;

    if (cat_idx < 0 || cat_idx >= s_category_count)
        return;

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (entry && entry->key)
            total++;
    }

    if (category_for_flag(s_goal_idx) == cat_idx)
    {
        const char *text = "[Goal]";
        while (text[pos] && pos < 31)
        {
            s_goal_category_count_text[cat_idx][pos] = text[pos];
            pos++;
        }
    }
    else
    {
        s_goal_category_count_text[cat_idx][pos++] = '[';
        append_uint(s_goal_category_count_text[cat_idx], &pos, 32, total);
        if (pos < 31)
            s_goal_category_count_text[cat_idx][pos++] = ']';
    }
    s_goal_category_count_text[cat_idx][pos] = '\0';

    recompui_open_context(s_ctx);
    if (s_goal_category_count_lbls[cat_idx] != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_goal_category_count_lbls[cat_idx], s_goal_category_count_text[cat_idx]);
    recompui_close_context(s_ctx);
}

static void set_screen(int screen)
{
    int i;
    s_screen = screen;

    recompui_open_context(s_ctx);
    recompui_set_display(s_setup_view, screen == 0 ? DISPLAY_FLEX : DISPLAY_NONE);
    recompui_set_display(s_goal_view, screen == 1 ? DISPLAY_FLEX : DISPLAY_NONE);
    recompui_set_display(s_location_view, screen == 4 ? DISPLAY_FLEX : DISPLAY_NONE);
    recompui_set_display(s_challenges_view, screen == 5 ? DISPLAY_FLEX : DISPLAY_NONE);
    for (i = 0; i < s_category_count; i++)
    {
        if (s_category_views[i] != RECOMPUI_NULL_RESOURCE)
            recompui_set_display(s_category_views[i],
                                 (screen == 2 && i == s_active_category) ? DISPLAY_FLEX : DISPLAY_NONE);
        if (s_goal_category_views[i] != RECOMPUI_NULL_RESOURCE)
            recompui_set_display(s_goal_category_views[i],
                                 (screen == 3 && i == s_active_goal_category) ? DISPLAY_FLEX : DISPLAY_NONE);
    }
    recompui_close_context(s_ctx);
}

static void update_location_label(void)
{
    const char *text = "Goemon's House";

    if (s_location_idx >= 0 && s_location_idx < s_start_location_count)
        text = s_start_locations[s_location_idx].name;

    recompui_open_context(s_ctx);
    if (s_location_lbl != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_location_lbl, text);
    recompui_close_context(s_ctx);
    refresh_live_config_code();
}

static void update_goal_label(void)
{
    const AnchorFlagEntry *entry = anchor_flag_catalog_get(s_goal_idx);

    recompui_open_context(s_ctx);
    if (entry && entry->key)
        recompui_set_text(s_goal_lbl, entry->display);
    else
        recompui_set_text(s_goal_lbl, "No end condition selected");
    recompui_close_context(s_ctx);
    refresh_live_config_code();
}

static void update_damage_sync_label(void)
{
    int enabled_count = (s_damage_sync_enabled ? 1 : 0) +
                        (s_no_hit_enabled ? 1 : 0) +
                        (s_one_life_enabled ? 1 : 0) +
                        (s_ryo_sync_enabled ? 1 : 0) +
                        (s_enemy_multiplier > 1 ? 1 : 0);
    int pos = 0;
    char enemy_mult_text[4];

    copy_text(s_challenges_summary, "Challenges: ", (int)sizeof(s_challenges_summary));
    pos = 12;
    if (enabled_count == 0)
    {
        append_text_limited(s_challenges_summary, &pos, (int)sizeof(s_challenges_summary), "None Enabled");
    }
    else
    {
        append_uint(s_challenges_summary, &pos, (int)sizeof(s_challenges_summary), enabled_count);
        append_text_limited(s_challenges_summary, &pos, (int)sizeof(s_challenges_summary), " Enabled");
    }
    s_challenges_summary[pos] = '\0';

    recompui_open_context(s_ctx);
    if (s_damage_sync_lbl != RECOMPUI_NULL_RESOURCE)
    {
        recompui_set_text(s_damage_sync_lbl, s_challenges_summary);
        recompui_set_color(s_damage_sync_lbl, enabled_count > 0 ? &R_GREEN : &R_DIM);
    }
    if (s_damage_sync_detail_lbl != RECOMPUI_NULL_RESOURCE)
    {
        recompui_set_text(s_damage_sync_detail_lbl, s_damage_sync_enabled ? "Enabled" : "Disabled");
        recompui_set_color(s_damage_sync_detail_lbl, s_damage_sync_enabled ? &R_GREEN : &R_DIM);
    }
    if (s_no_hit_detail_lbl != RECOMPUI_NULL_RESOURCE)
    {
        recompui_set_text(s_no_hit_detail_lbl, s_no_hit_enabled ? "Enabled" : "Disabled");
        recompui_set_color(s_no_hit_detail_lbl, s_no_hit_enabled ? &R_GREEN : &R_DIM);
    }
    if (s_one_life_detail_lbl != RECOMPUI_NULL_RESOURCE)
    {
        recompui_set_text(s_one_life_detail_lbl, s_one_life_enabled ? "Enabled" : "Disabled");
        recompui_set_color(s_one_life_detail_lbl, s_one_life_enabled ? &R_GREEN : &R_DIM);
    }
    if (s_ryo_sync_detail_lbl != RECOMPUI_NULL_RESOURCE)
    {
        recompui_set_text(s_ryo_sync_detail_lbl, s_ryo_sync_enabled ? "Enabled" : "Disabled");
        recompui_set_color(s_ryo_sync_detail_lbl, s_ryo_sync_enabled ? &R_GREEN : &R_DIM);
    }
    enemy_mult_text[0] = (char)('0' + s_enemy_multiplier);
    enemy_mult_text[1] = 'x';
    enemy_mult_text[2] = '\0';
    if (s_enemy_multiplier_detail_lbl != RECOMPUI_NULL_RESOURCE)
    {
        recompui_set_text(s_enemy_multiplier_detail_lbl, enemy_mult_text);
        recompui_set_color(s_enemy_multiplier_detail_lbl, s_enemy_multiplier > 1 ? &R_GREEN : &R_DIM);
    }
    recompui_close_context(s_ctx);

    apply_race_challenge_runtime();
    refresh_live_config_code();
}

static void update_start_button(int idx)
{
    const AnchorFlagEntry *entry = anchor_flag_catalog_get(idx);
    if (!entry || !entry->key || idx < 0 || idx >= RACE_MAX_FLAGS)
        return;

    make_toggle_text(s_start_btn_text[idx],
                     s_start_selected[idx] ? "[x] " : "[ ] ",
                     entry->display);

    recompui_open_context(s_ctx);
    if (s_start_btns[idx] != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_start_btns[idx], s_start_btn_text[idx]);
    recompui_close_context(s_ctx);

    update_category_count(category_for_flag(idx));
    refresh_live_config_code();
}

static void update_location_buttons(int old_idx, int new_idx)
{
    recompui_open_context(s_ctx);

    if (old_idx >= 0 && old_idx < s_start_location_count && old_idx < RACE_MAX_LOCATIONS &&
        s_location_btns[old_idx] != RECOMPUI_NULL_RESOURCE)
    {
        make_toggle_text(s_location_btn_text[old_idx], "[ ] ", s_start_locations[old_idx].name);
        recompui_set_text(s_location_btns[old_idx], s_location_btn_text[old_idx]);
    }

    if (new_idx >= 0 && new_idx < s_start_location_count && new_idx < RACE_MAX_LOCATIONS &&
        s_location_btns[new_idx] != RECOMPUI_NULL_RESOURCE)
    {
        make_toggle_text(s_location_btn_text[new_idx], "[x] ", s_start_locations[new_idx].name);
        recompui_set_text(s_location_btns[new_idx], s_location_btn_text[new_idx]);
    }

    recompui_close_context(s_ctx);
    refresh_live_config_code();
}

static void update_goal_buttons(int old_idx, int new_idx)
{
    const AnchorFlagEntry *entry;

    recompui_open_context(s_ctx);

    if (old_idx >= 0 && old_idx < s_flag_count && old_idx < RACE_MAX_FLAGS)
    {
        entry = anchor_flag_catalog_get(old_idx);
        if (entry && entry->key && s_goal_btns[old_idx] != RECOMPUI_NULL_RESOURCE)
        {
            make_toggle_text(s_goal_btn_text[old_idx], "", entry->display);
            recompui_set_text(s_goal_btns[old_idx], s_goal_btn_text[old_idx]);
        }
    }

    if (new_idx >= 0 && new_idx < s_flag_count && new_idx < RACE_MAX_FLAGS)
    {
        entry = anchor_flag_catalog_get(new_idx);
        if (entry && entry->key && s_goal_btns[new_idx] != RECOMPUI_NULL_RESOURCE)
        {
            make_toggle_text(s_goal_btn_text[new_idx], "[Goal] ", entry->display);
            recompui_set_text(s_goal_btns[new_idx], s_goal_btn_text[new_idx]);
        }
    }

    recompui_close_context(s_ctx);
    refresh_live_config_code();
}

static void on_back_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_back = 1;
}

static void on_choose_goal_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_choose_goal = 1;
}

static void on_choose_location_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_choose_location = 1;
}

static void on_choose_challenges_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_choose_challenges = 1;
}

static void on_import_config_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_import_config = 1;
}

static void on_paste_config_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_paste_config = 1;
}

static void on_export_config_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_export_config = 1;
}

static void on_goal_done_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_goal_done = 1;
}

static void on_location_done_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_location_done = 1;
}

static void on_challenges_done_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_challenges_done = 1;
}

static void on_damage_sync_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_toggle_damage_sync = 1;
}

static void on_no_hit_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_toggle_no_hit = 1;
}

static void on_one_life_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_toggle_one_life = 1;
}

static void on_ryo_sync_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_toggle_ryo_sync = 1;
}

static void on_enemy_multiplier_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_cycle_enemy_multiplier = 1;
}

static void on_category_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_category_idx = (int)(unsigned long)ud;
}

static void on_goal_category_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_goal_category_idx = (int)(unsigned long)ud;
}

static void on_goal_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_goal_idx = (int)(unsigned long)ud;
}

static void on_location_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_location_idx = (int)(unsigned long)ud;
}

static void on_start_flag_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_toggle_idx = (int)(unsigned long)ud;
}

static RecompuiResource make_grid_row(RecompuiResource section)
{
    RecompuiResource row = recompui_create_element(s_ctx, section);
    recompui_set_display(row, DISPLAY_FLEX);
    recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
    recompui_set_gap(row, 8.0f, UNIT_DP);
    recompui_set_min_height(row, 38.0f, UNIT_DP);
    return row;
}

static void add_start_flag_button(RecompuiResource row, int idx, const AnchorFlagEntry *entry)
{
    make_toggle_text(s_start_btn_text[idx],
                     s_start_selected[idx] ? "[x] " : "[ ] ",
                     entry->display);

    s_start_btns[idx] = recompui_create_button(s_ctx, row, s_start_btn_text[idx], BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(s_start_btns[idx], CURSOR_POINTER);
    recompui_set_font_size(s_start_btns[idx], 12.0f, UNIT_DP);
    recompui_set_text_align(s_start_btns[idx], TEXT_ALIGN_LEFT);
    recompui_set_flex_grow(s_start_btns[idx], 1.0f);
    recompui_set_flex_basis(s_start_btns[idx], 50.0f, UNIT_PERCENT);
    recompui_set_height(s_start_btns[idx], 34.0f, UNIT_DP);
    recompui_set_padding_top(s_start_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_bottom(s_start_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_left(s_start_btns[idx], 12.0f, UNIT_DP);
    recompui_set_padding_right(s_start_btns[idx], 12.0f, UNIT_DP);
    recompui_set_tab_index(s_start_btns[idx], TAB_INDEX_AUTO);
    recompui_register_callback(s_start_btns[idx], on_start_flag_clicked, (void *)(unsigned long)idx);
}

static void add_goal_flag_button(RecompuiResource row, int idx, const AnchorFlagEntry *entry)
{
    copy_text(s_goal_btn_text[idx], entry->display, RACE_TEXT_LEN);

    s_goal_btns[idx] = recompui_create_button(s_ctx, row, s_goal_btn_text[idx], BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(s_goal_btns[idx], CURSOR_POINTER);
    recompui_set_font_size(s_goal_btns[idx], 12.0f, UNIT_DP);
    recompui_set_text_align(s_goal_btns[idx], TEXT_ALIGN_LEFT);
    recompui_set_flex_grow(s_goal_btns[idx], 1.0f);
    recompui_set_flex_basis(s_goal_btns[idx], 50.0f, UNIT_PERCENT);
    recompui_set_height(s_goal_btns[idx], 34.0f, UNIT_DP);
    recompui_set_padding_top(s_goal_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_bottom(s_goal_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_left(s_goal_btns[idx], 12.0f, UNIT_DP);
    recompui_set_padding_right(s_goal_btns[idx], 12.0f, UNIT_DP);
    recompui_set_tab_index(s_goal_btns[idx], TAB_INDEX_AUTO);
    recompui_register_callback(s_goal_btns[idx], on_goal_clicked, (void *)(unsigned long)idx);
}

static void add_location_button(RecompuiResource row, int idx)
{
    make_toggle_text(s_location_btn_text[idx],
                     idx == s_location_idx ? "[x] " : "[ ] ",
                     s_start_locations[idx].name);

    s_location_btns[idx] = recompui_create_button(s_ctx, row, s_location_btn_text[idx], BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(s_location_btns[idx], CURSOR_POINTER);
    recompui_set_font_size(s_location_btns[idx], 12.0f, UNIT_DP);
    recompui_set_text_align(s_location_btns[idx], TEXT_ALIGN_LEFT);
    recompui_set_flex_grow(s_location_btns[idx], 1.0f);
    recompui_set_flex_basis(s_location_btns[idx], 50.0f, UNIT_PERCENT);
    recompui_set_height(s_location_btns[idx], 34.0f, UNIT_DP);
    recompui_set_padding_top(s_location_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_bottom(s_location_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_left(s_location_btns[idx], 12.0f, UNIT_DP);
    recompui_set_padding_right(s_location_btns[idx], 12.0f, UNIT_DP);
    recompui_set_tab_index(s_location_btns[idx], TAB_INDEX_AUTO);
    recompui_register_callback(s_location_btns[idx], on_location_clicked, (void *)(unsigned long)idx);
}

static void build_category_index(void)
{
    int current = -1;
    int i;

    s_category_count = 0;
    for (i = 0; i < s_flag_count; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry)
            continue;

        if (!entry->key)
        {
            if (current >= 0)
                s_category_end[current] = i;
            if (s_category_count < RACE_MAX_CATEGORIES)
            {
                current = s_category_count++;
                s_category_names[current] = entry->display;
                s_category_start[current] = i + 1;
                s_category_end[current] = i + 1;
            }
            continue;
        }

        if (current >= 0)
            s_category_end[current] = i + 1;
    }
}

static void build_category_picker(RecompuiResource list)
{
    int i;

    for (i = 0; i < s_category_count; i++)
    {
        RecompuiResource row = recompui_create_element(s_ctx, list);
        recompui_set_display(row, DISPLAY_FLEX);
        recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(row, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(row, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_min_height(row, 52.0f, UNIT_DP);
        recompui_set_padding_left(row, 16.0f, UNIT_DP);
        recompui_set_padding_right(row, 12.0f, UNIT_DP);
        recompui_set_background_color(row, (i & 1) ? &R_CARD_ALT : &R_CARD);
        recompui_set_border_radius(row, 3.0f, UNIT_DP);

        RecompuiResource name = recompui_create_label(s_ctx, row, s_category_names[i], LABELSTYLE_SMALL);
        recompui_set_color(name, &R_PURPLE);
        recompui_set_font_weight(name, 700);
        recompui_set_font_size(name, 18.0f, UNIT_DP);
        recompui_set_flex_grow(name, 1.0f);

        s_category_count_lbls[i] = recompui_create_label(s_ctx, row, "[0/0]", LABELSTYLE_ANNOTATION);
        recompui_set_color(s_category_count_lbls[i], &R_DIM);
        recompui_set_font_size(s_category_count_lbls[i], 14.0f, UNIT_DP);
        recompui_set_margin_right(s_category_count_lbls[i], 12.0f, UNIT_DP);

        RecompuiResource button = recompui_create_button(s_ctx, row, "Configure", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(button, CURSOR_POINTER);
        recompui_set_font_size(button, 13.0f, UNIT_DP);
        recompui_set_height(button, 32.0f, UNIT_DP);
        recompui_set_width(button, 112.0f, UNIT_DP);
        recompui_set_padding_top(button, 4.0f, UNIT_DP);
        recompui_set_padding_bottom(button, 4.0f, UNIT_DP);
        recompui_set_tab_index(button, TAB_INDEX_AUTO);
        recompui_register_callback(button, on_category_clicked, (void *)(unsigned long)i);
    }
}

static void build_goal_category_picker(RecompuiResource list)
{
    int i;

    for (i = 0; i < s_category_count; i++)
    {
        RecompuiResource row = recompui_create_element(s_ctx, list);
        recompui_set_display(row, DISPLAY_FLEX);
        recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(row, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(row, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_min_height(row, 52.0f, UNIT_DP);
        recompui_set_padding_left(row, 16.0f, UNIT_DP);
        recompui_set_padding_right(row, 12.0f, UNIT_DP);
        recompui_set_background_color(row, (i & 1) ? &R_CARD_ALT : &R_CARD);
        recompui_set_border_radius(row, 3.0f, UNIT_DP);

        RecompuiResource name = recompui_create_label(s_ctx, row, s_category_names[i], LABELSTYLE_SMALL);
        recompui_set_color(name, &R_PURPLE);
        recompui_set_font_weight(name, 700);
        recompui_set_font_size(name, 18.0f, UNIT_DP);
        recompui_set_flex_grow(name, 1.0f);

        s_goal_category_count_lbls[i] = recompui_create_label(s_ctx, row, "[0]", LABELSTYLE_ANNOTATION);
        recompui_set_color(s_goal_category_count_lbls[i], &R_DIM);
        recompui_set_font_size(s_goal_category_count_lbls[i], 14.0f, UNIT_DP);
        recompui_set_margin_right(s_goal_category_count_lbls[i], 12.0f, UNIT_DP);

        RecompuiResource button = recompui_create_button(s_ctx, row, "Choose", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(button, CURSOR_POINTER);
        recompui_set_font_size(button, 13.0f, UNIT_DP);
        recompui_set_height(button, 32.0f, UNIT_DP);
        recompui_set_width(button, 112.0f, UNIT_DP);
        recompui_set_padding_top(button, 4.0f, UNIT_DP);
        recompui_set_padding_bottom(button, 4.0f, UNIT_DP);
        recompui_set_tab_index(button, TAB_INDEX_AUTO);
        recompui_register_callback(button, on_goal_category_clicked, (void *)(unsigned long)i);
    }
}

static void build_category_detail_view(RecompuiResource parent, int cat_idx)
{
    RecompuiResource row = RECOMPUI_NULL_RESOURCE;
    int cols = 0;
    int i;

    s_category_views[cat_idx] = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_category_views[cat_idx], DISPLAY_NONE);
    recompui_set_flex_direction(s_category_views[cat_idx], FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_category_views[cat_idx], 10.0f, UNIT_DP);
    recompui_set_padding_left(s_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_right(s_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_top(s_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_category_views[cat_idx], 1.0f);

    RecompuiResource title = recompui_create_label(
        s_ctx, s_category_views[cat_idx], s_category_names[cat_idx], LABELSTYLE_NORMAL);
    recompui_set_color(title, &R_PURPLE);
    recompui_set_font_weight(title, 700);
    recompui_set_font_size(title, 20.0f, UNIT_DP);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_category_views[cat_idx],
        "Choose the flags this race should grant before the run starts.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    if (text_equals(s_category_names[cat_idx], "Characters"))
    {
        RecompuiResource character_note = recompui_create_label(
            s_ctx, s_category_views[cat_idx],
            "During a race, Goemon and Ebisumaru unlock when Goemon's House warp is set.",
            LABELSTYLE_SMALL);
        recompui_set_color(character_note, &R_DIM);
        recompui_set_font_size(character_note, 13.0f, UNIT_DP);
    }

    RecompuiResource list = recompui_create_element(s_ctx, s_category_views[cat_idx]);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 4.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry || !entry->key)
            continue;

        if (row == RECOMPUI_NULL_RESOURCE || cols >= 2)
        {
            row = make_grid_row(list);
            cols = 0;
        }
        add_start_flag_button(row, i, entry);
        cols++;
    }
}

static void build_goal_category_detail_view(RecompuiResource parent, int cat_idx)
{
    RecompuiResource row = RECOMPUI_NULL_RESOURCE;
    int cols = 0;
    int i;

    s_goal_category_views[cat_idx] = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_goal_category_views[cat_idx], DISPLAY_NONE);
    recompui_set_flex_direction(s_goal_category_views[cat_idx], FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_goal_category_views[cat_idx], 10.0f, UNIT_DP);
    recompui_set_padding_left(s_goal_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_right(s_goal_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_top(s_goal_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_goal_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_goal_category_views[cat_idx], 1.0f);

    RecompuiResource title = recompui_create_label(
        s_ctx, s_goal_category_views[cat_idx], s_category_names[cat_idx], LABELSTYLE_NORMAL);
    recompui_set_color(title, &R_PURPLE);
    recompui_set_font_weight(title, 700);
    recompui_set_font_size(title, 20.0f, UNIT_DP);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_goal_category_views[cat_idx],
        "Pick the flag that ends the race when it becomes synced.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_goal_category_views[cat_idx]);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 4.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry || !entry->key)
            continue;

        if (row == RECOMPUI_NULL_RESOURCE || cols >= 2)
        {
            row = make_grid_row(list);
            cols = 0;
        }
        add_goal_flag_button(row, i, entry);
        cols++;
    }
}

static void build_race_config_share_card(RecompuiResource parent)
{
    RecompuiResource share_card = recompui_create_element(s_ctx, parent);
    recompui_set_display(share_card, DISPLAY_FLEX);
    recompui_set_flex_direction(share_card, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(share_card, 14.0f, UNIT_DP);
    recompui_set_padding_left(share_card, 18.0f, UNIT_DP);
    recompui_set_padding_right(share_card, 18.0f, UNIT_DP);
    recompui_set_padding_top(share_card, 14.0f, UNIT_DP);
    recompui_set_padding_bottom(share_card, 16.0f, UNIT_DP);
    recompui_set_min_height(share_card, 224.0f, UNIT_DP);
    recompui_set_flex_shrink(share_card, 0.0f);
    recompui_set_background_color(share_card, &R_CARD_ALT);
    recompui_set_border_radius(share_card, 6.0f, UNIT_DP);
    recompui_set_border_width(share_card, 1.0f, UNIT_DP);
    recompui_set_border_color(share_card, &R_BORDER);

    RecompuiResource share_header = recompui_create_element(s_ctx, share_card);
    recompui_set_display(share_header, DISPLAY_FLEX);
    recompui_set_flex_direction(share_header, FLEX_DIRECTION_ROW);
    recompui_set_align_items(share_header, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(share_header, JUSTIFY_CONTENT_SPACE_BETWEEN);

    RecompuiResource share_title = recompui_create_label(s_ctx, share_header, "Race Config Code", LABELSTYLE_SMALL);
    recompui_set_color(share_title, &R_DIM);
    recompui_set_font_weight(share_title, 700);
    recompui_set_font_size(share_title, 15.0f, UNIT_DP);

    RecompuiResource share_badge = recompui_create_label(s_ctx, share_header, "LIVE", LABELSTYLE_ANNOTATION);
    recompui_set_color(share_badge, &R_GREEN);
    recompui_set_font_weight(share_badge, 700);
    recompui_set_font_size(share_badge, 12.0f, UNIT_DP);

    RecompuiResource code_box = recompui_create_element(s_ctx, share_card);
    recompui_set_display(code_box, DISPLAY_FLEX);
    recompui_set_flex_direction(code_box, FLEX_DIRECTION_COLUMN);
    recompui_set_padding_left(code_box, 14.0f, UNIT_DP);
    recompui_set_padding_right(code_box, 14.0f, UNIT_DP);
    recompui_set_padding_top(code_box, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(code_box, 10.0f, UNIT_DP);
    recompui_set_min_height(code_box, 42.0f, UNIT_DP);
    recompui_set_background_color(code_box, &R_CARD);
    recompui_set_border_radius(code_box, 5.0f, UNIT_DP);
    recompui_set_border_width(code_box, 1.0f, UNIT_DP);
    recompui_set_border_color(code_box, &R_BORDER);

    build_race_config_code(s_config_code, (int)sizeof(s_config_code));
    s_live_config_lbl = recompui_create_label(s_ctx, code_box, s_config_code, LABELSTYLE_SMALL);
    recompui_set_color(s_live_config_lbl, &R_WHITE);
    recompui_set_font_weight(s_live_config_lbl, 700);
    recompui_set_font_size(s_live_config_lbl, 13.0f, UNIT_DP);
    recompui_set_line_height(s_live_config_lbl, 20.0f, UNIT_DP);

    RecompuiResource share_row = recompui_create_element(s_ctx, share_card);
    recompui_set_display(share_row, DISPLAY_FLEX);
    recompui_set_flex_direction(share_row, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(share_row, 18.0f, UNIT_DP);

    RecompuiResource input_box = recompui_create_element(s_ctx, share_row);
    recompui_set_display(input_box, DISPLAY_FLEX);
    recompui_set_flex_direction(input_box, FLEX_DIRECTION_COLUMN);
    recompui_set_padding_top(input_box, 8.0f, UNIT_DP);
    recompui_set_padding_bottom(input_box, 12.0f, UNIT_DP);
    recompui_set_min_height(input_box, 68.0f, UNIT_DP);

    s_import_input = recompui_create_textinput(s_ctx, input_box);
    recompui_set_font_size(s_import_input, 14.0f, UNIT_DP);
    recompui_set_flex_grow(s_import_input, 1.0f);
    recompui_set_width(s_import_input, 100.0f, UNIT_PERCENT);
    recompui_set_height(s_import_input, 52.0f, UNIT_DP);
    recompui_set_tab_index(s_import_input, TAB_INDEX_AUTO);
    recompui_set_input_text(s_import_input, "");

    RecompuiResource action_row = recompui_create_element(s_ctx, share_row);
    recompui_set_display(action_row, DISPLAY_FLEX);
    recompui_set_flex_direction(action_row, FLEX_DIRECTION_ROW);
    recompui_set_align_items(action_row, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(action_row, JUSTIFY_CONTENT_FLEX_END);
    recompui_set_gap(action_row, 10.0f, UNIT_DP);
    recompui_set_min_height(action_row, 44.0f, UNIT_DP);
    recompui_set_margin_top(action_row, 4.0f, UNIT_DP);

    RecompuiResource import_btn = recompui_create_button(s_ctx, action_row, "Import", BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(import_btn, CURSOR_POINTER);
    recompui_set_font_size(import_btn, 13.0f, UNIT_DP);
    recompui_set_font_weight(import_btn, 700);
    recompui_set_height(import_btn, 38.0f, UNIT_DP);
    recompui_set_width(import_btn, 104.0f, UNIT_DP);
    recompui_set_flex_shrink(import_btn, 0.0f);
    recompui_set_padding_top(import_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(import_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(import_btn, 12.0f, UNIT_DP);
    recompui_set_padding_right(import_btn, 12.0f, UNIT_DP);
    recompui_set_tab_index(import_btn, TAB_INDEX_AUTO);
    recompui_register_callback(import_btn, on_import_config_clicked, 0);

    RecompuiResource paste_btn = recompui_create_button(s_ctx, action_row, "Paste", BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(paste_btn, CURSOR_POINTER);
    recompui_set_font_size(paste_btn, 13.0f, UNIT_DP);
    recompui_set_font_weight(paste_btn, 700);
    recompui_set_height(paste_btn, 38.0f, UNIT_DP);
    recompui_set_width(paste_btn, 104.0f, UNIT_DP);
    recompui_set_flex_shrink(paste_btn, 0.0f);
    recompui_set_padding_top(paste_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(paste_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(paste_btn, 12.0f, UNIT_DP);
    recompui_set_padding_right(paste_btn, 12.0f, UNIT_DP);
    recompui_set_tab_index(paste_btn, TAB_INDEX_AUTO);
    recompui_register_callback(paste_btn, on_paste_config_clicked, 0);

    RecompuiResource export_btn = recompui_create_button(s_ctx, action_row, "Copy Code", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(export_btn, CURSOR_POINTER);
    recompui_set_font_size(export_btn, 13.0f, UNIT_DP);
    recompui_set_font_weight(export_btn, 700);
    recompui_set_height(export_btn, 38.0f, UNIT_DP);
    recompui_set_width(export_btn, 142.0f, UNIT_DP);
    recompui_set_flex_shrink(export_btn, 0.0f);
    recompui_set_padding_top(export_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(export_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(export_btn, 12.0f, UNIT_DP);
    recompui_set_padding_right(export_btn, 12.0f, UNIT_DP);
    recompui_set_tab_index(export_btn, TAB_INDEX_AUTO);
    recompui_register_callback(export_btn, on_export_config_clicked, 0);
}

static void build_setup_view(RecompuiResource parent)
{
    s_setup_view = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_setup_view, DISPLAY_FLEX);
    recompui_set_flex_direction(s_setup_view, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_setup_view, 10.0f, UNIT_DP);
    recompui_set_padding_left(s_setup_view, 18.0f, UNIT_DP);
    recompui_set_padding_right(s_setup_view, 18.0f, UNIT_DP);
    recompui_set_padding_top(s_setup_view, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_setup_view, 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_setup_view, 1.0f);

    build_race_config_share_card(s_setup_view);

    RecompuiResource goal_card = recompui_create_element(s_ctx, s_setup_view);
    recompui_set_display(goal_card, DISPLAY_FLEX);
    recompui_set_flex_direction(goal_card, FLEX_DIRECTION_ROW);
    recompui_set_align_items(goal_card, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(goal_card, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(goal_card, 12.0f, UNIT_DP);
    recompui_set_padding_left(goal_card, 16.0f, UNIT_DP);
    recompui_set_padding_right(goal_card, 12.0f, UNIT_DP);
    recompui_set_padding_top(goal_card, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(goal_card, 10.0f, UNIT_DP);
    recompui_set_min_height(goal_card, 66.0f, UNIT_DP);
    recompui_set_background_color(goal_card, &R_CARD_ALT);
    recompui_set_border_radius(goal_card, 6.0f, UNIT_DP);
    recompui_set_border_width(goal_card, 1.0f, UNIT_DP);
    recompui_set_border_color(goal_card, &R_BORDER);

    RecompuiResource goal_text = recompui_create_element(s_ctx, goal_card);
    recompui_set_display(goal_text, DISPLAY_FLEX);
    recompui_set_flex_direction(goal_text, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(goal_text, 4.0f, UNIT_DP);
    recompui_set_flex_grow(goal_text, 1.0f);

    RecompuiResource goal_title = recompui_create_label(s_ctx, goal_text, "Race End Condition", LABELSTYLE_SMALL);
    recompui_set_color(goal_title, &R_DIM);
    recompui_set_font_size(goal_title, 14.0f, UNIT_DP);

    s_goal_lbl = recompui_create_label(s_ctx, goal_text, "No end condition selected", LABELSTYLE_NORMAL);
    recompui_set_color(s_goal_lbl, &R_WHITE);
    recompui_set_font_size(s_goal_lbl, 18.0f, UNIT_DP);

    RecompuiResource choose_btn = recompui_create_button(s_ctx, goal_card, "Choose Goal", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(choose_btn, CURSOR_POINTER);
    recompui_set_font_size(choose_btn, 13.0f, UNIT_DP);
    recompui_set_height(choose_btn, 36.0f, UNIT_DP);
    recompui_set_width(choose_btn, 140.0f, UNIT_DP);
    recompui_set_padding_top(choose_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(choose_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(choose_btn, 10.0f, UNIT_DP);
    recompui_set_padding_right(choose_btn, 10.0f, UNIT_DP);
    recompui_set_tab_index(choose_btn, TAB_INDEX_AUTO);
    recompui_register_callback(choose_btn, on_choose_goal_clicked, 0);

    RecompuiResource location_card = recompui_create_element(s_ctx, s_setup_view);
    recompui_set_display(location_card, DISPLAY_FLEX);
    recompui_set_flex_direction(location_card, FLEX_DIRECTION_ROW);
    recompui_set_align_items(location_card, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(location_card, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(location_card, 12.0f, UNIT_DP);
    recompui_set_padding_left(location_card, 16.0f, UNIT_DP);
    recompui_set_padding_right(location_card, 12.0f, UNIT_DP);
    recompui_set_padding_top(location_card, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(location_card, 10.0f, UNIT_DP);
    recompui_set_min_height(location_card, 66.0f, UNIT_DP);
    recompui_set_background_color(location_card, &R_CARD_ALT);
    recompui_set_border_radius(location_card, 6.0f, UNIT_DP);
    recompui_set_border_width(location_card, 1.0f, UNIT_DP);
    recompui_set_border_color(location_card, &R_BORDER);

    RecompuiResource location_text = recompui_create_element(s_ctx, location_card);
    recompui_set_display(location_text, DISPLAY_FLEX);
    recompui_set_flex_direction(location_text, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(location_text, 4.0f, UNIT_DP);
    recompui_set_flex_grow(location_text, 1.0f);

    RecompuiResource location_title = recompui_create_label(s_ctx, location_text, "Starting Location", LABELSTYLE_SMALL);
    recompui_set_color(location_title, &R_DIM);
    recompui_set_font_size(location_title, 14.0f, UNIT_DP);

    s_location_lbl = recompui_create_label(s_ctx, location_text, "Goemon's House", LABELSTYLE_NORMAL);
    recompui_set_color(s_location_lbl, &R_WHITE);
    recompui_set_font_size(s_location_lbl, 18.0f, UNIT_DP);

    RecompuiResource choose_location_btn = recompui_create_button(s_ctx, location_card, "Choose Start", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(choose_location_btn, CURSOR_POINTER);
    recompui_set_font_size(choose_location_btn, 13.0f, UNIT_DP);
    recompui_set_height(choose_location_btn, 36.0f, UNIT_DP);
    recompui_set_width(choose_location_btn, 140.0f, UNIT_DP);
    recompui_set_padding_top(choose_location_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(choose_location_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(choose_location_btn, 10.0f, UNIT_DP);
    recompui_set_padding_right(choose_location_btn, 10.0f, UNIT_DP);
    recompui_set_tab_index(choose_location_btn, TAB_INDEX_AUTO);
    recompui_register_callback(choose_location_btn, on_choose_location_clicked, 0);

    RecompuiResource challenges_card = recompui_create_element(s_ctx, s_setup_view);
    recompui_set_display(challenges_card, DISPLAY_FLEX);
    recompui_set_flex_direction(challenges_card, FLEX_DIRECTION_ROW);
    recompui_set_align_items(challenges_card, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(challenges_card, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(challenges_card, 12.0f, UNIT_DP);
    recompui_set_padding_left(challenges_card, 16.0f, UNIT_DP);
    recompui_set_padding_right(challenges_card, 12.0f, UNIT_DP);
    recompui_set_padding_top(challenges_card, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(challenges_card, 10.0f, UNIT_DP);
    recompui_set_min_height(challenges_card, 66.0f, UNIT_DP);
    recompui_set_background_color(challenges_card, &R_CARD_ALT);
    recompui_set_border_radius(challenges_card, 6.0f, UNIT_DP);
    recompui_set_border_width(challenges_card, 1.0f, UNIT_DP);
    recompui_set_border_color(challenges_card, &R_BORDER);

    RecompuiResource challenges_text = recompui_create_element(s_ctx, challenges_card);
    recompui_set_display(challenges_text, DISPLAY_FLEX);
    recompui_set_flex_direction(challenges_text, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(challenges_text, 4.0f, UNIT_DP);
    recompui_set_flex_grow(challenges_text, 1.0f);

    RecompuiResource challenges_title = recompui_create_label(s_ctx, challenges_text, "Challenges", LABELSTYLE_SMALL);
    recompui_set_color(challenges_title, &R_DIM);
    recompui_set_font_size(challenges_title, 14.0f, UNIT_DP);

    s_damage_sync_lbl = recompui_create_label(
        s_ctx, challenges_text, "Challenges: None Enabled", LABELSTYLE_NORMAL);
    recompui_set_color(s_damage_sync_lbl, &R_DIM);
    recompui_set_font_size(s_damage_sync_lbl, 18.0f, UNIT_DP);

    RecompuiResource choose_challenges_btn = recompui_create_button(
        s_ctx, challenges_card, "Open", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(choose_challenges_btn, CURSOR_POINTER);
    recompui_set_font_size(choose_challenges_btn, 13.0f, UNIT_DP);
    recompui_set_height(choose_challenges_btn, 36.0f, UNIT_DP);
    recompui_set_width(choose_challenges_btn, 160.0f, UNIT_DP);
    recompui_set_padding_top(choose_challenges_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(choose_challenges_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(choose_challenges_btn, 10.0f, UNIT_DP);
    recompui_set_padding_right(choose_challenges_btn, 10.0f, UNIT_DP);
    recompui_set_tab_index(choose_challenges_btn, TAB_INDEX_AUTO);
    recompui_register_callback(choose_challenges_btn, on_choose_challenges_clicked, 0);

    RecompuiResource intro = recompui_create_label(
        s_ctx, s_setup_view,
        "Starting flags are applied when the race begins. Pick only what the route should begin with.",
        LABELSTYLE_SMALL);
    recompui_set_color(intro, &R_DIM);
    recompui_set_font_size(intro, 14.0f, UNIT_DP);
    recompui_set_margin_top(intro, 2.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_setup_view);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 3.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    build_category_picker(list);
}

static void build_challenge_toggle_card(RecompuiResource parent,
                                        const char *title_text,
                                        const char *hint_text,
                                        const char *button_text,
                                        int enabled,
                                        RecompuiResource *detail_out,
                                        void (*callback)(RecompuiResource, const RecompuiEventData *, void *))
{
    RecompuiResource card = recompui_create_element(s_ctx, parent);
    recompui_set_display(card, DISPLAY_FLEX);
    recompui_set_flex_direction(card, FLEX_DIRECTION_ROW);
    recompui_set_align_items(card, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(card, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(card, 12.0f, UNIT_DP);
    recompui_set_padding_left(card, 16.0f, UNIT_DP);
    recompui_set_padding_right(card, 12.0f, UNIT_DP);
    recompui_set_padding_top(card, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(card, 12.0f, UNIT_DP);
    recompui_set_min_height(card, 74.0f, UNIT_DP);
    recompui_set_background_color(card, &R_CARD_ALT);
    recompui_set_border_radius(card, 6.0f, UNIT_DP);
    recompui_set_border_width(card, 1.0f, UNIT_DP);
    recompui_set_border_color(card, &R_BORDER);

    RecompuiResource text = recompui_create_element(s_ctx, card);
    recompui_set_display(text, DISPLAY_FLEX);
    recompui_set_flex_direction(text, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(text, 4.0f, UNIT_DP);
    recompui_set_flex_grow(text, 1.0f);

    RecompuiResource title = recompui_create_label(s_ctx, text, title_text, LABELSTYLE_NORMAL);
    recompui_set_color(title, &R_WHITE);
    recompui_set_font_size(title, 18.0f, UNIT_DP);
    recompui_set_font_weight(title, 700);

    RecompuiResource hint = recompui_create_label(s_ctx, text, hint_text, LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    *detail_out = recompui_create_label(s_ctx, text, enabled ? "Enabled" : "Disabled", LABELSTYLE_SMALL);
    recompui_set_color(*detail_out, enabled ? &R_GREEN : &R_DIM);
    recompui_set_font_size(*detail_out, 14.0f, UNIT_DP);
    recompui_set_font_weight(*detail_out, 700);

    RecompuiResource button = recompui_create_button(s_ctx, card, button_text, BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(button, CURSOR_POINTER);
    recompui_set_font_size(button, 13.0f, UNIT_DP);
    recompui_set_height(button, 36.0f, UNIT_DP);
    recompui_set_width(button, 110.0f, UNIT_DP);
    recompui_set_padding_top(button, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(button, 4.0f, UNIT_DP);
    recompui_set_tab_index(button, TAB_INDEX_AUTO);
    recompui_register_callback(button, callback, 0);
}

static void build_challenges_view(RecompuiResource parent)
{
    s_challenges_view = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_challenges_view, DISPLAY_NONE);
    recompui_set_flex_direction(s_challenges_view, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_challenges_view, 12.0f, UNIT_DP);
    recompui_set_padding_left(s_challenges_view, 18.0f, UNIT_DP);
    recompui_set_padding_right(s_challenges_view, 18.0f, UNIT_DP);
    recompui_set_padding_top(s_challenges_view, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_challenges_view, 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_challenges_view, 1.0f);

    RecompuiResource top = recompui_create_element(s_ctx, s_challenges_view);
    recompui_set_display(top, DISPLAY_FLEX);
    recompui_set_flex_direction(top, FLEX_DIRECTION_ROW);
    recompui_set_align_items(top, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(top, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(top, 12.0f, UNIT_DP);

    RecompuiResource label = recompui_create_label(s_ctx, top, "Challenges", LABELSTYLE_NORMAL);
    recompui_set_color(label, &R_WHITE);
    recompui_set_font_weight(label, 700);
    recompui_set_font_size(label, 20.0f, UNIT_DP);

    RecompuiResource done_btn = recompui_create_button(s_ctx, top, "Done", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(done_btn, CURSOR_POINTER);
    recompui_set_font_size(done_btn, 13.0f, UNIT_DP);
    recompui_set_height(done_btn, 36.0f, UNIT_DP);
    recompui_set_width(done_btn, 110.0f, UNIT_DP);
    recompui_set_padding_top(done_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(done_btn, 4.0f, UNIT_DP);
    recompui_set_tab_index(done_btn, TAB_INDEX_AUTO);
    recompui_register_callback(done_btn, on_challenges_done_clicked, 0);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_challenges_view,
        "Optional race rules that apply to every client when the race starts.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    build_challenge_toggle_card(s_challenges_view,
                                "Team Damage Sync",
                                "Sync player damage as part of the race challenge.",
                                "Toggle",
                                s_damage_sync_enabled,
                                &s_damage_sync_detail_lbl,
                                on_damage_sync_clicked);
    build_challenge_toggle_card(s_challenges_view,
                                "No Hit",
                                "Any damage immediately drops the active player's health to zero.",
                                "Toggle",
                                s_no_hit_enabled,
                                &s_no_hit_detail_lbl,
                                on_no_hit_clicked);
    build_challenge_toggle_card(s_challenges_view,
                                "1 Life",
                                "Keep the player at one internal life so the next death is game over.",
                                "Toggle",
                                s_one_life_enabled,
                                &s_one_life_detail_lbl,
                                on_one_life_clicked);
    build_challenge_toggle_card(s_challenges_view,
                                "Ryo Sync",
                                "Sync both ryo earned and ryo spent between teammates during the race.",
                                "Toggle",
                                s_ryo_sync_enabled,
                                &s_ryo_sync_detail_lbl,
                                on_ryo_sync_clicked);
    build_challenge_toggle_card(s_challenges_view,
                                "Enemy Multiplier",
                                "Experimental. Choose how many copies of each enemy actor spawn when a room loads; the game could crash.",
                                "Cycle",
                                s_enemy_multiplier > 1,
                                &s_enemy_multiplier_detail_lbl,
                                on_enemy_multiplier_clicked);
}

static void build_goal_view(RecompuiResource parent)
{
    s_goal_view = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_goal_view, DISPLAY_NONE);
    recompui_set_flex_direction(s_goal_view, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_goal_view, 10.0f, UNIT_DP);
    recompui_set_padding_left(s_goal_view, 18.0f, UNIT_DP);
    recompui_set_padding_right(s_goal_view, 18.0f, UNIT_DP);
    recompui_set_padding_top(s_goal_view, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_goal_view, 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_goal_view, 1.0f);

    RecompuiResource top = recompui_create_element(s_ctx, s_goal_view);
    recompui_set_display(top, DISPLAY_FLEX);
    recompui_set_flex_direction(top, FLEX_DIRECTION_ROW);
    recompui_set_align_items(top, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(top, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(top, 12.0f, UNIT_DP);

    RecompuiResource label = recompui_create_label(s_ctx, top, "Choose Race End Condition", LABELSTYLE_NORMAL);
    recompui_set_color(label, &R_WHITE);
    recompui_set_font_weight(label, 700);
    recompui_set_font_size(label, 20.0f, UNIT_DP);

    RecompuiResource done_btn = recompui_create_button(s_ctx, top, "Done", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(done_btn, CURSOR_POINTER);
    recompui_set_font_size(done_btn, 13.0f, UNIT_DP);
    recompui_set_height(done_btn, 36.0f, UNIT_DP);
    recompui_set_width(done_btn, 110.0f, UNIT_DP);
    recompui_set_padding_top(done_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(done_btn, 4.0f, UNIT_DP);
    recompui_set_tab_index(done_btn, TAB_INDEX_AUTO);
    recompui_register_callback(done_btn, on_goal_done_clicked, 0);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_goal_view,
        "Any synced multiplayer flag can be used as the finish condition.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_goal_view);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 3.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    build_goal_category_picker(list);
}

static void build_location_view(RecompuiResource parent)
{
    RecompuiResource row = RECOMPUI_NULL_RESOURCE;
    int cols = 0;
    int i;

    s_location_view = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_location_view, DISPLAY_NONE);
    recompui_set_flex_direction(s_location_view, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_location_view, 10.0f, UNIT_DP);
    recompui_set_padding_left(s_location_view, 18.0f, UNIT_DP);
    recompui_set_padding_right(s_location_view, 18.0f, UNIT_DP);
    recompui_set_padding_top(s_location_view, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_location_view, 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_location_view, 1.0f);

    RecompuiResource top = recompui_create_element(s_ctx, s_location_view);
    recompui_set_display(top, DISPLAY_FLEX);
    recompui_set_flex_direction(top, FLEX_DIRECTION_ROW);
    recompui_set_align_items(top, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(top, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(top, 12.0f, UNIT_DP);

    RecompuiResource label = recompui_create_label(s_ctx, top, "Choose Starting Location", LABELSTYLE_NORMAL);
    recompui_set_color(label, &R_WHITE);
    recompui_set_font_weight(label, 700);
    recompui_set_font_size(label, 20.0f, UNIT_DP);

    RecompuiResource done_btn = recompui_create_button(s_ctx, top, "Done", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(done_btn, CURSOR_POINTER);
    recompui_set_font_size(done_btn, 13.0f, UNIT_DP);
    recompui_set_height(done_btn, 36.0f, UNIT_DP);
    recompui_set_width(done_btn, 110.0f, UNIT_DP);
    recompui_set_padding_top(done_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(done_btn, 4.0f, UNIT_DP);
    recompui_set_tab_index(done_btn, TAB_INDEX_AUTO);
    recompui_register_callback(done_btn, on_location_done_clicked, 0);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_location_view,
        "Only one start can be active. The race launch will skip the original intro and spawn here.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_location_view);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 4.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    for (i = 0; i < s_start_location_count && i < RACE_MAX_LOCATIONS; i++)
    {
        if (row == RECOMPUI_NULL_RESOURCE || cols >= 2)
        {
            row = make_grid_row(list);
            cols = 0;
        }
        add_location_button(row, i);
        cols++;
    }
}

static void race_ui_init(void)
{
    int i;

    if (s_ui_built)
        return;
    s_ui_built = 1;

    for (i = 0; i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        s_goal_btns[i] = RECOMPUI_NULL_RESOURCE;
        s_start_btns[i] = RECOMPUI_NULL_RESOURCE;
        s_start_selected[i] = is_default_start_flag(entry);
        s_goal_btn_text[i][0] = '\0';
        s_start_btn_text[i][0] = '\0';
    }
    for (i = 0; i < RACE_MAX_LOCATIONS; i++)
    {
        s_location_btns[i] = RECOMPUI_NULL_RESOURCE;
        s_location_btn_text[i][0] = '\0';
    }
    for (i = 0; i < RACE_MAX_CATEGORIES; i++)
    {
        s_category_views[i] = RECOMPUI_NULL_RESOURCE;
        s_goal_category_views[i] = RECOMPUI_NULL_RESOURCE;
        s_category_count_lbls[i] = RECOMPUI_NULL_RESOURCE;
        s_goal_category_count_lbls[i] = RECOMPUI_NULL_RESOURCE;
        s_category_count_text[i][0] = '\0';
        s_goal_category_count_text[i][0] = '\0';
        s_category_names[i] = 0;
        s_category_start[i] = 0;
        s_category_end[i] = 0;
    }

    s_flag_count = anchor_flag_catalog_count();
    if (s_flag_count > RACE_MAX_FLAGS)
        s_flag_count = RACE_MAX_FLAGS;
    build_category_index();
    s_goal_idx = find_flag_index_by_key("fl_congo");
    s_damage_sync_enabled = 0;
    s_no_hit_enabled = 0;
    s_one_life_enabled = 0;
    s_ryo_sync_enabled = 0;
    s_enemy_multiplier = 1;
    apply_race_challenge_runtime();

    s_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_ctx, 1);
    recompui_set_context_captures_mouse(s_ctx, 1);

    recompui_open_context(s_ctx);
    {
        RecompuiResource root = recompui_context_root(s_ctx);

        RecompuiResource overlay = recompui_create_element(s_ctx, root);
        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &R_BG);

        RecompuiResource panel = recompui_create_element(s_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 5.0f, UNIT_PERCENT);
        recompui_set_top(panel, 4.0f, UNIT_PERCENT);
        recompui_set_width(panel, 90.0f, UNIT_PERCENT);
        recompui_set_height(panel, 92.0f, UNIT_PERCENT);
        recompui_set_background_color(panel, &R_PANEL);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &R_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        RecompuiResource header = recompui_create_element(s_ctx, panel);
        recompui_set_display(header, DISPLAY_FLEX);
        recompui_set_flex_direction(header, FLEX_DIRECTION_ROW);
        recompui_set_align_items(header, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(header, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_padding_left(header, 22.0f, UNIT_DP);
        recompui_set_padding_right(header, 12.0f, UNIT_DP);
        recompui_set_padding_top(header, 14.0f, UNIT_DP);
        recompui_set_padding_bottom(header, 14.0f, UNIT_DP);
        recompui_set_background_color(header, &R_HEAD);
        recompui_set_border_bottom_width(header, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(header, &R_BORDER);

        RecompuiResource title = recompui_create_label(s_ctx, header, "Race Setup", LABELSTYLE_NORMAL);
        recompui_set_color(title, &R_PURPLE);
        recompui_set_font_weight(title, 700);
        recompui_set_font_size(title, 26.0f, UNIT_DP);

        RecompuiResource back_btn = recompui_create_button(s_ctx, header, "Back", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(back_btn, CURSOR_POINTER);
        recompui_set_font_size(back_btn, 13.0f, UNIT_DP);
        recompui_set_height(back_btn, 34.0f, UNIT_DP);
        recompui_set_width(back_btn, 96.0f, UNIT_DP);
        recompui_set_padding_top(back_btn, 4.0f, UNIT_DP);
        recompui_set_padding_bottom(back_btn, 4.0f, UNIT_DP);
        recompui_set_tab_index(back_btn, TAB_INDEX_AUTO);
        recompui_register_callback(back_btn, on_back_clicked, 0);

        build_setup_view(panel);
        build_goal_view(panel);
        build_location_view(panel);
        build_challenges_view(panel);
        for (i = 0; i < s_category_count; i++)
        {
            build_category_detail_view(panel, i);
            build_goal_category_detail_view(panel, i);
        }

        RecompuiResource footer = recompui_create_element(s_ctx, panel);
        recompui_set_display(footer, DISPLAY_FLEX);
        recompui_set_flex_direction(footer, FLEX_DIRECTION_ROW);
        recompui_set_align_items(footer, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(footer, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_gap(footer, 12.0f, UNIT_DP);
        recompui_set_padding_left(footer, 16.0f, UNIT_DP);
        recompui_set_padding_right(footer, 12.0f, UNIT_DP);
        recompui_set_padding_top(footer, 10.0f, UNIT_DP);
        recompui_set_padding_bottom(footer, 10.0f, UNIT_DP);
        recompui_set_border_top_width(footer, 1.0f, UNIT_DP);
        recompui_set_border_top_color(footer, &R_BORDER);

        s_status_lbl = recompui_create_label(
            s_ctx, footer,
            "Configure the race, then return to the lobby.",
            LABELSTYLE_SMALL);
        recompui_set_color(s_status_lbl, &R_DIM);
        recompui_set_font_size(s_status_lbl, 14.0f, UNIT_DP);
    }
    recompui_close_context(s_ctx);

    for (i = 0; i < s_category_count; i++)
    {
        update_category_count(i);
        update_goal_category_count(i);
    }
    update_goal_buttons(-1, s_goal_idx);
    update_goal_label();
    update_damage_sync_label();
}

const char *anchor_race_get_config_json(void)
{
    int i;
    int pos = 0;
    const AnchorFlagEntry *goal = anchor_flag_catalog_get(s_goal_idx);
    int wrote_flag = 0;

    s_config_json[0] = '\0';
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), "{\"g\":\"");
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json),
                        goal && goal->key ? goal->key : "");
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), "\",\"loc\":");
    append_uint(s_config_json, &pos, (int)sizeof(s_config_json), s_location_idx);
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), ",\"ch\":{\"dmg\":");
    append_uint(s_config_json, &pos, (int)sizeof(s_config_json), s_damage_sync_enabled ? 1 : 0);
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), ",\"nh\":");
    append_uint(s_config_json, &pos, (int)sizeof(s_config_json), s_no_hit_enabled ? 1 : 0);
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), ",\"ol\":");
    append_uint(s_config_json, &pos, (int)sizeof(s_config_json), s_one_life_enabled ? 1 : 0);
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), ",\"ryo\":");
    append_uint(s_config_json, &pos, (int)sizeof(s_config_json), s_ryo_sync_enabled ? 1 : 0);
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), ",\"em\":");
    append_uint(s_config_json, &pos, (int)sizeof(s_config_json), s_enemy_multiplier);
    append_char_limited(s_config_json, &pos, (int)sizeof(s_config_json), '}');
    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), ",\"flags\":[");

    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!s_start_selected[i] || !entry || !entry->key)
            continue;
        if (wrote_flag)
            append_char_limited(s_config_json, &pos, (int)sizeof(s_config_json), ',');
        append_char_limited(s_config_json, &pos, (int)sizeof(s_config_json), '"');
        append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), entry->key);
        append_char_limited(s_config_json, &pos, (int)sizeof(s_config_json), '"');
        wrote_flag = 1;
    }

    append_text_limited(s_config_json, &pos, (int)sizeof(s_config_json), "]}");
    s_config_json[pos] = '\0';
    return s_config_json;
}

void anchor_race_apply_config_json(const char *json)
{
    int i;
    int old_goal = s_goal_idx;
    int old_location = s_location_idx;
    char key[RACE_TEXT_LEN];
    const char *flags;

    if (!json || !json[0])
        return;

    if (parse_json_string_after(json, "\"g\"", key, RACE_TEXT_LEN))
    {
        int idx = find_flag_index_by_key(key);
        if (idx >= 0)
            s_goal_idx = idx;
    }

    s_location_idx = parse_json_int_after(json, "\"loc\"", s_location_idx);
    if (s_location_idx < 0 || s_location_idx >= s_start_location_count)
        s_location_idx = 0;

    s_damage_sync_enabled = parse_json_int_after(json, "\"dmg\"", s_damage_sync_enabled) ? 1 : 0;
    s_no_hit_enabled = parse_json_int_after(json, "\"nh\"", s_no_hit_enabled) ? 1 : 0;
    s_one_life_enabled = parse_json_int_after(json, "\"ol\"", s_one_life_enabled) ? 1 : 0;
    /* Older host configs do not contain this challenge; keep it off rather
     * than inheriting a stale local setup choice. */
    s_ryo_sync_enabled = parse_json_int_after(json, "\"ryo\"", 0) ? 1 : 0;
    s_enemy_multiplier = parse_json_int_after(json, "\"em\"", s_enemy_multiplier);
    if (s_enemy_multiplier < 1 || s_enemy_multiplier > 3)
        s_enemy_multiplier = parse_json_int_after(json, "\"de\"", 0) ? 2 : 1;
    apply_race_challenge_runtime();

    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
        s_start_selected[i] = 0;

    flags = find_text(json, "\"flags\"");
    if (flags)
    {
        flags = find_text(flags, "[");
        while (flags && *flags && *flags != ']')
        {
            if (*flags == '"')
            {
                int out = 0;
                flags++;
                while (*flags && *flags != '"' && out < RACE_TEXT_LEN - 1)
                    key[out++] = *flags++;
                key[out] = '\0';
                i = find_flag_index_by_key(key);
                if (i >= 0 && i < RACE_MAX_FLAGS)
                    s_start_selected[i] = 1;
            }
            if (*flags)
                flags++;
        }
    }

    if (s_ui_built)
    {
        for (i = 0; i < s_category_count; i++)
        {
            update_category_count(i);
            update_goal_category_count(i);
        }
        update_goal_buttons(old_goal, s_goal_idx);
        update_location_buttons(old_location, s_location_idx);
        update_goal_label();
        update_location_label();
        update_damage_sync_label();
    }
}

static void finish_race_with_payload(const char *payload_json, int local_finish)
{
    char team[96];
    char players[384];
    int victory;

    if (s_race_finished)
        return;

    parse_json_string_after(payload_json, "\"team\"", team, (int)sizeof(team));
    parse_json_string_after(payload_json, "\"players\"", players, (int)sizeof(players));

    victory = local_finish || local_team_matches(team);
    s_race_finished = 1;
    s_race_started = 0;
    s_race_apply_pending = 0;
    s_race_character_enforce_frames = 0;

    if (local_finish)
    {
        anchor_send_custom_packet("MNSG_RACE_FINISH", payload_json ? payload_json : "{}", "", 0, 1);
        anchor_send_game_complete();
    }
    anchor_set_race_lobby_state("finished", anchor_race_get_config_json());
    show_race_result(victory, team, players);

    recomp_printf("[Race] Race finished. Winning team='%s' players='%s'\n",
                  team[0] ? team : "default", players[0] ? players : "Player");
}

void anchor_race_on_finish_packet(const char *packet_json)
{
    if (!s_race_started || !packet_json || !packet_json[0])
        return;

    finish_race_with_payload(packet_json, 0);
}

static int race_flag_is_goal(const char *flag_name, int flag_value)
{
    const AnchorFlagEntry *goal;

    if (!s_race_started || s_race_finished || !flag_name || flag_value == 0)
        return 0;

    goal = anchor_flag_catalog_get(s_goal_idx);
    if (!goal || !goal->key || !race_key_equals(goal->key, flag_name))
        return 0;

    return 1;
}

void anchor_race_on_flag_synced(const char *flag_name, int flag_value)
{
    char *payload;

    if (!race_flag_is_goal(flag_name, flag_value))
        return;

    payload = anchor_get_race_finish_payload_json();
    finish_race_with_payload(payload ? payload : "{}", 1);
    if (payload)
        recomp_free(payload);
}

void anchor_race_on_remote_flag_synced(const char *flag_name, int flag_value)
{
    char *payload;

    if (!race_flag_is_goal(flag_name, flag_value))
        return;

    payload = anchor_get_race_finish_payload_json();
    finish_race_with_payload(payload ? payload : "{}", 0);
    if (payload)
        recomp_free(payload);
}

int anchor_race_start_from_lobby(void)
{
    int i;

    if (s_goal_idx < 0 || !anchor_flag_catalog_get(s_goal_idx) ||
        !anchor_flag_catalog_get(s_goal_idx)->key)
    {
        set_status("Select a race end condition first.", 0);
        return 0;
    }
    if (s_location_idx < 0 || s_location_idx >= s_start_location_count)
    {
        set_status("Select a starting location first.", 0);
        return 0;
    }
    if (!race_has_start_character())
    {
        set_status("Select at least one starting character.", 0);
        return 0;
    }

    s_race_pending_count = 0;
    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!s_start_selected[i] || !entry || !entry->key)
            continue;

        s_race_pending_keys[s_race_pending_count] = entry->key;
        s_race_pending_values[s_race_pending_count] =
            entry->force_val != 0 ? entry->force_val : 1;
        s_race_pending_count++;
    }

    s_race_apply_pending = 1;
    s_race_character_enforce_frames = RACE_CHARACTER_ENFORCE_FRAMES;
    s_race_started = 1;
    s_race_finished = 0;
    s_result_visible = 0;
    anchor_startup_menu_finish();
    if (s_visible)
    {
        recompui_hide_context(s_ctx);
        s_visible = 0;
    }
    launch_race_save_directly();
    return 1;
}

void anchor_startup_race_open(void)
{
    s_pending_open = 1;
}

RECOMP_CALLBACK("*", recomp_on_init)
void anchor_startup_race_on_init(void)
{
    /* Recomp UI init callback. Build the race setup resources as early as the
     * recomp runtime allows so the startup/lobby flow can open the race menu
     * without waiting for a game-frame hook. */
    race_ui_init();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_startup_race_frame_hook(void)
{
    /*
     * Main per-frame hook. func_80002040 is the game's step dispatcher, called
     * once per game frame from game_thread_entrypoint. This hook keeps all race
     * UI work on the game thread:
     *
     * - lazily initializes Recomp UI if recomp_on_init ran too early,
     * - hides the setup context after the startup menu has handed control to
     *   gameplay,
     * - applies pending race flags once save memory is live,
     * - enforces selected starting characters for a short window while the game
     *   is still rebuilding player/save state,
     * - drains button/input pending flags produced by Recomp UI event handlers.
     */
    if (!s_initialized)
    {
        s_initialized = 1;
        race_ui_init();
    }

    if (anchor_startup_menu_is_complete())
    {
        if (s_visible)
        {
            recompui_hide_context(s_ctx);
            s_visible = 0;
        }
        apply_pending_race_flags();
        if (s_race_character_enforce_frames > 0 && item_sync_save_is_loaded())
        {
            apply_race_character_selection(0);
            anchor_set_current_character_if_needed();
            s_race_character_enforce_frames--;
        }
        return;
    }

    if (s_pending_open)
    {
        s_pending_open = 0;
        if (!s_visible)
        {
            s_active_category = -1;
            s_active_goal_category = -1;
            set_screen(0);
            update_goal_label();
            update_location_label();
            recompui_show_context(s_ctx);
            s_visible = 1;
        }
    }

    if (s_pending_choose_goal)
    {
        s_pending_choose_goal = 0;
        set_screen(1);
    }

    if (s_pending_choose_location)
    {
        s_pending_choose_location = 0;
        set_screen(4);
    }

    if (s_pending_choose_challenges)
    {
        s_pending_choose_challenges = 0;
        set_screen(5);
    }

    if (s_pending_export_config)
    {
        s_pending_export_config = 0;
        build_race_config_code(s_config_code, (int)sizeof(s_config_code));
        if (anchor_set_clipboard_text(s_config_code))
            set_status("Race config code copied to clipboard.", 1);
        else
            set_status("Could not access clipboard. The live code is shown above.", 0);
    }

    if (s_pending_paste_config)
    {
        char *code;

        s_pending_paste_config = 0;
        code = anchor_get_clipboard_text();
        recompui_open_context(s_ctx);
        if (s_import_input != RECOMPUI_NULL_RESOURCE)
            recompui_set_input_text(s_import_input, code ? code : "");
        recompui_close_context(s_ctx);

        if (code && code[0])
        {
            if (apply_race_config_code(code))
                set_status("Race config pasted and imported.", 1);
            else
                set_status("Clipboard does not contain a valid race config code.", 0);
        }
        else
        {
            set_status("Clipboard is empty or unavailable.", 0);
        }

        if (code)
            recomp_free(code);
    }

    if (s_pending_import_config)
    {
        char *code;

        s_pending_import_config = 0;
        recompui_open_context(s_ctx);
        code = s_import_input != RECOMPUI_NULL_RESOURCE ? recompui_get_input_text(s_import_input) : 0;
        recompui_close_context(s_ctx);

        if (apply_race_config_code(code))
            set_status("Race config imported.", 1);
        else
            set_status("Invalid race config code.", 0);

        if (code)
            recomp_free(code);
    }

    if (s_pending_goal_done)
    {
        s_pending_goal_done = 0;
        s_active_goal_category = -1;
        set_screen(0);
        update_goal_label();
    }

    if (s_pending_location_done)
    {
        s_pending_location_done = 0;
        set_screen(0);
        update_location_label();
    }

    if (s_pending_challenges_done)
    {
        s_pending_challenges_done = 0;
        set_screen(0);
        update_damage_sync_label();
    }

    if (s_pending_category_idx >= 0)
    {
        s_active_category = s_pending_category_idx;
        s_pending_category_idx = -1;
        set_screen(2);
    }

    if (s_pending_goal_category_idx >= 0)
    {
        s_active_goal_category = s_pending_goal_category_idx;
        s_pending_goal_category_idx = -1;
        set_screen(3);
    }

    if (s_pending_goal_idx >= 0)
    {
        int old_goal = s_goal_idx;
        int old_cat = category_for_flag(old_goal);
        int new_cat;
        s_goal_idx = s_pending_goal_idx;
        s_pending_goal_idx = -1;
        new_cat = category_for_flag(s_goal_idx);
        update_goal_buttons(old_goal, s_goal_idx);
        update_goal_category_count(old_cat);
        update_goal_category_count(new_cat);
        set_status("Race end condition selected.", 1);
    }

    if (s_pending_location_idx >= 0)
    {
        int old_location = s_location_idx;
        s_location_idx = s_pending_location_idx;
        s_pending_location_idx = -1;
        update_location_buttons(old_location, s_location_idx);
        update_location_label();
        set_status("Starting location selected.", 1);
    }

    if (s_pending_toggle_idx >= 0)
    {
        int idx = s_pending_toggle_idx;
        s_pending_toggle_idx = -1;
        if (idx >= 0 && idx < s_flag_count && idx < RACE_MAX_FLAGS)
        {
            s_start_selected[idx] = !s_start_selected[idx];
            update_start_button(idx);
            set_status(s_start_selected[idx] ? "Starting flag added." : "Starting flag removed.", 1);
        }
    }

    if (s_pending_toggle_damage_sync)
    {
        s_pending_toggle_damage_sync = 0;
        s_damage_sync_enabled = !s_damage_sync_enabled;
        update_damage_sync_label();
        set_status(s_damage_sync_enabled ? "Team damage sync enabled." : "Team damage sync disabled.", 1);
    }

    if (s_pending_toggle_no_hit)
    {
        s_pending_toggle_no_hit = 0;
        s_no_hit_enabled = !s_no_hit_enabled;
        update_damage_sync_label();
        set_status(s_no_hit_enabled ? "No hit enabled." : "No hit disabled.", 1);
    }

    if (s_pending_toggle_one_life)
    {
        s_pending_toggle_one_life = 0;
        s_one_life_enabled = !s_one_life_enabled;
        update_damage_sync_label();
        set_status(s_one_life_enabled ? "1 life enabled." : "1 life disabled.", 1);
    }

    if (s_pending_toggle_ryo_sync)
    {
        s_pending_toggle_ryo_sync = 0;
        s_ryo_sync_enabled = !s_ryo_sync_enabled;
        update_damage_sync_label();
        set_status(s_ryo_sync_enabled ? "Ryo sync enabled." : "Ryo sync disabled.", 1);
    }

    if (s_pending_cycle_enemy_multiplier)
    {
        s_pending_cycle_enemy_multiplier = 0;
        s_enemy_multiplier++;
        if (s_enemy_multiplier > 3)
            s_enemy_multiplier = 1;
        update_damage_sync_label();
        if (s_enemy_multiplier == 1)
            set_status("Enemy multiplier set to 1x.", 1);
        else if (s_enemy_multiplier == 2)
            set_status("Enemy multiplier set to 2x.", 1);
        else
            set_status("Enemy multiplier set to 3x.", 1);
    }

    if (s_pending_back)
    {
        s_pending_back = 0;
        if (s_screen == 3)
        {
            s_active_goal_category = -1;
            set_screen(1);
        }
        else if (s_screen == 1 || s_screen == 2 || s_screen == 4 || s_screen == 5)
        {
            s_active_category = -1;
            s_active_goal_category = -1;
            set_screen(0);
            update_goal_label();
            update_location_label();
            update_damage_sync_label();
        }
        else
        {
            if (s_visible)
            {
                recompui_hide_context(s_ctx);
                s_visible = 0;
            }
            anchor_race_lobby_open();
        }
    }
}

RECOMP_HOOK_RETURN("func_8000B640_C240")
void anchor_race_save_start_hook(void)
{
    /*
     * Fresh-save return hook. func_8000B640 has just cleared and seeded
     * D_8015C608. Applying flags here is harmless but usually deferred because
     * item_sync_save_is_loaded() will not report live save memory until
     * func_8000B5D0 mirrors the save/control state.
     */
    apply_pending_race_flags();
}

RECOMP_HOOK_RETURN("func_8000B5D0_C1D0")
void anchor_race_file_started_hook(void)
{
    /*
     * Save-start return hook. This is the first reliable point where save
     * memory is considered loaded by item_sync. It covers both direct race
     * launch and any original game path that starts a file while a race setup is
     * pending.
     */
    apply_pending_race_flags();
}

RECOMP_HOOK("func_8000B2A0_BEA0")
void anchor_race_spawn_read_hook(void)
{
    /*
     * Spawn-read pre-hook. The original function copies spawn room/position and
     * rotation from D_8015C608 into g_system destination fields. Race startup
     * rewrites those save fields immediately before the copy so later original
     * game logic consumes the selected race start location.
     */
    if (s_race_apply_pending && item_sync_save_is_loaded())
    {
        apply_race_character_selection(0);
        apply_race_start_location();
    }
}

int anchor_race_is_active(void)
{
    return s_race_started;
}
