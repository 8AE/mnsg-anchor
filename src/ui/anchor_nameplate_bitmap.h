#ifndef ANCHOR_NAMEPLATE_BITMAP_H
#define ANCHOR_NAMEPLATE_BITMAP_H

#define ANCHOR_NAMEPLATE_TEXTURE_WIDTH 256u
#define ANCHOR_NAMEPLATE_TEXTURE_HEIGHT 16u
#define ANCHOR_NAMEPLATE_TEXTURE_BYTES \
    (ANCHOR_NAMEPLATE_TEXTURE_WIDTH * ANCHOR_NAMEPLATE_TEXTURE_HEIGHT)
#define ANCHOR_NAMEPLATE_INPUT_BYTES 31u

/* Rasterize a bounded player name from the game's paired 8x12 CI4 glyphs.
 * The output is an IA8 texture centered horizontally. Unsupported bytes are
 * rendered as '?', with UTF-8 continuation bytes collapsed into one fallback.
 * Returns the text width in pixels, or zero for unusable inputs. */
unsigned int anchor_nameplate_bitmap_build(
    unsigned char *out, unsigned int out_size, const char *name,
    const unsigned char *glyphs, const unsigned char *widths);

#endif
