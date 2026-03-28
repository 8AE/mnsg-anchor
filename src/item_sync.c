/**
 * @file item_sync.c
 * @brief Randomizer item / flag synchronisation for Anchor co-op.
 *
 * Every game frame (RECOMP_HOOK_RETURN on func_80002040_2C40) this module:
 *   1. Drains the Anchor packet queue, applying any incoming SET_FLAG and
 *      REQUEST_TEAM_STATE packets to the local save data.
 *   2. Compares the current save data against a cached snapshot; whenever
 *      a tracked item is newly gained, broadcasts a SET_FLAG packet with
 *      addToQueue=1 so offline teammates receive it upon joining.
 *   3. On first connect with a loaded save, schedules a one-item-per-frame
 *      "full push" that refreshes every non-zero value in the server queue.
 *
 * When a REQUEST_TEAM_STATE packet is received, the same full-push sequence
 * is restarted so the requesting player's client can absorb the team's
 * collective progress.
 *
 * ── Data sources ────────────────────────────────────────────────────────
 * Offsets and flag IDs are sourced from the MNSGRecompRando mod:
 *   https://github.com/Killklli/MNSGRecompRando
 *   include/save_data_tool.h  – save offsets and flag bit definitions
 *   apworld/Items.py          – item classification
 *   apworld/Logic/            – per-location flag_id assignments
 *
 * ── Layout of D_8015C608_15D208[] ───────────────────────────────────────
 *   [0x000 .. 0x303]  packed 1-bit flags  (flag_id / 8 = byte, flag_id % 8 = bit)
 *   Positive offsets  32-bit item fields  (READ_SAVE32 / WRITE_SAVE32)
 *   Negative offsets  stats struct before the array base (e.g. hp_max at -0x28)
 */

#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"

/* =========================================================================
   Game save-data symbol (datasyms VRAM 0x8015C608)
   ========================================================================= */
extern unsigned char D_8015C608_15D208[];

/* ─── Helpers ──────────────────────────────────────────────────────────── */

/* Read / write a signed 32-bit value at byte offset `off` from the base.
 * `off` may be negative (accesses the stats fields below the flag array). */
#define SAVE_READ32(off) (*(signed int *)((char *)D_8015C608_15D208 + (off)))
#define SAVE_WRITE32(off, val) (*(signed int *)((char *)D_8015C608_15D208 + (off)) = (signed int)(val))

/* Packed flag-bit helpers (flag IDs up to 0x17FF fit in the 0x304-byte array). */
#define FLAG_IS_SET(id) ((D_8015C608_15D208[(unsigned)(id) >> 3] >> ((id) & 7u)) & 1u)
#define FLAG_SET_BIT(id) (D_8015C608_15D208[(unsigned)(id) >> 3] |= (unsigned char)(1u << ((id) & 7u)))

/* True when a save file is loaded (hp_max lives just before the flag array). */
#define SAVE_HP_MAX_OFFSET (-0x28) /* SAVE_TOTAL_HEALTH – see save_data_tool.h */
static int save_is_loaded(void)
{
    return SAVE_READ32(SAVE_HP_MAX_OFFSET) > 0;
}

/* =========================================================================
   Minimal string / JSON helpers  (no <string.h> / <stdio.h> required)
   ========================================================================= */

/** Locate the first occurrence of `needle` in `hay`; NULL if not found. */
static const char *sfind(const char *hay, const char *needle)
{
    if (!needle || !*needle)
        return hay;
    for (; *hay; ++hay)
    {
        const char *h = hay, *n = needle;
        while (*n && *h == *n)
        {
            ++h;
            ++n;
        }
        if (!*n)
            return hay;
    }
    return 0;
}

/**
 * Parse a leading decimal integer (with optional minus sign) pointed to by
 * *sp.  Advances *sp past the digits.  Returns 1 on success, 0 on failure.
 */
static int parse_int(const char **sp, signed int *out)
{
    const char *p = *sp;
    signed int sign = 1, v = 0;
    if (*p == '-')
    {
        sign = -1;
        ++p;
    }
    if (*p < '0' || *p > '9')
        return 0;
    while (*p >= '0' && *p <= '9')
        v = v * 10 + (*p++ - '0');
    *sp = p;
    *out = sign * v;
    return 1;
}

/**
 * Check whether the packet type field equals `target`.
 * Matches: "type":"TARGET"
 */
static int is_packet_type(const char *json, const char *target)
{
    const char *pos = sfind(json, "\"type\":\"");
    if (!pos)
        return 0;
    pos += 8;
    const char *t = target;
    while (*t && *pos == *t)
    {
        ++pos;
        ++t;
    }
    return !*t && *pos == '"';
}

/**
 * Extract the string value of `"flag":"..."` into buf[buf_len].
 * Returns 1 on success.
 */
static int get_flag_name(const char *json, char *buf, int buf_len)
{
    const char *pos = sfind(json, "\"flag\":\"");
    if (!pos)
        return 0;
    pos += 8;
    int i = 0;
    while (*pos && *pos != '"' && i < buf_len - 1)
        buf[i++] = *pos++;
    buf[i] = '\0';
    return i > 0;
}

/**
 * Extract the integer value of `"value":N` from a JSON packet string.
 * Returns 1 on success.
 */
static int get_flag_value(const char *json, signed int *out)
{
    const char *pos = sfind(json, "\"value\":");
    if (!pos)
        return 0;
    pos += 8;
    while (*pos == ' ')
        ++pos;
    return parse_int(&pos, out);
}

/**
 * Compare two null-terminated strings for equality.
 * Returns 1 if equal, 0 otherwise.
 */
