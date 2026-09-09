#include <assert.h>
#include <stdio.h>

#include "anchor_rom_icons.h"

#define SHEET_WIDTH 64u
#define SHEET_HEIGHT 32u

static int s_query_size = (int)ANCHOR_FLUTE_RESOURCE_PACKED_SIZE;
static unsigned int s_query_address = ANCHOR_FLUTE_RESOURCE_ROM_ADDRESS;
static unsigned int s_query_resource = ANCHOR_FLUTE_RESOURCE_ID;
static int s_decode_called;
static int s_bad_decoded_end;

int func_80014698_15298(unsigned int resource_id, void *rom_address_out)
{
    assert(resource_id == ANCHOR_FLUTE_RESOURCE_ID ||
           resource_id == ANCHOR_MAP_FACE_RESOURCE_ID);
    assert(resource_id == s_query_resource);
    if (rom_address_out)
        *(unsigned int *)rom_address_out = s_query_address;
    return s_query_size;
}

unsigned char *func_800144E8_150E8(unsigned int resource_id,
                                   void *packed_scratch,
                                   void *decoded_out)
{
    unsigned short *sheet = decoded_out;
    unsigned int i;

    assert(resource_id == ANCHOR_FLUTE_RESOURCE_ID ||
           resource_id == ANCHOR_MAP_FACE_RESOURCE_ID);
    assert(packed_scratch != 0);
    ++s_decode_called;

    for (i = 0; i < SHEET_WIDTH * SHEET_HEIGHT; ++i)
        sheet[i] = 0;

    if (resource_id == ANCHOR_FLUTE_RESOURCE_ID)
    {
        /* Prove the crop starts at (40, 0), advances rows at width 64, and
         * keeps the RGBA5551 alpha bit. */
        sheet[39] = 0xffff;
        sheet[40] = 0xf801;                 /* opaque red */
        sheet[41] = 0x07c1;                 /* opaque green */
        sheet[SHEET_WIDTH + 40u] = 0x003f;  /* opaque blue */
        sheet[SHEET_WIDTH + 41u] = 0xfffe;  /* transparent white */
    }
    else
    {
        /* The map quads reverse T: each output icon's first pixel comes from
         * the source cell's bottom-left, and its last comes from top-right. */
        sheet[63u * 32u + 0u] = 0xf801;   /* Goemon first: red */
        sheet[48u * 32u + 15u] = 0x07c1;  /* Goemon last: green */
        sheet[63u * 32u + 16u] = 0x07c1;  /* Ebisumaru first: green */
        sheet[48u * 32u + 31u] = 0x003f;  /* Ebisumaru last: blue */
        sheet[47u * 32u + 0u] = 0x003f;   /* Sasuke first: blue */
        sheet[32u * 32u + 15u] = 0xffff;  /* Sasuke last: white */
        sheet[47u * 32u + 16u] = 0xfffe;  /* Yae first: transparent white */
        sheet[32u * 32u + 31u] = 0x0801;  /* Yae last: low red */
    }

    return (unsigned char *)decoded_out +
           SHEET_WIDTH * SHEET_HEIGHT * sizeof(*sheet) -
           (s_bad_decoded_end ? 2u : 0u);
}

