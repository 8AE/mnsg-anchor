#include "anchor_render_scratch.h"

typedef struct BudgetWalk
{
    AnchorRenderResolve resolve;
    const void *context;
    AnchorRenderBudget available;
    AnchorRenderBudget used;
} BudgetWalk;

static int charge(BudgetWalk *walk, unsigned int commands, unsigned int matrices)
{
    if (commands > walk->available.commands - walk->used.commands ||
        matrices > walk->available.matrices - walk->used.matrices)
        return 0;
    walk->used.commands += commands;
    walk->used.matrices += matrices;
    return 1;
}

static int offset_node(unsigned int address, signed char offset,
                       unsigned int *out)
{
    unsigned int amount = (unsigned int)(offset < 0 ? -offset : offset) * 24u;
    if (offset < 0)
    {
        if (address < amount)
            return 0;
        *out = address - amount;
    }
    else
    {
        *out = address + amount;
        if (*out < address)
            return 0;
    }
    return 1;
}

/* Left links are siblings: native 18CA0 pops its matrix before following
 * them. Right links retain the current transform. Every visit charges a
 * positive amount, so cyclic/malformed graphs exhaust the caller's budget. */
static int walk_skeleton(BudgetWalk *walk, unsigned int address,
                         unsigned int depth, int root)
{
    while (address)
    {
        const unsigned char *node = walk->resolve(address, 24, walk->context);
        unsigned int display;
        unsigned int child;
        unsigned int weight;
        unsigned int billboard;
        if (!node)
            return 0;
        display = *(const unsigned int *)node;
        billboard = (*(const unsigned short *)(node + 12) |
                     *(const unsigned short *)(node + 14) |
                     *(const unsigned short *)(node + 16)) & 0x4000u;
        weight = root ? 0u : billboard ? 2u : 1u;
        if (depth + weight > 16u ||
            !charge(walk, 32u, root ? 0u : billboard ? 3u : 1u))
            return 0;

        /* A tagged display reference wraps another skeleton root. Native
         * code draws that root without a new transform, then visits its two
         * branches before returning to the current node's own branches. */
        if (!root && (display & 0x10000000u))
        {
            unsigned int nested = display & 0x8fffffffu;
            const unsigned char *wrapper = walk->resolve(nested, 24, walk->context);
            if (!wrapper || !charge(walk, 32u, 0))
                return 0;
            if ((signed char)wrapper[4])
            {
                if (!offset_node(nested, (signed char)wrapper[4], &child) ||
                    !walk_skeleton(walk, child, depth + weight, 0))
                    return 0;
            }
            if ((signed char)wrapper[5])
            {
                if (!offset_node(nested, (signed char)wrapper[5], &child) ||
                    !walk_skeleton(walk, child, depth + weight, 0))
                    return 0;
            }
        }
        if ((signed char)node[4])
        {
            if (!offset_node(address, (signed char)node[4], &child) ||
                !walk_skeleton(walk, child, depth + weight, 0))
                return 0;
        }
        if (!(signed char)node[5])
            return 1;
        if (!offset_node(address, (signed char)node[5], &address))
            return 0;
        root = 0;
    }
    return 1;
}

int anchor_render_player_budget(const void *pointer,
                                AnchorRenderResolve resolve,
                                const void *context,
                                const AnchorRenderBudget *available,
                                AnchorRenderBudget *out)
{
    const unsigned char *object = pointer;
    const unsigned int *header;
    unsigned int model;
    BudgetWalk walk;
    if (!object || !resolve || !available || !out)
        return 0;
    model = *(const unsigned int *)(object + 0x2c);
    /* Low bit zero makes 16C44 set native 8016852D=1 for this complete
     * draw. Only 16C44 writes that flag, so 196F0's CPU-transform matrix
     * branch cannot add an uncounted matrix at a root or wrapper. */
    if ((model & 0x70000001u) != 0x60000000u ||
        *(const unsigned int *)(object + 0x30) != 0xc01fc680u)
        return 0;
    header = resolve(model & 0x8ffffffeu, 12u, context);
    if (!header)
        return 0;
    walk.resolve = resolve;
    walk.context = context;
    walk.available = *available;
    walk.used.commands = walk.used.matrices = 0;
    /* Root, default camera setup, lighting and return command. Six matrices
     * cover camera's four plus the root's possible billboard pair. */
    if (!charge(&walk, 96u, 6u))
        return 0;
    if ((header[0] & 0x80000000u) &&
        (!header[2] || !walk_skeleton(&walk, header[2], 0, 1)))
        return 0;
    *out = walk.used;
    return 1;
}

int anchor_render_scratch_plan(unsigned int cursor, unsigned int *start,
                               unsigned int *bank_bytes, unsigned int *end)
{
    unsigned int aligned = (cursor & 0xbfffffffu);
    unsigned int bank;
    if (!start || !bank_bytes || !end || aligned < 0x80001000u ||
        aligned >= 0x80800000u)
        return 0;
    aligned = (aligned + 63u) & ~63u;
    /* Allocate half the remaining original-RDRAM window, in two equal
     * banks. Keep the other half available to later native scene resources. */
    bank = ((0x80800000u - aligned) / 4u) & ~63u;
    if (bank < 1024u)
        return 0;
    *start = aligned;
    *bank_bytes = bank;
    *end = aligned + bank * 2u;
    return 1;
}