static int streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b)
    {
        ++a;
        ++b;
    }
    return *a == *b;
}

/* =========================================================================
   Sync tables – 32-bit save-data fields
   ─────────────────────────────────────────────────────────────────────────
   off      : byte offset into D_8015C608_15D208 (may be negative)
   cached   : last-sent value
   use_max  : 1 = apply incoming if strictly greater (counts / stats)
              0 = apply if local is zero / unset (one-shot acquisitions)
   name     : key used in SET_FLAG packets (max 15 chars to stay compact)
   ========================================================================= */
typedef struct
{
    int off;
    signed int cached;
    int use_max;
    const char *name;
} SyncField;

static SyncField s_fields[] = {
    /* ── Characters ────────────────────────────────────────────────── */
    /* SAVE_GOEMON_RECRUITED / EBISUMARU / SASUKE / YAE (save_data_tool.h) */
    {0x094, 0, 0, "chr_goemon"},
    {0x098, 0, 0, "chr_ebisu"},
    {0x09C, 0, 0, "chr_sasuke"},
    {0x0A0, 0, 0, "chr_yae"},

    /* ── Equipment / weapons ────────────────────────────────────────── */
    /* SAVE_CHAIN_PIPE / MEATSAW_HAMMER / FIRECRACKER / FLUTE /
       WINDUP_CAMERA / ICE_KUNAI / BAZOOKA / FIRE_RYO (Medal of Flames) */
    {0x0B4, 0, 0, "eq_chain"},
    {0x0B8, 0, 0, "eq_hammer"},
    {0x0BC, 0, 0, "eq_firecrk"},
    {0x0C0, 0, 0, "eq_flute"},
    {0x0C8, 0, 0, "eq_camera"},
    {0x0CC, 0, 0, "eq_kunai"},
    {0x0D0, 0, 0, "eq_bazooka"},
    {0x0D4, 0, 0, "eq_fire_ryo"},

    /* ── Abilities / magic ──────────────────────────────────────────── */
    /* SAVE_SUDDEN_IMPACT_MAGIC / MINI_EBISU_MAGIC / SUPER_JUMP_MAGIC /
       MERMAID_MAGIC                                                     */
    {0x0E4, 0, 0, "ab_impact"},
    {0x0E8, 0, 0, "ab_mini_ebi"},
    {0x0EC, 0, 0, "ab_jetpack"},
    {0x0F0, 0, 0, "ab_mermaid"},

    /* ── Quest / key items ──────────────────────────────────────────── */
    /* SAVE_TRITON_SHELL / SUPER_PASS / ACHILLES_HEEL / QUALITY_CUCUMBER */
    {0x0F4, 0, 0, "ki_triton"},
    {0x0F8, 0, 0, "ki_superps"},
    {0x104, 0, 0, "ki_achilles"},
    {0x10C, 0, 0, "ki_cucumber"},

    /* ── Miracle items ──────────────────────────────────────────────── */
    /* SAVE_MIRACLE_STAR / MIRACLE_MOON / MIRACLE_FLOWER / MIRACLE_SNOW  */
    {0x250, 0, 0, "mi_star"},
    {0x254, 0, 0, "mi_moon"},
    {0x258, 0, 0, "mi_flower"},
    {0x25C, 0, 0, "mi_snow"},

    /* NOTE: Dungeon keys are tracked via individual per-room pickup flags
       (KEY_SILVER_*, KEY_GOLD_*, KEY_DIAMOND_* in save_data_tool.h) in the
       rando, not via the vanilla SAVE_KEY_RELATED_1-5 counters.  Those flag
       bits are listed in s_flag_bits[] below.                              */

    /* ── Stats and collectible counts (take the maximum) ───────────── */
    /* SAVE_TOTAL_HEALTH  (at -0x28, before the array base)             */
    {-0x028, 0, 1, "stat_hpmax"},
    /* SAVE_FORTUNE_DOLL_TOTAL – total dolls ever seen                  */
    {0x100, 0, 1, "stat_dolls"},
    /* SAVE_FORTUNE_DOLL_PROGRESS – dolls traded with Benkei            */
    {0x0FC, 0, 1, "stat_doll_p"},
    /* Map of Japan                                                     */
    {0x268, 0, 0, "ki_map_jpn"},
    /* Mr. Elephant / Mr. Arrow collectible progress per dungeon        */
    {0x26C, 0, 1, "mr_ely_oedo"},
    {0x270, 0, 1, "mr_ely_gtc"},
    {0x274, 0, 1, "mr_ely_ftc"},
    {0x278, 0, 1, "mr_ely_sub"},
    {0x27C, 0, 1, "mr_ely_mus"},
    {0x280, 0, 1, "mr_arr_oedo"},
    {0x284, 0, 1, "mr_arr_gtc"},
    {0x288, 0, 1, "mr_arr_ftc"},
    {0x28C, 0, 1, "mr_arr_sub"},
    {0x290, 0, 1, "mr_arr_mus"},
};

#define NUM_FIELDS ((int)(sizeof(s_fields) / sizeof(s_fields[0])))

/* =========================================================================
   Sync tables – individual flag bits
   ─────────────────────────────────────────────────────────────────────────
   id     : Flag ID (bit-indexed into D_8015C608_15D208)
   cached : 0 = not set (last sent state)
   name   : key used in SET_FLAG packets
   ========================================================================= */
typedef struct
{
    unsigned short id;
    unsigned char cached;
    const char *name;
} SyncFlagBit;

