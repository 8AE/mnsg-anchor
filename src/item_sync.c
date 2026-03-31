/**
 * @file item_sync.c
 * @brief Randomizer item / flag synchronisation for Anchor multiplayer.
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
#include "recompconfig.h"
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
 * Extract the integer value of `"damage":N` from a DAMAGE_SYNC packet string.
 * Returns 1 on success.
 */
static int get_ds_damage(const char *json, signed int *out)
{
    const char *pos = sfind(json, "\"damage\":");
    if (!pos)
        return 0;
    pos += 9;
    while (*pos == ' ')
        ++pos;
    return parse_int(&pos, out);
}

/**
 * Extract the integer value of `"heal":N` from a HEAL_SYNC packet string.
 * Returns 1 on success.
 */
static int get_ds_heal(const char *json, signed int *out)
{
    const char *pos = sfind(json, "\"heal\":");
    if (!pos)
        return 0;
    pos += 7;
    while (*pos == ' ')
        ++pos;
    return parse_int(&pos, out);
}

/**
 * Extract the integer value of `"ryo":N` from a RYO_SYNC packet string.
 * Returns 1 on success.
 */
static int get_ds_ryo(const char *json, signed int *out)
{
    const char *pos = sfind(json, "\"ryo\":");
    if (!pos)
        return 0;
    pos += 6;
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

    /* ── Weapon upgrade tiers (0=Iron/Default, 1=Silver, 2=Gold) ───── */
    /* SAVE_GOEMON_WEAPON_LEVEL / EBISUMARU / SASUKE / YAE              */
    {0x0A4, 0, 1, "wpn_goemon"},
    {0x0A8, 0, 1, "wpn_ebisu"},
    {0x0AC, 0, 1, "wpn_sasuke"},
    {0x0B0, 0, 1, "wpn_yae"},

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

    /* ── Fish collectible counts (take the maximum) ─────────────────── */
    /* Total Red / Yellow / Blue Fish Obtained                           */
    /* 0x8015C718 / 0x8015C71C / 0x8015C720                             */
    {0x110, 0, 1, "fish_red"},
    {0x114, 0, 1, "fish_yellow"},
    {0x118, 0, 1, "fish_blue"},

    /* ── Key to Training (door-key item, distinct from fl_gym_key) ───── */
    /* 0x8015C710                                                         */
    {0x108, 0, 0, "ki_traindoor"},

    /* ── Warp points (towns / coffee shops / fast-travel gates) ──────── */
    /* SAVE_WARP_* offsets from save_data_tool.h (0x2A4 – 0x2D4)          */
    {0x2A4, 0, 0, "wp_goemon_h"},  /* SAVE_WARP_GOEMON_HOUSE             */
    {0x2A8, 0, 0, "wp_kai_hwy"},   /* SAVE_WARP_KAI_HIGHWAY              */
    {0x2AC, 0, 0, "wp_oedo"},      /* SAVE_WARP_OEDO_CASTLE              */
    {0x2B0, 0, 0, "wp_zazen"},     /* SAVE_WARP_ZAZEN_TOWN               */
    {0x2B4, 0, 0, "wp_kii_cafe"},  /* SAVE_WARP_KII_COFFEE               */
    {0x2B8, 0, 0, "wp_folkypoke"}, /* SAVE_WARP_FOLKYPOKE_VILLAGE        */
    {0x2BC, 0, 0, "wp_kompira"},   /* SAVE_WARP_KOMPIRA_MOUNTAIN         */
    {0x2C0, 0, 0, "wp_iyo_tea"},   /* SAVE_WARP_IYO_TEA_HOUSE            */
    {0x2C4, 0, 0, "wp_ghost"},     /* SAVE_WARP_GHOST_TOYS               */
    {0x2C8, 0, 0, "wp_izumo_tea"}, /* SAVE_WARP_IZUMO_TEA_HOUSE          */
    {0x2CC, 0, 0, "wp_festival"},  /* SAVE_WARP_FESTIVAL_TEMPLE          */
    {0x2D0, 0, 0, "wp_fest_vill"}, /* SAVE_WARP_FESTIVAL_VILLAGE         */
    {0x2D4, 0, 0, "wp_witch"},     /* SAVE_WARP_WITCHES_HUT              */
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

    /* ── Weapon upgrade flags ──────────────────────────────────────── */
    /* FLAG_UPGRADED_GOLD_WEAPONS  0x01A Byte 0x03 Bit 2               */
    {0x01A, 0, "fl_gold_wpn"},

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

    /* ── Fishing quest event flags ──────────────────────────────────── */
    /* Set when a fish-collecting quest is opened / limits reached.     */
    /* flag_id source: parser_config_en.yaml prettify section           */
    {0x02A, 0, "fl_fish_r_on"}, /* Looking for Red Fish                  */
    {0x02B, 0, "fl_fish_y_on"}, /* Looking for Yellow Fish               */
    {0x02C, 0, "fl_fish_b_on"}, /* Looking for Blue Fish                 */
    {0x04F, 0, "fl_fish_b_mx"}, /* Cannot Catch More Blue Fish           */
    {0x050, 0, "fl_fish_y_mx"}, /* Cannot Catch More Yellow Fish         */
    {0x051, 0, "fl_fish_r_mx"}, /* Cannot Catch More Red Fish            */

    /* ── Cat Eyes shop purchases ────────────────────────────────────── */
    /* flag_ids 0x1C5-0x1C7 (parser_config_en.yaml)                     */
    {0x1C5, 0, "fl_ce_dharma"}, /* Bought Dharma from Cat Eyes           */
    {0x1C6, 0, "fl_ce_notice"}, /* Bought Notice Board from Cat Eyes     */
    {0x1C7, 0, "fl_ce_doll"},   /* Bought Doll from Cat Eyes             */

    /* ── World / story event flags ──────────────────────────────────── */
    {0x014, 0, "fl_koryuta"},    /* Freed Koryuta the Dragon              */
    {0x06B, 0, "fl_outerspace"}, /* Went to Outer Space                   */
    {0x06F, 0, "fl_baron_iga"},  /* Met Baron in Iga                      */

    /* ── World-state / NPC-unlock flags ─────────────────────────────── */
    /* 0x011  Mokubei Can Upgrade Weapons – gates weapon smithing        */
    {0x011, 0, "fl_mokubei"},
    /* 0x015  Met Ghost of Wise Man – NPC encounter, gates quest         */
    {0x015, 0, "fl_wiseman"},
    /* 0x016  Spoke to Witch (But Didn't Pay) – gates witch services     */
    {0x016, 0, "fl_witch_np"},
    /* 0x017  Kyushu Disappeared – major world-state change              */
    {0x017, 0, "fl_kyushu"},
    /* 0x06C  Going to Outer Space – pre-space world state               */
    {0x06C, 0, "fl_to_space"},
    /* 0x07F  Riding Dragon From Folkypoke – progression unlock          */
    {0x07F, 0, "fl_dragon_fp"},
    /* 0x080  Plasma Told You to Resurrect Sasuke – quest trigger        */
    {0x080, 0, "fl_sas_res"},

    /* ── Benkei / Ushiwaka quest chain ──────────────────────────────── */
    /* 0x034  Met Benkei – gates Benkei fight                            */
    {0x034, 0, "fl_met_benkei"},
    /* 0x025  Can Talk to Ushiwaka about Benkei                          */
    {0x025, 0, "fl_ushi_ben"},
    /* 0x02D  Asked Ushiwaka Who He Is                                   */
    {0x02D, 0, "fl_ushi_id"},
    /* 0x028  Ushiwaka Went to Golden Temple – world state               */
    {0x028, 0, "fl_ushi_gt"},

    /* ── Kihachi's Favorite Food quest ──────────────────────────────── */
    /* 0x023  Heard about Kihachi from Benkei                            */
    {0x023, 0, "fl_kihachi_b"},
    /* 0x022  Looking for Kihachi's Favorite Food                        */
    {0x022, 0, "fl_kihachi_q"},
    /* 0x024  Learned Kihachi's Favorite Food                            */
    {0x024, 0, "fl_kihachi_f"},
    /* 0x037  Kihachi Will Give Favorite Food Hint                       */
    {0x037, 0, "fl_kihachi_h1"},
    /* 0x038  Kihachi Gave Favorite Food Hint                            */
    {0x038, 0, "fl_kihachi_h2"},

    /* ── Folkypoke / Priest / training quest chain ───────────────────── */
    /* 0x03B  Spoke to Priest's Son                                      */
    {0x03B, 0, "fl_priest_son"},
    /* 0x03D  Arrived at Folkypoke Tourist Center                        */
    {0x03D, 0, "fl_folkypoke"},
    /* 0x03F  Heard about Inaba Desert Battery – battery quest trigger   */
    {0x03F, 0, "fl_inaba_bat"},
    /* 0x063  Heard about Dragon Problem                                 */
    {0x063, 0, "fl_dragon_p"},
    /* 0x064  Received Gratitude from Tourist Center                     */
    {0x064, 0, "fl_tourist_g"},
    /* 0x06E  Spoke to Dancin Alnite                                     */
    {0x06E, 0, "fl_dancin"},

    /* ── Goemon Tenements Cousins quest ─────────────────────────────── */
    {0x052, 0, "fl_cous1"}, /* Goemon Tenements Cousins #1           */
    {0x053, 0, "fl_cous2"}, /* Goemon Tenements Cousins #2           */
    {0x054, 0, "fl_cous3"}, /* Goemon Tenements Cousins #3           */
    {0x055, 0, "fl_cous4"}, /* Goemon Tenements Cousins #4           */

    /* ── Old Woman in Zazen quest ────────────────────────────────────── */
    {0x056, 0, "fl_zazen_bef"}, /* Met Old Woman in Zazen Before Dharmanyo */
    {0x057, 0, "fl_zazen_aft"}, /* Met Old Woman in Zazen After Dharmanyo  */
    {0x058, 0, "fl_zazen_ba"},  /* Met Old Woman in Zazen Before And After  */
    {0x059, 0, "fl_zazen_rwd"}, /* Obtained Reward from Old Woman in Zazen  */

    /* ── First-meeting / intro NPC cutscenes ────────────────────────── */
    /* These gate dialogue trees and are important to sync so teammates
       don't get stuck waiting for intros they've already seen.          */
    /* 0x002  Old Man in South Oedo – gates Oedo Castle bridge           */
    {0x002, 0, "cs_oldman_oe"},
    /* 0x007  Spoke to Omitsu about UFO – gates Omitsu quest            */
    {0x007, 0, "cs_omitsu"},
    /* 0x008  Omitsu Fan – quest completion flag                         */
    {0x008, 0, "cs_omitsu_f"},
    /* 0x00A  Heard about Zazen Dwarf – gates Zazen quest chain          */
    {0x00A, 0, "cs_zazen_dw"},
    /* 0x00B  Spoke to Kompira Priest – gates priest training offer      */
    {0x00B, 0, "cs_kompira"},

    /* ── Post-boss cutscenes (gate doors / world-state dialogue) ───────
       These flags are set after boss-defeat cutscenes and commonly
       prevent doors from opening if a teammate hasn't seen them.        */
    /* 0x073  After Dharmanyo #1 (story beat before bridge unlocks)      */
    {0x073, 0, "cs_dhrm_1"},
    /* 0x070  After Dharmanyo #2                                         */
    {0x070, 0, "cs_dhrm_2"},
    /* 0x071  After Dharmanyo #3                                         */
    {0x071, 0, "cs_dhrm_3"},
    /* 0x072  After Dharmanyo #4                                         */
    {0x072, 0, "cs_dhrm_4"},
    /* 0x074  After Tsurami (gate for Festival Temple exit)              */
    {0x074, 0, "cs_tsurami"},

    /* ── Gorgeous Stage / Sogen Girl performance cutscenes ─────────── */
    /* Set when the Gorgeous Stage show plays; gates Sogen area events. */
    {0x076, 0, "cs_gorge_1"}, /* Gorgeous Stage Cutscene Flag #1         */
    {0x075, 0, "cs_gorge_2"}, /* Gorgeous Stage Cutscene Flag #2         */
    {0x077, 0, "cs_sogen_l"}, /* Sogen Girl Imitated Lily                */
    {0x078, 0, "cs_sogen_d"}, /* Sogen Girl Imitated Dancin              */

    /* ── Witch cutscene chain ────────────────────────────────────────── */
    /* Witch services (weapon upgrades, etc.) unlock after these play.  */
    {0x07C, 0, "cs_witch_1"}, /* Witch Cutscene Flag #1                  */
    {0x09C, 0, "cs_witch_2"}, /* Witch Cutscene Flag #2                  */
    {0x09D, 0, "cs_witch_3"}, /* Witch Cutscene Flag #3                  */
    {0x09E, 0, "cs_witch_4"}, /* Witch Cutscene Flag #4                  */
    {0x09F, 0, "cs_witch_5"}, /* Witch Cutscene Flag #5                  */
    {0x0A0, 0, "cs_witch_6"}, /* Witch Cutscene Flag #6                  */

    /* ── Special meeting cutscenes ──────────────────────────────────── */
    /* 0x098  Tourist Center related cutscene                            */
    {0x098, 0, "cs_tourist_c"},
    /* 0x099  Baron / Ghost of Wise Man meeting cutscene                 */
    {0x099, 0, "cs_baron_wm"},

    /* ── Silver fortune doll room pickup flags ──────────────────────── */
    /* Syncing these prevents a player from re-collecting a doll from a  */
    /* room their teammate already cleared, which would inflate the doll  */
    /* count and give unbalanced HP upgrades.                             */
    /* (SILVER_DOLL_* in save_data_tool.h, flag IDs 0x00CA – 0x00F1)     */
    {0x00CA, 0, "sd_oe_trt"},  /* SILVER_DOLL_OEDO_TURTLE             */
    {0x00CB, 0, "sd_oe_sc"},   /* SILVER_DOLL_OEDO_SILVER_CAT         */
    {0x00CC, 0, "sd_oe_blk"},  /* SILVER_DOLL_OEDO_BLOCKED            */
    {0x00CD, 0, "sd_oe_elv"},  /* SILVER_DOLL_OEDO_ELEVATOR           */
    {0x00CE, 0, "sd_gh_elv"},  /* SILVER_DOLL_GHOST_ELEVATOR          */
    {0x00CF, 0, "sd_gh_crn"},  /* SILVER_DOLL_GHOST_CRANE             */
    {0x00D0, 0, "sd_gh_can"},  /* SILVER_DOLL_GHOST_CANNON            */
    {0x00D1, 0, "sd_gh_spk"},  /* SILVER_DOLL_GHOST_BIG_SPIKE         */
    {0x00D2, 0, "sd_ft_sc"},   /* SILVER_DOLL_FESTIVAL_SILVER_CAT     */
    {0x00D3, 0, "sd_ft_rng"},  /* SILVER_DOLL_FESTIVAL_RING           */
    {0x00D4, 0, "sd_ft_ik"},   /* SILVER_DOLL_FESTIVAL_ICE_KUNAI      */
    {0x00D5, 0, "sd_ft_fk"},   /* SILVER_DOLL_FESTIVAL_FISH_KIT       */
    {0x00D6, 0, "sd_mc_gf"},   /* SILVER_DOLL_MUSICAL_GOLD_FAN        */
    {0x00D7, 0, "sd_mc_ml"},   /* SILVER_DOLL_MUSICAL_MULTI_LOCK      */
    {0x00D8, 0, "sd_mc_ba"},   /* SILVER_DOLL_MUSICAL_BIG_AQUA        */
    {0x00D9, 0, "sd_mc_ds"},   /* SILVER_DOLL_MUSICAL_DESCENT         */
    {0x00DA, 0, "sd_kai_hw"},  /* SILVER_DOLL_KAI_HIGHWAY             */
    {0x00DB, 0, "sd_mtfuji"},  /* SILVER_DOLL_MT_FUJI                 */
    {0x00DC, 0, "sd_musashi"}, /* SILVER_DOLL_MUSASHI                 */
    {0x00DD, 0, "sd_ym_up"},   /* SILVER_DOLL_YAMATO_UPPER            */
    {0x00DE, 0, "sd_ym_lo"},   /* SILVER_DOLL_YAMATO_LOWER            */
    {0x00DF, 0, "sd_ym_uw"},   /* SILVER_DOLL_YAMATO_UNDERWATER       */
    {0x00E0, 0, "sd_trt_rk"},  /* SILVER_DOLL_TURTLE_ROCK             */
    {0x00E1, 0, "sd_kii_aw"},  /* SILVER_DOLL_KII_AWAJI               */
    {0x00E2, 0, "sd_hwr"},     /* SILVER_DOLL_HUSBAND_WIFE_ROCKS      */
    {0x00E3, 0, "sd_iga_vn"},  /* SILVER_DOLL_IGA_VINE                */
    {0x00E4, 0, "sd_dogo"},    /* SILVER_DOLL_DOGO_HOT_SPRINGS        */
    {0x00E5, 0, "sd_bizen"},   /* SILVER_DOLL_BIZEN                   */
    {0x00E6, 0, "sd_ngt_me"},  /* SILVER_DOLL_NAGATO_MINI_EBI         */
    {0x00E7, 0, "sd_ngt_gt"},  /* SILVER_DOLL_NAGATO_GATE             */
    {0x00E8, 0, "sd_inaba"},   /* SILVER_DOLL_INABA                   */
    {0x00E9, 0, "sd_jp_sea"},  /* SILVER_DOLL_JAPAN_SEA               */
    {0x00EA, 0, "sd_ne_tun"},  /* SILVER_DOLL_NORTHEAST_TUNNEL        */
    {0x00EB, 0, "sd_uz_tun"},  /* SILVER_DOLL_UZEN_TUNNEL             */
    {0x00EC, 0, "sd_oe_brm"},  /* SILVER_DOLL_OEDO_BRIDGE_MUSASHI     */
    {0x00ED, 0, "sd_oe_pth"},  /* SILVER_DOLL_OEDO_PATH_CASTLE        */
    {0x00EE, 0, "sd_zz_wh"},   /* SILVER_DOLL_WATERING_HOLE           */
    {0x00EF, 0, "sd_zz_wy"},   /* SILVER_DOLL_ZAZEN_WATERWAY          */
    {0x00F0, 0, "sd_fp_est"},  /* SILVER_DOLL_FOLKYPOKE_EAST          */
    {0x00F1, 0, "sd_ft_hid"},  /* SILVER_DOLL_FESTIVAL_HIDDEN         */

    /* ── Gold fortune doll room pickup flags ────────────────────────── */
    /* (GOLD_DOLL_* in save_data_tool.h, flag IDs 0x00F2 – 0x00F6)       */
    {0x00F2, 0, "gd_gh_sg"},  /* GOLD_DOLL_GHOST_SHOGI_CANNON        */
    {0x00F3, 0, "gd_ft_wg"},  /* GOLD_DOLL_FESTIVAL_WEST_GOLD        */
    {0x00F4, 0, "gd_mc2_gc"}, /* GOLD_DOLL_MUSICAL_2_GOLD_CAT        */
    {0x00F5, 0, "gd_bizen"},  /* GOLD_DOLL_BIZEN                     */
    {0x00F6, 0, "gd_kegon"},  /* GOLD_DOLL_WATERFALL_KEGON           */
};

