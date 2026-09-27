#include <assert.h>
#include <string.h>

#include "../src/ui/anchor_nameplate_bitmap.h"

static unsigned char font[48u * 48u];
static unsigned char widths[95u];
static unsigned char bitmap[ANCHOR_NAMEPLATE_TEXTURE_BYTES];

static void set_first_pixel(unsigned char c)
{
    unsigned int glyph = (unsigned int)c - 0x20u;
    font[(glyph >> 1) * 48u] = (glyph & 1u) ? 0xc0u : 0x30u;
}

static void set_full_glyph(unsigned char c)
{
    unsigned int glyph = (unsigned int)c - 0x20u;
    unsigned int i;
    for (i = 0; i < 48u; ++i)
        font[(glyph >> 1) * 48u + i] |= (glyph & 1u) ? 0xccu : 0x33u;
}

static unsigned int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return (unsigned int)(c - '0');
    assert(c >= 'a' && c <= 'f');
    return (unsigned int)(c - 'a' + 10);
}

static void load_font_pair(unsigned char c, const char *hex)
{
    unsigned int glyph = (unsigned int)c - 0x20u;
    unsigned int i;
    assert(strlen(hex) == 96u);
    for (i = 0; i < 48u; ++i)
        font[(glyph >> 1) * 48u + i] =
            (unsigned char)((hex_digit(hex[i * 2u]) << 4) |
                             hex_digit(hex[i * 2u + 1u]));
}

int main(void)
{
    unsigned int width, row, column, a_pixels, b_pixels;
    unsigned int y = 2u * ANCHOR_NAMEPLATE_TEXTURE_WIDTH;
    char long_name[32];
    memset(widths, 8, sizeof(widths));
    set_full_glyph('A');
    set_full_glyph('B');
    set_first_pixel('?');

    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), "AB",
                                          font, widths);
    assert(width == 18u); /* Eight + two interglyph texels + eight. */
    assert(bitmap[y + 119u] == 0xffu);
    assert(bitmap[y + 126u] == 0xffu);
    assert(bitmap[y + 129u] == 0xffu);
    assert(bitmap[y + 136u] == 0xffu);
    assert(bitmap[y + 118u] == 0x0fu); /* Opaque black outline. */
    assert(bitmap[y + 127u] == 0x0fu);
    assert(bitmap[y + 128u] == 0x0fu);
    assert(bitmap[y + 137u] == 0x0fu);
    assert(bitmap[y + 116u] == 0x08u); /* Half-alpha black box. */
    assert(bitmap[y + 139u] == 0x08u);
    assert(bitmap[y + 115u] == 0x00u); /* Outside stays transparent. */
    assert(bitmap[y + 140u] == 0x00u);
    assert(bitmap[y - ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 119u] == 0x0fu);
    for (row = 0; row < ANCHOR_NAMEPLATE_TEXTURE_HEIGHT; ++row)
    {
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 116u] == 0x08u);
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 139u] == 0x08u);
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 115u] == 0u);
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 140u] == 0u);
    }

    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), "\xc3\xa9",
                                          font, widths);
    assert(width == 8u);
    assert(bitmap[y + 124u] == 0xffu);

    memset(long_name, 'A', 31u);
    long_name[31] = 0;
    long_name[23] = 0;
    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), long_name,
                                          font, widths);
    assert(width == 228u); /* Default 2px gap still fits 23 glyphs. */
    long_name[23] = 'A';
    long_name[24] = 0;
    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), long_name,
                                          font, widths);
    assert(width == 238u); /* Still uses the 2px gap. */
    long_name[24] = 'A';
    long_name[25] = 0;
    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), long_name,
                                          font, widths);
    assert(width == 248u); /* Longest widest-glyph name with a 2px gap. */
    assert(bitmap[y + 4u] == 0xffu);
    assert(bitmap[y + 244u] == 0xffu);
    assert(bitmap[y + 0u] == 0u);
    assert(bitmap[y + 1u] == 0x08u);
    assert(bitmap[y + 254u] == 0x08u);
    assert(bitmap[y + 255u] == 0u);
    long_name[25] = 'A';
    long_name[26] = 0;
    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), long_name,
                                          font, widths);
    assert(width == 233u); /* A 1px gap retains the 26th glyph. */
    long_name[26] = 'A';
    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), long_name,
                                          font, widths);
    assert(width == 248u); /* All 31 letters fit with zero-pixel gaps. */
    assert(bitmap[y + 4u] == 0xffu);
    assert(bitmap[y + 251u] == 0xffu);
    assert(bitmap[y + 3u] == 0x0fu);
    assert(bitmap[y + 252u] == 0x0fu);
    assert(bitmap[y + 0u] == 0u);
    assert(bitmap[y + 1u] == 0x08u);
    assert(bitmap[y + 254u] == 0x08u);
    assert(bitmap[y + 255u] == 0u);

    /* Real US .main font pairs for A (odd) and B (even), copied from
     * D_800629A0. Both native widths are seven pixels. */
    memset(font, 0, sizeof(font));
    memset(widths, 0, sizeof(widths));
    load_font_pair('A',
        "0000000000fff0000fc0cf00fc033cf0fc303cf0fccfcff0"
        "cf000cc0cc333cc000000000000000000000000000000000");
    load_font_pair('B',
        "0000000033ffff003fc00ff0ff000330ff333300ff000330"
        "3fc00ff033ffff0000000000000000000000000000000000");
    widths['A' - 0x20] = 7u;
    widths['B' - 0x20] = 7u;
    width = anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap), "AB",
                                          font, widths);
    assert(width == 16u);
    a_pixels = b_pixels = 0;
    for (row = 0; row < ANCHOR_NAMEPLATE_TEXTURE_HEIGHT; ++row)
    {
        for (column = 120u; column <= 126u; ++column)
            a_pixels += (bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH +
                                column] & 0xf0u) != 0;
        for (column = 129u; column <= 135u; ++column)
            b_pixels += (bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH +
                                column] & 0xf0u) != 0;
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 116u] == 0u);
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 117u] == 0x08u);
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 138u] == 0x08u);
        assert(bitmap[row * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + 139u] == 0u);
    }
    assert(a_pixels == 30u);
    assert(b_pixels == 34u);

    assert(anchor_nameplate_bitmap_build(bitmap, sizeof(bitmap) - 1u, "A",
                                         font, widths) == 0u);
    return 0;
}