static SyncFlagBit s_flag_bits[] = {
    /* ── Boss defeats – unlock progression walls and dialogue ───────── */
    /* FLAG_DEFEATED_DHARMANYO 0x018  Byte 0x03 Bit 0 (Ghost Toys boss) */
    {0x018, 0, "fl_dharmanyo"},
    /* FLAG_DEFEATED_THAISAMBA 0x032  Byte 0x06 Bit 2 (underwater boss) */
    {0x032, 0, "fl_thaisamba"},
    /* FLAG_DEFEATED_TSURAMI   0x040  Byte 0x08 Bit 0 (Fest. Temple)    */
    {0x040, 0, "fl_tsurami"},
    /* FLAG_DEFEATED_BENKEI    0x033  Byte 0x06 Bit 3                   */
    {0x033, 0, "fl_benkei"},
    /* FLAG_BEAT_CONGO         0x12D  Byte 0x25 Bit 5 (Oedo Castle)     */
    {0x12D, 0, "fl_congo"},

    /* ── Character / ability acquisition flags ──────────────────────── */
    /* FLAG_RECRUITED_SASUKE   0x00E  Byte 0x01 Bit 6                   */
    {0x00E, 0, "fl_sasuke_rc"},
    /* FLAG_OBTAINED_MINI_EBISU 0x031 Byte 0x06 Bit 1                   */
    {0x031, 0, "fl_mini_ebi"},
    /* FLAG_RECRUITED_YAE      0x03A  Byte 0x07 Bit 2                   */
    {0x03A, 0, "fl_yae_rc"},
    /* FLAG_OBTAINED_SUDDEN_IMPACT 0x01C Byte 0x03 Bit 4                */
    {0x01C, 0, "fl_s_impact"},
    /* FLAG_OBTAINED_MERMAID_MAGIC  0x01F Byte 0x03 Bit 7               */
    {0x01F, 0, "fl_mermaid"},
    /* FLAG_OBTAINED_SUPER_JUMP     0x020 Byte 0x04 Bit 0 (Jetpack)     */
    {0x020, 0, "fl_superjmp"},

    /* ── Miracle-item acquisition flags ────────────────────────────── */
    /* FLAG_OBTAINED_MIRACLE_SNOW 0x035 Byte 0x06 Bit 5                 */
    {0x035, 0, "fl_mi_snow"},

    /* ── Quest-critical flags ───────────────────────────────────────── */
    /* FLAG_RECEIVED_SUPER_PASS  0x000  Bit 0 of flag byte              */
    {0x000, 0, "fl_superpass"},
    /* FLAG_OPENED_SUPER_PASS_GATE 0x001                                */
    {0x001, 0, "fl_sp_gate"},
    /* FLAG_OBTAINED_KEY_TO_TRAINING 0x027 Byte 0x04 Bit 7 (Jump Gym)  */
    {0x027, 0, "fl_gym_key"},
    /* FLAG_RECEIVED_CHAIN_PIPE  0x010  Byte 0x02 Bit 0                 */
    {0x010, 0, "fl_chain"},
    /* FLAG_RECEIVED_FIRE_RYO   0x013  Byte 0x02 Bit 3 (Medal Flames)  */
    {0x013, 0, "fl_fire_ryo"},
    /* FLAG_CRANE_POWER_ON      0x15A  Byte 0x2B Bit 2                  */
    {0x15A, 0, "fl_crane_on"},
    /* FLAG_RECEIVED_MAP_JAPAN  0x009  Byte 0x01 Bit 1                  */
    {0x009, 0, "fl_map_jpn"},
    /* FLAG_RECEIVED_SASUKE_BATTERY 0x00D Byte 0x01 Bit 5               */
    {0x00D, 0, "fl_bat_sas"},

    /* ── Per-room dungeon key pickup flags (rando key system) ──────────
       The rando computes available key counts by diffing KEY_* (collected)
       against LOCK_* (used) flag bits.  Syncing the KEY_* bits gives
       teammates the same collected-key credit without duplicating consumption.

       Oedo Castle ──────────────────────────────────────────────────── */
    {0x010A, 0, "ky_s_oc_tile"}, /* KEY_SILVER_OEDO_CASTLE_1F_TILE          */
    {0x010C, 0, "ky_s_oc_1f"},   /* KEY_SILVER_OEDO_CASTLE_1F               */
    {0x010E, 0, "ky_g_oc_1f"},   /* KEY_GOLD_OEDO_CASTLE_1F                 */
    {0x0110, 0, "ky_s_oc_cp"},   /* KEY_SILVER_OEDO_CASTLE_1F_CHAIN_PIPE    */
    {0x0112, 0, "ky_s_oc_crsh"}, /* KEY_SILVER_OEDO_CASTLE_2F_CRUSHER       */
    {0x0114, 0, "ky_s_oc_2f"},   /* KEY_SILVER_OEDO_CASTLE_2F               */

    /* Ghost Toys Castle ─────────────────────────────────────────────── */
    {0x01AD, 0, "ky_s_gt_flwr"}, /* KEY_SILVER_GHOST_TOYS_1F_FLOWER         */
    {0x01AF, 0, "ky_s_gt_crn"},  /* KEY_SILVER_GHOST_TOYS_1F_CRANE          */
    {0x01B1, 0, "ky_s_gt_inv"},  /* KEY_SILVER_GHOST_TOYS_1F_INVISIBLE      */
    {0x01B3, 0, "ky_s_gt_spin"}, /* KEY_SILVER_GHOST_TOYS_2F_SPINNING       */
    {0x01B5, 0, "ky_s_gt_dar"},  /* KEY_SILVER_GHOST_TOYS_2F_DARUMANYO      */
    {0x01B7, 0, "ky_g_gt_ff"},   /* KEY_GOLD_GHOST_TOYS_2F_FALSE_FLOOR      */
    {0x01B9, 0, "ky_d_gt_sc"},   /* KEY_DIAMOND_GHOST_TOYS_2F_SPIKE_CANNON  */
    {0x01BB, 0, "ky_s_gt_bil"},  /* KEY_SILVER_GHOST_TOYS_2F_BILLIARDS      */

    /* Festival Temple Castle ────────────────────────────────────────── */
    {0x016E, 0, "ky_g_ft_hot"},  /* KEY_GOLD_FESTIVAL_TEMPLE_HOT            */
    {0x0171, 0, "ky_s_ft_ring"}, /* KEY_SILVER_FESTIVAL_TEMPLE_RING         */

    /* Gourmet Submarine ─────────────────────────────────────────────── */
    {0x017B, 0, "ky_s_gs_baz"},  /* KEY_SILVER_GOURMET_SUB_2F_BAZOOKA       */
    {0x017D, 0, "ky_g_gs_jet"},  /* KEY_GOLD_GOURMET_SUB_2F_JETPACK         */
    {0x017F, 0, "ky_s_gs_lava"}, /* KEY_SILVER_GOURMET_SUB_2F_LAVA          */
    {0x0181, 0, "ky_s_gs_uw"},   /* KEY_SILVER_GOURMET_SUB_2F_UNDERWATER    */
    {0x0183, 0, "ky_s_gs_swd"},  /* KEY_SILVER_GOURMET_SUB_3F_SWORD         */
    {0x0185, 0, "ky_d_gs_inv"},  /* KEY_DIAMOND_GOURMET_SUB_3F_INVISIBLE    */
    {0x0187, 0, "ky_s_gs_sus"},  /* KEY_SILVER_GOURMET_SUB_3F_SUSHI         */

    /* Gorgeous Music Castle ─────────────────────────────────────────── */
    {0x0189, 0, "ky_g_mc_fan"},  /* KEY_GOLD_MUSICAL_CASTLE_1_FAN           */
    {0x018B, 0, "ky_s_mc_tall"}, /* KEY_SILVER_MUSICAL_CASTLE_1_TALL        */
    {0x018C, 0, "ky_g_mc_hj"},   /* KEY_GOLD_MUSICAL_CASTLE_1_HIGH_JUMP     */
    {0x018F, 0, "ky_g_mc_mini"}, /* KEY_GOLD_MUSICAL_CASTLE_1_MINI          */
    {0x0191, 0, "ky_d_mc_cube"}, /* KEY_DIAMOND_MUSICAL_CASTLE_1_CUBE       */
    {0x0193, 0, "ky_d_mc2"},     /* KEY_DIAMOND_MUSICAL_CASTLE_2            */
};

