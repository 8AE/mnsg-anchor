#include "anchor_nameplate_bitmap.h"

#define GLYPH_WIDTH 8u
#define GLYPH_HEIGHT 12u
#define GLYPH_PAIR_BYTES 48u
#define NAMEPLATE_BOX_PADDING 3u
#define NAMEPLATE_BOX_IA8 0x08u
static unsigned int glyph_width(const unsigned char *widths, unsigned char c)
{
    unsigned int width = widths[c - 0x20u];
    return width > GLYPH_WIDTH ? GLYPH_WIDTH : width;
}

static unsigned int sanitize(const char *name, unsigned char *out)
{
    unsigned int in = 0, count = 0;
    while (in < ANCHOR_NAMEPLATE_INPUT_BYTES && name[in] &&
           count < ANCHOR_NAMEPLATE_INPUT_BYTES)
    {
        unsigned char c = (unsigned char)name[in++];
        if (c < 0x20u || c > 0x7eu)
        {
            if (c >= 0xc0u)
            {
                while (in < ANCHOR_NAMEPLATE_INPUT_BYTES &&
                       ((unsigned char)name[in] & 0xc0u) == 0x80u)
                    ++in;
            }
            else if (c >= 0x80u && c <= 0xbfu)
                continue;
            c = '?';
        }
        out[count++] = c;
    }
    if (!count)
    {
        out[0] = 'P';
        count = 1;
    }
    return count;
}

static unsigned int glyph_shade(const unsigned char *glyphs,
                                unsigned char c, unsigned int x,
                                unsigned int y)
{
    unsigned int glyph = (unsigned int)c - 0x20u;
    unsigned int pixel = y * GLYPH_WIDTH + x;
    unsigned char packed = glyphs[(glyph >> 1) * GLYPH_PAIR_BYTES +
                                  (pixel >> 1)];
    unsigned int index = (pixel & 1u) ? packed & 15u : packed >> 4;
    return (glyph & 1u) ? index >> 2 : index & 3u;
}

unsigned int anchor_nameplate_bitmap_build(
    unsigned char *out, unsigned int out_size, const char *name,
    const unsigned char *glyphs, const unsigned char *widths)
{
    unsigned char ascii[ANCHOR_NAMEPLATE_INPUT_BYTES];
    unsigned int count, text_width = 0, cursor, gap = 2u, i, x, y;
    unsigned int box_left, box_right;
    if (!out || out_size < ANCHOR_NAMEPLATE_TEXTURE_BYTES ||
        !name || !glyphs || !widths)
        return 0;
    for (i = 0; i < ANCHOR_NAMEPLATE_TEXTURE_BYTES; ++i)
        out[i] = 0;
    count = sanitize(name, ascii);
    for (i = 0; i < count; ++i)
        text_width += glyph_width(widths, ascii[i]);
    /* Keep the usual names compact, then tighten gaps for long names. Three
     * texels of box padding fit on both sides when text width stays at 250 or
     * less; even 31 eight-pixel glyphs fit without a trailing gap. */
    while (gap && text_width + (count - 1u) * gap >
                      ANCHOR_NAMEPLATE_TEXTURE_WIDTH -
                          2u * NAMEPLATE_BOX_PADDING)
        --gap;
    text_width += (count - 1u) * gap;
    cursor = (ANCHOR_NAMEPLATE_TEXTURE_WIDTH - text_width) / 2u;
    box_left = cursor - NAMEPLATE_BOX_PADDING;
    box_right = cursor + text_width + NAMEPLATE_BOX_PADDING;
    for (i = 0; i < count; ++i)
    {
        unsigned int width = glyph_width(widths, ascii[i]);
        for (y = 0; y < GLYPH_HEIGHT; ++y)
        {
            for (x = 0; x < width; ++x)
            {
                unsigned int shade = glyph_shade(glyphs, ascii[i], x, y);
                unsigned int dst = (y + 2u) * ANCHOR_NAMEPLATE_TEXTURE_WIDTH +
                                   cursor + x;
                if (shade)
                    out[dst] = (unsigned char)(0xf0u | (shade * 5u));
            }
        }
        cursor += width + gap;
    }
    /* One-pixel native-style dark edge around the game-font mask. IA8 keeps
     * the tintable glyph intensity separate from its opaque outline. */
    for (y = 1; y < ANCHOR_NAMEPLATE_TEXTURE_HEIGHT - 1u; ++y)
        for (x = 1; x < ANCHOR_NAMEPLATE_TEXTURE_WIDTH - 1u; ++x)
        {
            unsigned int dst = y * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + x;
            if (out[dst])
                continue;
            if ((out[dst - 1u] & 0xf0u) || (out[dst + 1u] & 0xf0u) ||
                (out[dst - ANCHOR_NAMEPLATE_TEXTURE_WIDTH] & 0xf0u) ||
                (out[dst + ANCHOR_NAMEPLATE_TEXTURE_WIDTH] & 0xf0u))
                out[dst] = 0x0fu;
        }
    /* Fill only untouched texels so the black half-alpha rectangle stays
     * behind the opaque game font and its black outline. It spans the strip
     * height but only the padded bounds of this centered name. */
    for (y = 0; y < ANCHOR_NAMEPLATE_TEXTURE_HEIGHT; ++y)
        for (x = box_left; x < box_right; ++x)
        {
            unsigned int dst = y * ANCHOR_NAMEPLATE_TEXTURE_WIDTH + x;
            if (!out[dst])
                out[dst] = NAMEPLATE_BOX_IA8;
        }
    return text_width;
}
