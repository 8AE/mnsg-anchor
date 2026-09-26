#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ANCHOR_FREEZE_PROMPT_HOST_TEST
#include "../src/ui/anchor_freeze_prompt.c"

void *D_80167C48_168848[3];
void *D_80167C54_168854 = (void *)1;
void *D_80077858_78458;
void *D_80077860_78460 = (void *)2;
const unsigned char D_8005BB10_5C710[95] = {
    8, 2, 6, 7, 2, 7, 2, 3, 6, 5, 6, 6, 3, 6, 2, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 4, 2, 6, 6, 8, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 2, 5, 7, 2,
    2, 6, 6, 6, 6, 6, 5, 6, 6, 4, 5, 6, 4, 8, 6, 6,
    6, 6, 6, 6, 5, 6, 6, 8, 6, 6, 6, 2, 2, 2, 2
};

static union { unsigned long long alignment; unsigned char bytes[0x1818]; } window;
static union { unsigned long long alignment; unsigned char bytes[0x1818]; } other;
static int frozen, allocations, frees, flash, glyphs, newlines;
static unsigned char control, buttons;
static char rendered[64];

int anchor_player_freeze_active(void) { return frozen; }

void func_8000C260_CE60(int slot, int x, int y, int tx, int ty,
                        int style, int animation)
{
    anchor_freeze_prompt_before_native_window(slot, x, y, tx, ty,
                                               style, animation);
    assert(!D_80167C48_168848[slot]);
    if (slot == PROMPT_SLOT) {
        assert(x == 32 && y == 84 && tx == 32 && ty == 84);
        assert(style == 10 && animation == 0);
        memset(window.bytes, 0, sizeof(window.bytes));
        glyphs = newlines = flash = 0;
        memset(rendered, 0, sizeof(rendered));
        D_80167C48_168848[slot] = window.bytes;
    } else {
        D_80167C48_168848[slot] = other.bytes;
    }
    ++allocations;
}

void func_8000C878_D478(int slot)
{
    assert(slot == PROMPT_SLOT && D_80167C48_168848[slot] == window.bytes);
    D_80167C48_168848[slot] = 0;
    ++frees;
}

void func_8000C454_D054(int slot) { assert(slot == PROMPT_SLOT); flash = 1; }
void func_8000C488_D088(int slot) { assert(slot == PROMPT_SLOT); flash = 0; }
void func_8000C3EC_CFEC(int slot) { assert(slot == PROMPT_SLOT); ++newlines; }
void func_8000D060_DC60(int slot, unsigned int glyph)
{
    unsigned char *record;
    assert(slot == PROMPT_SLOT && glyph < 95u && glyphs < 120);
    record = window.bytes + 0x198u + glyphs * 0x30u;
    *(unsigned short *)(record + 4) = 1;
    *(unsigned short *)(record + 6) =
        (unsigned short)(0xa5a0u | (glyph & 1u));
    *(unsigned short *)(record + 0x2c) = flash ? 2u : 0u;
    rendered[glyphs] = (char)(glyph + 0x20u);
    ++glyphs;
}

static void reset(void)
{
    anchor_freeze_prompt_yield();
    D_80167C48_168848[0] = 0;
    D_80167C48_168848[1] = 0;
    D_80167C48_168848[2] = 0;
    D_80167C54_168854 = (void *)1;
    D_80077858_78458 = 0;
    D_80077860_78460 = (void *)2;
    frozen = allocations = frees = flash = glyphs = newlines = 0;
    control = 2;
    buttons = 3;
    memset(rendered, 0, sizeof(rendered));
}