#define NUM_FLAGS ((int)(sizeof(s_flag_bits) / sizeof(s_flag_bits[0])))

/* =========================================================================
   State machine
   ========================================================================= */

static int s_was_connected = 0;  /* connection state from the previous frame */
static int s_save_was_valid = 0; /* true if save was loaded last time we checked */

/* When s_push_cursor >= 0 the module is performing an initial full push.
 * Each frame one item is sent, cursor advances.
 * Values NUM_FIELDS + NUM_FLAGS .. (past end) → push complete.             */
static int s_push_cursor = -1;

/* Cooldown timer (in frames) before we respond to another REQUEST_TEAM_STATE.
 * Prevents response floods when multiple requests arrive close together.    */
static int s_push_cooldown = 0;
#define PUSH_COOLDOWN_FRAMES 180 /* ~3 seconds @ 60 fps */

/* Maximum SET_FLAG packets sent per frame during incremental monitoring.
 * Kept at 1 to match the full-push rate and avoid flooding the Anchor
 * server's per-client overlapped-I/O queue on area transitions where
 * many flags change simultaneously.  Extra changes are deferred to
 * subsequent frames by leaving their cache un-updated this frame.       */
#define MAX_SENDS_PER_FRAME 1

#define PUSH_IDLE -1

/* Total items in both tables combined, used as the end sentinel.           */
static int push_total(void) { return NUM_FIELDS + NUM_FLAGS; }

/* Start (or restart) the full-push sequence.                               */
static void start_full_push(void)
{
    s_push_cursor = 0;
    recomp_printf("[ItemSync] Starting full state push (%d fields + %d flags)\n",
                  NUM_FIELDS, NUM_FLAGS);
}

/* Reset all cached values to 0 so every non-zero field is re-broadcast on
 * the next monitoring pass (or full push).                                  */
static void reset_caches(void)
{
    int i;
    for (i = 0; i < NUM_FIELDS; ++i)
        s_fields[i].cached = 0;
    for (i = 0; i < NUM_FLAGS; ++i)
        s_flag_bits[i].cached = 0;
}

/* =========================================================================
   Item notification UI  –  bottom-right toast stack
   =========================================================================

   Up to NOTIF_SLOTS cards are shown simultaneously, stacked from the
   bottom-right corner upward.  Each card auto-dismisses after NOTIF_FRAMES
   game frames.  Slots are allocated round-robin; idle slots are preferred.

   Card layout (each slot):
     ┌─[teal strip]──────────────────────────────────┐
     │  Received from team                           │  ← annotation, teal
     │  Chain Pipe                                   │  ← small, white
     └───────────────────────────────────────────────┘
   ========================================================================= */

