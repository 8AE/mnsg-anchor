#include "anchor_rom_icons.h"
#include "utils/string_utils.h"

AnchorRomIcon anchor_icon_for_check(const char *key, int value)
{
    static const struct { const char *key; AnchorRomIcon icon; } checks[] = {
        {"eq_chain", ANCHOR_ICON_CHAIN_PIPE},
        {"eq_hammer", ANCHOR_ICON_MEAT_HAMMER},
        {"eq_firecrk", ANCHOR_ICON_BOMB},
        {"eq_flute", ANCHOR_ICON_FLUTE},
        {"eq_camera", ANCHOR_ICON_CAMERA},
        {"eq_kunai", ANCHOR_ICON_ICE_KUNAI},
        {"eq_bazooka", ANCHOR_ICON_BAZOOKA},
        {"eq_fire_ryo", ANCHOR_ICON_FIRE_RYO},
        {"ab_impact", ANCHOR_ICON_GOEMON},
        {"fl_s_impact", ANCHOR_ICON_GOEMON},
        {"ab_mini_ebi", ANCHOR_ICON_EBISUMARU},
        {"fl_mini_ebi", ANCHOR_ICON_EBISUMARU},
        {"ab_jetpack", ANCHOR_ICON_SASUKE},
        {"fl_superjmp", ANCHOR_ICON_SASUKE},
        {"ab_mermaid", ANCHOR_ICON_YAE},
        {"fl_mermaid", ANCHOR_ICON_YAE},
        {"ki_triton", ANCHOR_ICON_TRITON_SHELL},
        {"ki_superps", ANCHOR_ICON_SUPER_PASS},
        {"ki_achilles", ANCHOR_ICON_ACHILLES_HEEL},
        {"ki_cucumber", ANCHOR_ICON_CUCUMBER},
        {"ki_traindoor", ANCHOR_ICON_GOLD_KEY},
        {"fl_gym_key", ANCHOR_ICON_GOLD_KEY},
        {"mi_star", ANCHOR_ICON_MIRACLE_STAR},
        {"mi_moon", ANCHOR_ICON_MIRACLE_MOON},
        {"fl_mi_moon", ANCHOR_ICON_MIRACLE_MOON},
        {"mi_flower", ANCHOR_ICON_MIRACLE_FLOWER},
        {"mi_snow", ANCHOR_ICON_MIRACLE_SNOW},
        {"fl_mi_snow", ANCHOR_ICON_MIRACLE_SNOW},
        {"fish_red", ANCHOR_ICON_RED_FISH},
        {"fish_blue", ANCHOR_ICON_BLUE_FISH},
        {"fish_yellow", ANCHOR_ICON_YELLOW_FISH},
        {"fl_bat_sas", ANCHOR_ICON_BATTERY},
        {"fl_gold_wpn", ANCHOR_ICON_GOLD_GOEMON},
        /* Dungeon key colors verified from native actor 0x193 definitions:
         * definition+5 is color (0=silver, 1=gold, 2=diamond), and +8
         * is the pickup flag. Some historical display labels disagree. */
        {"ky_s_oc_tile", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_oc_1f", ANCHOR_ICON_SILVER_KEY},
        {"ky_g_oc_1f", ANCHOR_ICON_GOLD_KEY},
        {"ky_s_oc_cp", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_oc_crsh", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_oc_2f", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gt_flwr", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gt_crn", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gt_inv", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gt_spin", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gt_dar", ANCHOR_ICON_SILVER_KEY},
        {"ky_g_gt_ff", ANCHOR_ICON_GOLD_KEY},
        {"ky_d_gt_sc", ANCHOR_ICON_DIAMOND_KEY},
        {"ky_s_gt_bil", ANCHOR_ICON_SILVER_KEY},
        {"ky_g_ft_hot", ANCHOR_ICON_GOLD_KEY},
        {"ky_s_ft_ring", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gs_baz", ANCHOR_ICON_SILVER_KEY},
        {"ky_g_gs_jet", ANCHOR_ICON_GOLD_KEY},
        {"ky_s_gs_lava", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gs_uw", ANCHOR_ICON_SILVER_KEY},
        {"ky_s_gs_swd", ANCHOR_ICON_SILVER_KEY},
        {"ky_d_gs_inv", ANCHOR_ICON_DIAMOND_KEY},
        {"ky_s_gs_sus", ANCHOR_ICON_SILVER_KEY},
        {"ky_g_mc_fan", ANCHOR_ICON_GOLD_KEY},
        {"ky_s_mc_tall", ANCHOR_ICON_SILVER_KEY},
        {"ky_g_mc_hj", ANCHOR_ICON_GOLD_KEY},
        {"ky_g_mc_mini", ANCHOR_ICON_GOLD_KEY},
        {"ky_d_mc_cube", ANCHOR_ICON_DIAMOND_KEY},
        {"ky_d_mc2", ANCHOR_ICON_DIAMOND_KEY},
    };
    static const struct {
        const char *key;
        AnchorRomIcon silver, gold;
    } weapons[] = {
        {"wpn_goemon", ANCHOR_ICON_SILVER_GOEMON, ANCHOR_ICON_GOLD_GOEMON},
        {"wpn_ebisu", ANCHOR_ICON_SILVER_EBISUMARU, ANCHOR_ICON_GOLD_EBISUMARU},
        {"wpn_sasuke", ANCHOR_ICON_SILVER_SASUKE, ANCHOR_ICON_GOLD_SASUKE},
        {"wpn_yae", ANCHOR_ICON_SILVER_YAE, ANCHOR_ICON_GOLD_YAE},
    };
    unsigned int i;

    if (!key || value <= 0)
        return ANCHOR_ICON_NONE;
    for (i = 0; i < sizeof(weapons) / sizeof(weapons[0]); ++i)
        if (mnsg_string_equal(key, weapons[i].key))
            return value == 1 ? weapons[i].silver :
                   value == 2 ? weapons[i].gold : ANCHOR_ICON_NONE;
    for (i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i)
        if (mnsg_string_equal(key, checks[i].key))
            return checks[i].icon;
    if (mnsg_string_starts_with(key, "sd_")) return ANCHOR_ICON_SILVER_DOLL;
    if (mnsg_string_starts_with(key, "gd_")) return ANCHOR_ICON_GOLD_DOLL;
    if (mnsg_string_starts_with(key, "me_") ||
        mnsg_string_starts_with(key, "mr_ely_")) return ANCHOR_ICON_MR_ELLY_FANT;
    if (mnsg_string_starts_with(key, "ma_") ||
        mnsg_string_starts_with(key, "mr_arr_")) return ANCHOR_ICON_MR_ARROW;
    return ANCHOR_ICON_NONE;
}
