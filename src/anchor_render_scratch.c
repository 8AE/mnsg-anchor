#include "anchor_render_scratch.h"
#include "anchor_dialog.h"

#ifndef ANCHOR_RENDER_SCRATCH_HOST_TEST
#include "modding.h"
#include "recomputils.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
extern void recomp_printf(const char *format, ...);
#endif

typedef struct RenderSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} RenderSceneResource;

extern RenderSceneResource D_80167FC0_168BC0[48];
extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned int *D_8015C5CC_15D1CC;
extern unsigned char *D_80168504_169104;
extern short D_800C7A72_C8672;
extern int D_801684F8_1690F8;
extern int D_801684FC_1690FC;
extern int anchor_player_models_is_remote_object(const void *object);
extern const void *anchor_player_models_resolve_render_address(
    const void *object, unsigned int address, unsigned int bytes);

#define NATIVE_GRAPHICS_BANK_BYTES 0x1d6d8u
#define NATIVE_GRAPHICS_COMMAND_BYTES (10640u * 8u)
/* Remote draws use a single stock call per contiguous group; leave the
 * stock command tail for the engine's later frame-finalization/HUD work. */
#define NATIVE_GRAPHICS_TAIL_BYTES (512u * 8u)
/* 8000B8F0 emits 11 commands for each of at most 120 visible glyphs.
 * The style-8 window adds nine 16-command frame strips in 8000C904, setup,
 * clipping and a choice cursor. 2048 commands cover that complete window
 * in addition to the ordinary HUD/finalization allowance, including its
 * opening and closing animation. */
#define NATIVE_DIALOG_TAIL_BYTES (2048u * 8u)

static unsigned char *s_arena;
static unsigned int s_bank_bytes;
static unsigned int s_used[2];
static int s_bank = -1;
static unsigned int s_reported;
static unsigned char *s_object;
static unsigned char s_hidden;
static int s_drawing;
static unsigned int *s_native_head;
static unsigned char *s_native_matrix;
static unsigned char *s_matrix_end;
static unsigned int *s_draw_head;
static unsigned int *s_draw_end;
static unsigned int *s_chain_end;
static unsigned int *s_chain_resume;

static void report(unsigned int reason)
{
    if (!(s_reported & reason))
    {
        s_reported |= reason;
        recomp_printf("[remote_render] draw skipped reason=%u bank_bytes=%u; player remains active\n",
                      reason, s_bank_bytes);
    }
}

static const void *resolve(unsigned int address, unsigned int bytes,
                            const void *object)
{
    return anchor_player_models_resolve_render_address(object, address, bytes);
}

void anchor_render_scratch_load_resources(void)
{
    unsigned int start, end, bank;
    int i;
    s_arena = 0;
    s_bank_bytes = 0;
    s_bank = -1;
    s_object = 0;
    s_drawing = 0;
    s_chain_end = s_chain_resume = 0;
    s_used[0] = s_used[1] = 0;
    s_reported = 0;
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !anchor_render_scratch_plan(
            (unsigned int)(unsigned long)D_80167FC0_168BC0[i].data,
            &start, &bank, &end))
    {
        report(1u);
        return;
    }
    D_80167FC0_168BC0[i].data = (unsigned char *)(unsigned long)end;
    s_arena = (unsigned char *)(unsigned long)start;
    s_bank_bytes = bank;
    recomp_printf("[remote_render] scratch %x..%x; %u bytes per native graphics bank\n",
                  start, end, bank);
}

/* This native initializer runs when its selected graphics bank is reusable.
 * Submission toggles that bank; frame-counter parity is not the ownership
 * signal when graphics submission is skipped. */
RECOMP_HOOK_RETURN("func_80016950_17550")
void anchor_render_scratch_begin_bank(void)
{
    int bank = D_800C7A72_C8672;
    s_bank = bank >= 0 && bank < 2 ? bank : -1;
    if (s_bank >= 0)
        s_used[s_bank] = 0;
    s_chain_end = s_chain_resume = 0;
}

