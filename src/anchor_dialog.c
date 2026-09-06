#include "anchor_dialog.h"

#ifdef ANCHOR_DIALOG_HOST_TEST
#include <stdint.h>
#define RECOMP_HOOK_RETURN(name)
#define RECOMP_HOOK(name)
typedef uintptr_t DialogWord;
#else
#include "modding.h"
typedef unsigned int DialogWord;
#endif

/* Native scenario VM, shared by the save sign and other ordinary dialogs.
 * The VM consumes 32-bit instructions and 16-bit glyph/control streams.
 * A private script uses only text, branch, callback, choice, and end opcodes;
 * it never invokes the save sign's save callbacks or save-flag instructions. */
extern DialogWord *D_80077858_78458;
extern void *D_80077860_78460;
extern void *D_80167C48_168848[3];
extern void *D_80167C54_168854;
extern unsigned char D_800C7AE0;
extern unsigned char D_800C7AE2;
extern unsigned char D_800C7DB0_C89B0[0x60];
extern unsigned char *D_8015C5C8_15D1C8;
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC604_5B8514;
extern int D_801C7768_1C8368;
extern int D_801C7900_1C8500;
extern DialogWord D_801C7774_1C8374;
extern DialogWord D_801C7800_1C8400;
extern DialogWord D_801C7808_1C8408[];
extern int func_8003D468_3E068(void *script, int file_id);
extern void func_8003CFD0_3DBD0(void *task, void *object);
extern void func_8000C260_CE60(int slot, int x, int y, int target_x,
                             int target_y, int style, int animation);
extern void func_8000C878_D478(int slot);
extern int func_800240DC_24CDC(unsigned short flag);
extern void func_80024038_24C38(unsigned short flag);

enum {
    VM_JUMP = 0x800a, VM_CALLBACK = 0x800b, VM_END = 0x8008,
    VM_TEXT = 0x8010, VM_CHOICE = 0x8011, VM_CHOICE_END = 0x8012,
    TEXT_OPEN = 0xffba, TEXT_CLOSE = 0xffb9, TEXT_NEWLINE = 0xffc4,
    TEXT_YELLOW = 0xffdf, TEXT_WHITE = 0xffe0, TEXT_END = 0xffff,
    SCRIPT_WORDS = 40, TEXT_CAPACITY = 128, MODAL_CONTROL_BIT = 2,
    WORLD_TASK_PAUSE = 1, SYS_STEP = 0x3add4,
    SYS_WORLD_SUBSTATE = 0x3adde, SYS_TASK_MASK = 0x3ae24
};

static DialogWord s_script[SCRIPT_WORDS];
static unsigned short s_prompt[TEXT_CAPACITY];
static const unsigned short s_open[] = {TEXT_OPEN, TEXT_END};
static const unsigned short s_close[] = {TEXT_CLOSE, TEXT_END};
/* USA glyphs are ASCII minus 0x20. The spacing/palette match native choices. */
static const unsigned short s_yes[] = {
    0, 0, TEXT_YELLOW, 'Y' - 32, 'e' - 32, 's' - 32,
    0, 0, 0, 0, 0, 0, TEXT_END
};
static const unsigned short s_no[] = {
    0, 0, TEXT_YELLOW, 'N' - 32, 'o' - 32, TEXT_END
};
static void *s_manager;
static void *s_window;
static int s_active;
static int s_pre_control_bit;
static int s_late_control_bit;
static int s_saved_silent_flag;
static unsigned char *s_scheduler_system;
static int s_scenario_ticked;
static AnchorDialogResult s_result;

static int owns_script(void)
{
    DialogWord address = (DialogWord)D_80077858_78458;
    DialogWord begin = (DialogWord)s_script;
    return address >= begin && address < begin + sizeof(s_script) &&
           (address - begin) % sizeof(s_script[0]) == 0;
}

static void selected_yes(void) { s_result = ANCHOR_DIALOG_YES; }
static void selected_no(void) { s_result = ANCHOR_DIALOG_NO; }

