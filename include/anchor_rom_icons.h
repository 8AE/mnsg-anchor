#ifndef ANCHOR_ROM_ICONS_H
#define ANCHOR_ROM_ICONS_H

#define ANCHOR_FLUTE_RESOURCE_ID          0x8016u
#define ANCHOR_FLUTE_RESOURCE_ROM_ADDRESS 0x007EB740u
#define ANCHOR_FLUTE_RESOURCE_PACKED_SIZE 0x02C0u

#define ANCHOR_FLUTE_ICON_WIDTH  24u
#define ANCHOR_FLUTE_ICON_HEIGHT 24u
#define ANCHOR_FLUTE_ICON_RGBA32_SIZE \
    (ANCHOR_FLUTE_ICON_WIDTH * ANCHOR_FLUTE_ICON_HEIGHT * 4u)

#define ANCHOR_MAP_FACE_RESOURCE_ID          0x868Cu
#define ANCHOR_MAP_FACE_RESOURCE_ROM_ADDRESS 0x013F08D0u
#define ANCHOR_MAP_FACE_RESOURCE_PACKED_SIZE 0x04A0u

#define ANCHOR_MAP_FACE_ICON_COUNT  4u
#define ANCHOR_MAP_FACE_ICON_WIDTH  16u
#define ANCHOR_MAP_FACE_ICON_HEIGHT 16u
#define ANCHOR_MAP_FACE_ICON_RGBA32_SIZE \
    (ANCHOR_MAP_FACE_ICON_WIDTH * ANCHOR_MAP_FACE_ICON_HEIGHT * 4u)
#define ANCHOR_MAP_FACE_ICONS_RGBA32_SIZE \
    (ANCHOR_MAP_FACE_ICON_COUNT * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE)

typedef enum
{
    ANCHOR_ICON_NONE,
#define ICON(name, ...) ANCHOR_ICON_##name,
#include "anchor_rom_icon_defs.inc"
#undef ICON
    ANCHOR_ICON_COUNT
} AnchorRomIcon;

typedef struct
{
    unsigned int resource_id, rom_address, packed_size;
    unsigned short sheet_width, sheet_height;
    /* width/height describe the output; crop_width describes the ROM cell.
     * Mirrored cells append a horizontally reversed copy of that cell. */
    unsigned char x, y, crop_width, width, height, flip_y, mirror_x;
} AnchorRomIconInfo;

#define ANCHOR_ROM_ICON_MAX_RGBA32_SIZE (32u * 32u * 4u)

const AnchorRomIconInfo *anchor_rom_icon_info(AnchorRomIcon icon);
int anchor_rom_load_icon_rgba32(AnchorRomIcon icon, unsigned char *rgba_out,
                               unsigned int rgba_out_size);

/* Select by stable check key and awarded value (weapon tiers: 1/2).
 * Unknown checks return NONE and remain text-only. */
AnchorRomIcon anchor_icon_for_check(const char *key, int value);

/* Load the stock pause-menu sheet from the game's ROM resource table, decode
 * it with the native PIC0000 decoder, and copy Yae's 24x24 flute crop to
 * rgba_out. The caller owns rgba_out; no ROM-derived pixels are baked into the
 * mod binary. Returns nonzero only for the verified US resource/address. */
int anchor_rom_load_flute_icon_rgba32(unsigned char *rgba_out,
                                      unsigned int rgba_out_size);

/* Load the four character faces used by the stock Japan-map player marker.
 * Output order matches the game's character IDs: Goemon, Ebisumaru, Sasuke,
 * then Yae. */
int anchor_rom_load_map_face_icons_rgba32(unsigned char *rgba_out,
                                          unsigned int rgba_out_size);

#endif /* ANCHOR_ROM_ICONS_H */