#define NOTIF_SLOTS 8      /* max concurrent toasts             */
#define NOTIF_FRAMES 300   /* display duration (~5 s @ 60 fps)  */
#define NOTIF_WIDTH 240.0f /* card width in DP                  */
#define NOTIF_SLOT_H 52.0f /* vertical spacing between slots    */
#define NOTIF_BOTTOM 16.0f /* bottom edge of slot 0             */
#define NOTIF_RIGHT 16.0f  /* right edge of all cards           */

static const RecompuiColor C_NTEAL = {0, 180, 160, 255};
static const RecompuiColor C_NBLACK = {0, 0, 0, 210};
static const RecompuiColor C_NBORDER = {60, 60, 60, 200};
static const RecompuiColor C_NWHITE = {255, 255, 255, 255};

static RecompuiContext s_notif_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_notif_card[NOTIF_SLOTS];
static RecompuiResource s_notif_hdr[NOTIF_SLOTS]; /* header label per slot */
static RecompuiResource s_notif_name[NOTIF_SLOTS];
static int s_notif_timer[NOTIF_SLOTS];
static int s_notif_next = 0;        /* round-robin write cursor */
static int s_notif_ctx_visible = 0; /* 1 when context is currently shown */

/**
 * @brief Map a SET_FLAG key to a human-readable display string.
 *
 * Returns NULL for internal-mechanic flags that should not show a toast.
 */
static const char *get_flag_display_name(const char *n)
{
    /* Characters */
    if (streq(n, "chr_goemon"))
        return "Goemon";
    if (streq(n, "chr_ebisu"))
        return "Ebisumaru";
    if (streq(n, "chr_sasuke"))
        return "Sasuke";
    if (streq(n, "chr_yae"))
        return "Yae";
    /* Equipment */
    if (streq(n, "eq_chain"))
        return "Chain Pipe";
    if (streq(n, "eq_hammer"))
        return "Meat Hammer";
    if (streq(n, "eq_firecrk"))
        return "Firecracker Bomb";
    if (streq(n, "eq_flute"))
        return "Flute";
    if (streq(n, "eq_camera"))
        return "Wind-up Camera";
    if (streq(n, "eq_kunai"))
        return "Ice Kunai";
    if (streq(n, "eq_bazooka"))
        return "Bazooka";
    if (streq(n, "eq_fire_ryo"))
        return "Medal of Flames";
    /* Abilities */
    if (streq(n, "ab_impact"))
        return "Sudden Impact";
    if (streq(n, "ab_mini_ebi"))
        return "Mini Ebisumaru";
    if (streq(n, "ab_jetpack"))
        return "Jetpack";
    if (streq(n, "ab_mermaid"))
        return "Mermaid Magic";
    /* Quest items */
    if (streq(n, "ki_triton"))
        return "Triton Shell";
    if (streq(n, "ki_superps"))
        return "Super Pass";
    if (streq(n, "ki_achilles"))
        return "Achilles' Heel";
    if (streq(n, "ki_cucumber"))
        return "Cucumber";
    if (streq(n, "ki_map_jpn"))
        return "Map of Japan";
    /* Miracle items */
    if (streq(n, "mi_star"))
        return "Miracle Star";
    if (streq(n, "mi_moon"))
        return "Miracle Moon";
    if (streq(n, "mi_flower"))
        return "Miracle Flower";
    if (streq(n, "mi_snow"))
        return "Miracle Snow";
    /* Stats / upgrades */
    if (streq(n, "stat_hpmax"))
        return "HP Max Up";
    if (streq(n, "stat_dolls"))
        return "Fortune Doll";
    /* Boss defeats */
    if (streq(n, "fl_dharmanyo"))
        return "Dharmanyo Defeated";
    if (streq(n, "fl_thaisamba"))
        return "Thaisamba Defeated";
    if (streq(n, "fl_tsurami"))
        return "Tsurami Defeated";
    if (streq(n, "fl_benkei"))
        return "Benkei Defeated";
    if (streq(n, "fl_congo"))
        return "Congo Defeated";
    /* Character / ability flags */
    if (streq(n, "fl_sasuke_rc"))
        return "Sasuke Recruited";
    if (streq(n, "fl_mini_ebi"))
        return "Mini Ebisumaru";
    if (streq(n, "fl_yae_rc"))
        return "Yae Recruited";
    if (streq(n, "fl_s_impact"))
        return "Sudden Impact";
    if (streq(n, "fl_mermaid"))
        return "Mermaid Magic";
    if (streq(n, "fl_superjmp"))
        return "Jetpack";
    if (streq(n, "fl_mi_snow"))
        return "Miracle Snow";
    if (streq(n, "fl_superpass"))
        return "Super Pass";
    if (streq(n, "fl_gym_key"))
        return "Jump Gym Key";
    if (streq(n, "fl_chain"))
        return "Chain Pipe";
    if (streq(n, "fl_fire_ryo"))
        return "Medal of Flames";
    if (streq(n, "fl_map_jpn"))
        return "Map of Japan";
    if (streq(n, "fl_bat_sas"))
        return "Sasuke Battery";
    /* Per-room dungeon key pickups – match by prefix ky_s_ / ky_g_ / ky_d_ */
    if (n[0] == 'k' && n[1] == 'y' && n[2] == '_')
    {
        if (n[3] == 's')
            return "Silver Key";
        if (n[3] == 'g')
            return "Gold Key";
        if (n[3] == 'd')
            return "Diamond Key";
    }
    /* Suppress internal mechanics (gates, crane switch, dolls progress,
       Mr. Elly/Arrow per-dungeon counts) – too noisy to toast.              */
    return 0;
}