static void append_ascii(unsigned short **out, const char *text)
{
    while (*text)
        *(*out)++ = (unsigned char)*text++ - 32u;
}

/* The original US font contains printable ASCII, not Unicode. Consume one
 * unsupported UTF-8 character per '?' and never let names inject controls.
 * Names occupy their own line, bounded to 24 glyphs (at most 216 pixels). */
static void append_name(unsigned short **out, const char *text, int limit)
{
    int count = 0;
    if (!text || !*text)
        text = "Player";
    while (*text && count < limit) {
        unsigned char c = (unsigned char)*text++;
        if (c >= 0x20 && c <= 0x7e) {
            *(*out)++ = c - 32u;
        } else {
            *(*out)++ = '?' - 32;
            if (c >= 0xc0) {
                int remaining = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
                while (remaining-- && ((unsigned char)*text & 0xc0u) == 0x80u)
                    ++text;
            }
        }
        ++count;
    }
}

static void build_prompt(const char *name, const char *arena)
{
    unsigned short *out = s_prompt;
    *out++ = TEXT_WHITE;
    append_name(&out, name, 24);
    *out++ = TEXT_NEWLINE;
    /* Keep the catalog's complete arena labels on this line. The longest,
     * "entered Control Machine's Arena.", advances 220 pixels in the US
     * font; style 8 has a 10-pixel inset inside its 256-pixel window. */
    append_ascii(&out, "entered ");
    append_name(&out, arena, 24);
    append_ascii(&out, ".");
    *out++ = TEXT_NEWLINE;
    append_ascii(&out, "Would you like to join them?");
    *out++ = TEXT_NEWLINE;
    *out = TEXT_END;
}

static void build_script(void)
{
    /* Native 0x8011 parses four-word records until 0x8012. Selecting a
     * record resumes at its last two words. B resumes four words after
     * 0x8012, so that continuation explicitly follows the No branch. */
    s_script[0] = VM_TEXT; s_script[1] = (DialogWord)s_open;
    s_script[2] = VM_TEXT; s_script[3] = (DialogWord)s_prompt;
    s_script[4] = VM_CHOICE;
    s_script[5] = VM_TEXT; s_script[6] = (DialogWord)s_yes;
    s_script[7] = VM_JUMP; s_script[8] = (DialogWord)&s_script[24];
    s_script[9] = VM_TEXT; s_script[10] = (DialogWord)s_no;
    s_script[11] = VM_JUMP; s_script[12] = (DialogWord)&s_script[30];
    s_script[13] = VM_CHOICE_END;
    s_script[14] = VM_END; s_script[15] = VM_END; s_script[16] = VM_END;
    s_script[17] = VM_JUMP; s_script[18] = (DialogWord)&s_script[30];
    s_script[24] = VM_CALLBACK; s_script[25] = (DialogWord)selected_yes;
    s_script[26] = VM_TEXT; s_script[27] = (DialogWord)s_close;
    s_script[28] = VM_END;
    s_script[30] = VM_CALLBACK; s_script[31] = (DialogWord)selected_no;
    s_script[32] = VM_TEXT; s_script[33] = (DialogWord)s_close;
    s_script[34] = VM_END;
}

static void finish_owned_dialog(void)
{
    if (s_saved_silent_flag)
        func_80024038_24C38(0x82);
    s_saved_silent_flag = 0;
    s_active = 0;
    s_manager = 0;
    s_window = 0;
}

static void abandon_replaced_dialog(void)
{
    /* The old scene no longer owns native state. Do not restore its saved
     * transient flag after a replacement or scene reinitialization. */
    s_saved_silent_flag = 0;
    s_active = 0;
    s_manager = 0;
    s_window = 0;
    s_result = ANCHOR_DIALOG_CANCELLED;
}

static void clear_gameplay_input(void)
{
    /* Raw controller one at system+3B078 remains available to the scenario.
     * Clear only the derived first-player input at +3B0F0; preserve the
     * connected-status halfword and every other controller record. */
    for (int offset = 2; offset < 0x18; ++offset)
        ((volatile unsigned char *)D_800C7DB0_C89B0)[offset] = 0;
}

