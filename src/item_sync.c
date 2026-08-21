/**
 * @file item_sync.c
 * @brief Randomizer item / flag synchronisation for Anchor multiplayer.
 *
 * Every game frame (RECOMP_HOOK_RETURN on func_80002040_2C40) this module:
 *   1. Drains the Anchor packet queue, applying incoming item updates and
 *      dispatching live MNSG_BOSS_DEFEAT events before durable boss flags.
 *   2. Compares the current save data against a cached snapshot; whenever
 *      a tracked item is newly gained, broadcasts a SET_FLAG packet with
 *      addToQueue=1 so offline teammates receive it upon joining.
 *   3. Answers team-state requests with one compact UPDATE_TEAM_STATE snapshot
 *      instead of broadcasting every non-zero value as a separate packet.
 *
 * Python elects one loaded teammate to answer each REQUEST_TEAM_STATE packet,
 * preventing every team member from publishing the same snapshot.
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
#include "anchor_runtime.h"
#include "anchor_flag_catalog.h"
#include "boss_sync.h"

void anchor_race_on_flag_synced(const char *flag_name, int flag_value);
void anchor_race_on_remote_flag_synced(const char *flag_name, int flag_value);
void anchor_race_on_finish_packet(const char *packet_json);
int anchor_race_is_active(void);
void anchor_race_on_forced_disconnect(void);

/* =========================================================================
   Game save-data symbol (datasyms VRAM 0x8015C608)

   Ghidra xrefs show func_8000B640 clears and seeds this 0x304-byte block for
   a new file, and func_8000B5D0 copies it into the runtime mirror. The mod
   therefore treats it as the authoritative packed flag/item backing store.
   ========================================================================= */
extern unsigned char D_8015C608_15D208[];
extern unsigned short D_800C7AB2;

/* ─── Helpers ──────────────────────────────────────────────────────────── */

/* Read / write a signed 32-bit value at byte offset `off` from the base.
 * `off` may be negative (accesses the stats fields below the flag array). */
#define SAVE_READ32(off) (*(signed int *)((char *)D_8015C608_15D208 + (off)))
#define SAVE_WRITE32(off, val) (*(signed int *)((char *)D_8015C608_15D208 + (off)) = (signed int)(val))

/* Packed flag-bit helpers (flag IDs up to 0x17FF fit in the 0x304-byte array). */
#define FLAG_IS_SET(id) ((D_8015C608_15D208[(unsigned)(id) >> 3] >> ((id) & 7u)) & 1u)
#define FLAG_SET_BIT(id) (D_8015C608_15D208[(unsigned)(id) >> 3] |= (unsigned char)(1u << ((id) & 7u)))

/* Benkei's durable defeat bit controls dialogue, while these native companion
 * bits select the already-met/post-fight bridge layout.  They are derived
 * locally from fl_benkei instead of being independently published: 0x069 is
 * set before the reward scene finishes during an ordinary local victory. */
#define BENKEI_MET_FLAG 0x034u
#define BENKEI_WON_FLAG 0x069u

/* True when a save file is loaded (hp_max lives just before the flag array). */
#define SAVE_HP_MAX_OFFSET (-0x28) /* SAVE_TOTAL_HEALTH – see save_data_tool.h */
static int save_is_loaded(void)
{
    return SAVE_READ32(SAVE_HP_MAX_OFFSET) > 0;
}