/** Lazy-initialise the shared notification context and all NOTIF_SLOTS cards. */
static void item_notif_ensure_init(void)
{
    int i;
    if (s_notif_ctx != RECOMPUI_NULL_CONTEXT)
        return;

    s_notif_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_notif_ctx, 0);
    recompui_set_context_captures_mouse(s_notif_ctx, 0);
    recompui_open_context(s_notif_ctx);

    RecompuiResource root = recompui_context_root(s_notif_ctx);

    for (i = 0; i < NOTIF_SLOTS; ++i)
    {
        /* ── Outer card ──────────────────────────────────────────────── */
        RecompuiResource card = recompui_create_element(s_notif_ctx, root);
        recompui_set_position(card, POSITION_ABSOLUTE);
        recompui_set_right(card, NOTIF_RIGHT, UNIT_DP);
        recompui_set_bottom(card, NOTIF_BOTTOM + (float)i * NOTIF_SLOT_H, UNIT_DP);
        recompui_set_width(card, NOTIF_WIDTH, UNIT_DP);
        recompui_set_display(card, DISPLAY_NONE); /* hidden until needed */
        recompui_set_flex_direction(card, FLEX_DIRECTION_ROW);
        recompui_set_align_items(card, ALIGN_ITEMS_STRETCH);
        recompui_set_background_color(card, &C_NBLACK);
        recompui_set_border_radius(card, 6.0f, UNIT_DP);
        recompui_set_border_width(card, 1.0f, UNIT_DP);
        recompui_set_border_color(card, &C_NBORDER);
        s_notif_card[i] = card;

        /* ── Left teal accent strip ──────────────────────────────────── */
        RecompuiResource accent = recompui_create_element(s_notif_ctx, card);
        recompui_set_width(accent, 4.0f, UNIT_DP);
        recompui_set_background_color(accent, &C_NTEAL);
        recompui_set_border_top_left_radius(accent, 6.0f, UNIT_DP);
        recompui_set_border_bottom_left_radius(accent, 6.0f, UNIT_DP);

        /* ── Text area ───────────────────────────────────────────────── */
        RecompuiResource area = recompui_create_element(s_notif_ctx, card);
        recompui_set_flex_grow(area, 1.0f);
        recompui_set_padding(area, 7.0f, UNIT_DP);
        recompui_set_flex_direction(area, FLEX_DIRECTION_COLUMN);
        recompui_set_display(area, DISPLAY_FLEX);

        /* Header – text is set dynamically in item_notif_push */
        RecompuiResource hdr = recompui_create_label(s_notif_ctx, area,
                                                     "",
                                                     LABELSTYLE_ANNOTATION);
        recompui_set_color(hdr, &C_NTEAL);
        recompui_set_font_weight(hdr, 700);
        recompui_set_margin_bottom(hdr, 2.0f, UNIT_DP);
        s_notif_hdr[i] = hdr;

        /* Item name */
        RecompuiResource nam = recompui_create_label(s_notif_ctx, area,
                                                     "", LABELSTYLE_SMALL);
        recompui_set_color(nam, &C_NWHITE);
        recompui_set_font_weight(nam, 600);
        s_notif_name[i] = nam;

        s_notif_timer[i] = 0;
    }

    recompui_close_context(s_notif_ctx);
    /* Context stays hidden until the first push.                         */
}

/**
 * @brief Show a toast notification for an item acquisition.
 *
 * Picks the first idle slot (timer == 0), falling back to round-robin so
 * bursts from a syncing player fill all 8 slots before recycling.
 *
 * @param header        Small label above the item name (e.g. "Item found!" or
 *                      "Received from team").
 * @param item_display  Human-readable item name (e.g. "Chain Pipe").
 */
static void item_notif_push(const char *header, const char *item_display)
{
    int i, slot;
    if (!item_display || !item_display[0])
        return;

    item_notif_ensure_init();

    /* Prefer an idle slot so we don't prematurely evict existing toasts. */
    slot = s_notif_next % NOTIF_SLOTS;
    for (i = 0; i < NOTIF_SLOTS; ++i)
    {
        if (s_notif_timer[i] <= 0)
        {
            slot = i;
            break;
        }
    }
    /* Advance round-robin past this slot. */
    s_notif_next = (slot + 1) % NOTIF_SLOTS;

    /* Update card texts and make it visible. */
    recompui_open_context(s_notif_ctx);
    recompui_set_text(s_notif_hdr[slot], header ? header : "");
    recompui_set_text(s_notif_name[slot], item_display);
    recompui_set_display(s_notif_card[slot], DISPLAY_FLEX);
    recompui_close_context(s_notif_ctx);

    s_notif_timer[slot] = NOTIF_FRAMES;

    /* Reveal the context if it was hidden. */
    if (!s_notif_ctx_visible)
    {
        recompui_show_context(s_notif_ctx);
        s_notif_ctx_visible = 1;
    }
}

/**
 * @brief Tick notification timers; hide expired cards.  Call once per frame.
 */
static void item_notif_tick(void)
{
    int any_active = 0, i;
    if (s_notif_ctx == RECOMPUI_NULL_CONTEXT)
        return;

    for (i = 0; i < NOTIF_SLOTS; ++i)
    {
        if (s_notif_timer[i] <= 0)
            continue;
        --s_notif_timer[i];
        if (s_notif_timer[i] == 0)
        {
            recompui_open_context(s_notif_ctx);
            recompui_set_display(s_notif_card[i], DISPLAY_NONE);
            recompui_close_context(s_notif_ctx);
        }
        else
        {
            any_active = 1;
        }
    }

    if (!any_active && s_notif_ctx_visible)
    {
        recompui_hide_context(s_notif_ctx);
        s_notif_ctx_visible = 0;
    }
}

