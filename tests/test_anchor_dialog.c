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
        } else if (c < 0x8000) {
            assert(c < 95);
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
static void reset_test(void)
{
    anchor_dialog_cancel();
    (void)anchor_dialog_poll();
    D_80077860_78460 = (void *)1;
    D_80077858_78458 = 0;
    D_80167C48_168848[0] = D_80167C48_168848[1] = D_80167C48_168848[2] = 0;
    D_800C7AE0 = 0;
    D_800C7AE2 = 0;
    memset(D_800C7DB0_C89B0, 0xab, sizeof(D_800C7DB0_C89B0));
    fail_alloc = frees = reset_calls = silent_flag = choice = closing = 0;
    visible_glyphs = rendered_length = 0;
    rendered[0] = 0;
}
static void to_choice(void)
{
    for (int i = 0; i < 10 && !choice; ++i)
        vm_tick(0, 0);
    assert(choice);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING);
}
static void check_result(int button, int selection, AnchorDialogResult expected)
{
    reset_test();
    silent_flag = 1;
    D_800C7AE0 = 4; /* A separate native flag is never overwritten. */
    assert(anchor_dialog_begin("Ahmad", "Congo's Arena"));
    assert(D_800C7AE0 == 4 && !silent_flag);
    assert(D_800C7AE2 == 0);
    for (int i = 0; i < 0x60; ++i)
        assert(D_800C7DB0_C89B0[i] == (i < 2 || i >= 0x18 ? 0xab : 0));
    assert(D_801C7900_1C8500 == 1);
    to_choice();
    assert(strstr(rendered, "Ahmad\nhas entered Congo's Arena.\nWould you like to join them?\n"));
    assert(strstr(rendered, "Yes") && strstr(rendered, "No"));
    vm_tick(button, selection);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING);
    vm_tick(0, 0); /* Choice callback + close command. */
    assert(closing == 2);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING && D_800C7AE0 == 4);
    vm_tick(0, 0);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_PENDING);
    vm_tick(0, 0);
    vm_tick(0, 0); /* Native END clears VM PC after the close animation. */
    assert(anchor_dialog_poll() == expected);
    assert(!anchor_dialog_busy() && !D_80167C48_168848[0]);
    assert(D_800C7AE0 == 4 && silent_flag == 1 && frees == 1);
    assert(D_800C7AE2 == 0);
    assert(anchor_dialog_poll() == ANCHOR_DIALOG_IDLE);
}
int main(void)
{
    check_result(1, 0, ANCHOR_DIALOG_YES);
    check_result(1, 1, ANCHOR_DIALOG_NO);
    check_result(2, 0, ANCHOR_DIALOG_NO);

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
    puts("native dialog VM contract, safe lifecycle, and glyph bounds passed");
    return 0;
}
