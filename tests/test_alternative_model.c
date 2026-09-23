#include <stdio.h>
#include <string.h>

#include "player/alternative_ebisumaru/anchor_alternative_model.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return 1; \
} } while (0)

static void word(unsigned char *p, unsigned int value)
{
    memcpy(p, &value, sizeof(value));
}

static unsigned int read_word(const unsigned char *p)
{
    unsigned int value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static int test_mesh_rebase(void)
{
    static unsigned char broad[ANCHOR_ALTERNATIVE_RENDER_DATA_SIZE];
    unsigned int i;
    unsigned char *mesh = broad + ANCHOR_ALTERNATIVE_MESH_OFFSET;

    memset(broad, 0, sizeof(broad));
    word(broad + 0x40, 0x08001234u); /* playable cue data: untouched */
    word(mesh + 0x3000, 0x08004567u); /* vertex data: untouched */
    for (i = 0; i < 72u; ++i)
        word(mesh + ANCHOR_ALTERNATIVE_DISPLAY_START + i * 4u,
             0x08000000u + i * 0x100u);
    CHECK(anchor_alternative_rebase_mesh(broad, ANCHOR_ALTERNATIVE_RESOURCE_BYTES));
    CHECK(read_word(broad + 0x40) == 0x08001234u);
    CHECK(read_word(mesh + 0x3000) == 0x08004567u);
    for (i = 0; i < 72u; ++i)
        CHECK(read_word(mesh + ANCHOR_ALTERNATIVE_DISPLAY_START + i * 4u) ==
              0x09018000u + i * 0x100u);
    CHECK(!anchor_alternative_rebase_mesh(broad, ANCHOR_ALTERNATIVE_RESOURCE_BYTES - 1u));
    return 0;
}

static int test_playable_tree_graft(void)
{
    static const unsigned int clothed[] = {
        0x09004568u, 0x090046a0u, 0x09004860u, 0x09004960u,
        0x09004a98u, 0x09004c58u, 0x09004d70u, 0x09004f20u,
        0x09005078u, 0x09005260u, 0x090053b8u, 0x09005598u,
        0x090056e8u, 0x09005aa8u,
    };
    static const unsigned int alternative[] = {
        0x0901c798u, 0x0901c8d8u, 0x0901ca98u, 0x0901cba0u,
        0x0901cce0u, 0x0901cea0u, 0x0901cfa8u, 0x0901d918u,
        0x0901da48u, 0x0901d1a8u, 0x0901d2d8u, 0x0901d430u,
        0x0901d588u, 0x0901dbc0u,
    };
    unsigned char slice[0x220] = {0};
    unsigned int i;

    word(slice + 0x20, 0x8000010cu);
    word(slice + 0x28, 0x07000040u);
    for (i = 0; i < 14u; ++i)
    {
        word(slice + 0x40u + i * 24u, clothed[i]);
        slice[0x40u + i * 24u + 5u] = i == 13u ? 0u : 1u;
    }
    CHECK(anchor_alternative_patch_action(slice, 0, sizeof(slice), 0x07000020u));
    CHECK(read_word(slice + 0x20) == 0x8000010cu);
    CHECK(read_word(slice + 0x28) == 0x07000040u);
    for (i = 0; i < 14u; ++i)
    {
        CHECK(read_word(slice + 0x40u + i * 24u) == alternative[i]);
        CHECK(slice[0x40u + i * 24u + 5u] == (i == 13u ? 0u : 1u));
    }
    return 0;
}

int main(void)
{
    if (test_mesh_rebase() || test_playable_tree_graft())
        return 1;
    puts("alternative Ebisumaru mesh graft tests passed");
    return 0;
}