/* =========================================================================
   Apply helpers  (incoming packet → local save)
   ========================================================================= */

/**
 * @brief Apply an incoming item flag to the local save.
 *
 * Looks up `flag_name` in both sync tables, then writes the value if the
 * local rule allows it (take-max or apply-if-zero).  Also updates the
 * cached copy so the outgoing monitor does not re-broadcast the change.
 *
 * @param flag_name  The `flag` key from the incoming SET_FLAG packet.
 * @param val        The `value` field from the packet.
 */
/**
 * Returns the human-readable display name if the flag was actually applied
 * (i.e. the save data changed), or NULL if it was a no-op or unknown.
 */
static const char *apply_flag(const char *flag_name, signed int val)
{
    int i;

    /* Check 32-bit save-data fields. */
    for (i = 0; i < NUM_FIELDS; ++i)
    {
        if (!streq(s_fields[i].name, flag_name))
            continue;

        signed int cur = SAVE_READ32(s_fields[i].off);
        int should_apply;
        if (s_fields[i].use_max)
            should_apply = (val > cur);
        else
            should_apply = (cur == 0 && val != 0);

        if (should_apply)
        {
            SAVE_WRITE32(s_fields[i].off, val);
            s_fields[i].cached = val;
            recomp_printf("[ItemSync] Applied field '%s' = %d\n", flag_name, val);
            return get_flag_display_name(flag_name);
        }
        return 0;
    }

    /* Check single-bit flags. */
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (!streq(s_flag_bits[i].name, flag_name))
            continue;

        if (val && !FLAG_IS_SET(s_flag_bits[i].id))
        {
            FLAG_SET_BIT(s_flag_bits[i].id);
            s_flag_bits[i].cached = 1;
            recomp_printf("[ItemSync] Applied flag '%s' (id=0x%X)\n",
                          flag_name, s_flag_bits[i].id);
            return get_flag_display_name(flag_name);
        }
        return 0;
    }
    /* Unknown flag name – ignore (could be from a later version). */
    return 0;
}

/* =========================================================================
   Packet processing
   ========================================================================= */

/**
 * @brief Extract the `clientId` integer from a packet JSON string.
 * Returns 0 (invalid / missing) if the field is absent.
 */
static unsigned int get_packet_client_id(const char *json)
{
    const char *pos = sfind(json, "\"clientId\":");
    if (!pos)
        return 0;
    pos += 11;
    while (*pos == ' ')
        ++pos;
    signed int v = 0;
    return parse_int(&pos, &v) ? (unsigned int)v : 0u;
}

/**
 * @brief Drain the incoming Anchor packet queue and handle item-related packets.
 *
 * Processes up to MAX_PACKETS_PER_FRAME packets per frame.
 *
 * - SET_FLAG packets matching a tracked name update the local save.
 * - REQUEST_TEAM_STATE packets from *other* clients trigger a full-push so
 *   the requester absorbs the team's combined progress.  Self-issued packets
 *   (same clientId) and packets within PUSH_COOLDOWN_FRAMES of the last push
 *   are silently ignored to prevent response cascades.
 */
#define MAX_PACKETS_PER_FRAME 16

static void process_incoming_packets(void)
{
    unsigned int own_id = anchor_get_client_id();
    int processed = 0;

    while (anchor_has_packet() && processed < MAX_PACKETS_PER_FRAME)
    {
        char *pkt = anchor_poll_packet();
        if (!pkt)
            break;
        ++processed;

        if (is_packet_type(pkt, "SET_FLAG"))
        {
            char fname[32];
            signed int fval = 0;
            if (get_flag_name(pkt, fname, (int)sizeof(fname)) &&
                get_flag_value(pkt, &fval))
            {
                const char *display = apply_flag(fname, fval);
                if (display)
                    item_notif_push("Received from team", display);
            }
        }
        else if (is_packet_type(pkt, "REQUEST_TEAM_STATE"))
        {
            unsigned int requester = get_packet_client_id(pkt);
            /* Skip own requests (shouldn't normally arrive, but be safe). */
            int is_self = (own_id != 0 && requester == own_id);
            if (!is_self && s_push_cooldown <= 0 && save_is_loaded())
            {
                recomp_printf("[ItemSync] REQUEST_TEAM_STATE from client %u – re-push scheduled.\n",
                              requester);
                reset_caches();
                start_full_push();
                s_push_cooldown = PUSH_COOLDOWN_FRAMES;
            }
        }
        /* All other packet types (ALL_CLIENT_STATE, UPDATE_CLIENT_STATE,
         * UPDATE_TEAM_STATE, etc.) are consumed here.  Python's internal
         * _player_states dict is maintained by the recv thread, so the
         * player-list overlay continues to work correctly without these
         * packets being processed on the C side.                            */

        recomp_free(pkt);
    }
}

/* =========================================================================
   Outgoing – full push (one item per frame)
   ========================================================================= */

/**
 * @brief Send one item from the sync tables each frame during the initial push.
 *
 * Loops over all 32-bit fields then all flag bits, sending each non-zero
 * value.  When the entire table has been walked, reverts to monitoring mode.
 */