int item_sync_save_is_loaded(void)
{
    return save_is_loaded();
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

/** Extract and validate the raw 16-bit game room in a live door event. */
static int get_door_room_id(const char *json, unsigned short *out)
{
    const char *pos = sfind(json, "\"roomId\":");
    signed int value;

    if (!pos)
        return 0;
    pos += 9;
    while (*pos == ' ')
        ++pos;
    if (!parse_int(&pos, &value) || value < 0 || value > 0xFFFF)
        return 0;

    *out = (unsigned short)value;
    return 1;
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

/* Append a signed decimal integer and return the first unwritten byte. */
static char *append_signed_decimal(char *out, signed int value)
{
    char digits[10];
    int count = 0;
    unsigned int magnitude;

    if (value < 0)
    {
        *out++ = '-';
        magnitude = 0u - (unsigned int)value;
    }
    else
    {
        magnitude = (unsigned int)value;
    }

    do
    {
        digits[count++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u);

    while (count-- > 0)
        *out++ = digits[count];
    return out;
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

    /* ── Sasuke recovery quest state ───────────────────────────────── */
    /* The Benkei reward script increments this profile immediately
       before setting FLAG_DEFEATED_BENKEI.  It is separate from both the
       playable-character field and the later battery acquisition flags. */
    {0x260, 0, 1, "sasuke_body"},

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
    /* Congo's health-zero handler sets 0x1A1 before disabling the boss. */
    {0x1A1, 0, "fl_congo_killed"},
    /* FLAG_BEAT_CONGO 0x12D is the later reward-spawn/cutscene flag.    */
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

    /* ── Dungeon door unlock flags ──────────────────────────────────────
       These are the LOCK_* bits consumed by the rando key manager.  They are
       deliberately separate from the adjacent KEY_* pickup bits above: a
       shared lock consumes the team's corresponding shared key credit.       */

    /* Oedo Castle */
    {0x0109, 0, "lk_s_oc_tile"}, /* LOCK_SILVER_OEDO_CASTLE_1F_TILE          */
    {0x010B, 0, "lk_s_oc_turt"}, /* LOCK_SILVER_OEDO_CASTLE_1F_TURTLE        */
    {0x010D, 0, "lk_g_oc_fork"}, /* LOCK_GOLD_OEDO_CASTLE_1F_FORK            */
    {0x010F, 0, "lk_s_oc_pipe"}, /* LOCK_SILVER_OEDO_CASTLE_1F_CHAIN_PIPE    */
    {0x0111, 0, "lk_s_oc_crsh"}, /* LOCK_SILVER_OEDO_CASTLE_2F_CRUSHER       */
    {0x0113, 0, "lk_s_oc_spk"},  /* LOCK_SILVER_OEDO_CASTLE_2F_SPIKE         */

    /* Ghost Toys Castle */
    {0x01AC, 0, "lk_s_gt_flwr"}, /* LOCK_SILVER_GHOST_TOYS_1F_FLOWER         */
    {0x01AE, 0, "lk_s_gt_shgi"}, /* LOCK_SILVER_GHOST_TOYS_1F_SHOGI          */
    {0x01B0, 0, "lk_s_gt_spdar"}, /* LOCK_SILVER_GHOST_TOYS_1F_SPIKE_DAR      */
    {0x01B2, 0, "lk_s_gt_elv"},  /* LOCK_SILVER_GHOST_TOYS_1F_2F_ELEVATOR    */
    {0x01B4, 0, "lk_s_gt_spk2"}, /* LOCK_SILVER_GHOST_TOYS_2F_BIG_SPIKE_2    */
    {0x01B6, 0, "lk_g_gt_spk1"}, /* LOCK_GOLD_GHOST_TOYS_2F_BIG_SPIKE_1      */
    {0x01B8, 0, "lk_d_gt_2f"},   /* LOCK_DIAMOND_GHOST_TOYS_2F               */
    {0x01BA, 0, "lk_s_gt_bill"}, /* LOCK_SILVER_GHOST_TOYS_2F_BILLIARDS      */

    /* Festival Temple Castle */
    {0x016F, 0, "lk_g_ft"},      /* LOCK_GOLD_FESTIVAL_TEMPLE                */
    {0x0170, 0, "lk_s_ft_fork"}, /* LOCK_SILVER_FESTIVAL_TEMPLE_FORK         */

    /* Gourmet Submarine */
    {0x017A, 0, "lk_s_gs_jet2"}, /* LOCK_SILVER_GOURMET_SUB_2F_JETPACK_2     */
    {0x017C, 0, "lk_g_gs_jet1"}, /* LOCK_GOLD_GOURMET_SUB_2F_JETPACK_1       */
    {0x017E, 0, "lk_s_gs_lava"}, /* LOCK_SILVER_GOURMET_SUB_2F_LAVA          */
    {0x0180, 0, "lk_s_gs_uw"},   /* LOCK_SILVER_GOURMET_SUB_2F_UNDERWATER    */
    {0x0182, 0, "lk_s_gs_fox2"}, /* LOCK_SILVER_GOURMET_SUB_3F_FOX_2         */
    {0x0184, 0, "lk_d_gs_fox1"}, /* LOCK_DIAMOND_GOURMET_SUB_3F_FOX_1        */
    {0x0186, 0, "lk_s_gs_tofu"}, /* LOCK_SILVER_GOURMET_SUB_3F_TOFU          */

    /* Gorgeous Musical Castle */
    {0x0188, 0, "lk_g_mc_ent"},  /* LOCK_GOLD_MUSICAL_CASTLE_1_ENTRANCE      */
    {0x018A, 0, "lk_s_mc_tall"}, /* LOCK_SILVER_MUSICAL_CASTLE_1_TALL        */
    {0x018D, 0, "lk_g_mc_fan"},  /* LOCK_GOLD_MUSICAL_CASTLE_1_FAN           */
    {0x018E, 0, "lk_g_mc_mul1"}, /* LOCK_GOLD_MUSICAL_CASTLE_1_MULTI_1       */
    {0x0190, 0, "lk_d_mc_mul2"}, /* LOCK_DIAMOND_MUSICAL_CASTLE_1_MULTI_2    */
    {0x0192, 0, "lk_d_mc2_ent"}, /* LOCK_DIAMOND_MUSICAL_CASTLE_2_ENTRANCE   */

    /* Special training-key door */
    {0x0196, 0, "lk_sp_bizen"},  /* LOCK_SPECIAL_BIZEN                        */

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
    /* 0x029  Ushiwaka handed over the Achilles' Heel.  The item field at
       0x104 alone does not advance his dialogue past the fishing reward. */
    {0x029, 0, "fl_achilles"},

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
   Live collectible despawn

   SET_FLAG updates the durable save state, but an overworld pickup that was
   already instantiated does not re-run its initializer.  Remember only
   remotely-applied visual checks in the receiver's current room, then let the
   common actor finalizer retire the matching live pickup with the same
   remove-pending bit used by the native pickup initializers.

   This is deliberately an allowlist.  Miracle items use boss/cutscene actors
   and never enter this path.
   ========================================================================= */

#define ENTITY_MR_ELLY_FANT 0x0086u
#define ENTITY_MR_ARROW 0x0087u
#define ENTITY_SILVER_DOLL 0x0088u
#define ENTITY_GOLD_DOLL 0x0089u
#define ENTITY_DUNGEON_KEY 0x0193u

#define ACTOR_ENTITY_ID(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0x5C))
#define ACTOR_STATUS(actor) \
    (*(volatile unsigned int *)((char *)(actor) + 0x68))
#define ACTOR_FLAG_PARAM(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0xD0))
#define ACTOR_KEY_FLAG_PARAM(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0xD4))
#define ACTOR_STATUS_REMOVE_PENDING 0x00000002u

#define ACTOR_DOOR_LOCK_FLAG(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0xD4))
#define ACTOR_DOOR_INTERACTION(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0xDA))
#define ACTOR_DOOR_STATE(actor) \
    (*(volatile unsigned int *)((char *)(actor) + 0xEC))
#define DOOR_STATE_UNLOCKING 0x00000001u
#define DOOR_STATE_UNLOCKED 0x00000002u
#define DOOR_STATE_INTERACTION 0x00000004u

static unsigned char s_visual_pending_fields[NUM_FIELDS];
static unsigned char s_visual_pending_flags[NUM_FLAGS];
static unsigned short s_visual_pending_room;
static unsigned char s_visual_pending_room_valid;

static unsigned char s_door_pending_flags[NUM_FLAGS];
static unsigned short s_door_pending_room;
static unsigned char s_door_pending_room_valid;

static int visual_name_has_prefix(const char *name, const char *prefix)
{
    while (*prefix)
    {
        if (*name++ != *prefix++)
            return 0;
    }
    return 1;
}

static int is_door_unlock_flag(int index)
{
    return index >= 0 && index < NUM_FLAGS &&
           visual_name_has_prefix(s_flag_bits[index].name, "lk_");
}

static int is_door_unlock_name(const char *name)
{
    return name && visual_name_has_prefix(name, "lk_");
}

static int is_visual_collectible_field(int index)
{
    const char *name = s_fields[index].name;
    return visual_name_has_prefix(name, "mr_ely_") ||
           visual_name_has_prefix(name, "mr_arr_");
}

static int is_visual_collectible_flag(int index)
{
    const char *name = s_flag_bits[index].name;
    return visual_name_has_prefix(name, "ky_") ||
           visual_name_has_prefix(name, "sd_") ||
           visual_name_has_prefix(name, "gd_");
}

static void clear_visual_collectible_pending(void)
{
    int i;

    for (i = 0; i < NUM_FIELDS; ++i)
        s_visual_pending_fields[i] = 0;
    for (i = 0; i < NUM_FLAGS; ++i)
        s_visual_pending_flags[i] = 0;
    s_visual_pending_room_valid = 0;
}

static void prepare_visual_collectible_room(void)
{
    if (s_visual_pending_room_valid &&
        s_visual_pending_room != D_800C7AB2)
        clear_visual_collectible_pending();

    s_visual_pending_room = D_800C7AB2;
    s_visual_pending_room_valid = 1;
}

static void arm_visual_collectible_field(int index)
{
    if (!is_visual_collectible_field(index))
        return;

    prepare_visual_collectible_room();
    s_visual_pending_fields[index] = 1;
}

static void arm_visual_collectible_flag(int index)
{
    if (!is_visual_collectible_flag(index))
        return;

    prepare_visual_collectible_room();
    s_visual_pending_flags[index] = 1;
}

static int consume_visual_collectible_flag(unsigned short flag_id,
                                           const char *prefix)
{
    int i;

    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (!s_visual_pending_flags[i] ||
            s_flag_bits[i].id != flag_id ||
            !visual_name_has_prefix(s_flag_bits[i].name, prefix))
            continue;

        s_visual_pending_flags[i] = 0;
        return FLAG_IS_SET(flag_id) != 0;
    }
    return 0;
}

static int mr_collectible_field_offset(unsigned short entity_id,
                                       unsigned short location_flag)
{
    if (entity_id == ENTITY_MR_ELLY_FANT &&
        location_flag >= 0x00FFu && location_flag <= 0x0103u)
        return 0x26C + (int)(location_flag - 0x00FFu) * 4;

    if (entity_id == ENTITY_MR_ARROW &&
        location_flag >= 0x0104u && location_flag <= 0x0108u)
        return 0x280 + (int)(location_flag - 0x0104u) * 4;

    return -1;
}

static int consume_visual_collectible_field(int field_offset)
{
    int i;

    for (i = 0; i < NUM_FIELDS; ++i)
    {
        if (!s_visual_pending_fields[i] ||
            s_fields[i].off != field_offset ||
            !is_visual_collectible_field(i))
            continue;

        s_visual_pending_fields[i] = 0;
        return SAVE_READ32(field_offset) != 0;
    }
    return 0;
}

/* func_80218F30 runs the common actor finalizer and immediately consumes
 * status bit 1 (0x2) through the game's normal task-removal path. */
RECOMP_HOOK("func_80218F30_5D4400")
void item_sync_despawn_remote_collectible(void *actor, void *unused)
{
    unsigned short entity_id;
    unsigned short check_id;
    int field_offset;
    int should_remove = 0;

    (void)unused;

    if (!actor || !s_visual_pending_room_valid || !save_is_loaded())
        return;
    if (s_visual_pending_room != D_800C7AB2)
    {
        clear_visual_collectible_pending();
        return;
    }
    entity_id = ACTOR_ENTITY_ID(actor);
    switch (entity_id)
    {
    case ENTITY_SILVER_DOLL:
        check_id = ACTOR_FLAG_PARAM(actor);
        should_remove = consume_visual_collectible_flag(check_id, "sd_");
        break;
    case ENTITY_GOLD_DOLL:
        check_id = ACTOR_FLAG_PARAM(actor);
        should_remove = consume_visual_collectible_flag(check_id, "gd_");
        break;
    case ENTITY_DUNGEON_KEY:
        check_id = ACTOR_KEY_FLAG_PARAM(actor);
        should_remove = consume_visual_collectible_flag(check_id, "ky_");
        break;
    case ENTITY_MR_ELLY_FANT:
    case ENTITY_MR_ARROW:
        check_id = ACTOR_FLAG_PARAM(actor);
        field_offset = mr_collectible_field_offset(entity_id, check_id);
        if (field_offset >= 0)
            should_remove = consume_visual_collectible_field(field_offset);
        break;
    default:
        break;
    }

    /* Consume an exact marker even when native pickup code has already armed
       removal.  This prevents the one-shot from leaking to another actor. */
    if (!should_remove ||
        (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING) != 0)
        return;

    ACTOR_STATUS(actor) |= ACTOR_STATUS_REMOVE_PENDING;
    recomp_printf("[ItemSync] Despawned remote collectible entity=0x%X check=0x%X room=0x%X.\n",
                  entity_id, check_id, D_800C7AB2);
}

/* =========================================================================
   Live locked-door transition

   A LOCK_* save bit prevents a padlock child from being spawned when a room
   is built, but changing that bit does not refresh a door already in memory.
   Arm only newly-applied remote unlocks in the receiver's current room.  If
   the packet wins the race with actor construction, the initializer consumes
   the marker and builds the already-unlocked door normally.  Otherwise the
   active door update starts the native padlock-removal state machine.
   ========================================================================= */

static void clear_door_unlock_pending(void)
{
    int i;

    for (i = 0; i < NUM_FLAGS; ++i)
        s_door_pending_flags[i] = 0;
    s_door_pending_room_valid = 0;
}

static void arm_door_unlock_flag(int index)
{
    if (!is_door_unlock_flag(index))
        return;

    if (s_door_pending_room_valid &&
        s_door_pending_room != D_800C7AB2)
        clear_door_unlock_pending();

    s_door_pending_room = D_800C7AB2;
    s_door_pending_room_valid = 1;
    s_door_pending_flags[index] = 1;
}

static int consume_door_unlock_flag(unsigned short flag_id)
{
    int i;

    if (!s_door_pending_room_valid)
        return 0;
    if (s_door_pending_room != D_800C7AB2)
    {
        clear_door_unlock_pending();
        return 0;
    }

    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (!s_door_pending_flags[i] ||
            s_flag_bits[i].id != flag_id ||
            !is_door_unlock_flag(i))
            continue;

        s_door_pending_flags[i] = 0;
        return FLAG_IS_SET(flag_id) != 0;
    }
    return 0;
}

/* A packet received before this actor exists already changed the save bit.
 * Consume its one-shot now and let the native initializer skip spawning the
 * padlock child.  Do not enter the live animation path: there is no child to
 * signal completion back to the parent in this ordering. */
RECOMP_HOOK("func_08000AA8_6F3F88")
void item_sync_prepare_remote_unlocked_door(void *actor, void *unused)
{
    unsigned short lock_flag;

    (void)unused;

    if (!actor || !save_is_loaded())
        return;

    lock_flag = ACTOR_DOOR_LOCK_FLAG(actor);
    if (consume_door_unlock_flag(lock_flag))
    {
        recomp_printf("[DoorSync] Initialized remote-unlocked door flag=0x%X room=0x%X.\n",
                      lock_flag, D_800C7AB2);
    }
}

/* Arm the same lock-child state used by a local successful key use, after the
 * key has already been accounted for by the synchronized LOCK_* bit.  Clearing
 * the interaction latch prevents the original callback from also entering its
 * key-use/already-open branch during this frame.  The child observes bit 0x1,
 * plays its native disappearance, and sets bit 0x2 on completion.  Leave the
 * parent callback alone so a remote unlock never pulls this player through the
 * doorway; their next normal interaction uses the now-set flag without a key. */
RECOMP_HOOK("func_08001020_6F4500")
void item_sync_unlock_remote_door(void *actor, void *unused)
{
    unsigned short lock_flag;
    unsigned int state;

    (void)unused;

    if (!actor || !save_is_loaded())
        return;

    lock_flag = ACTOR_DOOR_LOCK_FLAG(actor);
    if (!consume_door_unlock_flag(lock_flag))
        return;

    state = ACTOR_DOOR_STATE(actor);
    if ((state & (DOOR_STATE_UNLOCKING | DOOR_STATE_UNLOCKED)) != 0)
        return;

    ACTOR_DOOR_INTERACTION(actor) = 0;
    ACTOR_DOOR_STATE(actor) =
        (state & ~DOOR_STATE_INTERACTION) | DOOR_STATE_UNLOCKING;

    recomp_printf("[DoorSync] Started remote unlock flag=0x%X room=0x%X.\n",
                  lock_flag, D_800C7AB2);
}

static int send_door_unlock_event(const char *flag_name,
                                  unsigned short source_room)
{
    char payload[80];
    char *wp = payload;
    char *team_id;
    int sent;
    const char *name = flag_name;

    if (!is_door_unlock_name(flag_name))
        return 0;

    *wp++ = '{';
    *wp++ = '"';
    *wp++ = 'f';
    *wp++ = 'l';
    *wp++ = 'a';
    *wp++ = 'g';
    *wp++ = '"';
    *wp++ = ':';
    *wp++ = '"';
    while (*name && wp < payload + sizeof(payload) - 24)
        *wp++ = *name++;
    *wp++ = '"';
    *wp++ = ',';
    *wp++ = '"';
    *wp++ = 'r';
    *wp++ = 'o';
    *wp++ = 'o';
    *wp++ = 'm';
    *wp++ = 'I';
    *wp++ = 'd';
    *wp++ = '"';
    *wp++ = ':';
    wp = append_signed_decimal(wp, (signed int)source_room);
    *wp++ = '}';
    *wp = '\0';

    team_id = anchor_get_team_id();
    if (!team_id || !team_id[0])
    {
        if (team_id)
            recomp_free(team_id);
        return 0;
    }

    /* Live room animation is transient.  The queued SET_FLAG sent next is
     * the durable/offline authority for this same lock. */
    sent = anchor_send_custom_packet("MNSG_DOOR_UNLOCK", payload,
                                     team_id, 0, 0);
    recomp_free(team_id);
    return sent;
}

/* =========================================================================
   State machine
   ========================================================================= */

static int s_was_connected = 0;     /* connection state from the previous frame */
static int s_save_was_valid = 0;    /* true if save was loaded last time we checked */

/* A non-negative cursor schedules one compact UPDATE_TEAM_STATE snapshot. */
static int s_push_cursor = -1;

/* Cooldown timer (in frames) before we respond to another REQUEST_TEAM_STATE.
 * Prevents response floods when multiple requests arrive close together.    */
static int s_push_cooldown = 0;
#define PUSH_COOLDOWN_FRAMES 180 /* ~3 seconds @ 60 fps */
static int s_set_flag_send_timer = 0;
#define SET_FLAG_SEND_INTERVAL_FRAMES 4 /* cap durable deltas at ~15/s */
static int s_team_state_request_pending = 0;
static int s_team_snapshot_broadcast_timer = -1;
#define TEAM_SNAPSHOT_BROADCAST_DELAY_FRAMES 90

/* A boss progression bit received during the corresponding live encounter is
 * held until the native encounter has ended.  This also protects compact team
 * snapshots, not just individual SET_FLAG packets. */
static unsigned char s_pending_boss_flags[NUM_FLAGS];
/* A live defeat event has applied lethal damage to the local boss.  In that case the
 * durable flag must wait for the game's native death task, even after the
 * actor's active bit is cleared during its death animation. */
static unsigned char s_pending_native_boss_defeat[NUM_FLAGS];
static unsigned char s_boss_defeat_announced[NUM_FLAGS];
/* Live room-scoped door events are sent before their queued durable bit. */
static unsigned char s_door_unlock_announced[NUM_FLAGS];
static unsigned short s_door_unlock_source_room[NUM_FLAGS];
static unsigned char s_door_unlock_source_room_valid[NUM_FLAGS];
/* The transient live event reports a remote race finish immediately.  A
 * durable-only receipt defers that report with its flag until native teardown.
 * Remember the first report so the terminal path cannot duplicate its toast. */
static unsigned char s_remote_boss_completion_notified[NUM_FLAGS];
/* Benkei's reward script increments the dead-Sasuke profile before it sets
 * fl_benkei.  Hold a teammate's profile delta while the local native fight is
 * still finishing so the script cannot increment an already-applied value and
 * accidentally skip a recovery stage. */
/* Use -1 as the empty sentinel because zero is a valid profile value and
 * therefore must remain representable while a remote update is pending. */
static signed int s_pending_benkei_sasuke_profile = -1;

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

/* Remaining lives: runtime copy at D_8015C608 - 0x1c, save copy at +0x78.
 * Ghidra shows FUN_8000b578 decrementing the runtime value and game-overing
 * when the result is zero, so one-life mode keeps this at 1 while alive. */
#define DS_LIVES_OFFSET (-0x1C)
#define DS_SAVE_LIVES_OFFSET (0x78)
#define DS_LIVES_READ() SAVE_READ32(DS_LIVES_OFFSET)
#define DS_LIVES_WRITE(v)        \
    do                           \
    {                            \
        SAVE_WRITE32(DS_LIVES_OFFSET, (v));      \
        SAVE_WRITE32(DS_SAVE_LIVES_OFFSET, (v)); \
    } while (0)

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
 * Extra changes are deferred by leaving their cache un-updated. */
#define MAX_SENDS_PER_FRAME 1

#define PUSH_IDLE -1
#define TEAM_STATE_JSON_MAX 16384

static char s_team_state_json[TEAM_STATE_JSON_MAX];

/* Schedule one compact response to a team-state request. */
static void schedule_team_state_response(void)
{
    s_push_cursor = 0;
    recomp_printf("[ItemSync] Compact team-state snapshot scheduled.\n");
}

/* Reset cached values while no valid local save is available. */
static void reset_caches(void)
{
    int i;

    boss_sync_reset();
    if (!boss_sync_has_active_encounter("fl_benkei"))
        s_pending_benkei_sasuke_profile = -1;
    for (i = 0; i < NUM_FIELDS; ++i)
        s_fields[i].cached = 0;
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        int preserve_remote_notification =
            s_remote_boss_completion_notified[i] &&
            boss_sync_has_active_encounter(s_flag_bits[i].name);

        s_flag_bits[i].cached = 0;
        s_boss_defeat_announced[i] = 0;
        s_door_unlock_announced[i] = 0;
        s_door_unlock_source_room[i] = 0;
        s_door_unlock_source_room_valid[i] = 0;
        s_pending_boss_flags[i] = 0;
        s_pending_native_boss_defeat[i] = 0;
        if (!preserve_remote_notification)
            s_remote_boss_completion_notified[i] = 0;
    }
}

/* Capture the loaded save as the local baseline without broadcasting it. */
static void capture_caches(void)
{
    int i;

    for (i = 0; i < NUM_FIELDS; ++i)
        s_fields[i].cached = SAVE_READ32(s_fields[i].off);
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        s_flag_bits[i].cached = (unsigned char)FLAG_IS_SET(s_flag_bits[i].id);
        s_boss_defeat_announced[i] = s_flag_bits[i].cached;
        if (s_flag_bits[i].cached &&
            boss_sync_is_completion_flag(s_flag_bits[i].name))
            s_remote_boss_completion_notified[i] = 1;
    }
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
 * Uses the same catalog that backs the debug and race-config screens.
 */
static const char *get_flag_display_name(const char *n)
{
    return anchor_flag_catalog_find_display(n);
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

/* Benkei's native controller does not use the durable 0x033 dialogue gate to
 * choose its bridge layout.  Keep the implied prerequisite and post-victory
 * selector aligned without publishing the transient 0x069 bit separately.
 * Mark fl_met_benkei cached as well so a remotely-derived prerequisite is not
 * echoed back to the team as a new local event. */
void item_sync_apply_benkei_postfight_state(void)
{
    int i;
    int changed = 0;

    if (!FLAG_IS_SET(BENKEI_MET_FLAG))
    {
        FLAG_SET_BIT(BENKEI_MET_FLAG);
        changed = 1;
    }
    if (!FLAG_IS_SET(BENKEI_WON_FLAG))
    {
        FLAG_SET_BIT(BENKEI_WON_FLAG);
        changed = 1;
    }

    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (s_flag_bits[i].id == BENKEI_MET_FLAG)
        {
            s_flag_bits[i].cached = 1;
            break;
        }
    }

    if (changed)
        recomp_printf("[ItemSync] Applied Benkei's post-fight bridge state.\n");
}

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

        /* Positive incoming snapshots/deltas are authoritative even if this
           save already contains the value.  Arming on a no-op also repairs an
           already-instantiated stale actor after replay or reconnect. */
        if (val > 0)
            arm_visual_collectible_field(i);

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

        /* 0x033 alone only changes scenario 0x30C's dialogue.  Pair the
         * native world-state prerequisites before applying it so a remote
         * completion builds the interaction at Benkei rather than leaving
         * the challenge trigger at the bridge entrance.  This also heals a
         * save produced by an older build even when 0x033 is already set. */
        if (val && streq(flag_name, "fl_benkei"))
            item_sync_apply_benkei_postfight_state();

        if (val)
            arm_visual_collectible_flag(i);

        if (val && !FLAG_IS_SET(s_flag_bits[i].id))
        {
            FLAG_SET_BIT(s_flag_bits[i].id);
            s_flag_bits[i].cached = 1;
            /* A queued delta or compact snapshot can arrive after this room
             * already instantiated the locked door.  Arm only the real 0->1
             * write: the initializer hook safely consumes pre-init arrivals,
             * while a positive no-op may have no lock child to animate. */
            arm_door_unlock_flag(i);
            if (boss_sync_is_completion_flag(flag_name))
                s_boss_defeat_announced[i] = 1;
            recomp_printf("[ItemSync] Applied flag '%s' (id=0x%X)\n",
                          flag_name, s_flag_bits[i].id);
            return get_flag_display_name(flag_name);
        }
        return 0;
    }
    /* Unknown flag name – ignore (could be from a later version). */
    return 0;
}