#define NUM_FLAGS ((int)(sizeof(s_flag_bits) / sizeof(s_flag_bits[0])))

/* =========================================================================
   State machine
   ========================================================================= */

static int s_was_connected = 0;     /* connection state from the previous frame */
static int s_save_was_valid = 0;    /* true if save was loaded last time we checked */
static int s_prev_player_count = 0; /* player count from the previous frame */

/* When s_push_cursor >= 0 the module is performing an initial full push.
 * Each frame one item is sent, cursor advances.
 * Values NUM_FIELDS + NUM_FLAGS .. (past end) → push complete.             */
static int s_push_cursor = -1;

/* Cooldown timer (in frames) before we respond to another REQUEST_TEAM_STATE.
 * Prevents response floods when multiple requests arrive close together.    */
static int s_push_cooldown = 0;
#define PUSH_COOLDOWN_FRAMES 180 /* ~3 seconds @ 60 fps */

/* =========================================================================
   Damage sync state
   =========================================================================
   Current HP byte: 0x8015C5E7  =  D_8015C608_15D208 + (-0x21)
   Current character index:     0x8015C5DC  (0=Goemon 1=Ebisumaru 2=Sasuke 3=Yae)
   ========================================================================= */

/* Read / write the active character's current HP (u8, half-hearts).        */
#define DS_HP_OFFSET (-0x21)
#define DS_HP_READ() (*(volatile unsigned char *)((char *)D_8015C608_15D208 + DS_HP_OFFSET))
#define DS_HP_WRITE(v) (*(unsigned char *)((char *)D_8015C608_15D208 + DS_HP_OFFSET) = (unsigned char)(v))