static void do_push_step(void)
{
    if (s_push_cursor < 0 || s_push_cursor >= push_total())
    {
        s_push_cursor = PUSH_IDLE;
        recomp_printf("[ItemSync] Full push complete.\n");
        return;
    }

    if (s_push_cursor < NUM_FIELDS)
    {
        /* Push one 32-bit field. */
        int i = s_push_cursor;
        signed int val = SAVE_READ32(s_fields[i].off);
        if (val != 0)
        {
            anchor_send_flag(s_fields[i].name, (int)val, 1);
            s_fields[i].cached = val;
        }
    }
    else
    {
        /* Push one flag bit. */
        int i = s_push_cursor - NUM_FIELDS;
        if (FLAG_IS_SET(s_flag_bits[i].id))
        {
            anchor_send_flag(s_flag_bits[i].name, 1, 1);
            s_flag_bits[i].cached = 1;
        }
    }

    ++s_push_cursor;
    /* Mark push as idle when we've passed the last entry. */
    if (s_push_cursor >= push_total())
        s_push_cursor = PUSH_IDLE;
}

/* =========================================================================
   Outgoing – incremental monitoring
   ========================================================================= */

/**
 * @brief Detect and broadcast any newly acquired items this frame.
 *
 * Runs after the initial full-push sequence completes.  Compares each
 * tracked value against the cached snapshot; any change that can only be
 * an acquisition (value gained, not lost) is immediately sent as a
 * queued SET_FLAG packet.
 */
static void monitor_and_send_changes(void)
{
    int i;
    int sends = 0; /* per-frame send budget */

    /* 32-bit save-data fields. */
    for (i = 0; i < NUM_FIELDS; ++i)
    {
        signed int cur = SAVE_READ32(s_fields[i].off);
        if (cur == s_fields[i].cached)
            continue; /* no change */

        /* Only broadcast gains (cur > 0 for items; cur > cached for counts). */
        int should_send;
        if (s_fields[i].use_max)
            should_send = (cur > s_fields[i].cached);
        else
            should_send = (cur != 0);

        if (should_send)
        {
            if (sends >= MAX_SENDS_PER_FRAME)
                continue; /* budget exhausted – defer to next frame (cache not updated) */

            anchor_send_flag(s_fields[i].name, (int)cur, 1);
            ++sends;
            recomp_printf("[ItemSync] Sent field '%s' = %d\n",
                          s_fields[i].name, cur);
            const char *display = get_flag_display_name(s_fields[i].name);
            if (display)
                item_notif_push("Item found!", display);
        }
        /* Update cache when we send, or when we don't need to send (e.g. item
           lost / dropped) – but NOT when the send was deferred due to budget. */
        s_fields[i].cached = cur;
    }

    /* Single-bit flags. */
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        unsigned char cur = (unsigned char)FLAG_IS_SET(s_flag_bits[i].id);
        if (cur == s_flag_bits[i].cached)
            continue;

        if (cur)
        { /* only broadcast when flag becomes set, not when cleared */
            if (sends >= MAX_SENDS_PER_FRAME)
                continue; /* defer to next frame */

            anchor_send_flag(s_flag_bits[i].name, 1, 1);
            ++sends;
            recomp_printf("[ItemSync] Sent flag '%s' (id=0x%X)\n",
                          s_flag_bits[i].name, s_flag_bits[i].id);
            const char *display = get_flag_display_name(s_flag_bits[i].name);
            if (display)
                item_notif_push("Item found!", display);
        }
        s_flag_bits[i].cached = cur;
    }
}

/* =========================================================================
   Per-frame update hook
   ========================================================================= */

/**
 * @brief Called every game frame.
 *
 * Hooked on func_80002040_2C40 (the game's main per-frame tick) via
 * RECOMP_HOOK_RETURN, matching the pattern used by anchor_ui.c.
 */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void item_sync_update(void)
{
    int is_connected = anchor_is_connected() && !anchor_is_disabled();

    /* ── Tick notification timers (runs even when disconnected so in-flight
       toasts from a just-dropped connection still expire gracefully). ────── */
    item_notif_tick();

    /* ── Tick the re-push cooldown regardless of connection state ────── */
    if (s_push_cooldown > 0)
        --s_push_cooldown;

    /* ── Connection transitions ──────────────────────────────────────── */
    if (is_connected && !s_was_connected)
    {
        /* Just connected: reset caches so we re-broadcast everything.    */
        recomp_printf("[ItemSync] Connected – preparing full state push.\n");
        reset_caches();
        s_push_cooldown = 0;
        /* Request any state teammates may have queued for our team.      */
        anchor_request_team_state("");
        /* If a save is already loaded (reconnect mid-game), push now.    */
        if (save_is_loaded())
        {
            s_save_was_valid = 1;
            start_full_push();
        }
    }
    if (!is_connected && s_was_connected)
    {
        /* Disconnected: cancel any in-progress push.                     */
        recomp_printf("[ItemSync] Disconnected – clearing push state.\n");
        s_push_cursor = PUSH_IDLE;
        s_push_cooldown = 0;
        s_save_was_valid = 0;
    }
    s_was_connected = is_connected;

    if (!is_connected)
        return;

    /* ── Wait for a loaded save file ─────────────────────────────────── */
    int valid = save_is_loaded();

    if (valid && !s_save_was_valid)
    {
        /* Save just became valid (player loaded a file): begin the push. */
        recomp_printf("[ItemSync] Save loaded – starting full push.\n");
        reset_caches();
        start_full_push();
    }
    s_save_was_valid = valid;

    if (!valid)
        return;

    /* ── Process incoming packets ─────────────────────────────────────── */
    process_incoming_packets();

    /* ── Full push (one step per frame) ──────────────────────────────── */
    if (s_push_cursor >= 0)
    {
        do_push_step();
        return; /* skip monitor pass while pushing to keep frame budget */
    }

    /* ── Incremental monitoring ───────────────────────────────────────── */
    monitor_and_send_changes();
}
