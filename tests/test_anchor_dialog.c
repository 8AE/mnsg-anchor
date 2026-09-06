#define ANCHOR_DIALOG_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/anchor_dialog.c"

DialogWord *D_80077858_78458;
void *D_80077860_78460 = (void *)1;
void *D_80167C48_168848[3];
void *D_80167C54_168854 = (void *)2;
unsigned char D_800C7AE0;
unsigned char D_800C7AE2;
unsigned char D_800C7DB0_C89B0[0x60];
static unsigned short system_storage[0x3ae28 / 2];
unsigned char *D_8015C5C8_15D1C8 = (unsigned char *)system_storage;
void *D_8016DAB4_16E6B4;
void *D_801FC604_5B8514 = (void *)0x1234;
int D_801C7768_1C8368;
int D_801C7900_1C8500;
DialogWord D_801C7774_1C8374;
DialogWord D_801C7800_1C8400;
DialogWord D_801C7808_1C8408[50];
static unsigned char window_storage[0x1818];
static int fail_alloc, silent_flag, frees, reset_calls, choice, closing;
static unsigned int visible_glyphs;
static char rendered[256];
static unsigned int rendered_length;
static unsigned int rendered_line, rendered_line_width;
static int native_scenario_ticks, world_updates, interface_updates;
static int frame_button, frame_selection;

/* US resident width table at 8005BB10 (ROM 5C710), printable ASCII 20..7E.
 * Native D060 advances by width + 1. C6A8 sets the text inset to (10, 7)
 * and line spacing to 16 pixels; style 8 supplies a 256 by 72 window. */
static const unsigned char native_glyph_widths[95] = {
    8, 2, 6, 7, 2, 7, 2, 3, 6, 5, 6, 6, 3, 6, 2, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 4, 2, 6, 6, 8, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 2, 5, 7, 2,
    2, 6, 6, 6, 6, 6, 5, 6, 6, 4, 5, 6, 4, 8, 6, 6,
    6, 6, 6, 6, 5, 6, 6, 8, 6, 6, 6, 2, 2, 2, 2
};
static const char *const arena_labels[] = {
    "Congo's Arena", "Dharumanyo's Arena", "Tsurami's Arena",
    "Control Machine's Arena"
};

int func_800240DC_24CDC(unsigned short flag)
{
    assert(flag == 0x82);
    return silent_flag;
}
void func_80024038_24C38(unsigned short flag)
{
    assert(flag == 0x82);
    silent_flag = 1;
}
int func_8003D468_3E068(void *script, int file)
{
    assert(!script && file == -1);
    if (!D_80077860_78460)
        return 0;
    ++reset_calls;
    D_80077858_78458 = 0;
    D_801C7774_1C8374 = 0;
    silent_flag = 0;
    return 1;
}
void func_8000C260_CE60(int slot, int x, int y, int tx, int ty, int style, int animation)
{
    assert(slot == 0 && x == 24 && y == 24 && tx == 24 && ty == 24);
    assert(style == 8 && animation == 0);
    if (!fail_alloc)
        D_80167C48_168848[0] = window_storage;
}
void func_8000C878_D478(int slot)
{
    assert(slot == 0);
    assert(D_80167C48_168848[0] == window_storage);
    D_80167C48_168848[0] = 0;
    ++frees;
}

/* Execute the relevant original VM contract, independently following native
 * 8003D68C / 8003E6C0 / 8003E888 instruction-pointer behavior. Window closing
 * is asynchronous like the native renderer, so a Yes cannot leak out early. */
static void render_text(const unsigned short *text)
{
    assert(D_80167C48_168848[0]);
    while (*text != 0xffff) {
        unsigned int c = *text++;
        if (c == 0xffb9) {
            closing = 2;
        } else if (c == 0xffc4) {
            rendered[rendered_length++] = '\n';
            rendered_line_width = 0;
            ++rendered_line;
            assert(rendered_line < 4);
        } else if (c < 0x8000) {
            assert(c < 95);
            rendered_line_width += native_glyph_widths[c] + 1u;
            assert(rendered_line_width <= 236); /* Keep both 10-pixel insets. */
            rendered[rendered_length++] = (char)(c + 32);
            ++visible_glyphs;
            assert(visible_glyphs <= 120);
        }
    }
    rendered[rendered_length] = 0;
}
static void vm_tick(int button, int selection)
{
    if (closing) {
        if (--closing == 0)
            func_8000C878_D478(0);
        return;
    }
    if (choice) {
        if (button == 1) {
            D_80077858_78458 = (DialogWord *)D_801C7808_1C8408[selection * 5 + 4];
            choice = 0;
        } else if (button == 2) {
            D_80077858_78458 += 4;
            choice = 0;
        }
        return;
    }
    while (D_80077858_78458) {
        DialogWord *pc = D_80077858_78458++;
        switch (*pc) {
        case 0x8010:
            render_text((unsigned short *)*D_80077858_78458++);
            return;
        case 0x8011: {
            int count = 0;
            pc = D_80077858_78458;
            while (*pc != 0x8012) {
                render_text((unsigned short *)pc[1]);
                D_801C7808_1C8408[count * 5 + 4] = (DialogWord)(pc + 2);
                pc += 4;
                ++count;
                assert(count <= 10);
            }
            assert(count == 2);
            anchor_dialog_default_no();
            assert(D_801C7800_1C8400 == (DialogWord)&D_801C7808_1C8408[5]);
            D_80077858_78458 = pc;
            choice = 1;
            return;
        }
        case 0x800a:
            D_80077858_78458 = (DialogWord *)*D_80077858_78458;
            break;
        case 0x800b:
            ((void (*)(void))*D_80077858_78458++)();
            break;
        case 0x8008:
            func_8003D468_3E068(0, -1);
            return;
        default:
            assert(!"unexpected opcode: private dialog must not access saves");
        }
    }
}