static void opens_nonmodal_centered_blinking_prompt(void)
{
    int i;
    reset();
    frozen = 1;
    anchor_freeze_prompt_update();
    assert(anchor_freeze_prompt_visible());
    assert(allocations == 1 && frees == 0 && newlines == 1);
    assert(glyphs == (int)strlen("FROZEN!!!Mash A + B to escape!"));
    assert(strcmp(rendered, "FROZEN!!!Mash A + B to escape!") == 0);
    for (i = 0; i < glyphs; ++i) {
        unsigned char *record = window.bytes + 0x198u + i * 0x30u;
        int title = i < (int)strlen("FROZEN!!!");
        unsigned int glyph = (unsigned char)rendered[i] - 0x20u;
        unsigned short palette = (unsigned short)(glyph & 1u);
        if (!title && rendered[i] == 'A')
            palette = 7u;
        if (!title && rendered[i] == 'B')
            palette = 8u;
        assert(*(unsigned short *)(record + 6) ==
               (unsigned short)(0xa5a0u | palette));
        assert(*(unsigned short *)(record + 0x2c) == (title ? 2u : 0u));
        assert(*(float *)(record + 0x14) == (title ? 1.5f : 1.0f));
        assert(*(float *)(record + 0x18) == (title ? 1.5f : 1.0f));
        assert(*(short *)(record + 10) == (title ? TITLE_Y : BODY_Y));
        assert(*(short *)(record + 8) > 0 &&
               *(short *)(record + 8) < PROMPT_WIDTH);
        if (i && i != (int)strlen("FROZEN!!!"))
            assert(*(short *)(record + 8) >
                   *(short *)(record - 0x30u + 8));
    }
    /* A native window by itself does not set a scenario, control bit or
     * consume the A/B input used by the freeze-mash state machine. */
    assert(!D_80077858_78458 && control == 2 && buttons == 3 && frozen);
    anchor_freeze_prompt_update();
    assert(allocations == 1 && frees == 0);
    frozen = 0;
    anchor_freeze_prompt_update();
    assert(!anchor_freeze_prompt_visible() && frees == 1);
}

static void keeps_other_ab_glyphs_white(void)
{
    unsigned int record_index;
    unsigned char *a, *b;
    reset();
    frozen = 1;
    anchor_freeze_prompt_update();
    record_index = (unsigned int)glyphs;
    assert(append_line("AB", &record_index, 1.0f, TITLE_Y, 0));
    a = window.bytes + 0x198u + (record_index - 2u) * 0x30u;
    b = a + 0x30u;
    assert(*(unsigned short *)(a + 6) == 0xa5a1u);
    assert(*(unsigned short *)(b + 6) == 0xa5a0u);
}

static void yields_to_other_windows_and_scenarios(void)
{
    reset();
    frozen = 1;
    D_80167C48_168848[0] = other.bytes;
    anchor_freeze_prompt_update();
    assert(allocations == 0);
    D_80167C48_168848[0] = 0;
    anchor_freeze_prompt_update();
    assert(anchor_freeze_prompt_visible());
    func_8000C260_CE60(0, 0, 0, 0, 0, 0, 0);
    assert(frees == 1 && !anchor_freeze_prompt_visible());
    anchor_freeze_prompt_update();
    assert(allocations == 2);
    D_80167C48_168848[0] = 0;
    anchor_freeze_prompt_update();
    assert(anchor_freeze_prompt_visible());
    anchor_freeze_prompt_before_native_scenario(0, -1);
    assert(anchor_freeze_prompt_visible() && frees == 1);
    anchor_freeze_prompt_before_native_scenario((void *)1, 5);
    assert(!anchor_freeze_prompt_visible() && frees == 2);
    D_80077858_78458 = (void *)1;
    anchor_freeze_prompt_update();
    assert(allocations == 3);
}

static void never_frees_replaced_slot(void)
{
    reset();
    frozen = 1;
    anchor_freeze_prompt_update();
    D_80167C48_168848[PROMPT_SLOT] = other.bytes;
    anchor_freeze_prompt_yield();
    assert(frees == 0 && D_80167C48_168848[PROMPT_SLOT] == other.bytes);
    anchor_freeze_prompt_update();
    assert(allocations == 1);
    D_80167C48_168848[PROMPT_SLOT] = 0;
    anchor_freeze_prompt_update();
    assert(allocations == 2 && anchor_freeze_prompt_visible());
    D_80077860_78460 = (void *)3;
    frozen = 0;
    anchor_freeze_prompt_update();
    assert(frees == 1 && !anchor_freeze_prompt_visible());
    assert(!D_80167C48_168848[PROMPT_SLOT]);
}

int main(void)
{
    opens_nonmodal_centered_blinking_prompt();
    keeps_other_ab_glyphs_white();
    yields_to_other_windows_and_scenarios();
    never_frees_replaced_slot();
    puts("freeze prompt tests passed");
    return 0;
}