/* Apply ordinary incoming values immediately, except for the save counter
 * mutated by Benkei's own reward scene.  The exact native fl_benkei write
 * commits this deferred value after the scene has performed its increment. */
static const char *apply_incoming_value(const char *flag_name, signed int val)
{
    if (streq(flag_name, "sasuke_body") && val > SAVE_READ32(0x260) &&
        boss_sync_has_active_encounter("fl_benkei"))
    {
        if (val > s_pending_benkei_sasuke_profile)
            s_pending_benkei_sasuke_profile = val;
        recomp_printf("[BossSync] Deferred Sasuke body profile until Benkei's reward script.\n");
        return 0;
    }

    return apply_flag(flag_name, val);
}

static int sync_flag_index(const char *flag_name)
{
    int i;
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (streq(s_flag_bits[i].name, flag_name))
            return i;
    }
    return -1;
}

/* Native boss hooks can announce the live defeat before the durable save bit
 * changes.  Remember that send so the ordinary flag monitor does not emit the
 * same transient event again when the later progression bit rises. */
void item_sync_mark_boss_defeat_announced(const char *flag_name)
{
    int index = sync_flag_index(flag_name);

    if (index >= 0 && boss_sync_is_completion_flag(flag_name))
        s_boss_defeat_announced[index] = 1;
}

