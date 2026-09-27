#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "combat/anchor_projectile_models.h"

extern int anchor_projectile_recipe_id(unsigned int model, int family);
extern int anchor_projectile_material_decode(unsigned int context,
                                             const unsigned int *commands,
                                             int *material,
                                             unsigned int *prim_rgb,
                                             unsigned int *env_rgba);
extern int anchor_projectile_material_build(int material, unsigned int prim_rgb,
                                            unsigned int env_rgba,
                                            unsigned int commands[8]);
extern int anchor_projectile_material_word_offset_for_test(int index, int layer,
                                                            int fire_trail_bank);
short D_800C7A72_C8672;

static void test_character_and_effect_recipes(void)
{
    /* Native constructor model commands: all four character files, plus
     * common file 0x80. Similar offsets in different files are not aliases. */
    static const struct { unsigned int model; int family; } required[] = {
        {0x49009498u, 0}, {0x1900065cu, 0},
        {0x49009020u, 0}, {0x49009238u, 0}, {0x49009348u, 0},
        {0x49007c40u, 1}, {0x190003e0u, 4},
        {0x49007f90u, 2}, {0x19000064u, 2}, {0x49008470u, 2},
        {0x49008220u, 2}, {0x190006c4u, 2}, {0x19000a24u, 2},
        {0x19000120u, 2}, {0x190001e0u, 2}, {0x190002a0u, 2},
        {0x19000360u, 2},
        {0x4900a350u, 3}, {0x4900a430u, 3},
        {0x19000124u, 3}, {0x1900025cu, 3},
        {0x1900025cu, 4},
        {0x4900b878u, 0}, {0x49002790u, 4},
    };
    int ids[sizeof(required) / sizeof(required[0])];
    unsigned int i, j;
    for (i = 0; i < sizeof(required) / sizeof(required[0]); ++i)
    {
        ids[i] = anchor_projectile_recipe_id(required[i].model, required[i].family);
        assert(ids[i] > 0 && ids[i] <= 255);
        for (j = 0; j < i; ++j)
            assert(ids[i] != ids[j]);
    }
    assert(anchor_projectile_recipe_id(0x49009498u, 2) == 0);
    assert(anchor_projectile_recipe_id(0x49007c40u, 0) == 0);
    assert(anchor_projectile_recipe_id(0x1900025cu, 0) == 0);
    assert(anchor_projectile_recipe_id(0x49009498u, -1) == 0);
    assert(anchor_projectile_recipe_id(0x49009498u, 5) == 0);
    assert(anchor_projectile_recipe_id(0xdeadbeefu, 2) == 0);
    assert(anchor_projectile_recipe_id(0u, 4) == 0);
    assert(anchor_projectile_recipe_id(0x49007f91u, 2) == 0);
}

static void test_fire_ryo_trail_cadence_and_fade(void)
{
    AnchorFireRyoTrail trails[ANCHOR_FIRE_RYO_TRAIL_COUNT] = {0};
    int tick;
    for (tick = 2; tick <= 12; ++tick)
    {
        anchor_projectile_fire_ryo_trail_step(trails, 1, tick,
            (float)tick, 2.0f, 3.0f, 4, 5, 6);
        if (tick == 2)
            assert(trails[0].alpha == 0);
        if (tick == 3)
        {
            assert(trails[0].alpha == 0xf0);
            assert(trails[0].scale == 0.25f);
            assert(trails[0].x == 3.0f && trails[0].ry == 5);
        }
    }
    assert(trails[0].alpha > 0 && trails[1].alpha > 0 &&
           trails[2].alpha > 0 && trails[3].alpha > 0);
    anchor_projectile_fire_ryo_trail_step(trails, 0, 15,
        15.0f, 2.0f, 3.0f, 4, 5, 6);
    assert(trails[0].x == 3.0f && trails[0].scale > 0.25f);
    for (tick = 0; tick < 10; ++tick)
        anchor_projectile_fire_ryo_trail_step(trails, 0, 16 + tick,
            0, 0, 0, 0, 0, 0);
    for (tick = 0; tick < ANCHOR_FIRE_RYO_TRAIL_COUNT; ++tick)
        assert(trails[tick].alpha == 0);
}

