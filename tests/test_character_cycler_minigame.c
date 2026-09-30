#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

unsigned char D_8015C608_15D208[256];
unsigned int D_8015C5D8_15D1D8[12];
static unsigned char s_task[0xe0];
void *D_801FC604_5B8514 = s_task;
static int s_required = -1;
static int s_swap_calls, s_skin_clears;

int anchor_minigame_invites_required_character(void) { return s_required; }
int anchor_dialog_world_paused(void) { return 0; }
void anchor_player_skin_clear_for_character_change(void) { ++s_skin_clears; }
void func_801DD5C0_5994D0(void *task, unsigned char character)
{
    assert(task == s_task);
    assert(character < 4);
    ++s_swap_calls;
}
int recomp_printf(const char *format, ...)
{
    (void)format;
    return 0;
}

void anchor_set_current_character_if_needed(void);
int func_801DD50C_59941C(void *task);

static int read_save_word(unsigned int offset)
{
    int result;
    memcpy(&result, D_8015C608_15D208 + offset, sizeof(result));
    return result;
}

static void write_save_word(unsigned int offset, int value)
{
    memcpy(D_8015C608_15D208 + offset, &value, sizeof(value));
}

int main(void)
{
    write_save_word(0x94, 1); /* Goemon is recruited; Sasuke is locked. */
    write_save_word(0x9c, 0);
    write_save_word(0x68, 3);
    D_8015C5D8_15D1D8[1] = 2;
    s_task[0x60] = 2;
    s_required = 2;
    anchor_set_current_character_if_needed();
    assert(D_8015C5D8_15D1D8[1] == 2);
    assert(read_save_word(0x68) == 3 && read_save_word(0x9c) == 0);
    assert(func_801DD50C_59941C(s_task) == 1);
    assert(s_task[0x60] == 2 && s_swap_calls == 0 && s_skin_clears == 0);

    s_required = -1;
    anchor_set_current_character_if_needed();
    assert(D_8015C5D8_15D1D8[1] == 0);
    assert(read_save_word(0x68) == 0 && read_save_word(0x9c) == 0);
    assert(s_skin_clears == 1);

    write_save_word(0x98, 1); /* Outside the visit, native cycling resumes. */
    s_task[0x60] = 0;
    assert(func_801DD50C_59941C(s_task) == 0);
    assert(s_task[0x60] == 1 && D_8015C5D8_15D1D8[1] == 1);
    assert(s_swap_calls == 1 && s_skin_clears == 2);
    puts("character cycler: locked minigame override and normal restoration passed");
    return 0;
}