/* Active character index.                                                   */
#define DS_CHAR_IDX() (((*(volatile unsigned int *)0x8015C5DC)) & 0xFFu)

static unsigned char s_ds_prev_hp = 0;  /* HP observed last frame              */
static unsigned int s_ds_prev_char = 0; /* character index observed last frame  */
static int s_ds_initialized = 0;        /* 0 until first baseline is captured   */

/* Ryo delta sync state                                                      */
#define DS_RYO_OFFSET (-0x20)
#define DS_RYO_READ() SAVE_READ32(DS_RYO_OFFSET)
#define DS_RYO_WRITE(v) SAVE_WRITE32(DS_RYO_OFFSET, (v))

static signed int s_ryo_prev = 0; /* ryo observed last frame     */
static int s_ryo_initialized = 0; /* 0 until baseline captured   */

/* Maximum SET_FLAG packets sent per frame during incremental monitoring.
 * Kept at 1 to match the full-push rate and avoid flooding the Anchor
 * server's per-client overlapped-I/O queue on area transitions where
 * many flags change simultaneously.  Extra changes are deferred to
 * subsequent frames by leaving their cache un-updated this frame.       */
#define MAX_SENDS_PER_FRAME 1

#define PUSH_IDLE -1

/* Total items in both tables combined, used as the end sentinel.           */
static int push_total(void) { return NUM_FIELDS + NUM_FLAGS; }

