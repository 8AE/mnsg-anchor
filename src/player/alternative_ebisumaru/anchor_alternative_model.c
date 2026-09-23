#include "player/alternative_ebisumaru/anchor_alternative_model.h"

typedef struct AlternativeDisplayMap
{
    unsigned int clothed;
    unsigned int alternative;
} AlternativeDisplayMap;

/* Match the body meshes by branch and ROM vertex bounds. Both actors have
 * corresponding left/right limbs, head and torso. The opening tree has one
 * extra node without a playable joint; extra gameplay nodes retain their
 * native displays. */
static const AlternativeDisplayMap s_body_map[] = {
    {0x09004568u, 0x08004798u},
    {0x090046a0u, 0x080048d8u},
    {0x09004860u, 0x08004a98u},
    {0x09004960u, 0x08004ba0u},
    {0x09004a98u, 0x08004ce0u},
    {0x09004c58u, 0x08004ea0u},
    {0x09004d70u, 0x08004fa8u},
    {0x09004f20u, 0x08005918u},
    {0x09005078u, 0x08005a48u},
    {0x09005260u, 0x080051a8u},
    {0x090053b8u, 0x080052d8u},
    {0x09005598u, 0x08005430u},
    {0x090056e8u, 0x08005588u},
    {0x09005aa8u, 0x08005bc0u},
};

static int inside(unsigned int offset, unsigned int bytes,
                  unsigned int start, unsigned int size)
{
    return offset >= start && bytes <= size && offset - start <= size - bytes;
}

int anchor_alternative_rebase_mesh(unsigned char *broad, unsigned int resident_size)
{
    unsigned int i;
    unsigned int changed = 0;
    unsigned char *mesh;

    if (!broad || resident_size < ANCHOR_ALTERNATIVE_RESOURCE_BYTES)
        return 0;
    mesh = broad + ANCHOR_ALTERNATIVE_MESH_OFFSET;
    /* Only the known display-list region contains relocatable pointer words.
     * Earlier bytes include animation and vertex data with coincidental 0x08
     * high bytes, and the decoded texture pages must remain byte-exact. */
    for (i = ANCHOR_ALTERNATIVE_DISPLAY_START; i < ANCHOR_ALTERNATIVE_DISPLAY_END; i += 4u)
    {
        unsigned int *word = (unsigned int *)(mesh + i);
        unsigned int value = *word;
        if ((value & 0xff000000u) != 0x08000000u)
            continue;
        if ((value & 0x00ffffffu) >= ANCHOR_ALTERNATIVE_RESOURCE_BYTES)
            return 0;
        *word = 0x09000000u + ANCHOR_ALTERNATIVE_MESH_OFFSET +
                (value & 0x00ffffffu);
        ++changed;
    }
    /* 72 pointers are present in the verified US display lists. A mismatch
     * means the loaded resource is different and cannot be safely grafted. */
    return changed == 72u;
}

typedef struct AlternativePatchWalk
{
    unsigned char *slice;
    unsigned int start;
    unsigned int size;
    unsigned int visited[64];
    unsigned int visited_count;
    unsigned int matched_mask;
} AlternativePatchWalk;

static int patch_node(AlternativePatchWalk *walk, unsigned int offset,
                      unsigned int depth)
{
    unsigned char *node;
    unsigned int *display;
    unsigned int i;
    signed char left;
    signed char right;
    unsigned int child;

    if (depth >= 32u || walk->visited_count >= 64u ||
        !inside(offset, 24u, walk->start, walk->size))
        return 0;
    for (i = 0; i < walk->visited_count; ++i)
        if (walk->visited[i] == offset)
            return 0;
    walk->visited[walk->visited_count++] = offset;
    node = walk->slice + offset - walk->start;
    display = (unsigned int *)node;
    for (i = 0; i < sizeof(s_body_map) / sizeof(s_body_map[0]); ++i)
    {
        if (*display == s_body_map[i].clothed)
        {
            if (walk->matched_mask & (1u << i))
                return 0;
            walk->matched_mask |= 1u << i;
            *display = 0x09000000u + ANCHOR_ALTERNATIVE_MESH_OFFSET +
                       (s_body_map[i].alternative & 0x00ffffffu);
            break;
        }
    }
    left = (signed char)node[4];
    right = (signed char)node[5];
    if (left)
    {
        child = offset + (int)left * 24;
        if (!patch_node(walk, child, depth + 1u))
            return 0;
    }
    if (right)
    {
        child = offset + (int)right * 24;
        if (!patch_node(walk, child, depth + 1u))
            return 0;
    }
    return 1;
}

int anchor_alternative_patch_action(unsigned char *slice, unsigned int slice_start,
                              unsigned int slice_size, unsigned int model_ptr)
{
    AlternativePatchWalk walk;
    unsigned int model_offset = model_ptr & 0x00ffffffu;
    const unsigned int *header;

    if (!slice || (model_ptr & 0xff000000u) != 0x07000000u ||
        !inside(model_offset, 12u, slice_start, slice_size))
        return 0;
    header = (const unsigned int *)(slice + model_offset - slice_start);
    if (!(header[0] & 0x80000000u) ||
        (header[2] & 0xff000000u) != 0x07000000u)
        return 0;
    walk.visited_count = 0;
    walk.matched_mask = 0;
    walk.slice = slice;
    walk.start = slice_start;
    walk.size = slice_size;
    if (!patch_node(&walk, header[2] & 0x00ffffffu, 0u))
        return 0;
    return walk.matched_mask == (1u <<
        (sizeof(s_body_map) / sizeof(s_body_map[0]))) - 1u;
}
