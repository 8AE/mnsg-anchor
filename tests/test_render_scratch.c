#include "anchor_render_scratch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef union AlignedBytes
{
    unsigned long long alignment;
    unsigned char bytes[0x40000];
} AlignedBytes;
typedef struct RenderSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} RenderSceneResource;

RenderSceneResource D_80167FC0_168BC0[48];
unsigned char *D_8015C5C8_15D1C8;
unsigned int *D_8015C5CC_15D1CC;
unsigned char *D_80168504_169104;
short D_800C7A72_C8672;
int D_801684F8_1690F8, D_801684FC_1690FC;
static AlignedBytes s_native, s_scratch, s_assets, s_objects;
static unsigned int s_asset_bytes;
static unsigned int s_reports;
void anchor_render_scratch_begin_bank(void);
void anchor_render_scratch_begin_object(void *object);
void anchor_render_scratch_end_object(void);
void anchor_render_scratch_test_bind(void *arena, unsigned int bank_bytes);

void recomp_printf(const char *format, ...)
{
    (void)format;
    ++s_reports;
}

static void word(unsigned char *base, unsigned int offset, unsigned int value)
{
    memcpy(base + offset, &value, sizeof(value));
}

static void half(unsigned char *base, unsigned int offset, unsigned short value)
{
    memcpy(base + offset, &value, sizeof(value));
}

int anchor_player_models_is_remote_object(const void *object)
{
    return object == s_objects.bytes || object == s_objects.bytes + 0x100;
}

static const void *resolve_asset(unsigned int address, unsigned int bytes,
                                  const void *context)
{
    unsigned int offset;
    (void)context;
    if (address < 0x07001000u)
        return 0;
    offset = address - 0x07001000u;
    if (offset > s_asset_bytes || bytes > s_asset_bytes - offset)
        return 0;
    return s_assets.bytes + offset;
}

const void *anchor_player_models_resolve_render_address(
    const void *object, unsigned int address, unsigned int bytes)
{
    return resolve_asset(address, bytes, object);
}

static void fixture(void)
{
    memset(&s_assets, 0, sizeof(s_assets));
    memset(&s_objects, 0, sizeof(s_objects));
    s_asset_bytes = 4096;
    word(s_objects.bytes, 0x2c, 0x67001000u);
    word(s_objects.bytes, 0x30, 0xc01fc680u);
    s_objects.bytes[0x64] = 0x80;
    s_objects.bytes[0x65] = 1;
    word(s_objects.bytes, 0x1c, 0x3dcccccdu); /* Preserve model scale. */
    word(s_objects.bytes, 0x90, 0x11223344u); /* Opaque unrelated object state. */
    memcpy(s_objects.bytes + 0x100, s_objects.bytes, 0x100);
    word(s_assets.bytes, 0, 0x80000010u);
    word(s_assets.bytes, 8, 0x07001040u);
    word(s_assets.bytes, 64, 0x09001000u);
    s_assets.bytes[68] = 1; /* Root's right child, at +24 bytes. */
    word(s_assets.bytes, 88, 0x09002000u);
}

static int budget(AnchorRenderBudget available, AnchorRenderBudget *out)
{
    return anchor_render_player_budget(s_objects.bytes, resolve_asset,
                                       0, &available, out);
}

static void native_tree_budget(void)
{
    AnchorRenderBudget enough = {100000u, 100000u}, result;
    fixture();
    assert(budget(enough, &result));
    assert(result.commands == 160 && result.matrices == 7);
    half(s_assets.bytes, 88 + 12, 0x4000u);
    assert(budget(enough, &result));
    assert(result.commands == 160 && result.matrices == 9);
    word(s_assets.bytes, 0, 0x00000010u); /* Native skips its skeleton tree. */
    assert(budget(enough, &result));
    assert(result.commands == 96 && result.matrices == 6);
    word(s_assets.bytes, 8, 0xfffffff0u);
    assert(budget(enough, &result));
    word(s_assets.bytes, 0, 0x80000010u);
    assert(!budget(enough, &result));
    word(s_assets.bytes, 8, 0);
    assert(!budget(enough, &result));

    fixture();
    assert(!budget((AnchorRenderBudget){159, 7}, &result));
    assert(!budget((AnchorRenderBudget){160, 6}, &result));
    assert(budget((AnchorRenderBudget){160, 7}, &result));
    s_asset_bytes = 100; /* The last node extends past its bound asset. */
    assert(!budget(enough, &result));
    fixture();
    word(s_objects.bytes, 0x2c, 0x17001000u);
    assert(!budget(enough, &result));
    word(s_objects.bytes, 0x2c, 0x67001001u);
    assert(!budget(enough, &result));
    word(s_objects.bytes, 0x2c, 0x67001000u);
    word(s_objects.bytes, 0x30, 0xe01fc680u); /* Unbounded inline material copy. */
    assert(!budget(enough, &result));
}

