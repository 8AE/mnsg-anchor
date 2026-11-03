#include <assert.h>
#include <string.h>

#include "../src/ui/anchor_nameplate_bitmap.h"

static unsigned char font[48u * 48u];
static unsigned char widths[95u];
static unsigned char heart[ANCHOR_NAMEPLATE_HEART_RGBA_BYTES];
static unsigned short pixels[ANCHOR_NAMEPLATE_HEALTH_PIXELS + 1u];

static void glyph_pixel(unsigned char c, unsigned int y)
{
    unsigned int glyph = c - 0x20u;
    unsigned int offset = (glyph >> 1) * 48u + y * 4u;
    font[offset] |= (glyph & 1u) ? 0xc0u : 0x30u;
}

static unsigned int build(int health)
{
    return anchor_nameplate_health_bitmap_build(pixels,
        ANCHOR_NAMEPLATE_HEALTH_PIXELS, health, heart, font, widths);
}

static void digit(unsigned int x, unsigned int value)
{
    /* Each synthetic number has its first pixel on a distinct row. */
    assert(pixels[(value + 2u) * 64u + x] == 0xf801u);
}

static void no_decimal(void)
{
    unsigned int x;
    /* The synthetic decimal glyph is on row 12, below every digit's
     * red pixel and the native heart. Odd values must never draw it. */
    for (x = 0; x < ANCHOR_NAMEPLATE_HEALTH_WIDTH; ++x)
        assert(pixels[12u * ANCHOR_NAMEPLATE_HEALTH_WIDTH + x] != 0xf801u);
}

int main(void)
{
    unsigned int i, width;
    AnchorNameplateLayout layout;
    memset(widths, 8, sizeof(widths));
    for (i = 0; i < 10u; ++i)
        glyph_pixel((unsigned char)('0' + i), i);
    glyph_pixel('.', 10u);
    /* Three distinct colored pixels prove source RGBA5551 colors and alpha
     * survive the crop copy without inheriting the other-team red name tint. */
    heart[0] = 255u; heart[3] = 255u;
    heart[4u + 1u] = 255u; heart[4u + 3u] = 255u;
    heart[8u + 2u] = 255u; heart[8u + 3u] = 0u;
    pixels[ANCHOR_NAMEPLATE_HEALTH_PIXELS] = 0x1234u;

    assert(build(0) == 24u); digit(3u, 0u);
    assert(pixels[4u * 64u + 13u] == 0xf801u);
    assert(pixels[4u * 64u + 14u] == 0x07c1u);
    assert(pixels[4u * 64u + 15u] == 0x003eu);
    assert(pixels[2u * 64u + 2u] == 0x0001u); /* Number outline. */
    assert(pixels[0] == 0u); /* Backdrop is supplied by the IA8 quad. */
    assert(build(9) == 24u); digit(3u, 9u); no_decimal();
    assert(build(1) == 24u); digit(3u, 1u); no_decimal();
    assert(build(10) == 34u); digit(3u, 1u); digit(13u, 0u); no_decimal();
    assert(build(40) == 34u); digit(3u, 4u); digit(13u, 0u);
    assert(build(255) == 44u);
    digit(3u, 2u); digit(13u, 5u); digit(23u, 5u); no_decimal();
    assert(pixels[ANCHOR_NAMEPLATE_HEALTH_PIXELS] == 0x1234u);

    assert(!build(-1));
    for (i = 0; i < ANCHOR_NAMEPLATE_HEALTH_PIXELS; ++i)
        assert(!pixels[i]);
    assert(!build(256));
    assert(!anchor_nameplate_health_bitmap_build(pixels,
        ANCHOR_NAMEPLATE_HEALTH_PIXELS, 10, 0, font, widths));
    assert(!anchor_nameplate_health_bitmap_build(pixels,
        ANCHOR_NAMEPLATE_HEALTH_PIXELS - 1u, 10, heart, font, widths));

    /* 31 widest name glyphs are retained. The visible combined rectangle is
     * centered to one texel, its seam is adjacent, and UVs stay inside tiles. */
    for (width = 8u; width <= 248u; width += 8u)
    {
        int name_left, name_right;
        anchor_nameplate_layout(width, 64u, &layout);
        name_left = -128 + layout.name_shift + (256 - (int)width) / 2 - 3;
        name_right = name_left + (int)width + 6;
        assert(name_right == layout.backdrop_left);
        assert(layout.backdrop_right == layout.suffix_left + 64);
        assert(layout.suffix_left - name_right == 4);
        assert(name_left + layout.backdrop_right >= 0);
        assert(name_left + layout.backdrop_right <= 1);
        assert(layout.backdrop_s >= 0 && layout.backdrop_s < 8192);
    }
    anchor_nameplate_layout(248u, 0u, &layout);
    assert(layout.name_shift == 0 && layout.backdrop_left == 0 &&
           layout.backdrop_right == 0);
    return 0;
}