void func_8003CFD0_3DBD0(void *task, void *object)
{
    assert(task == D_80077860_78460 && !object);
    assert(D_8016DAB4_16E6B4 == task);
    anchor_dialog_note_scenario_tick();
    ++native_scenario_ticks;
    vm_tick(frame_button, frame_selection);
}

/* Native 34734 walks a flattened task tree, skips flagged roots AND every
 * deeper descendant, and still dispatches unrelated eligible roots. The
 * scenario's observed creation category is 0x81 under the world tree. */
static void frame_tick(int button, int selection)
{
    static const struct { unsigned short depth, flags; int scenario; } tasks[] = {
        {0, 1, 0}, {1, 1, 0}, {1, 1, 0}, {1, 0x81, 1},
        {2, 0, 0}, {0, 2, 0}
    };
    int skipped_depth = -1;
    frame_button = button;
    frame_selection = selection;
    anchor_dialog_before_task_dispatch();
    unsigned short mask = *(unsigned short *)(D_8015C5C8_15D1C8 + SYS_TASK_MASK);
    for (unsigned int i = 0; i < sizeof(tasks) / sizeof(tasks[0]); ++i) {
        if (skipped_depth >= 0 && tasks[i].depth > skipped_depth)
            continue;
        skipped_depth = -1;
        if (tasks[i].flags & mask & 7u) {
            skipped_depth = tasks[i].depth;
            continue;
        }
        if (i == 5) {
            ++interface_updates;
        } else if (tasks[i].scenario) {
            D_8016DAB4_16E6B4 = D_80077860_78460;
            func_8003CFD0_3DBD0(D_80077860_78460, 0);
        } else {
            ++world_updates;
        }
    }
    D_8016DAB4_16E6B4 = 0;
    anchor_dialog_after_task_dispatch();
    assert(!D_8016DAB4_16E6B4);
}