static void test_direct_material_styles(void)
{
    int material = -1;
    unsigned int prim, env, style;
    assert(anchor_projectile_material_decode(0, 0, &material, &prim, &env));
    assert(material == 0 && prim == 0 && env == 0xffffffffu);
    for (style = 0; style < 4; ++style)
    {
        unsigned int context = 0x8006d920u | (style << 29);
        assert(anchor_projectile_material_decode(context, 0, &material, &prim, &env));
        assert(material == (int)style + 1);
        assert(prim == 0 && env == 0xffffffffu);
    }
    assert(!anchor_projectile_material_decode(0x8006d924u, 0, &material, &prim, &env));
    assert(!anchor_projectile_material_decode(0x80123450u, 0, &material, &prim, &env));
}

static void test_dynamic_templates_styles_and_alpha(void)
{
    static const unsigned int templates[] = {
        0x802049c0u, 0xe0204c28u, 0xa0204c78u,
        0xa0204c90u, 0x80204ca8u, 0x8006dde8u,
    };
    unsigned int template_index, primitive, style;
    int identifiers[53] = {0};
    for (template_index = 0; template_index < sizeof(templates) / sizeof(templates[0]); ++template_index)
    {
        for (primitive = 0; primitive < 2; ++primitive)
        {
            for (style = 0; style < 4; ++style)
            {
                unsigned int commands[8] = {0x06000000u, templates[template_index], 0, 0, 0, 0, 0, 0};
                unsigned int rebuilt[8];
                unsigned int prim, env;
                unsigned int context = 0x80100000u | (style << 29);
                unsigned int offset = primitive ? 4 : 2;
                int material, again;
                if (primitive)
                {
                    commands[2] = 0xfa000000u;
                    commands[3] = 0x0044ff00u;
                }
                commands[offset] = 0xfb000000u;
                commands[offset + 1] = 0x2277cc00u | (style == 0 ? 0u : style == 3 ? 255u : 127u);
                commands[offset + 2] = 0xb8000000u;
                assert(anchor_projectile_material_decode(context, commands, &material, &prim, &env));
                assert(material >= 5 && material < 53);
                assert(!identifiers[material]);
                identifiers[material] = 1;
                assert(prim == (primitive ? 0x0044ffu : 0));
                assert(env == commands[offset + 1]);
                assert(anchor_projectile_material_build(material, prim, env, rebuilt));
                assert(memcmp(commands, rebuilt, sizeof(commands)) == 0);
                assert(anchor_projectile_material_decode(context, rebuilt, &again, &prim, &env));
                assert(again == material);
            }
        }
    }
}

static void test_blue_explosion_fade_round_trip(void)
{
    unsigned int native_commands[8] = {
        0x06000000u, 0xe0204c28u, 0xfa000000u, 0x0000aa00u,
        0xfb000000u, 0x0056ffe7u, 0xb8000000u, 0,
    };
    unsigned int rebuilt[8];
    unsigned int prim, env;
    int material;
    /* Sasuke 801EFF74 / 801EFBDC call FUN_801DC554 with the task+0xA0
     * scratch list tagged by 0x60000000. Phase one fades alpha by 12. */
    assert(anchor_projectile_material_decode(0xe01000a0u, native_commands, &material, &prim, &env));
    assert(prim == 0x0000aau && env == 0x0056ffe7u);
    assert(anchor_projectile_material_build(material, prim, env, rebuilt));
    assert(memcmp(rebuilt, native_commands, sizeof(rebuilt)) == 0);
    native_commands[5] = 0x0056ffdbu;
    assert(anchor_projectile_material_decode(0xe01000a0u, native_commands, &material, &prim, &env));
    assert(anchor_projectile_material_build(material, prim, env, rebuilt));
    assert(memcmp(rebuilt, native_commands, sizeof(rebuilt)) == 0);
    /* The second native layer has a cyan primitive and white environment. */
    native_commands[3] = 0x28d4ff00u;
    native_commands[5] = 0xffffff00u;
    assert(anchor_projectile_material_decode(0xe01000a0u, native_commands, &material, &prim, &env));
    assert(anchor_projectile_material_build(material, prim, env, rebuilt));
    assert(memcmp(rebuilt, native_commands, sizeof(rebuilt)) == 0);
}

