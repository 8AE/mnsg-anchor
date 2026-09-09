#include "anchor_rom_icons.h"

#define FLUTE_SHEET_WIDTH  64u
#define FLUTE_SHEET_HEIGHT 32u
#define FLUTE_CROP_X       40u
#define FLUTE_CROP_Y       0u

#define MAP_FACE_SHEET_WIDTH  32u
#define MAP_FACE_SHEET_HEIGHT 64u
#define ROM_ICON_SHEET_PIXELS (64u * 32u)

/* The stock resource helper resolves the resource through the game's ROM
 * tables, loads its packed bytes into scratch, detects PIC0000, and invokes
 * the native decoder into decoded_out. */
extern unsigned char *func_800144E8_150E8(unsigned int resource_id,
                                          void *packed_scratch,
                                          void *decoded_out);
extern int func_80014698_15298(unsigned int resource_id,
                               void *rom_address_out);

/* func_800144E8 needs separate packed and decoded regions. Both known sheets
 * decode to 0x1000 bytes, and the map-face resource has the larger packed
 * representation. Calls are serialized on the game's update thread. */
static unsigned char s_icon_packed[ANCHOR_MAP_FACE_RESOURCE_PACKED_SIZE]
    __attribute__((aligned(8)));
static unsigned short s_icon_sheet[ROM_ICON_SHEET_PIXELS]
    __attribute__((aligned(8)));

static unsigned char expand_5_to_8(unsigned int value)
{
    value &= 0x1fu;
    return (unsigned char)((value << 3) | (value >> 2));
}

static int decode_rgba5551_resource(unsigned int resource_id,
                                    unsigned int expected_rom_address,
                                    unsigned int expected_packed_size)
{
    unsigned char *decoded_end;
    unsigned int rom_address = 0;
    int packed_size;

    if (expected_packed_size > sizeof(s_icon_packed))
        return 0;

    /* The native APIs have no capacity arguments. Pin the verified US stored
     * size and ROM address before allowing either buffer write. */
    packed_size = func_80014698_15298(resource_id, &rom_address);
    if (packed_size != (int)expected_packed_size ||
        rom_address != expected_rom_address)
        return 0;

    decoded_end = func_800144E8_150E8(resource_id, s_icon_packed,
                                      s_icon_sheet);
    return decoded_end ==
           (unsigned char *)s_icon_sheet + sizeof(s_icon_sheet);
}

static void copy_rgba5551_crop(unsigned char *rgba_out,
                               unsigned int sheet_width,
                               unsigned int crop_x,
                               unsigned int crop_y,
                               unsigned int crop_width,
                               unsigned int crop_height,
                               int flip_y)
{
    unsigned int x;
    unsigned int y;

    for (y = 0; y < crop_height; ++y)
    {
        unsigned int source_y = crop_y +
            (flip_y ? crop_height - 1u - y : y);

        for (x = 0; x < crop_width; ++x)
        {
            unsigned short pixel =
                s_icon_sheet[source_y * sheet_width + crop_x + x];
            unsigned int dst = (y * crop_width + x) * 4u;

            rgba_out[dst + 0u] = expand_5_to_8(pixel >> 11);
            rgba_out[dst + 1u] = expand_5_to_8(pixel >> 6);
            rgba_out[dst + 2u] = expand_5_to_8(pixel >> 1);
            rgba_out[dst + 3u] = (pixel & 1u) ? 0xffu : 0u;
        }
    }
}

int anchor_rom_load_flute_icon_rgba32(unsigned char *rgba_out,
                                      unsigned int rgba_out_size)
{
    if (!rgba_out || rgba_out_size < ANCHOR_FLUTE_ICON_RGBA32_SIZE)
        return 0;

    if (!decode_rgba5551_resource(ANCHOR_FLUTE_RESOURCE_ID,
                                  ANCHOR_FLUTE_RESOURCE_ROM_ADDRESS,
                                  ANCHOR_FLUTE_RESOURCE_PACKED_SIZE))
        return 0;

    copy_rgba5551_crop(rgba_out, FLUTE_SHEET_WIDTH, FLUTE_CROP_X,
                       FLUTE_CROP_Y, ANCHOR_FLUTE_ICON_WIDTH,
                       ANCHOR_FLUTE_ICON_HEIGHT, 0);

    return 1;
}

int anchor_rom_load_map_face_icons_rgba32(unsigned char *rgba_out,
                                          unsigned int rgba_out_size)
{
    /* Character order is the network/native character ID order. Coordinates
     * come from the four Japan-map model display lists' 16x16 UV cells. The
     * marker quads map their top vertices to the larger T coordinate, so flip
     * each crop vertically when moving it into RecompUI's top-down image. */
    static const unsigned char crop_x[ANCHOR_MAP_FACE_ICON_COUNT] = {
        0u, 16u, 0u, 16u};
    static const unsigned char crop_y[ANCHOR_MAP_FACE_ICON_COUNT] = {
        48u, 48u, 32u, 32u};
    unsigned int character;

    if (!rgba_out || rgba_out_size < ANCHOR_MAP_FACE_ICONS_RGBA32_SIZE)
        return 0;

    if (!decode_rgba5551_resource(ANCHOR_MAP_FACE_RESOURCE_ID,
                                  ANCHOR_MAP_FACE_RESOURCE_ROM_ADDRESS,
                                  ANCHOR_MAP_FACE_RESOURCE_PACKED_SIZE))
        return 0;

    for (character = 0; character < ANCHOR_MAP_FACE_ICON_COUNT; ++character)
    {
        copy_rgba5551_crop(
            rgba_out + character * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE,
            MAP_FACE_SHEET_WIDTH, crop_x[character], crop_y[character],
            ANCHOR_MAP_FACE_ICON_WIDTH, ANCHOR_MAP_FACE_ICON_HEIGHT, 1);
    }

    return 1;
}