static void notify_remote_boss_completion(int index)
{
    const char *display;

    if (index < 0 || index >= NUM_FLAGS ||
        s_remote_boss_completion_notified[index])
        return;

    anchor_race_on_remote_flag_synced(s_flag_bits[index].name, 1);
    display = get_flag_display_name(s_flag_bits[index].name);
    if (display)
        item_notif_push("Received from team", display);
    s_remote_boss_completion_notified[index] = 1;
}

/* Commit remote progression only after the boss-specific native terminal has
 * proved that the local death path completed.  Align the cache with the write
 * so it is not mistaken for a new local victory and echoed back to the team. */
void item_sync_commit_boss_completion(const char *flag_name)
{
    int index = sync_flag_index(flag_name);
    const char *profile_display = 0;

    if (index < 0 || !boss_sync_is_completion_flag(flag_name))
        return;

    apply_flag(flag_name, 1);
    if (streq(flag_name, "fl_benkei") && s_pending_benkei_sasuke_profile >= 0)
    {
        profile_display = apply_flag("sasuke_body",
                                     s_pending_benkei_sasuke_profile);
        s_pending_benkei_sasuke_profile = -1;
        if (profile_display)
            item_notif_push("Received from team", profile_display);
    }
    notify_remote_boss_completion(index);
    s_flag_bits[index].cached = 1;
    s_boss_defeat_announced[index] = 1;
    s_pending_boss_flags[index] = 0;
    s_pending_native_boss_defeat[index] = 0;
    recomp_printf("[BossSync] Committed progression '%s' at native completion.\n",
                  flag_name);
}