static void test_reject_malformed_commands_and_materials(void)
{
    static const unsigned int valid[8] = {
        0x06000000u, 0x802049c0u, 0xfa000000u, 0x12345600u,
        0xfb000000u, 0xffffffffu, 0xb8000000u, 0,
    };
    static const struct { int index; unsigned int bad; } corruptions[] = {
        {0, 0x06010000u}, {0, 0xde000000u}, {0, 0xb8000000u},
        {1, 0x802049c4u}, {1, 0xdeadbeefu}, {1, 0},
        {2, 0xfa000001u}, {2, 0xf8000000u}, {3, 0x12345601u},
        {4, 0xfb000001u}, {4, 0xfa000000u},
        {6, 0xb8000001u}, {6, 0x06000000u}, {7, 1},
    };
    unsigned int commands[8], preserved[8], prim, env, i;
    int material;
    assert(!anchor_projectile_material_decode(0x80100000u, valid, 0, &prim, &env));
    assert(!anchor_projectile_material_decode(0x80100000u, valid, &material, 0, &env));
    assert(!anchor_projectile_material_decode(0x80100000u, valid, &material, &prim, 0));
    assert(!anchor_projectile_material_build(5, 0, 0, 0));
    for (i = 0; i < sizeof(corruptions) / sizeof(corruptions[0]); ++i)
    {
        memcpy(commands, valid, sizeof(commands));
        commands[corruptions[i].index] = corruptions[i].bad;
        assert(!anchor_projectile_material_decode(0x80100000u, commands, &material, &prim, &env));
    }
    memcpy(commands, valid, sizeof(commands));
    memcpy(preserved, commands, sizeof(commands));
    for (material = -1; material <= 4; ++material)
        assert(!anchor_projectile_material_build(material, 0, 0, commands));
    assert(!anchor_projectile_material_build(53, 0, 0, commands));
    assert(!anchor_projectile_material_build(255, 0, 0, commands));
    assert(!anchor_projectile_material_build(5, 0x1000000u, 0, commands));
    assert(memcmp(preserved, commands, sizeof(commands)) == 0);
}

static void test_native_graphics_bank_material_ownership(void)
{
    unsigned int arena[2048];
    unsigned int first_bank_env[4];
    int layer, tick;
    memset(arena, 0x5a, sizeof(arena));
    D_800C7A72_C8672 = 0;
    /* Fire Ryo trail layer 1 uses its projectile slot; layers 2-4 use
     * a separate trail bank. All four must follow the same graphics bank. */
    for (tick = 0; tick < 3; ++tick)
        for (layer = 1; layer <= ANCHOR_FIRE_RYO_TRAIL_COUNT; ++layer)
        {
            int offset = anchor_projectile_material_word_offset_for_test(2, layer, 1);
            assert(offset >= 0 && offset + 8 <= (int)(sizeof(arena) / sizeof(arena[0])));
            assert(anchor_projectile_material_build(38, 0,
                0xf50d0000u | (unsigned int)(0xf0 - tick * 0x1a), arena + offset));
            assert(arena[offset + 8] == 0x5a5a5a5au);
        }
    for (layer = 1; layer <= ANCHOR_FIRE_RYO_TRAIL_COUNT; ++layer)
    {
        int offset = anchor_projectile_material_word_offset_for_test(2, layer, 1);
        first_bank_env[layer - 1] = arena[offset + 3];
    }
    D_800C7A72_C8672 = 1;
    for (layer = 1; layer <= ANCHOR_FIRE_RYO_TRAIL_COUNT; ++layer)
    {
        int offset = anchor_projectile_material_word_offset_for_test(2, layer, 1);
        assert(offset >= 0 && (offset & 7) == 0);
        assert(arena[offset] == 0x5a5a5a5au);
        assert(anchor_projectile_material_build(38, 0, 0xf50d0090u,
                                                arena + offset));
        assert(arena[offset - 8 + 3] == first_bank_env[layer - 1]);
    }
    /* Other dynamic projectile materials use the same native selector. */
    assert(anchor_projectile_material_word_offset_for_test(3, 0, -1) ==
           (3 * 2 * 2 + 1) * 8);
    D_800C7A72_C8672 = -1;
    assert(anchor_projectile_material_word_offset_for_test(2, 1, 1) == -1);
    D_800C7A72_C8672 = 2;
    assert(anchor_projectile_material_word_offset_for_test(2, 4, 1) == -1);
    D_800C7A72_C8672 = 0;
}

int main(void)
{
    test_character_and_effect_recipes();
    test_fire_ryo_trail_cadence_and_fade();
    test_direct_material_styles();
    test_dynamic_templates_styles_and_alpha();
    test_blue_explosion_fade_round_trip();
    test_reject_malformed_commands_and_materials();
    test_native_graphics_bank_material_ownership();
    puts("Projectile recipe and material tests passed");
    return 0;
}