static void reset_test(void)
{
    anchor_dialog_cancel();
    (void)anchor_dialog_poll();
    D_80077860_78460 = (void *)1;
    D_80077858_78458 = 0;
    D_80167C48_168848[0] = D_80167C48_168848[1] = D_80167C48_168848[2] = 0;
    D_800C7AE0 = 0;
    D_800C7AE2 = 0;
    D_8015C5C8_15D1C8 = (unsigned char *)system_storage;
    memset(system_storage, 0, sizeof(system_storage));
    D_8015C5C8_15D1C8[SYS_STEP] = 13;
    D_8015C5C8_15D1C8[SYS_WORLD_SUBSTATE] = 1;
    *(unsigned short *)(D_8015C5C8_15D1C8 + SYS_TASK_MASK) = 0x14;
    D_8016DAB4_16E6B4 = 0;
    native_scenario_ticks = world_updates = interface_updates = 0;
    memset(D_800C7DB0_C89B0, 0xab, sizeof(D_800C7DB0_C89B0));
    fail_alloc = frees = reset_calls = silent_flag = choice = closing = 0;
    visible_glyphs = rendered_length = 0;
    rendered_line = rendered_line_width = 0;
    rendered[0] = 0;
}
static void to_choice(void)
{
    for (int i = 0; i < 10 && !choice; ++i)
        frame_tick(0, 0);
    assert(choice);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING);
}
static void check_result(const char *arena, int button, int selection,
                         AnchorDialogResult expected)
{
    reset_test();
    silent_flag = 1;
    D_800C7AE0 = 4; /* A separate native flag is never overwritten. */
    assert(anchor_dialog_begin("Ahmad", arena));
    assert(anchor_dialog_world_paused());
    assert(D_800C7AE0 == 4 && !silent_flag);
    assert(D_800C7AE2 == 0);
    for (int i = 0; i < 0x60; ++i)
        assert(D_800C7DB0_C89B0[i] == (i < 2 || i >= 0x18 ? 0xab : 0));
    assert(D_801C7900_1C8500 == 1);
    to_choice();
    char expected_prompt[160];
    snprintf(expected_prompt, sizeof(expected_prompt),
             "Ahmad\nentered %s.\nWould you like to join them?\n", arena);
    assert(strstr(rendered, expected_prompt));
    assert(strstr(rendered, "Yes") && strstr(rendered, "No"));
    frame_tick(button, selection);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING);
    frame_tick(0, 0); /* Choice callback + close command. */
    assert(closing == 2);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING && D_800C7AE0 == 4);
    frame_tick(0, 0);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING);
    frame_tick(0, 0);
    frame_tick(0, 0); /* Native END clears VM PC after the close animation. */
    assert(anchor_dialog_world_paused()); /* Holds through the closing frame. */
    assert(world_updates == 0 && interface_updates == native_scenario_ticks);
    assert(*(unsigned short *)(D_8015C5C8_15D1C8 + SYS_TASK_MASK) == 0x14);
    assert(anchor_dialog_poll() == expected);
    assert(!anchor_dialog_busy() && !D_80167C48_168848[0]);
    assert(!anchor_dialog_world_paused());
    assert(D_800C7AE0 == 4 && silent_flag == 1 && frees == 1);
    assert(D_800C7AE2 == 0);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_IDLE);
    frame_tick(0, 0);
    assert(world_updates == 4); /* World resumes on either choice, including B. */
}
int main(void)
{
    for (unsigned int i = 0; i < sizeof(arena_labels) / sizeof(arena_labels[0]); ++i) {
        const char *arena = arena_labels[i];
        check_result(arena, 1, 0, ANCHOR_DIALOG_YES);
        check_result(arena, 1, 1, ANCHOR_DIALOG_NO);
        check_result(arena, 2, 0, ANCHOR_DIALOG_NO);

        reset_test();
        /* A widest-glyph player name stays bounded while every arena label
         * and the full question/choices remain visible without wrapping. */
        assert(anchor_dialog_begin("wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww", arena));
        to_choice();
        assert(strstr(rendered, "wwwwwwwwwwwwwwwwwwwwwwww\n"));
        assert(strstr(rendered, arena));
        assert(strstr(rendered, ".\nWould you like to join them?\n"));
        assert(rendered_line == 3 && visible_glyphs <= 120);
        anchor_dialog_cancel();
        assert(anchor_dialog_poll() == ANCHOR_DIALOG_CANCELLED);
        assert(!anchor_dialog_world_paused());
    }

    reset_test();
    assert(anchor_dialog_begin("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "Congo's Arena"));
    to_choice();
    assert(strstr(rendered, "AAAAAAAAAAAAAAAAAAAAAAAA\n"));
    anchor_dialog_cancel();
    assert(!D_80077858_78458 && !D_80167C48_168848[0] && D_800C7AE0 == 0);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_CANCELLED);

    reset_test();
    assert(anchor_dialog_begin("Jos\xc3\xa9\xf0\x9f\x99\x82\n\x1b", "Congo's Arena"));
    to_choice();
    assert(strstr(rendered, "Jos????\n"));

    reset_test();
    silent_flag = fail_alloc = 1;
    assert(!anchor_dialog_begin("A", "Congo's Arena"));
    assert(!anchor_dialog_busy() && !D_80077858_78458 && D_800C7AE0 == 0 && silent_flag);

    reset_test();
    DialogWord native_program[] = {0x8008};
    D_80077858_78458 = native_program;
    assert(!anchor_dialog_begin("A", "Congo's Arena") && !reset_calls);
    D_80077858_78458 = 0;
    D_80167C48_168848[1] = (void *)3;
    assert(!anchor_dialog_begin("A", "Congo's Arena") && !reset_calls);
    D_80167C48_168848[1] = 0;
    D_80077860_78460 = 0;
    assert(!anchor_dialog_begin("A", "Congo's Arena") && !reset_calls);

    reset_test();
    assert(anchor_dialog_begin("A", "Congo's Arena"));
    D_80077858_78458 = native_program;
    D_80167C48_168848[0] = (void *)4;
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_CANCELLED);
    assert(D_80077858_78458 == native_program && D_80167C48_168848[0] == (void *)4 && !frees);
    assert(D_800C7AE0 == 0 && D_800C7AE2 == 0);

    reset_test();
    assert(anchor_dialog_begin("A", "Congo's Arena"));
    anchor_dialog_before_native_scenario((void *)0x08000000, 123);
    assert(!anchor_dialog_busy() && D_800C7AE0 == 0 && frees == 1);
    D_800C7AE0 = 2; /* The replacement acquires its own lock after our hook. */
    D_800C7AE2 = 1;
    D_80077858_78458 = native_program;
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_CANCELLED);
    assert(D_800C7AE0 == 2 && D_80077858_78458 == native_program);
    assert(D_800C7AE2 == 1);

    reset_test();
    assert(anchor_dialog_begin("A", "Congo's Arena"));
    D_800C7AE0 = 4;
    anchor_dialog_before_player_update(D_801FC604_5B8514);
    assert(D_800C7AE0 == 6);
    anchor_dialog_after_player_update();
    assert(D_800C7AE0 == 4);
    anchor_dialog_before_player_movement(D_801FC604_5B8514);
    assert(D_800C7AE0 == 6);
    anchor_dialog_after_player_movement();
    assert(D_800C7AE0 == 4);
    anchor_dialog_before_player_update((void *)0x5678);
    assert(D_800C7AE0 == 4); /* No other task is affected. */
    anchor_dialog_after_player_update();
    D_800C7AE0 = 2;
    anchor_dialog_before_player_update(D_801FC604_5B8514);
    anchor_dialog_after_player_update();
    assert(D_800C7AE0 == 2); /* Never clear an existing native inhibit bit. */
    memset(D_800C7DB0_C89B0, 0xcd, sizeof(D_800C7DB0_C89B0));
    anchor_dialog_filter_gameplay_input();
    for (int i = 0; i < 0x60; ++i)
        assert(D_800C7DB0_C89B0[i] == (i < 2 || i >= 0x18 ? 0xcd : 0));
    /* NPC/sign callers can establish their lock before starting a script. */
    D_800C7AE0 = 2;
    D_800C7AE2 = 1;
    anchor_dialog_before_native_scenario((void *)0x08000000, 123);
    assert(D_800C7AE0 == 2 && D_800C7AE2 == 1);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_CANCELLED);
    assert(D_800C7AE0 == 2 && D_800C7AE2 == 1);

    reset_test();
    assert(anchor_dialog_begin("A", "Congo's Arena"));
    unsigned short *task_mask = (unsigned short *)(D_8015C5C8_15D1C8 + SYS_TASK_MASK);
    *task_mask = 0x115; /* Preserve an already-held pause and high native bits. */
    frame_tick(0, 0);
    assert(*task_mask == 0x115 && !world_updates && native_scenario_ticks == 1);
    *task_mask = 0x14;
    anchor_dialog_before_task_dispatch();
    assert(*task_mask == 0x15);
    *task_mask |= 0x80; /* A separate native mask change is retained. */
    D_8016DAB4_16E6B4 = (void *)0x9876;
    anchor_dialog_after_task_dispatch();
    assert(*task_mask == 0x94 && D_8016DAB4_16E6B4 == (void *)0x9876);

    /* If another mod lets the manager run normally, never advance it twice. */
    int ticks = native_scenario_ticks;
    anchor_dialog_before_task_dispatch();
    D_8016DAB4_16E6B4 = D_80077860_78460;
    func_8003CFD0_3DBD0(D_80077860_78460, 0);
    D_8016DAB4_16E6B4 = (void *)0x9876;
    anchor_dialog_after_task_dispatch();
    assert(native_scenario_ticks == ticks + 1 && D_8016DAB4_16E6B4 == (void *)0x9876);

    /* Ending a modal mid-dispatch releases only the scoped bit, then does
     * not manually tick a cancelled/replacement scenario. */
    ticks = native_scenario_ticks;
    anchor_dialog_before_task_dispatch();
    anchor_dialog_cancel();
    assert(!anchor_dialog_world_paused());
    anchor_dialog_after_task_dispatch();
    assert(*task_mask == 0x94 && native_scenario_ticks == ticks);

    reset_test();
    assert(anchor_dialog_begin("A", "Congo's Arena"));
    D_8015C5C8_15D1C8[SYS_STEP] = 12; /* A loader takeover must stay runnable. */
    assert(!anchor_dialog_world_paused());
    anchor_dialog_before_task_dispatch();
    anchor_dialog_after_task_dispatch();
    assert(*task_mask == 0x14 && !native_scenario_ticks);
    D_8015C5C8_15D1C8[SYS_STEP] = 13;
    D_80077858_78458 = native_program;
    assert(!anchor_dialog_world_paused());
    anchor_dialog_before_task_dispatch();
    anchor_dialog_after_task_dispatch();
    assert(*task_mask == 0x14 && !native_scenario_ticks);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_CANCELLED);
    puts("native dialog choices, scoped world pause/resume, ownership, and glyph bounds passed");
    return 0;
}