static void nested_graphs_and_native_stack_limit(void)
{
    AnchorRenderBudget enough = {100000u, 100000u}, result;
    unsigned int i;
    fixture();
    word(s_assets.bytes, 88, 0x17001100u); /* Nested wrapper at asset +256. */
    word(s_assets.bytes, 256, 0x09002000u);
    s_assets.bytes[260] = 1;
    s_assets.bytes[261] = 2;
    assert(budget(enough, &result));
    assert(result.commands == 256 && result.matrices == 9);

    fixture();
    for (i = 0; i < 16; ++i)
        s_assets.bytes[64 + i * 24 + 4] = 1;
    assert(budget(enough, &result)); /* Root consumes no stack; 16 children do. */
    s_assets.bytes[64 + 16 * 24 + 4] = 1;
    assert(!budget(enough, &result));
    s_assets.bytes[64 + 16 * 24 + 4] = 0;
    half(s_assets.bytes, 88 + 12, 0x4000u); /* Billboard costs two stack slots. */
    assert(!budget(enough, &result));

    fixture();
    s_assets.bytes[64 + 4] = 0;
    for (i = 0; i < 60; ++i)
        s_assets.bytes[64 + i * 24 + 5] = 1;
    assert(budget(enough, &result)); /* Sibling breadth is not stack depth. */
    fixture();
    s_assets.bytes[88 + 5] = (unsigned char)-1; /* Cycle back to root. */
    assert(!budget((AnchorRenderBudget){1024u, 1024u}, &result));
}

static void reserve_actual_scene_bytes(void)
{
    unsigned int start = 1, bank = 2, end = 3;
    assert(anchor_render_scratch_plan(0x80600003u, &start, &bank, &end));
    assert(start == 0x80600040u && (bank & 63u) == 0);
    assert(end == start + 2 * bank);
    assert(0x80800000u - end >= end - start); /* At least half stays free. */
    assert(anchor_render_scratch_plan(0xc0600003u, &start, &bank, &end));
    assert(!anchor_render_scratch_plan(0x81000000u, &start, &bank, &end));
    assert(!anchor_render_scratch_plan(0x807ffff0u, &start, &bank, &end));
    assert(!anchor_render_scratch_plan(0u, &start, &bank, &end));
}

static void setup_native(void)
{
    fixture();
    memset(&s_native, 0xa5, sizeof(s_native));
    memset(&s_scratch, 0x5a, sizeof(s_scratch));
    D_8015C5C8_15D1C8 = s_native.bytes;
    D_800C7A72_C8672 = 0;
    D_8015C5CC_15D1CC = (unsigned int *)s_native.bytes;
    D_80168504_169104 = s_native.bytes + 10640 * 8;
    D_801684F8_1690F8 = -1;
    D_801684FC_1690FC = 1;
    s_reports = 0;
    anchor_render_scratch_test_bind(s_scratch.bytes, 32768u);
}

static unsigned char *draw_object(unsigned char *object, unsigned char value)
{
    unsigned char *matrices;
    unsigned char old[0x100];
    memcpy(old, object, sizeof(old));
    anchor_render_scratch_begin_object(object);
    assert(object[0x64] == 0x80);
    matrices = D_80168504_169104;
    memset(matrices, value, 128u);
    D_80168504_169104 += 128u;
    D_8015C5CC_15D1CC[0] = 0x01040040u;
    D_8015C5CC_15D1CC[1] = (unsigned int)(unsigned long)matrices & 0x1fffffffu;
    D_8015C5CC_15D1CC += 2;
    anchor_render_scratch_end_object();
    assert(memcmp(old, object, sizeof(old)) == 0);
    return matrices;
}

