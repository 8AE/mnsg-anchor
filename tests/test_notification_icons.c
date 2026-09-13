#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "anchor_rom_icons.h"

static const AnchorRomIconInfo *s_expected;
static int s_wrong_address, s_wrong_size, s_wrong_end, s_decodes;

int func_80014698_15298(unsigned int id, void *address)
{
    assert(s_expected && id == s_expected->resource_id);
    *(unsigned int *)address = s_expected->rom_address + s_wrong_address;
    return (int)s_expected->packed_size + s_wrong_size;
}

static unsigned short sample(unsigned int x, unsigned int y)
{
    return ((x % 32u) << 11) | ((y % 32u) << 6) |
           (((x + y) % 32u) << 1) | ((x ^ y) & 1u);
}

unsigned char *func_800144E8_150E8(unsigned int id, void *packed, void *decoded)
{
    unsigned int x, y;
    unsigned short *sheet = decoded;
    assert(id == s_expected->resource_id && packed && packed != decoded);
    ++s_decodes;
    for (y = 0; y < s_expected->sheet_height; ++y)
        for (x = 0; x < s_expected->sheet_width; ++x)
            sheet[y * s_expected->sheet_width + x] = sample(x, y);
    return (unsigned char *)decoded + 4096 - s_wrong_end;
}

static void check_texel(const unsigned char *rgba, unsigned int x, unsigned int y)
{
    unsigned short v = sample(x, y);
    unsigned int red = v >> 11, green = (v >> 6) & 31, blue = (v >> 1) & 31;
    assert(rgba[0] == red * 8 + red / 4);
    assert(rgba[1] == green * 8 + green / 4);
    assert(rgba[2] == blue * 8 + blue / 4);
    assert(rgba[3] == ((v & 1) ? 255 : 0));
}