/**
 * Count the number of players currently in the room by parsing the JSON array
 * returned by anchor_get_player_names_json().  Each player name is a quoted
 * string; counting opening '"' characters and dividing by 2 gives the count.
 * Returns 0 when not connected or the array is empty ("[]").
 */
static int get_player_count(void)
{
    char *json = anchor_get_player_names_json();
    if (!json)
        return 0;
    int quotes = 0;
    const char *p = json;
    while (*p)
    {
        if (*p == '"')
            ++quotes;
        ++p;
    }
    recomp_free(json);
    return quotes / 2; /* each name has one opening and one closing quote */
}

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
    /* Weapon upgrade tiers */
    if (streq(n, "wpn_goemon"))
        return "Goemon Weapon Upgrade";
    if (streq(n, "wpn_ebisu"))
        return "Ebisumaru Weapon Upgrade";
    if (streq(n, "wpn_sasuke"))
        return "Sasuke Weapon Upgrade";
    if (streq(n, "wpn_yae"))
        return "Yae Weapon Upgrade";
    if (streq(n, "fl_gold_wpn"))
        return "Gold Weapons Unlocked";
    /* Stats / upgrades */
    if (streq(n, "stat_hpmax"))
        return "HP Max Up";
    /* stat_ryo: ryo delta sync – no toast shown for money gains */
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
    /* Fish collectible counts */
    if (streq(n, "fish_red"))
        return "Red Fish";
    if (streq(n, "fish_yellow"))
        return "Yellow Fish";
    if (streq(n, "fish_blue"))
        return "Blue Fish";
    /* Key to Training door-key item and Oedo warp */
    if (streq(n, "ki_traindoor"))
        return "Key to Training";
    if (streq(n, "ki_oedo_warp") || streq(n, "wp_oedo"))
        return "Oedo Castle Warp";
    /* Warp points – towns, coffee/tea shops, fast-travel gates */
    if (streq(n, "wp_goemon_h"))
        return "Goemon's House";
    if (streq(n, "wp_kai_hwy"))
        return "Kai Highway";
    if (streq(n, "wp_zazen"))
        return "Zazen Town";
    if (streq(n, "wp_kii_cafe"))
        return "Kii Coffee Shop";
    if (streq(n, "wp_folkypoke"))
        return "Folkypoke Village";
    if (streq(n, "wp_kompira"))
        return "Kompira Mountain";
    if (streq(n, "wp_iyo_tea"))
        return "Iyo Tea House";
    if (streq(n, "wp_ghost"))
        return "Ghost Toys Warp";
    if (streq(n, "wp_izumo_tea"))
        return "Izumo Tea House";
    if (streq(n, "wp_festival"))
        return "Festival Temple Warp";
    if (streq(n, "wp_fest_vill"))
        return "Festival Village";
    if (streq(n, "wp_witch"))
        return "Witch's Hut";
    /* Cat Eyes shop purchases */
    if (streq(n, "fl_ce_dharma"))
        return "Dharma (Cat Eyes)";
    if (streq(n, "fl_ce_notice"))
        return "Notice Board (Cat Eyes)";
    if (streq(n, "fl_ce_doll"))
        return "Doll (Cat Eyes)";
    /* World / story events */
    if (streq(n, "fl_koryuta"))
        return "Koryuta Freed";
    if (streq(n, "fl_outerspace"))
        return "Went to Outer Space";
    if (streq(n, "fl_baron_iga"))
        return "Met Baron in Iga";
    /* World-state / NPC-unlock */
    if (streq(n, "fl_mokubei"))
        return "Mokubei Unlocked";
    if (streq(n, "fl_wiseman"))
        return "Met Ghost of Wise Man";
    if (streq(n, "fl_witch_np"))
        return "Spoke to Witch";
    if (streq(n, "fl_kyushu"))
        return "Kyushu Disappeared";
    if (streq(n, "fl_to_space"))
        return "Going to Outer Space";
    if (streq(n, "fl_dragon_fp"))
        return "Dragon from Folkypoke";
    if (streq(n, "fl_sas_res"))
        return "Resurrect Sasuke";
    /* Benkei / Ushiwaka chain */
    if (streq(n, "fl_met_benkei"))
        return "Met Benkei";
    if (streq(n, "fl_ushi_ben"))
        return "Ushiwaka: Benkei Talk";
    if (streq(n, "fl_ushi_id"))
        return "Asked Ushiwaka Who He Is";
    if (streq(n, "fl_ushi_gt"))
        return "Ushiwaka: Golden Temple";
    /* Kihachi food quest */
    if (streq(n, "fl_kihachi_b"))
        return "Kihachi: Benkei Tip";
    if (streq(n, "fl_kihachi_q"))
        return "Kihachi: Seeking Food";
    if (streq(n, "fl_kihachi_f"))
        return "Kihachi: Learned Food";
    if (streq(n, "fl_kihachi_h1"))
        return "Kihachi: Will Hint";
    if (streq(n, "fl_kihachi_h2"))
        return "Kihachi: Hinted";
    /* Folkypoke chain */
    if (streq(n, "fl_priest_son"))
        return "Spoke to Priest's Son";
    if (streq(n, "fl_folkypoke"))
        return "Arrived at Folkypoke";
    if (streq(n, "fl_inaba_bat"))
        return "Inaba Desert Battery";
    if (streq(n, "fl_dragon_p"))
        return "Heard Dragon Problem";
    if (streq(n, "fl_tourist_g"))
        return "Tourist Center Thanks";
    if (streq(n, "fl_dancin"))
        return "Spoke to Dancin Alnite";
    /* Tenements cousins quest */
    if (streq(n, "fl_cous1"))
        return "Tenements Cousin #1";
    if (streq(n, "fl_cous2"))
        return "Tenements Cousin #2";
    if (streq(n, "fl_cous3"))
        return "Tenements Cousin #3";
    if (streq(n, "fl_cous4"))
        return "Tenements Cousin #4";
    /* Zazen old woman quest */
    if (streq(n, "fl_zazen_bef"))
        return "Zazen Woman (Before)";
    if (streq(n, "fl_zazen_aft"))
        return "Zazen Woman (After)";
    if (streq(n, "fl_zazen_ba"))
        return "Zazen Woman (Both)";
    if (streq(n, "fl_zazen_rwd"))
        return "Zazen Woman Reward";
    /* Fish quest flags – suppress toasts (internal quest state) */
    if (streq(n, "fl_fish_r_on") || streq(n, "fl_fish_y_on") || streq(n, "fl_fish_b_on") ||
        streq(n, "fl_fish_r_mx") || streq(n, "fl_fish_y_mx") || streq(n, "fl_fish_b_mx"))
        return 0;
    /* Cutscene flags – suppress toasts (internal progression state, not items) */
    if (n[0] == 'c' && n[1] == 's' && n[2] == '_')
        return 0;
    /* Fortune doll room pickup flags – suppress toasts (deduplication only) */
    if ((n[0] == 's' && n[1] == 'd' && n[2] == '_') ||
        (n[0] == 'g' && n[1] == 'd' && n[2] == '_'))
        return 0;
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
    if (recomp_get_config_u32("anchor_show_notifications") != 0)
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
            /* When HP max increases, refill current HP to the new max.
             * This matches vanilla behaviour where trading a fortune doll
             * with Benkei both raises max HP and fully restores HP.        */
            if (s_fields[i].off == -0x028)
            {
                SAVE_WRITE32(-0x024, val);         /* SAVE_CURRENT_HEALTH = new max */
                s_ds_prev_hp = (unsigned char)val; /* keep damage-sync baseline in sync */
            }
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
        else if (is_packet_type(pkt, "DAMAGE_SYNC"))
        {
            signed int dmg = 0;
            if (recomp_get_config_u32("anchor_damage_sync") == 0 &&
                get_ds_damage(pkt, &dmg) && dmg > 0 && save_is_loaded())
            {
                unsigned char cur_hp = DS_HP_READ();
                if (cur_hp > 0)
                {
                    unsigned char new_hp = ((signed int)cur_hp - dmg <= 0)
                                               ? 0u
                                               : (unsigned char)(cur_hp - dmg);
                    DS_HP_WRITE(new_hp);
                    /* Update prev so the monitor loop does not echo this back. */
                    s_ds_prev_hp = new_hp;
                    recomp_printf("[DamageSync] Applied %d damage (HP: %d -> %d)\n",
                                  dmg, (int)cur_hp, (int)new_hp);
                }
            }
        }
        else if (is_packet_type(pkt, "HEAL_SYNC"))
        {
            signed int heal = 0;
            if (recomp_get_config_u32("anchor_damage_sync") == 0 &&
                get_ds_heal(pkt, &heal) && heal > 0 && save_is_loaded())
            {
                unsigned char cur_hp = DS_HP_READ();
                signed int hp_max = SAVE_READ32(SAVE_HP_MAX_OFFSET);
                unsigned char new_hp = ((signed int)cur_hp + heal >= hp_max)
                                           ? (unsigned char)hp_max
                                           : (unsigned char)(cur_hp + heal);
                if (new_hp > cur_hp)
                {
                    DS_HP_WRITE(new_hp);
                    /* Update prev so the monitor loop does not echo this back. */
                    s_ds_prev_hp = new_hp;
                    recomp_printf("[HealSync] Applied %d heal (HP: %d -> %d)\n",
                                  heal, (int)cur_hp, (int)new_hp);
                }
            }
        }
        else if (is_packet_type(pkt, "RYO_SYNC"))
        {
            signed int delta = 0;
            if (get_ds_ryo(pkt, &delta) && delta > 0 && save_is_loaded())
            {
                signed int cur_ryo = DS_RYO_READ();
                DS_RYO_WRITE(cur_ryo + delta);
                /* Update baseline so the monitor does not echo this back.   */
                s_ryo_prev = cur_ryo + delta;
                recomp_printf("[RyoSync] Applied +%d ryo (%d -> %d)\n",
                              (int)delta, (int)cur_ryo, (int)(cur_ryo + delta));
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
        s_prev_player_count = 0;
        s_ds_initialized = 0;  /* reset damage-sync baseline on connect */
        s_ryo_initialized = 0; /* reset ryo-sync baseline on connect    */
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
        s_prev_player_count = 0;
        s_ds_initialized = 0;  /* reset damage-sync baseline on disconnect */
        s_ryo_initialized = 0; /* reset ryo-sync baseline on disconnect   */
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

    /* ── Detect new players joining and push state to them ───────────── */
    {
        int cur_count = get_player_count();
        if (cur_count > s_prev_player_count && s_prev_player_count > 0)
        {
            /* At least one new player joined.  Trigger an immediate full push
             * so they receive all items regardless of the queue or cooldown.
             * Skip the cooldown check here – a new join always warrants a push. */
            recomp_printf("[ItemSync] New player joined (%d->%d) – triggering full state push.\n",
                          s_prev_player_count, cur_count);
            reset_caches();
            start_full_push();
            s_push_cooldown = PUSH_COOLDOWN_FRAMES;
        }
        /* Only update the previous count when the room is non-empty to avoid
         * resetting the baseline during transient 0-player moments.           */
        if (cur_count > 0)
            s_prev_player_count = cur_count;
    }

    /* ── Process incoming packets ─────────────────────────────────────── */
    process_incoming_packets();

    /* ── Damage sync ─────────────────────────────────────────────────── */
    /* Monitor the active character's HP each frame.  When it drops without
     * a character switch, broadcast the delta as a DAMAGE_SYNC packet to
     * all teammates on the same team.  Incoming DAMAGE_SYNC packets are
     * handled in process_incoming_packets() above; s_ds_prev_hp is updated
     * there so the monitor loop never echoes received damage back out.     */
    if (!valid)
    {
        s_ds_initialized = 0;
    }
    else
    {
        unsigned char ds_cur_hp = DS_HP_READ();
        unsigned int ds_cur_char = DS_CHAR_IDX();

        if (!s_ds_initialized)
        {
            /* First frame with a loaded save – capture baseline. */
            s_ds_prev_hp = ds_cur_hp;
            s_ds_prev_char = ds_cur_char;
            s_ds_initialized = 1;
        }
        else if (ds_cur_char != s_ds_prev_char)
        {
            /* Character switch: new character may have different HP.
             * Update the baseline without broadcasting anything.           */
            s_ds_prev_hp = ds_cur_hp;
            s_ds_prev_char = ds_cur_char;
        }
        else if (ds_cur_hp < s_ds_prev_hp && is_connected)
        {
            /* HP decreased this frame – the active character took damage.  */
            unsigned char damage = (unsigned char)(s_ds_prev_hp - ds_cur_hp);

            if (recomp_get_config_u32("anchor_damage_sync") == 0)
            {
                /* Build JSON payload {"damage":N} without printf.          */
                char ds_payload[20];
                char *wp = ds_payload;
                *wp++ = '{';
                *wp++ = '"';
                *wp++ = 'd';
                *wp++ = 'a';
                *wp++ = 'm';
                *wp++ = 'a';
                *wp++ = 'g';
                *wp++ = 'e';
                *wp++ = '"';
                *wp++ = ':';
                if (damage >= 100)
                    *wp++ = (char)('0' + damage / 100);
                if (damage >= 10)
                    *wp++ = (char)('0' + (damage / 10) % 10);
                *wp++ = (char)('0' + damage % 10);
                *wp++ = '}';
                *wp = '\0';

                anchor_send_custom_packet("DAMAGE_SYNC", ds_payload, "", 0, 0);
                recomp_printf("[DamageSync] Sent damage=%d to team (HP: %d -> %d)\n",
                              (int)damage, (int)s_ds_prev_hp, (int)ds_cur_hp);
            }
            s_ds_prev_hp = ds_cur_hp;
        }
        else if (ds_cur_hp > s_ds_prev_hp && is_connected)
        {
            /* HP increased this frame – the active character was healed.   */
            unsigned char healed = (unsigned char)(ds_cur_hp - s_ds_prev_hp);

            if (recomp_get_config_u32("anchor_damage_sync") == 0)
            {
                /* Build JSON payload {"heal":N} without printf.            */
                char hs_payload[18];
                char *wp = hs_payload;
                *wp++ = '{';
                *wp++ = '"';
                *wp++ = 'h';
                *wp++ = 'e';
                *wp++ = 'a';
                *wp++ = 'l';
                *wp++ = '"';
                *wp++ = ':';
                if (healed >= 100)
                    *wp++ = (char)('0' + healed / 100);
                if (healed >= 10)
                    *wp++ = (char)('0' + (healed / 10) % 10);
                *wp++ = (char)('0' + healed % 10);
                *wp++ = '}';
                *wp = '\0';

                anchor_send_custom_packet("HEAL_SYNC", hs_payload, "", 0, 0);
                recomp_printf("[HealSync] Sent heal=%d to team (HP: %d -> %d)\n",
                              (int)healed, (int)s_ds_prev_hp, (int)ds_cur_hp);
            }
            s_ds_prev_hp = ds_cur_hp;
        }
        else
        {
            s_ds_prev_hp = ds_cur_hp;
            s_ds_prev_char = ds_cur_char;
        }
    }

    /* ── Ryo delta sync ───────────────────────────────────────────────── */
    /* Broadcasts only the amount gained (delta) to teammates so each     */
    /* player's balance increases by the same amount picked up.  Late    */
    /* joiners keep their own balance until the next pickup occurs.       */
    if (!valid)
    {
        s_ryo_initialized = 0;
    }
    else
    {
        signed int ryo_cur = DS_RYO_READ();
        if (!s_ryo_initialized)
        {
            s_ryo_prev = ryo_cur;
            s_ryo_initialized = 1;
        }
        else if (ryo_cur > s_ryo_prev && is_connected)
        {
            signed int delta = ryo_cur - s_ryo_prev;

            /* Build JSON payload {"ryo":N} without printf.                 */
            char ryo_payload[24];
            char *wp = ryo_payload;
            char tmp[10];
            int n_digits = 0;
            signed int v = delta;
            *wp++ = '{';
            *wp++ = '"';
            *wp++ = 'r';
            *wp++ = 'y';
            *wp++ = 'o';
            *wp++ = '"';
            *wp++ = ':';
            if (v == 0)
            {
                *wp++ = '0';
            }
            else
            {
                while (v > 0)
                {
                    tmp[n_digits++] = (char)('0' + v % 10);
                    v /= 10;
                }
                while (n_digits-- > 0)
                    *wp++ = tmp[n_digits];
            }
            *wp++ = '}';
            *wp = '\0';

            anchor_send_custom_packet("RYO_SYNC", ryo_payload, "", 0, 0);
            recomp_printf("[RyoSync] Sent delta=%d ryo (%d -> %d)\n",
                          (int)delta, (int)s_ryo_prev, (int)ryo_cur);
            s_ryo_prev = ryo_cur;
        }
        else
        {
            s_ryo_prev = ryo_cur;
        }
    }

    /* ── Full push (one step per frame) ──────────────────────────────── */
    if (s_push_cursor >= 0)
    {
        do_push_step();
        return; /* skip monitor pass while pushing to keep frame budget */
    }

    /* ── Incremental monitoring ───────────────────────────────────────── */
    monitor_and_send_changes();
}

/* =========================================================================
   Public debug API
   ========================================================================= */

/**
 * @brief Force a tracked flag into the local save and broadcast it to all
 *        connected players (including the caller) via Anchor.
 *
 * Unlike the regular apply path, this function writes the flag
 * unconditionally, bypassing the use_max and "already-set" guards.
 * The value is also queued on the server (add_to_queue=1) so offline
 * teammates receive it when they reconnect.
 *
 * Called by debug.c when the user clicks "Force" in the debug menu.
 *
 * @param name  Flag name key – must match an entry in s_fields[] or
 *              s_flag_bits[].  Unknown names are silently ignored with a
 *              debug print.
 */
void item_sync_force_flag(const char *name)
{
    int i;

    /* ── 32-bit save-data fields ──────────────────────────────────── */
    for (i = 0; i < NUM_FIELDS; ++i)
    {
        if (!streq(s_fields[i].name, name))
            continue;

        signed int val = 1; /* "obtained" sentinel for all item fields */
        SAVE_WRITE32(s_fields[i].off, val);
        s_fields[i].cached = val; /* suppress re-broadcast by monitor loop */
        anchor_send_flag(name, val, 1);
        recomp_printf("[Debug] Forced field '%s' = %d\n", name, val);
        return;
    }

    /* ── Single-bit flags ────────────────────────────────────────── */
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (!streq(s_flag_bits[i].name, name))
            continue;

        FLAG_SET_BIT(s_flag_bits[i].id);
        s_flag_bits[i].cached = 1; /* suppress re-broadcast by monitor loop */
        anchor_send_flag(name, 1, 1);
        recomp_printf("[Debug] Forced flag '%s' (id=0x%03X)\n",
                      name, s_flag_bits[i].id);
        return;
    }

    recomp_printf("[Debug] item_sync_force_flag: unknown name '%s'\n", name);
}

/**
 * @brief Force a tracked 32-bit field to a specific value and broadcast it.
 *
 * Extends item_sync_force_flag for fields that need values other than 1
 * (e.g. weapon upgrade tiers where 1=Silver and 2=Gold).  Falls back to
 * the standard force path (value=1) for single-bit flag entries.
 *
 * @param name  Flag name key – must match an entry in s_fields[] or
 *              s_flag_bits[].  Unknown names are silently ignored.
 * @param val   The exact value to write (for 32-bit fields).  Ignored
 *              for single-bit flags (always sets the bit).
 */
void item_sync_force_flag_val(const char *name, int val)
{
    int i;

    /* ── 32-bit save-data fields ──────────────────────────────────── */
    for (i = 0; i < NUM_FIELDS; ++i)
    {
        if (!streq(s_fields[i].name, name))
            continue;

        SAVE_WRITE32(s_fields[i].off, val);
        s_fields[i].cached = val; /* suppress re-broadcast by monitor loop */
        anchor_send_flag(name, val, 1);
        recomp_printf("[Debug] Forced field '%s' = %d\n", name, val);
        return;
    }

    /* ── Single-bit flags: ignore val, just set the bit ─────────── */
    item_sync_force_flag(name);
}