int anchor_dialog_begin(const char *name, const char *arena)
{
    if (s_active || D_80077858_78458 || !D_80077860_78460 || !D_801FC604_5B8514 ||
        !D_80167C54_168854 || (D_800C7AE0 & 3u) || D_800C7AE2 ||
        D_80167C48_168848[0] || D_80167C48_168848[1] || D_80167C48_168848[2])
        return 0;

    build_prompt(name, arena);
    build_script();
    s_saved_silent_flag = func_800240DC_24CDC(0x82) != 0;
    if (!func_8003D468_3E068(0, -1)) {
        s_saved_silent_flag = 0;
        return 0;
    }
    /* Allocate before publishing a VM script: native choice parsing assumes
     * the window exists and otherwise dereferences NULL on heap exhaustion.
     * Style 8 and the origin below are the save sign's native configuration. */
    func_8000C260_CE60(0, 24, 24, 24, 24, 8, 0);
    if (!D_80167C48_168848[0]) {
        finish_owned_dialog();
        return 0;
    }
    D_801C7768_1C8368 = 8;
    D_801C7900_1C8500 = 1; /* Controller one; no second-controller input. */
    D_801C7774_1C8374 = (DialogWord)&s_script[30];
    s_manager = D_80077860_78460;
    s_window = D_80167C48_168848[0];
    s_result = ANCHOR_DIALOG_PENDING;
    s_active = 1;
    clear_gameplay_input();
    D_80077858_78458 = s_script;
    return 1;
}

AnchorDialogResult anchor_dialog_poll(void)
{
    AnchorDialogResult result;
    if (s_active) {
        if (D_80077860_78460 != s_manager ||
            (D_80077858_78458 && !owns_script())) {
            /* A native room/scenario transition took ownership. Never stop
             * the replacement scenario or free its window. */
            abandon_replaced_dialog();
        } else if (D_80077858_78458) {
            return ANCHOR_DIALOG_PENDING;
        } else {
            if (D_80167C48_168848[0] == s_window)
                func_8000C878_D478(0);
            if (s_result == ANCHOR_DIALOG_PENDING)
                s_result = ANCHOR_DIALOG_NO;
            finish_owned_dialog();
        }
    }
    result = s_result;
    s_result = ANCHOR_DIALOG_IDLE;
    return result;
}

void anchor_dialog_cancel(void)
{
    if (!s_active)
        return;
    if (D_80077860_78460 != s_manager ||
        (D_80077858_78458 && !owns_script())) {
        abandon_replaced_dialog();
    } else {
        if (D_80167C48_168848[0] == s_window)
            func_8000C878_D478(0);
        func_8003D468_3E068(0, -1);
        s_result = ANCHOR_DIALOG_CANCELLED;
        finish_owned_dialog();
    }
}

int anchor_dialog_busy(void) { return s_active; }

int anchor_dialog_world_paused(void)
{
    return s_active && D_8015C5C8_15D1C8 &&
           D_8015C5C8_15D1C8[SYS_STEP] == 13 &&
           D_8015C5C8_15D1C8[SYS_WORLD_SUBSTATE] == 1 &&
           D_80077860_78460 == s_manager &&
           (!D_80077858_78458 || owns_script());
}

/* Start pauses world task subtrees with mask bit 1, including the ordinary
 * scenario manager. Hold the bit only during task dispatch, then tick only
 * our private scenario below. The native window/cursor draw runs separately.
 * Never enter the Start menu or retain a global pause lock across frames. */
RECOMP_HOOK("func_80034734_35334")
void anchor_dialog_before_task_dispatch(void)
{
    volatile unsigned short *mask;
    s_scheduler_system = 0;
    s_scenario_ticked = 0;
    if (!anchor_dialog_world_paused())
        return;
    mask = (volatile unsigned short *)(D_8015C5C8_15D1C8 + SYS_TASK_MASK);
    if (!(*mask & WORLD_TASK_PAUSE)) {
        s_scheduler_system = D_8015C5C8_15D1C8;
        *mask |= WORLD_TASK_PAUSE;
    }
}

RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_dialog_after_task_dispatch(void)
{
    if (s_scheduler_system && s_scheduler_system == D_8015C5C8_15D1C8)
        *(volatile unsigned short *)(s_scheduler_system + SYS_TASK_MASK) &=
            (unsigned short)~WORLD_TASK_PAUSE;
    s_scheduler_system = 0;
    if (anchor_dialog_world_paused() && !s_scenario_ticked) {
        void *previous_task = D_8016DAB4_16E6B4;
        /* The private script only uses text/choice/branch/completion commands.
         * Supply the manager's native task context, without dispatching any
         * world task or replacing another scenario's execution. */
        D_8016DAB4_16E6B4 = s_manager;
        func_8003CFD0_3DBD0(s_manager, 0);
        D_8016DAB4_16E6B4 = previous_task;
    }
}

RECOMP_HOOK("func_8003CFD0_3DBD0")
void anchor_dialog_note_scenario_tick(void)
{
    /* Normally paused with the world. Avoid a second tick if another mod
     * changes the manager's task category and it already ran this dispatch. */
    s_scenario_ticked = 1;
}

RECOMP_HOOK("func_8003D468_3E068")
void anchor_dialog_before_native_scenario(void *script, int file_id)
{
    (void)script;
    /* Stop owning the old window before the native initializer replaces the
     * VM. No control flags are held across calls: incoming scenarios can
     * establish AE20/AE22 before this hook without us undoing their state. */
    if (s_active && file_id != -1) {
        if (D_80077860_78460 != s_manager ||
            (D_80077858_78458 && !owns_script())) {
            abandon_replaced_dialog();
            return;
        }
        if (D_80167C48_168848[0] == s_window)
            func_8000C878_D478(0);
        s_result = ANCHOR_DIALOG_CANCELLED;
        finish_owned_dialog();
    }
}

RECOMP_HOOK_RETURN("func_80004AF8_56F8")
void anchor_dialog_filter_gameplay_input(void)
{
    if (s_active && D_80077860_78460 == s_manager && owns_script())
        clear_gameplay_input();
}

static int freeze_local_update(void *player)
{
    if (s_active && player && player == D_801FC604_5B8514 &&
        D_80077860_78460 == s_manager && owns_script() &&
        !(D_800C7AE0 & MODAL_CONTROL_BIT)) {
        D_800C7AE0 |= MODAL_CONTROL_BIT;
        return 1;
    }
    return 0;
}

/* Native CB824 checks the low control bits before damage/action intake;
 * CBAF8 checks them before late movement/collision. Add the inhibit bit only
 * for these calls on the local player, then immediately relinquish it. */
RECOMP_HOOK("func_801CB824_587734")
void anchor_dialog_before_player_update(void *player)
{
    s_pre_control_bit = freeze_local_update(player);
}

RECOMP_HOOK_RETURN("func_801CB824_587734")
void anchor_dialog_after_player_update(void)
{
    if (s_pre_control_bit)
        D_800C7AE0 &= (unsigned char)~MODAL_CONTROL_BIT;
    s_pre_control_bit = 0;
}

RECOMP_HOOK("func_801CBAF8_587A08")
void anchor_dialog_before_player_movement(void *player)
{
    s_late_control_bit = freeze_local_update(player);
}

RECOMP_HOOK_RETURN("func_801CBAF8_587A08")
void anchor_dialog_after_player_movement(void)
{
    if (s_late_control_bit)
        D_800C7AE0 &= (unsigned char)~MODAL_CONTROL_BIT;
    s_late_control_bit = 0;
}

RECOMP_HOOK_RETURN("func_8003E6C0_3F2C0")
void anchor_dialog_default_no(void)
{
    /* The native parser creates Yes first and No second. Keep the familiar
     * visual order while starting on No, so a held confirm cannot teleport. */
    if (s_active && owns_script())
        D_801C7800_1C8400 = (DialogWord)&D_801C7808_1C8408[5];
}
