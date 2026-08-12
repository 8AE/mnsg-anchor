#include "modding.h"

#define ANCHOR_REMOTE_MAX 25
#define ANCHOR_MODELS_PER_REMOTE 1
#define ANCHOR_REQUIRED_KIND2_RECORDS (ANCHOR_REMOTE_MAX * ANCHOR_MODELS_PER_REMOTE)
#define ANCHOR_KIND2_STOCK_RECORDS 0xC0
#define ANCHOR_ACTOR_CAP_TOTAL 0x90
#define ANCHOR_ACTOR_CAP_KIND_6 0x50
#define ANCHOR_ACTOR_CAP_KIND_8 0x50
#define ANCHOR_ACTOR_CAP_KIND_9 0x50
#define ANCHOR_ACTOR_CAP_KIND_10 0x40
#define ANCHOR_ACTOR_CAP_KIND_12 0x20

/* Actor-manager capacity globals. Ghidra shows initialization rewriting these
 * addresses, so the frame return hook raises them again while remote child
 * tasks coexist with normal room actors. */
#define ACTOR_CAP_KIND_8 (*(volatile int *)0x8015CCE0)
#define ACTOR_CAP_KIND_10 (*(volatile int *)0x8015CCE4)
#define ACTOR_CAP_KIND_6 (*(volatile int *)0x8015CCE8)
#define ACTOR_CAP_KIND_12 (*(volatile int *)0x8015CCEC)
#define ACTOR_CAP_KIND_9 (*(volatile int *)0x8015CCF0)
#define ACTOR_CAP_TOTAL (*(volatile int *)0x8015CCFC)

/* Per-kind active allocation counters. func_80035EEC_36AEC checks this table
 * against D_8006D350 before allocating a chain. */
extern short D_8016DAD8_16E6D8[];

/* Active allocation count for kind 2. Ghidra shows func_80036448_37048 using
 * this dedicated counter when it allocates a single kind-2 record. */
extern short D_8016DADC_16E6DC;

/* Per-kind maximum allocation table. Ghidra shows func_8000DBF0_E7F0 using
 * kind 2 for the model/display record attached to each cutscene-model task. */
extern unsigned short D_8006D350_6DF50[];

/* Optional emergency/overflow list consulted by func_80036448_37048 before it
 * falls back to the normal kind-2 free list. */
extern void *D_8006D320_6DF20;

/* Current and peak counters maintained by func_80036448_37048 for single
 * kind-2 allocations. The patch preserves the counter updates before changing
 * the capacity check. */
extern unsigned short D_8016DAB0_16E6B0;
extern unsigned short D_8016DAB2_16E6B2;

/* Pop one record from the free list for the requested kind and increment that
 * kind's active counter. The patched allocators reuse this helper so release
 * paths continue to see normal engine-owned records. */
extern void *func_80035D8C_3698C(short kind);

/* Replenish from the optional overflow list used by the stock single kind-2
 * allocator path. */
extern int func_80036058_36C58(void *list, int count);

static unsigned short anchor_remote_model_kind2_cap(short kind)
{
    /* Read the engine's per-kind limit so patched allocation preserves every
     * stock kind and changes only kind 2 when its configured cap is smaller. */
    unsigned short cap = D_8006D350_6DF50[kind];

    if (kind == 2 && cap < ANCHOR_KIND2_STOCK_RECORDS)
        cap = ANCHOR_KIND2_STOCK_RECORDS;

    return cap;
}

