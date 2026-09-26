#include "ui/anchor_freeze_prompt.h"
#include "combat/anchor_player_freeze.h"

#ifdef ANCHOR_FREEZE_PROMPT_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#else
#include "platform/modding.h"
#endif

extern void *D_80167C48_168848[3];
extern void *D_80167C54_168854;
extern void *D_80077858_78458;
extern void *D_80077860_78460;
extern void func_8000C260_CE60(int slot, int x, int y, int target_x,
                               int target_y, int style, int animation);
extern void func_8000C878_D478(int slot);
extern void func_8000C454_D054(int slot);
extern void func_8000C488_D088(int slot);
extern void func_8000C3EC_CFEC(int slot);
extern void func_8000D060_DC60(int slot, unsigned int glyph);
extern const unsigned char D_8005BB10_5C710[];

enum {
    PROMPT_SLOT = 2,
    PROMPT_X = 32,
    PROMPT_Y = 84,
    PROMPT_STYLE = 10,
    PROMPT_WIDTH = 256,
    TITLE_Y = 24,
    BODY_Y = 50,
    BUTTON_A_PALETTE = 6,
    BUTTON_B_PALETTE = 8
};

static void *s_window;
static void *s_manager;
static int s_adding;

int anchor_freeze_prompt_visible(void)
{
    return s_window && D_80167C48_168848[PROMPT_SLOT] == s_window;
}

void anchor_freeze_prompt_yield(void)
{
    if (anchor_freeze_prompt_visible())
        func_8000C878_D478(PROMPT_SLOT);
    s_window = 0;
    s_manager = 0;
}

static int windows_idle(void)
{
    return !D_80167C48_168848[0] && !D_80167C48_168848[1] &&
           !D_80167C48_168848[2];
}

static float line_width(const char *text, float scale)
{
    float width = 0.0f;
    while (*text)
        width += (D_8005BB10_5C710[(unsigned char)*text++ - 0x20u] + 1u) * scale;
    return width;
}

/* D060 chooses the next free 0x30-byte glyph record in a new window. Its
 * default cursor is left-aligned; set each record's verified window-relative
 * center and scales after insertion to center our two lines. The renderer
 * adds the window's screen position when it draws these records. */
static int append_line(const char *text, unsigned int *record_index,
                       float scale, short center_y, int tint_buttons)
{
    float start = (PROMPT_WIDTH - line_width(text, scale)) * 0.5f;
    float advance = 0.0f;
    unsigned char *window = s_window;
    while (*text)
    {
        unsigned int glyph = (unsigned char)*text++ - 0x20u;
        unsigned char *record;
        if (*record_index >= 120u ||
            D_80167C48_168848[PROMPT_SLOT] != window)
            return 0;
        func_8000D060_DC60(PROMPT_SLOT, glyph);
        if (D_80167C48_168848[PROMPT_SLOT] != window)
            return 0;
        record = window + 0x198u + *record_index * 0x30u;
        if (*(unsigned short *)(record + 4) != 1u)
            return 0;
        if (tint_buttons && (glyph == (unsigned int)('A' - 0x20) ||
                             glyph == (unsigned int)('B' - 0x20)))
        {
            unsigned short *flags = (unsigned short *)(record + 6);
            unsigned int palette = glyph == (unsigned int)('A' - 0x20) ?
                                   BUTTON_A_PALETTE : BUTTON_B_PALETTE;
            /* The low nibble selects the native CI palette. Keep all other
             * glyph flags, including visibility/effect bits, intact. */
            *flags = (unsigned short)((*flags & ~0x000Fu) |
                                      (palette + (glyph & 1u)));
        }
        *(short *)(record + 8) = (short)(start + advance + 4.0f * scale);
        *(short *)(record + 10) = center_y;
        *(float *)(record + 0x14) = scale;
        *(float *)(record + 0x18) = scale;
        advance += (D_8005BB10_5C710[glyph] + 1u) * scale;
        ++*record_index;
    }
    return 1;
}

/* The native renderer draws these glyph records without starting the scenario
 * interpreter. C454 marks subsequent glyphs with the native blink effect;
 * C488 returns to steady text for the escape instruction. */
static int fill_window(void)
{
    unsigned int record_index = 0;
    int title_ok, body_ok;
    func_8000C454_D054(PROMPT_SLOT);
    title_ok = append_line("FROZEN!!!", &record_index, 1.5f, TITLE_Y, 0);
    func_8000C488_D088(PROMPT_SLOT);
    func_8000C3EC_CFEC(PROMPT_SLOT);
    body_ok = append_line("Mash A + B to escape!", &record_index,
                          1.0f, BODY_Y, 1);
    return title_ok && body_ok;
}

RECOMP_HOOK("func_8000C260_CE60")
void anchor_freeze_prompt_before_native_window(int slot, int x, int y,
                                                int target_x, int target_y,
                                                int style, int animation)
{
    (void)slot; (void)x; (void)y; (void)target_x; (void)target_y;
    (void)style; (void)animation;
    if (!s_adding)
        anchor_freeze_prompt_yield();
}

RECOMP_HOOK("func_8003D468_3E068")
void anchor_freeze_prompt_before_native_scenario(void *script, int file_id)
{
    if (script || file_id != -1)
        anchor_freeze_prompt_yield();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_freeze_prompt_update(void)
{
    if (s_window && (!anchor_freeze_prompt_visible() ||
                     D_80077860_78460 != s_manager ||
                     D_80077858_78458 || !anchor_player_freeze_active()))
        anchor_freeze_prompt_yield();
    if (s_window || !anchor_player_freeze_active() ||
        !D_80167C54_168854 || !D_80077860_78460 ||
        D_80077858_78458 || !windows_idle())
        return;

    s_adding = 1;
    func_8000C260_CE60(PROMPT_SLOT, PROMPT_X, PROMPT_Y,
                        PROMPT_X, PROMPT_Y, PROMPT_STYLE, 0);
    s_adding = 0;
    if (!D_80167C48_168848[PROMPT_SLOT])
        return;
    s_window = D_80167C48_168848[PROMPT_SLOT];
    s_manager = D_80077860_78460;
    if (!fill_window())
        anchor_freeze_prompt_yield();
}