int main(void)
{
    unsigned char rgba[ANCHOR_FLUTE_ICON_RGBA32_SIZE];
    unsigned char map_rgba[ANCHOR_MAP_FACE_ICONS_RGBA32_SIZE];
    unsigned int second_row = ANCHOR_FLUTE_ICON_WIDTH * 4u;
    unsigned int icon_last = ANCHOR_MAP_FACE_ICON_RGBA32_SIZE - 4u;

    assert(!anchor_rom_load_flute_icon_rgba32(0, sizeof(rgba)));
    assert(!anchor_rom_load_flute_icon_rgba32(rgba, sizeof(rgba) - 1u));
    assert(s_decode_called == 0);

    s_query_size = 1;
    assert(!anchor_rom_load_flute_icon_rgba32(rgba, sizeof(rgba)));
    assert(s_decode_called == 0);
    s_query_size = (int)ANCHOR_FLUTE_RESOURCE_PACKED_SIZE;

    s_query_address = ANCHOR_FLUTE_RESOURCE_ROM_ADDRESS + 2u;
    assert(!anchor_rom_load_flute_icon_rgba32(rgba, sizeof(rgba)));
    assert(s_decode_called == 0);
    s_query_address = ANCHOR_FLUTE_RESOURCE_ROM_ADDRESS;

    s_bad_decoded_end = 1;
    assert(!anchor_rom_load_flute_icon_rgba32(rgba, sizeof(rgba)));
    assert(s_decode_called == 1);

    s_bad_decoded_end = 0;
    assert(anchor_rom_load_flute_icon_rgba32(rgba, sizeof(rgba)));
    assert(s_decode_called == 2);

    assert(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
    assert(rgba[4] == 0 && rgba[5] == 255 && rgba[6] == 0 && rgba[7] == 255);
    assert(rgba[second_row + 0u] == 0);
    assert(rgba[second_row + 1u] == 0);
    assert(rgba[second_row + 2u] == 255);
    assert(rgba[second_row + 3u] == 255);
    assert(rgba[second_row + 4u] == 255);
    assert(rgba[second_row + 5u] == 255);
    assert(rgba[second_row + 6u] == 255);
    assert(rgba[second_row + 7u] == 0);

    s_query_resource = ANCHOR_MAP_FACE_RESOURCE_ID;
    s_query_size = (int)ANCHOR_MAP_FACE_RESOURCE_PACKED_SIZE;
    s_query_address = ANCHOR_MAP_FACE_RESOURCE_ROM_ADDRESS;

    assert(!anchor_rom_load_map_face_icons_rgba32(0, sizeof(map_rgba)));
    assert(!anchor_rom_load_map_face_icons_rgba32(
        map_rgba, sizeof(map_rgba) - 1u));

    s_query_size = 1;
    assert(!anchor_rom_load_map_face_icons_rgba32(map_rgba,
                                                   sizeof(map_rgba)));
    s_query_size = (int)ANCHOR_MAP_FACE_RESOURCE_PACKED_SIZE;

    s_query_address = ANCHOR_MAP_FACE_RESOURCE_ROM_ADDRESS + 2u;
    assert(!anchor_rom_load_map_face_icons_rgba32(map_rgba,
                                                   sizeof(map_rgba)));
    s_query_address = ANCHOR_MAP_FACE_RESOURCE_ROM_ADDRESS;

    s_bad_decoded_end = 1;
    assert(!anchor_rom_load_map_face_icons_rgba32(map_rgba,
                                                   sizeof(map_rgba)));
    s_bad_decoded_end = 0;
    assert(anchor_rom_load_map_face_icons_rgba32(map_rgba,
                                                  sizeof(map_rgba)));

    /* First and last texels prove each crop's coordinate and output slot. */
    assert(map_rgba[0] == 255 && map_rgba[1] == 0 &&
           map_rgba[2] == 0 && map_rgba[3] == 255);
    assert(map_rgba[icon_last + 0u] == 0 &&
           map_rgba[icon_last + 1u] == 255 &&
           map_rgba[icon_last + 2u] == 0 &&
           map_rgba[icon_last + 3u] == 255);

    assert(map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 0u] == 0 &&
           map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 1u] == 255 &&
           map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 2u] == 0 &&
           map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 3u] == 255);
    assert(map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 0u] == 0 &&
           map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 1u] == 0 &&
           map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 2u] == 255 &&
           map_rgba[ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 3u] == 255);

    assert(map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 0u] == 0 &&
           map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 1u] == 0 &&
           map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 2u] == 255 &&
           map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 3u] == 255);
    assert(map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 0u] ==
               255 &&
           map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 1u] ==
               255 &&
           map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 2u] ==
               255 &&
           map_rgba[2u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 3u] ==
               255);

    assert(map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 0u] == 255 &&
           map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 1u] == 255 &&
           map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 2u] == 255 &&
           map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + 3u] == 0);
    assert(map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 0u] ==
               8 &&
           map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 1u] ==
               0 &&
           map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 2u] ==
               0 &&
           map_rgba[3u * ANCHOR_MAP_FACE_ICON_RGBA32_SIZE + icon_last + 3u] ==
               255);

    puts("ROM icon tests passed");
    return 0;
}