/* Apply a received progression value unless doing so would inject a boss flag
 * into the middle of that client's still-live encounter. */
static const char *apply_incoming_flag(const char *flag_name, signed int val)
{
    int index = sync_flag_index(flag_name);

    /* A boss completion may arrive from a durable SET_FLAG or compact team
     * snapshot after the transient live event has passed.  If that boss has a
     * validated local native-death path, queue it before writing progression. */
    if (index >= 0 && val &&
        boss_sync_is_completion_flag(flag_name) &&
        boss_sync_apply_remote_defeat(flag_name))
    {
        if (!s_pending_boss_flags[index])
        {
            recomp_printf("[BossSync] Deferred '%s' to the native boss defeat handler.\n",
                          flag_name);
        }
        s_pending_boss_flags[index] = 1;
        s_pending_native_boss_defeat[index] = 1;
        s_boss_defeat_announced[index] = 1;
        return 0;
    }

    if (index >= 0 && val && !FLAG_IS_SET(s_flag_bits[index].id) &&
        boss_sync_should_defer_flag(flag_name) &&
        (s_pending_native_boss_defeat[index] ||
         boss_sync_has_active_encounter(flag_name)))
    {
        if (!s_pending_boss_flags[index])
        {
            recomp_printf("[BossSync] Deferred '%s' until the local death path finishes.\n",
                          flag_name);
        }
        s_pending_boss_flags[index] = 1;
        return 0;
    }

    if (index >= 0)
    {
        s_pending_boss_flags[index] = 0;
        s_pending_native_boss_defeat[index] = 0;
        if (!val)
            s_remote_boss_completion_notified[index] = 0;
    }
    return apply_incoming_value(flag_name, val);
}

static void apply_pending_boss_flags(void)
{
    int i;

    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (!s_pending_boss_flags[i])
            continue;
        if (FLAG_IS_SET(s_flag_bits[i].id))
        {
            notify_remote_boss_completion(i);
            s_pending_boss_flags[i] = 0;
            s_pending_native_boss_defeat[i] = 0;
            s_flag_bits[i].cached = 1;
            s_boss_defeat_announced[i] = 1;
            continue;
        }
        /* The boss update clears its active bit before its scheduled victory
         * task sets the flag.  Keep waiting while that actor still belongs to
         * this room so SET_FLAG cannot jump ahead of the death animation. */
        if (s_pending_native_boss_defeat[i] &&
            boss_sync_has_local_encounter(s_flag_bits[i].name))
            continue;

        s_pending_native_boss_defeat[i] = 0;
        if (boss_sync_has_active_encounter(s_flag_bits[i].name))
            continue;

        s_pending_boss_flags[i] = 0;
        apply_flag(s_flag_bits[i].name, 1);
        notify_remote_boss_completion(i);
        recomp_printf("[BossSync] Applied deferred progression '%s'.\n",
                      s_flag_bits[i].name);
    }
}

static int get_team_state_value(const char *json, const char *name, signed int *out)
{
    char quoted_name[32];
    char *wp = quoted_name;
    const char *pos;

    *wp++ = '"';
    while (*name && wp < quoted_name + sizeof(quoted_name) - 2)
        *wp++ = *name++;
    *wp++ = '"';
    *wp = '\0';

    pos = sfind(json, quoted_name);
    if (!pos)
        return 0;
    while (*pos && *pos != ':')
        ++pos;
    if (*pos != ':')
        return 0;
    ++pos;
    while (*pos == ' ' || *pos == '\t')
        ++pos;
    return parse_int(&pos, out);
}

