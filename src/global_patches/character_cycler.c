#include "modding.h"
#include "recomputils.h"

extern unsigned char D_8015C608_15D208[];
extern unsigned int D_8015C5D8_15D1D8[];
extern void func_801DD5C0_5994D0(void *arg0, unsigned char value);

#define SAVE_READ32(off) (*(signed int *)((char *)D_8015C608_15D208 + (off)))
#define SAVE_WRITE32(off, val) (*(signed int *)((char *)D_8015C608_15D208 + (off)) = (signed int)(val))

#define SAVE_CHARACTER_BASE 0x94
#define SAVE_SPAWN_CHARACTER 0x68
#define SAVE_HP_MAX_OFFSET (-0x28)

static int character_is_enabled(int character)
{
    signed int val;

    if (character < 0 || character >= 4)
        return 0;

    val = SAVE_READ32(SAVE_CHARACTER_BASE + character * 4);
    if (val > 1)
    {
        val = 1;
        SAVE_WRITE32(SAVE_CHARACTER_BASE + character * 4, val);
    }

    return val != 0;
}

int anchor_first_enabled_character(void)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (character_is_enabled(i))
            return i;
    }

    return -1;
}

void anchor_set_current_character_if_needed(void)
{
    int current = D_8015C5D8_15D1D8[1] & 0xff;
    int replacement;

    if (character_is_enabled(current))
        return;

    replacement = anchor_first_enabled_character();
    if (replacement < 0)
        return;

    SAVE_WRITE32(SAVE_SPAWN_CHARACTER, replacement);
    D_8015C5D8_15D1D8[1] = replacement & 0xff;
    D_8015C5D8_15D1D8[0x2C / 4] = 1;
    recomp_printf("[Character] Switched unavailable character %d to %d\n",
                  current, replacement);
}

RECOMP_PATCH int func_801DD50C_59941C(void *arg0)
{
    unsigned char current = *(unsigned char *)((char *)arg0 + 0x60);
    int step = 1;
    int selected = -1;

    do
    {
        int candidate = (current + step) & 3;
        if (character_is_enabled(candidate))
        {
            selected = candidate;
            break;
        }
        step++;
    } while (step != 4);

    if (selected >= 0)
    {
        *(unsigned char *)((char *)arg0 + 0x60) = (unsigned char)selected;
        D_8015C5D8_15D1D8[1] = selected & 0xff;
        D_8015C5D8_15D1D8[0x2C / 4] = 1;
        func_801DD5C0_5994D0(arg0, *(unsigned char *)((char *)arg0 + 0x60));
        return 0;
    }

    return 1;
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_character_frame_hook(void)
{
    if (SAVE_READ32(SAVE_HP_MAX_OFFSET) > 0)
        anchor_set_current_character_if_needed();
}
