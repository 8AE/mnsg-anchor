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

#define ANCHOR_NAMEPLATE_HEALTH_WIDTH 64u
#define ANCHOR_NAMEPLATE_HEALTH_HEIGHT 16u
#define ANCHOR_NAMEPLATE_HEALTH_PIXELS \
    (ANCHOR_NAMEPLATE_HEALTH_WIDTH * ANCHOR_NAMEPLATE_HEALTH_HEIGHT)
#define ANCHOR_NAMEPLATE_HEART_RGBA_BYTES (8u * 8u * 4u)

/* Health displays the raw native integer, 0..255; -1 means unavailable.
 * The suffix uses the same game font in red and the supplied native 8x8 heart.
 * Returns its padded width, or zero (transparent) when unavailable. */
unsigned int anchor_nameplate_health_bitmap_build(
    unsigned short *out, unsigned int out_pixels, int health,
    const unsigned char *heart_rgba, const unsigned char *glyphs,
    const unsigned char *widths);

/* Fit the complete centered name and suffix without truncating either.
 * All values are model-space texels at the existing nameplate scale. */
typedef struct AnchorNameplateLayout
{
    short name_shift, suffix_left, backdrop_left, backdrop_right, backdrop_s;
} AnchorNameplateLayout;
void anchor_nameplate_layout(unsigned int name_width,
                             unsigned int health_width,
                             AnchorNameplateLayout *layout);

#endif