/* Apply one compact UPDATE_TEAM_STATE snapshot. Queued SET_FLAG deltas are
 * expanded by Python and arrive immediately after this packet. */
static void apply_team_state(const char *json)
{
    int i;
    int applied = 0;
    signed int value;

    for (i = 0; i < NUM_FIELDS; ++i)
    {
        if (get_team_state_value(json, s_fields[i].name, &value))
        {
            if (apply_incoming_value(s_fields[i].name, value))
                applied++;
        }
    }
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (get_team_state_value(json, s_flag_bits[i].name, &value))
        {
            const char *display =
                apply_incoming_flag(s_flag_bits[i].name, value);

            if (display)
            {
                applied++;
                /* Compact snapshots are remote progression too.  A direct
                 * boss application must be acknowledged before the baseline
                 * capture below marks its now-set bit as already seen. */
                if (value &&
                    boss_sync_is_completion_flag(s_flag_bits[i].name))
                    notify_remote_boss_completion(i);
            }
        }
    }
    capture_caches();
    recomp_printf("[ItemSync] Applied compact team state (%d changes).\n", applied);
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
 * - REQUEST_TEAM_STATE packets from *other* clients schedule one snapshot so
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
                int index = sync_flag_index(fname);
                const char *display = apply_incoming_flag(fname, fval);
                if (index < 0 || !s_pending_boss_flags[index])
                {
                    int acknowledged_boss_completion =
                        index >= 0 && fval &&
                        boss_sync_is_completion_flag(fname) &&
                        s_remote_boss_completion_notified[index];

                    if (!acknowledged_boss_completion &&
                        !is_door_unlock_name(fname))
                        anchor_race_on_remote_flag_synced(fname, (int)fval);
                    if (index >= 0 && fval &&
                        boss_sync_is_completion_flag(fname))
                        s_remote_boss_completion_notified[index] = 1;
                }
                /* Door transitions are world state, not item checks. */
                if (display && !is_door_unlock_name(fname))
                    item_notif_push("Received from team", display);
            }
        }
        else if (is_packet_type(pkt, "MNSG_DOOR_UNLOCK"))
        {
            char fname[32];
            unsigned short source_room;
            unsigned int sender = get_packet_client_id(pkt);

            /* The queued SET_FLAG remains authoritative for clients outside
             * this room.  Only a live event with the exact source room may
             * animate an already-instantiated lock on this client. */
            if ((own_id == 0 || sender == 0 || sender != own_id) &&
                get_flag_name(pkt, fname, (int)sizeof(fname)) &&
                get_door_room_id(pkt, &source_room) &&
                source_room == D_800C7AB2)
            {
                int index = sync_flag_index(fname);
                if (is_door_unlock_flag(index) &&
                    !FLAG_IS_SET(s_flag_bits[index].id))
                {
                    apply_incoming_flag(fname, 1);
                    if (FLAG_IS_SET(s_flag_bits[index].id))
                    {
                        arm_door_unlock_flag(index);
                        recomp_printf("[DoorSync] Accepted live unlock '%s' room=0x%X.\n",
                                      fname, source_room);
                    }
                }
            }
        }
        else if (is_packet_type(pkt, "MNSG_BOSS_DEFEAT"))
        {
            char fname[32];
            unsigned int sender = get_packet_client_id(pkt);

            /* Some Anchor servers echo team packets to the sender.  Never run
             * the native transition twice on the client that won the fight. */
            if ((own_id == 0 || sender == 0 || sender != own_id) &&
                get_flag_name(pkt, fname, (int)sizeof(fname)))
            {
                int index = sync_flag_index(fname);
                /* Do not gate the live transition on the durable bit.  A save
                 * produced by an older flag-only sync may contain it while
                 * the corresponding in-room boss is still alive. */
                if (index >= 0 && !s_pending_native_boss_defeat[index] &&
                    boss_sync_apply_remote_defeat(fname))
                {
                    /* Hold progression until the boss-specific native damage,
                     * death state, and completion callback remain ordered. */
                    notify_remote_boss_completion(index);
                    s_pending_boss_flags[index] = 1;
                    s_pending_native_boss_defeat[index] = 1;
                    s_boss_defeat_announced[index] = 1;
                }
            }
        }
        else if (is_packet_type(pkt, "UPDATE_TEAM_STATE"))
        {
            apply_team_state(pkt);
        }
        else if (is_packet_type(pkt, "MNSG_TEAM_STATE"))
        {
            apply_team_state(pkt);
        }
        else if (is_packet_type(pkt, "REQUEST_TEAM_STATE"))
        {
            unsigned int requester = get_packet_client_id(pkt);
            /* Skip own requests (shouldn't normally arrive, but be safe). */
            int is_self = (own_id != 0 && requester == own_id);
            if (!is_self && s_push_cooldown <= 0 && save_is_loaded())
            {
                recomp_printf("[ItemSync] REQUEST_TEAM_STATE from client %u – snapshot scheduled.\n",
                              requester);
                schedule_team_state_response();
                s_push_cooldown = PUSH_COOLDOWN_FRAMES;
            }
        }
        else if (is_packet_type(pkt, "DAMAGE_SYNC"))
        {
            signed int dmg = 0;
            if (anchor_race_is_active() && anchor_runtime_damage_sync_enabled() &&
                get_ds_damage(pkt, &dmg) && dmg > 0 && save_is_loaded())
            {
                unsigned char cur_hp = DS_HP_READ();
                if (cur_hp > 0)
                {
                    unsigned char new_hp =
                        (anchor_race_is_active() && anchor_runtime_no_hit_enabled())
                            ? 0u
                            : (((signed int)cur_hp - dmg <= 0)
                                   ? 0u
                                   : (unsigned char)(cur_hp - dmg));
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
            if (anchor_race_is_active() && anchor_runtime_damage_sync_enabled() &&
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
            if (anchor_race_is_active() && anchor_runtime_ryo_sync_enabled() &&
                get_ds_ryo(pkt, &delta) && delta != 0 && save_is_loaded())
            {
                signed int cur_ryo = DS_RYO_READ();
                signed int new_ryo = cur_ryo + delta;
                if (new_ryo < 0)
                    new_ryo = 0;
                DS_RYO_WRITE(new_ryo);
                /* Update baseline so the monitor does not echo this back.   */
                s_ryo_prev = new_ryo;
                recomp_printf("[RyoSync] Applied delta=%d ryo (%d -> %d)\n",
                              (int)delta, (int)cur_ryo, (int)new_ryo);
            }
        }
        else if (is_packet_type(pkt, "MNSG_RACE_FINISH"))
        {
            anchor_race_on_finish_packet(pkt);
        }
        /* All other packet types (ALL_CLIENT_STATE, UPDATE_CLIENT_STATE,
         * etc.) are consumed here.  Python's internal
         * _player_states dict is maintained by the recv thread, so the
         * player-list overlay continues to work correctly without these
         * packets being processed on the C side.                            */

        recomp_free(pkt);
    }
}

/* =========================================================================
   Outgoing – compact team-state snapshot
   ========================================================================= */

static int team_state_append_char(char **wp, char *end, char c)
{
    if (*wp >= end - 1)
        return 0;
    *(*wp)++ = c;
    return 1;
}

static int team_state_append_text(char **wp, char *end, const char *text)
{
    while (*text)
    {
        if (!team_state_append_char(wp, end, *text++))
            return 0;
    }
    return 1;
}

static int team_state_append_entry(char **wp, char *end, const char *name,
                                   signed int value, int *wrote_entry)
{
    char value_text[12];
    char *value_end = append_signed_decimal(value_text, value);

    *value_end = '\0';
    if (*wrote_entry && !team_state_append_char(wp, end, ','))
        return 0;
    if (!team_state_append_char(wp, end, '"') ||
        !team_state_append_text(wp, end, name) ||
        !team_state_append_char(wp, end, '"') ||
        !team_state_append_char(wp, end, ':') ||
        !team_state_append_text(wp, end, value_text))
        return 0;
    *wrote_entry = 1;
    return 1;
}

static int build_team_state_json(void)
{
    char *wp = s_team_state_json;
    char *end = s_team_state_json + TEAM_STATE_JSON_MAX;
    int wrote_entry = 0;
    int i;

    if (!team_state_append_char(&wp, end, '{'))
        return 0;
    for (i = 0; i < NUM_FIELDS; ++i)
    {
        signed int value = SAVE_READ32(s_fields[i].off);
        if (value != 0 &&
            !team_state_append_entry(&wp, end, s_fields[i].name, value, &wrote_entry))
            return 0;
    }
    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (FLAG_IS_SET(s_flag_bits[i].id) &&
            !team_state_append_entry(&wp, end, s_flag_bits[i].name, 1, &wrote_entry))
            return 0;
    }
    if (!team_state_append_char(&wp, end, '}'))
        return 0;
    *wp = '\0';
    return 1;
}