static void anchor_raise_remote_model_caps(void)
{
    /* Write the engine cap table and actor-manager globals here because their
     * stock initialization can overwrite the multiplayer headroom. */
    if (D_8006D350_6DF50[2] < ANCHOR_KIND2_STOCK_RECORDS)
        D_8006D350_6DF50[2] = ANCHOR_KIND2_STOCK_RECORDS;

    if (ACTOR_CAP_TOTAL < ANCHOR_ACTOR_CAP_TOTAL)
        ACTOR_CAP_TOTAL = ANCHOR_ACTOR_CAP_TOTAL;
    if (ACTOR_CAP_KIND_6 < ANCHOR_ACTOR_CAP_KIND_6)
        ACTOR_CAP_KIND_6 = ANCHOR_ACTOR_CAP_KIND_6;
    if (ACTOR_CAP_KIND_8 < ANCHOR_ACTOR_CAP_KIND_8)
        ACTOR_CAP_KIND_8 = ANCHOR_ACTOR_CAP_KIND_8;
    if (ACTOR_CAP_KIND_9 < ANCHOR_ACTOR_CAP_KIND_9)
        ACTOR_CAP_KIND_9 = ANCHOR_ACTOR_CAP_KIND_9;
    if (ACTOR_CAP_KIND_10 < ANCHOR_ACTOR_CAP_KIND_10)
        ACTOR_CAP_KIND_10 = ANCHOR_ACTOR_CAP_KIND_10;
    if (ACTOR_CAP_KIND_12 < ANCHOR_ACTOR_CAP_KIND_12)
        ACTOR_CAP_KIND_12 = ANCHOR_ACTOR_CAP_KIND_12;
}

static void anchor_mark_model_pool_record(void *record, short kind)
{
    if (record)
        *((unsigned char *)record + 4) = (unsigned char)((*((unsigned char *)record + 4) & 0x80) | (kind & 0x7f));
}

/* Patch the chain allocator reached by func_8000DBF0_E7F0. Ghidra shows the
 * stock function appending records through task offsets +0x18/+0x1c after its
 * cap check. This preserves those lists while making room for one model record
 * per remote cutscene character. */
RECOMP_PATCH void *func_80035EEC_36AEC(void *task, short kind, unsigned int count)
{
    void *first = 0;
    unsigned int allocated = 0;
    unsigned int needed = count & 0xff;

    /* Use the engine's active-count table for the same capacity decision as
     * the original chain allocator. */
    if ((int)(anchor_remote_model_kind2_cap(kind) - D_8016DAD8_16E6D8[kind]) < (int)needed)
        return 0;

    while (allocated < needed)
    {
        /* Pop through the engine helper so allocation counters and release
         * paths continue to own every model record. */
        void *record = func_80035D8C_3698C(kind);

        if (!record)
            return 0;

        anchor_mark_model_pool_record(record, kind);
        allocated = (allocated + 1) & 0xffff;

        if (!first)
            first = record;

        if (*(void **)((unsigned char *)task + 0x18))
        {
            **(void ***)((unsigned char *)task + 0x1c) = record;
        }
        else
        {
            *(void **)((unsigned char *)task + 0x18) = record;
        }

        *(void **)((unsigned char *)task + 0x1c) = record;
    }

    return first;
}

/* Patch the single kind-2 allocator so stock effects share the same raised cap
 * while remote model/display records are active. */
RECOMP_PATCH void *func_80036448_37048(void)
{
    void *record;
    unsigned int active;

    /* Preserve the engine's current/peak telemetry updates before applying
     * the enlarged kind-2 capacity decision. */
    active = D_8016DAB0_16E6B0 + 1;
    D_8016DAB0_16E6B0 = (unsigned short)active;
    if (D_8016DAB2_16E6B2 < active)
        D_8016DAB2_16E6B2 = (unsigned short)active;

    /* Prefer the engine overflow list when present, exactly as the original
     * single-record allocator does. */
    if (D_8006D320_6DF20)
    {
        func_80036058_36C58(D_8006D320_6DF20, 1);
    }
    else if ((int)(anchor_remote_model_kind2_cap(2) - D_8016DADC_16E6DC) < 1)
    {
        return 0;
    }

    /* Allocate through the engine free-list helper so normal cleanup can
     * return this remote model record without a mod-owned pool. */
    record = func_80035D8C_3698C(2);
    anchor_mark_model_pool_record(record, 2);
    return record;
}

/* actor_manager_initialize resets actor caps on each setup. This return hook
 * keeps enough tasks and model records available for remote cutscene models to
 * coexist with the room's normal actors and effects. */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_remote_model_pool_frame_caps_hook(void)
{
    anchor_raise_remote_model_caps();
}

typedef char anchor_remote_model_pool_sanity[
    (ANCHOR_REQUIRED_KIND2_RECORDS <= ANCHOR_KIND2_STOCK_RECORDS) ? 1 : -1];