static void private_lists_chain_and_keep_submitted_storage_alive(void)
{
    unsigned int *native_head;
    unsigned int *first_list;
    unsigned char *native_matrix;
    unsigned char *first;
    unsigned char *second;
    setup_native();
    native_head = D_8015C5CC_15D1CC;
    native_matrix = D_80168504_169104;
    first = draw_object(s_objects.bytes, 0x11);
    assert(first == s_scratch.bytes);
    assert(D_80168504_169104 == native_matrix);
    assert(D_8015C5CC_15D1CC == native_head + 2);
    assert(native_head[0] == 0x06000000u);
    first_list = (unsigned int *)(first + 7u * 64u);
    assert(first_list[2] == 0xb8000000u);
    second = draw_object(s_objects.bytes + 0x100, 0x22);
    assert(second >= (unsigned char *)(first_list + 4));
    assert(first[0] == 0x11 && second[0] == 0x22);
    assert(first_list[2] == 0x06010000u); /* Branch replaces the prior end. */
    assert(D_8015C5CC_15D1CC == native_head + 2); /* No per-peer stock growth. */
    D_8015C5CC_15D1CC += 2; /* An intervening native draw preserves ordering. */
    draw_object(s_objects.bytes, 0x33);
    assert(D_8015C5CC_15D1CC == native_head + 6);
    assert(first[0] == 0x11);

    D_800C7A72_C8672 = 1;
    D_8015C5CC_15D1CC = (unsigned int *)(s_native.bytes + 0x1d6d8u);
    anchor_render_scratch_begin_bank();
    second = draw_object(s_objects.bytes, 0x44);
    assert(second == s_scratch.bytes + 32768u && first[0] == 0x11);
    D_800C7A72_C8672 = 0;
    D_8015C5CC_15D1CC = native_head;
    anchor_render_scratch_begin_bank();
    assert(draw_object(s_objects.bytes, 0x55) == first);
}

static void exhaustion_skips_only_the_draw_and_preserves_colliders(void)
{
    unsigned char old[0x100];
    unsigned char *matrix;
    unsigned int *head;
    unsigned int i;
    setup_native();
    /* More than 26 successful draws use one stock command and keep all
     * previously submitted matrices intact until the frame bank is reset. */
    for (i = 0; i < 40; ++i)
        draw_object(s_objects.bytes, (unsigned char)i);
    for (; i < 200; ++i)
    {
        matrix = D_80168504_169104;
        head = D_8015C5CC_15D1CC;
        memcpy(old, s_objects.bytes, sizeof(old));
        anchor_render_scratch_begin_object(s_objects.bytes);
        if (s_objects.bytes[0x64] & 1u)
        {
            assert(D_80168504_169104 == matrix && D_8015C5CC_15D1CC == head);
            anchor_render_scratch_end_object();
            assert(memcmp(old, s_objects.bytes, sizeof(old)) == 0);
            break;
        }
        D_8015C5CC_15D1CC += 2;
        D_80168504_169104 += 128;
        anchor_render_scratch_end_object();
    }
    assert(i < 200 && s_reports > 0);
    setup_native();
    s_objects.bytes[0x64] |= 1; /* Existing recovery flicker remains hidden. */
    memcpy(old, s_objects.bytes, sizeof(old));
    anchor_render_scratch_begin_object(s_objects.bytes);
    anchor_render_scratch_end_object();
    assert(memcmp(old, s_objects.bytes, sizeof(old)) == 0);
    setup_native();
    D_801684F8_1690F8 = 33;
    anchor_render_scratch_begin_object(s_objects.bytes);
    assert(s_objects.bytes[0x64] & 1);
    anchor_render_scratch_end_object();
    assert(s_objects.bytes[0x64] == 0x80);
    setup_native();
    D_8015C5CC_15D1CC = (unsigned int *)(s_native.bytes + (10640u - 512u) * 8u);
    head = D_8015C5CC_15D1CC;
    matrix = D_80168504_169104;
    anchor_render_scratch_begin_object(s_objects.bytes);
    assert(s_objects.bytes[0x64] & 1u);
    anchor_render_scratch_end_object();
    assert(D_8015C5CC_15D1CC == head && D_80168504_169104 == matrix);
    assert(s_objects.bytes[0x64] == 0x80);
    setup_native();
    word(s_objects.bytes, 0x2c, 0x67001001u);
    anchor_render_scratch_begin_object(s_objects.bytes);
    assert(s_objects.bytes[0x64] & 1u);
    anchor_render_scratch_end_object();
    assert(s_objects.bytes[0x64] == 0x80);
}

int main(void)
{
    native_tree_budget();
    nested_graphs_and_native_stack_limit();
    reserve_actual_scene_bytes();
    private_lists_chain_and_keep_submitted_storage_alive();
    exhaustion_skips_only_the_draw_and_preserves_colliders();
    puts("Render scratch and native preflight tests passed");
    return 0;
}