/* Share this client's merged state once after load/reconnect. This preserves
 * pre-existing local progress without the old hundreds-of-SET_FLAG flood. */
static int broadcast_team_state_snapshot(void)
{
    char *team_id;
    int sent = 0;

    if (!build_team_state_json())
        return 0;
    team_id = anchor_get_team_id();
    if (team_id && team_id[0])
        sent = anchor_send_custom_packet("MNSG_TEAM_STATE", s_team_state_json,
                                         team_id, 0, 0);
    if (team_id)
        recomp_free(team_id);
    if (sent)
        capture_caches();
    return sent;
}

static void send_scheduled_team_state_response(void)
{
    if (s_push_cursor < 0)
        return;

    s_push_cursor = PUSH_IDLE;
    if (build_team_state_json() && anchor_update_team_state(s_team_state_json))
    {
        capture_caches();
        recomp_printf("[ItemSync] Compact team-state snapshot sent.\n");
    }
    else
    {
        recomp_printf("[ItemSync] Compact team-state snapshot failed.\n");
    }
}

/* =========================================================================
   Outgoing – incremental monitoring
   ========================================================================= */

/**
 * @brief Detect and broadcast newly acquired items within the send budget.
 *
 * Runs while no compact team-state response is pending. Compares each tracked
 * value against the cached snapshot; acquisitions are sent as queued
 * SET_FLAG packets at a bounded rate, and deferred values remain uncached so
 * a later frame retries them.
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
            if (sends >= MAX_SENDS_PER_FRAME || s_set_flag_send_timer > 0)
                continue; /* budget exhausted – defer to next frame (cache not updated) */

            if (!anchor_send_flag(s_fields[i].name, (int)cur, 1))
                continue;
            anchor_race_on_flag_synced(s_fields[i].name, (int)cur);
            ++sends;
            s_set_flag_send_timer = SET_FLAG_SEND_INTERVAL_FRAMES;
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
            /* Capture the room on the first frame that observes the native
             * 0->1 transition.  Keep it stable across send-budget deferrals
             * and transient transport failures. */
            if (is_door_unlock_flag(i) &&
                !s_door_unlock_source_room_valid[i])
            {
                s_door_unlock_source_room[i] = D_800C7AB2;
                s_door_unlock_source_room_valid[i] = 1;
            }

            /* Tell teammates in this encounter to run the native boss finish
             * first.  Leave the cache untouched so the durable SET_FLAG is
             * sent on a later frame, after this transient event is ordered. */
            if (boss_sync_is_completion_flag(s_flag_bits[i].name) &&
                !s_boss_defeat_announced[i])
            {
                if (sends >= MAX_SENDS_PER_FRAME)
                    continue;
                if (!boss_sync_send_defeat(s_flag_bits[i].name))
                    continue;
                s_boss_defeat_announced[i] = 1;
                ++sends;
                recomp_printf("[BossSync] Sent native defeat event '%s'.\n",
                              s_flag_bits[i].name);
                continue;
            }

            /* Carry the exact source room in a nonqueued live event before
             * publishing the durable lock bit.  Receivers outside this room
             * ignore the animation event and still inherit the unlocked state
             * through SET_FLAG/snapshots. */
            if (is_door_unlock_flag(i) && !s_door_unlock_announced[i])
            {
                if (sends >= MAX_SENDS_PER_FRAME)
                    continue;
                if (send_door_unlock_event(
                        s_flag_bits[i].name,
                        s_door_unlock_source_room[i]))
                {
                    s_door_unlock_announced[i] = 1;
                    ++sends;
                    recomp_printf("[DoorSync] Sent live unlock '%s' room=0x%X.\n",
                                  s_flag_bits[i].name,
                                  s_door_unlock_source_room[i]);
                    continue;
                }
                /* A transient visual send must never starve the queued save
                 * update.  Fall through and attempt the durable SET_FLAG. */
            }

            if (sends >= MAX_SENDS_PER_FRAME || s_set_flag_send_timer > 0)
                continue; /* defer to next frame */

            if (!anchor_send_flag(s_flag_bits[i].name, 1, 1))
                continue;
            if (!is_door_unlock_flag(i))
                anchor_race_on_flag_synced(s_flag_bits[i].name, 1);
            ++sends;
            s_set_flag_send_timer = SET_FLAG_SEND_INTERVAL_FRAMES;
            recomp_printf("[ItemSync] Sent flag '%s' (id=0x%X)\n",
                          s_flag_bits[i].name, s_flag_bits[i].id);
            if (is_door_unlock_flag(i))
            {
                s_door_unlock_source_room[i] = 0;
                s_door_unlock_source_room_valid[i] = 0;
            }
            if (!is_door_unlock_flag(i))
            {
                const char *display =
                    get_flag_display_name(s_flag_bits[i].name);
                if (display)
                    item_notif_push("Item found!", display);
            }
        }
        else
        {
            s_boss_defeat_announced[i] = 0;
            s_door_unlock_announced[i] = 0;
            s_door_unlock_source_room[i] = 0;
            s_door_unlock_source_room_valid[i] = 0;
            if (boss_sync_is_completion_flag(s_flag_bits[i].name) &&
                !boss_sync_has_active_encounter(s_flag_bits[i].name))
                s_remote_boss_completion_notified[i] = 0;
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

    /* A pending visual check is meaningful only in the room where its remote
       save update arrived.  Dropping it on transition prevents a later visit
       to the same numeric room from consuming a stale one-shot marker. */
    if (s_visual_pending_room_valid &&
        s_visual_pending_room != D_800C7AB2)
        clear_visual_collectible_pending();
    if (s_door_pending_room_valid &&
        s_door_pending_room != D_800C7AB2)
        clear_door_unlock_pending();

    /* ── Tick notification timers (runs even when disconnected so in-flight
       toasts from a just-dropped connection still expire gracefully). ────── */
    item_notif_tick();

    /* ── Tick packet-rate cooldowns regardless of connection state ───── */
    if (s_push_cooldown > 0)
        --s_push_cooldown;
    if (s_set_flag_send_timer > 0)
        --s_set_flag_send_timer;
    if (s_team_snapshot_broadcast_timer > 0)
        --s_team_snapshot_broadcast_timer;

    /* ── Connection transitions ──────────────────────────────────────── */
    if (is_connected && !s_was_connected)
    {
        /* Request the stored/online team snapshot. The local loaded save is
         * captured as a baseline below instead of flooding SET_FLAG packets. */
        recomp_printf("[ItemSync] Connected – requesting compact team state.\n");
        reset_caches();
        s_push_cooldown = 0;
        s_set_flag_send_timer = 0;
        s_team_state_request_pending = 1;
        s_team_snapshot_broadcast_timer = -1;
        s_save_was_valid = 0;
        s_ds_initialized = 0;  /* reset damage-sync baseline on connect */
        s_ryo_initialized = 0; /* reset ryo-sync baseline on connect    */
    }
    if (!is_connected && s_was_connected)
    {
        /* Disconnected: cancel any pending team-state response. */
        recomp_printf("[ItemSync] Disconnected – clearing sync state.\n");
        if (anchor_race_is_active())
            anchor_race_on_forced_disconnect();
        s_push_cursor = PUSH_IDLE;
        s_push_cooldown = 0;
        s_set_flag_send_timer = 0;
        s_team_state_request_pending = 0;
        s_team_snapshot_broadcast_timer = -1;
        clear_visual_collectible_pending();
        clear_door_unlock_pending();
        boss_sync_reset();
        s_ds_initialized = 0;  /* reset damage-sync baseline on disconnect */
        s_ryo_initialized = 0; /* reset ryo-sync baseline on disconnect   */
    }
    s_was_connected = is_connected;

    if (!is_connected)
    {
        /* Keep watching save unload while offline.  An accepted native boss
         * transition may intentionally survive a same-save disconnect, but it
         * must be force-cleared before another file can be loaded. */
        int offline_valid = save_is_loaded();
        if (!offline_valid)
            boss_sync_reset();
        s_save_was_valid = offline_valid;
        return;
    }

    /* ── Wait for a loaded save file ─────────────────────────────────── */
    int valid = save_is_loaded();

    if (valid != s_save_was_valid)
    {
        anchor_set_save_loaded(valid);
        if (valid)
        {
            capture_caches();
            s_team_snapshot_broadcast_timer = TEAM_SNAPSHOT_BROADCAST_DELAY_FRAMES;
            recomp_printf("[ItemSync] Save loaded – local sync baseline captured.\n");
        }
        else
        {
            reset_caches();
            clear_visual_collectible_pending();
            clear_door_unlock_pending();
            s_push_cursor = PUSH_IDLE;
            s_team_snapshot_broadcast_timer = -1;
        }
    }
    s_save_was_valid = valid;

    if (!valid)
        return;

    /* Wait for the server-assigned id so the request cannot be echoed back as
     * a clientId=0 room broadcast. */
    if (s_team_state_request_pending && anchor_get_client_id() != 0)
    {
        if (anchor_request_team_state(""))
        {
            s_team_state_request_pending = 0;
            recomp_printf("[ItemSync] Compact team state requested.\n");
        }
    }

    /* ── Process incoming packets ─────────────────────────────────────── */
    process_incoming_packets();
    apply_pending_boss_flags();

    if (s_team_snapshot_broadcast_timer == 0 &&
        !s_team_state_request_pending && anchor_get_client_id() != 0)
    {
        s_team_snapshot_broadcast_timer = -1;
        if (broadcast_team_state_snapshot())
            recomp_printf("[ItemSync] Compact local team snapshot broadcast.\n");
        else
            recomp_printf("[ItemSync] Compact local team snapshot broadcast failed.\n");
    }

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
        int race_challenges_active = anchor_race_is_active();
        unsigned char ds_cur_hp = DS_HP_READ();
        unsigned int ds_cur_char = DS_CHAR_IDX();

        if (race_challenges_active && anchor_runtime_one_life_enabled() && ds_cur_hp > 0 &&
            DS_LIVES_READ() != 1)
        {
            DS_LIVES_WRITE(1);
        }

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
        else if (ds_cur_hp < s_ds_prev_hp)
        {
            /* HP decreased this frame – the active character took damage.  */
            unsigned char damage = (unsigned char)(s_ds_prev_hp - ds_cur_hp);

            if (race_challenges_active && anchor_runtime_no_hit_enabled() && ds_cur_hp > 0)
            {
                DS_HP_WRITE(0);
                ds_cur_hp = 0;
                damage = s_ds_prev_hp;
                recomp_printf("[RaceChallenge] No hit forced HP to zero.\n");
            }

            if (is_connected && race_challenges_active &&
                anchor_runtime_damage_sync_enabled())
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

            if (race_challenges_active && anchor_runtime_damage_sync_enabled())
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

    /* ── Race challenge: Ryo delta sync ───────────────────────────────── */
    /* During an active race, broadcast both positive and negative balance */
    /* deltas so earning and spending ryo are mirrored by teammates.       */
    if (!valid || !anchor_race_is_active() || !anchor_runtime_ryo_sync_enabled())
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
        else if (ryo_cur != s_ryo_prev && is_connected)
        {
            signed int delta = ryo_cur - s_ryo_prev;

            /* Build JSON payload {"ryo":N} without printf.                 */
            char ryo_payload[24];
            char *wp = ryo_payload;
            *wp++ = '{';
            *wp++ = '"';
            *wp++ = 'r';
            *wp++ = 'y';
            *wp++ = 'o';
            *wp++ = '"';
            *wp++ = ':';
            wp = append_signed_decimal(wp, delta);
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

    /* ── Compact response to a pending team-state request ────────────── */
    if (s_push_cursor >= 0)
    {
        send_scheduled_team_state_response();
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

int item_sync_write_local_flag_val(const char *name, int val)
{
    int i;

    for (i = 0; i < NUM_FIELDS; ++i)
    {
        if (!streq(s_fields[i].name, name))
            continue;

        SAVE_WRITE32(s_fields[i].off, val);
        if (s_fields[i].off == -0x028)
        {
            SAVE_WRITE32(-0x024, val);
            s_ds_prev_hp = (unsigned char)val;
        }
        s_fields[i].cached = val;
        recomp_printf("[Race] Wrote local field '%s' = %d\n", name, val);
        return 1;
    }

    for (i = 0; i < NUM_FLAGS; ++i)
    {
        if (!streq(s_flag_bits[i].name, name))
            continue;

        if (val)
        {
            FLAG_SET_BIT(s_flag_bits[i].id);
            s_flag_bits[i].cached = 1;
        }
        recomp_printf("[Race] Wrote local flag '%s' (id=0x%03X)\n",
                      name, s_flag_bits[i].id);
        return 1;
    }

    recomp_printf("[Race] item_sync_write_local_flag_val: unknown name '%s'\n", name);
    return 0;
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
