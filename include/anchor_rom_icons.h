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