int main(void)
{
    unsigned char guarded[ANCHOR_ROM_ICON_MAX_RGBA32_SIZE + 2];
    unsigned int i, x, y, size;
    assert(!anchor_rom_icon_info(ANCHOR_ICON_NONE));
    assert(!anchor_rom_icon_info((AnchorRomIcon)-1));
    assert(!anchor_rom_icon_info(ANCHOR_ICON_COUNT));
    for (i = 1; i < ANCHOR_ICON_COUNT; ++i)
    {
        s_expected = anchor_rom_icon_info((AnchorRomIcon)i);
        if (i == ANCHOR_ICON_MR_ELLY_FANT || i == ANCHOR_ICON_MR_ARROW)
        {
            assert(s_expected->mirror_x && s_expected->flip_y);
            assert(s_expected->crop_width == 16);
            assert(s_expected->width == 32 && s_expected->height == 32);
        }
        else
        {
            assert(!s_expected->mirror_x);
            assert(s_expected->crop_width == s_expected->width);
        }
        size = s_expected->width * s_expected->height * 4u;
        assert(size <= ANCHOR_ROM_ICON_MAX_RGBA32_SIZE);
        s_decodes = 0;
        assert(!anchor_rom_load_icon_rgba32(i, NULL, size));
        assert(!anchor_rom_load_icon_rgba32(i, guarded + 1, size - 1));
        s_wrong_size = 1;
        assert(!anchor_rom_load_icon_rgba32(i, guarded + 1, size));
        s_wrong_size = 0; s_wrong_address = 2;
        assert(!anchor_rom_load_icon_rgba32(i, guarded + 1, size));
        s_wrong_address = 0;
        assert(s_decodes == 0); /* rejected before native unbounded writes */
        s_wrong_end = 2;
        assert(!anchor_rom_load_icon_rgba32(i, guarded + 1, size));
        s_wrong_end = 0;
        memset(guarded, 0xa5, sizeof(guarded));
        assert(anchor_rom_load_icon_rgba32(i, guarded + 1, size));
        assert(guarded[0] == 0xa5 && guarded[size + 1] == 0xa5);
        for (y = 0; y < s_expected->height; ++y)
            for (x = 0; x < s_expected->width; ++x)
                check_texel(guarded + 1 + (y * s_expected->width + x) * 4,
                            s_expected->x + (s_expected->mirror_x && x >= 16 ? 31 - x : x), s_expected->y +
                            (s_expected->flip_y ? s_expected->height - 1 - y : y));
    }
    assert(anchor_icon_for_check(NULL, 1) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("fl_congo", 1) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("eq_chain", 0) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("eq_chain", 1) == ANCHOR_ICON_CHAIN_PIPE);
    assert(anchor_icon_for_check("eq_firecrk", 1) == ANCHOR_ICON_BOMB);
    assert(anchor_icon_for_check("sd_oe_trt", 1) == ANCHOR_ICON_SILVER_DOLL);
    assert(anchor_icon_for_check("gd_bizen", 1) == ANCHOR_ICON_GOLD_DOLL);
    assert(anchor_icon_for_check("me_gour", 1) == ANCHOR_ICON_MR_ELLY_FANT);
    assert(anchor_icon_for_check("ma_music", 1) == ANCHOR_ICON_MR_ARROW);
    assert(anchor_icon_for_check("wpn_goemon", 1) == ANCHOR_ICON_SILVER_GOEMON);
    assert(anchor_icon_for_check("wpn_goemon", 2) == ANCHOR_ICON_GOLD_GOEMON);
    assert(anchor_icon_for_check("wpn_sasuke", 1) == ANCHOR_ICON_SILVER_SASUKE);
    assert(anchor_icon_for_check("wpn_sasuke", 2) == ANCHOR_ICON_GOLD_SASUKE);
    assert(anchor_icon_for_check("wpn_yae", 3) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("ab_impact", 1) == ANCHOR_ICON_GOEMON);
    assert(anchor_icon_for_check("fl_s_impact", 1) == ANCHOR_ICON_GOEMON);
    assert(anchor_icon_for_check("ab_mini_ebi", 1) == ANCHOR_ICON_EBISUMARU);
    assert(anchor_icon_for_check("fl_mini_ebi", 1) == ANCHOR_ICON_EBISUMARU);
    assert(anchor_icon_for_check("ab_jetpack", 1) == ANCHOR_ICON_SASUKE);
    assert(anchor_icon_for_check("fl_superjmp", 1) == ANCHOR_ICON_SASUKE);
    assert(anchor_icon_for_check("ab_mermaid", 1) == ANCHOR_ICON_YAE);
    assert(anchor_icon_for_check("fl_mermaid", 1) == ANCHOR_ICON_YAE);
    assert(anchor_icon_for_check("ky_s_oc_tile", 1) == ANCHOR_ICON_SILVER_KEY);
    assert(anchor_icon_for_check("ky_g_oc_1f", 1) == ANCHOR_ICON_GOLD_KEY);
    assert(anchor_icon_for_check("ky_d_gt_sc", 1) == ANCHOR_ICON_DIAMOND_KEY);
    assert(anchor_icon_for_check("ky_g_ft_hot", 1) == ANCHOR_ICON_GOLD_KEY);
    assert(anchor_icon_for_check("ky_d_gs_inv", 1) == ANCHOR_ICON_DIAMOND_KEY);
    /* Native pickup colors take precedence over old, conflicting labels. */
    assert(anchor_icon_for_check("ky_s_gt_crn", 1) == ANCHOR_ICON_SILVER_KEY);
    assert(anchor_icon_for_check("ky_g_gt_ff", 1) == ANCHOR_ICON_GOLD_KEY);
    assert(anchor_icon_for_check("ky_s_mc_tall", 1) == ANCHOR_ICON_SILVER_KEY);
    assert(anchor_icon_for_check("ky_d_mc_cube", 1) == ANCHOR_ICON_DIAMOND_KEY);
    assert(anchor_icon_for_check("ky_d_mc2", 1) == ANCHOR_ICON_DIAMOND_KEY);
    assert(anchor_icon_for_check("ky_s_oc_tile", 0) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("ky_s_unknown", 1) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("ky_g_unknown", 1) == ANCHOR_ICON_NONE);
    assert(anchor_icon_for_check("ky_d_unknown", 1) == ANCHOR_ICON_NONE);
    /* Native shell draw uses U=24; U=32 clips it and includes a white strip. */
    assert(anchor_rom_icon_info(ANCHOR_ICON_TRITON_SHELL)->x == 24);
    puts("All notification icon crops, ROM guards, and check mappings passed");
    return 0;
}