RECOMP_HOOK("func_80016C44_17844")
void anchor_render_scratch_begin_object(void *pointer)
{
    AnchorRenderBudget available, budget;
    unsigned char *bank_start;
    unsigned char *native_start;
    unsigned char *native_limit;
    unsigned int left, matrix_bytes, command_bytes, tail_bytes;
    unsigned char *object = pointer;

    /* Case-6 native drawing recurses through 18CA0, never through 16C44. */
    s_object = 0;
    s_drawing = 0;
    if (!anchor_player_models_is_remote_object(object) ||
        (object[0x64] & 1u) || (signed char)object[0x65] < 0 ||
        !*(unsigned int *)(object + 0x2c))
        return;
    s_object = object;
    s_hidden = object[0x64] & 1u;
    object[0x64] |= 1u;
    if (!s_arena || s_bank < 0 || s_bank != D_800C7A72_C8672)
    {
        report(1u);
        return;
    }
    if (D_801684F8_1690F8 < -1 || D_801684F8_1690F8 > 32 ||
        D_801684FC_1690FC < 0 || D_801684FC_1690FC > 32)
    {
        report(2u);
        return;
    }
    native_start = D_8015C5C8_15D1C8 + s_bank * NATIVE_GRAPHICS_BANK_BYTES;
    tail_bytes = NATIVE_GRAPHICS_TAIL_BYTES;
    if (anchor_dialog_busy())
        tail_bytes += NATIVE_DIALOG_TAIL_BYTES;
    native_limit = native_start + NATIVE_GRAPHICS_COMMAND_BYTES - tail_bytes;
    if ((unsigned char *)D_8015C5CC_15D1CC < native_start ||
        (unsigned char *)D_8015C5CC_15D1CC + 8 > native_limit)
    {
        report(4u);
        return;
    }
    left = s_bank_bytes - s_used[s_bank];
    available.commands = left / 8u;
    available.matrices = left / 64u;
    if (!anchor_render_player_budget(object, resolve, object, &available, &budget))
    {
        report(8u);
        return;
    }
    matrix_bytes = budget.matrices * 64u;
    command_bytes = budget.commands * 8u;
    if (matrix_bytes > left || command_bytes > left - matrix_bytes)
    {
        report(16u);
        return;
    }
    bank_start = s_arena + s_bank * s_bank_bytes + s_used[s_bank];
    s_native_head = D_8015C5CC_15D1CC;
    s_native_matrix = D_80168504_169104;
    s_matrix_end = bank_start + matrix_bytes;
    s_draw_head = (unsigned int *)s_matrix_end;
    s_draw_end = s_draw_head + command_bytes / 4u;
    D_80168504_169104 = bank_start;
    D_8015C5CC_15D1CC = s_draw_head;
    object[0x64] = (unsigned char)((object[0x64] & 0xfeu) | s_hidden);
    s_drawing = 1;
}

RECOMP_HOOK_RETURN("func_80016C44_17844")
void anchor_render_scratch_end_object(void)
{
    unsigned int *end;
    unsigned char *base;
    if (!s_object)
        return;
    s_object[0x64] = (unsigned char)((s_object[0x64] & 0xfeu) | s_hidden);
    s_object = 0;
    if (!s_drawing)
        return;
    s_drawing = 0;
    end = D_8015C5CC_15D1CC;
    D_8015C5CC_15D1CC = s_native_head;
    if (end < s_draw_head || end + 2 > s_draw_end ||
        D_80168504_169104 > s_matrix_end)
    {
        /* Preflight must prevent this. Do not submit a malformed partial
         * list if another patch violates the verified writer contract. */
        D_80168504_169104 = s_native_matrix;
        s_used[s_bank] = s_bank_bytes;
        report(32u);
        return;
    }
    D_80168504_169104 = s_native_matrix;
    end[0] = 0xb8000000u; /* G_ENDDL */
    end[1] = 0;
    if (s_chain_end && s_native_head == s_chain_resume)
    {
        /* Consecutive remote draws share one stock G_DL call. Each new
         * private list gets a branch from the previous end, preserving draw
         * order without consuming a stock command for every peer. */
        s_chain_end[0] = 0x06010000u; /* G_DL, no push */
        s_chain_end[1] = (unsigned int)(unsigned long)s_draw_head & 0x1fffffffu;
    }
    else
    {
        s_native_head[0] = 0x06000000u; /* G_DL, push */
        s_native_head[1] = (unsigned int)(unsigned long)s_draw_head & 0x1fffffffu;
        D_8015C5CC_15D1CC += 2;
    }
    s_chain_end = end;
    s_chain_resume = D_8015C5CC_15D1CC;
    base = s_arena + s_bank * s_bank_bytes;
    /* Keep every submitted matrix/list until this native bank is reusable;
     * reclaim only the unused command allowance after the written end. */
    s_used[s_bank] = ((unsigned int)((unsigned char *)(end + 2) - base) + 15u) & ~15u;
}

#ifdef ANCHOR_RENDER_SCRATCH_HOST_TEST
void anchor_render_scratch_test_bind(void *arena, unsigned int bank_bytes)
{
    s_arena = arena;
    s_bank_bytes = bank_bytes;
    s_used[0] = s_used[1] = 0;
    s_object = 0;
    s_drawing = 0;
    s_reported = 0;
    anchor_render_scratch_begin_bank();
}
#endif
