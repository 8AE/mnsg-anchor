#include "anchor_remote_model_pool.h"
#ifndef ANCHOR_MODEL_POOL_HOST_TEST
#include "modding.h"
#include "recomputils.h"
#else
#define RECOMP_PATCH
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
extern void *recomp_alloc(unsigned long size);
extern unsigned int anchor_pool_test_encode_pointer(const void *pointer);
extern void *anchor_pool_test_decode_pointer(unsigned int pointer);
#endif

/* Native animated models allocate a root display record AND cached child /
 * sibling records during drawing. The former stock cap-only patch reserved no
 * storage for those limbs. Grow real CPU records on the engine free lists.
 * Textures, matrices and display-list commands remain in their GPU arenas. */
#define MODEL_RECORD_SIZE 0x98u
#define TASK_RECORD_SIZE 0xf0u
#define MODEL_CHUNK_COUNT 64u
#define TASK_CHUNK_COUNT 32u
#define STOCK_MODEL_COUNT 192u
#define STOCK_TASK_COUNT 79u
#define MODEL_COUNT_MAX 32767u /* Native active kind counters are signed16. */
#define TASK_COUNT_MAX 65535u /* Native task active counter is unsigned16. */

typedef struct NativePoolChunk
{
    struct NativePoolChunk *next;
    unsigned int count;
    unsigned int record_size;
    /* Stock model stride0x98 preserves8-byte alignment; task stride0xF0
     * preserves16. Keep both when records follow the chunk metadata. */
    unsigned char records[] __attribute__((aligned(16)));
} NativePoolChunk;

extern void *D_8016DAC0_16E6C0[];
extern short D_8016DAD8_16E6D8[];
extern unsigned short D_8006D350_6DF50[];
extern void *D_8006D320_6DF20;
extern unsigned short D_8016DAB0_16E6B0, D_8016DAB2_16E6B2;
/* D8006D328 is the first word of the native task-list state; +8 is free head. */
extern unsigned char D_8006D328_6DF28[];
extern void *D_801684A0_1690A0;
extern void func_8000A5C4_B1C4(void *record);
extern void *func_80034A10_35610(void *task);
extern int func_80036058_36C58(void *task, int count);
extern int func_80019160_19D60(void *parent, unsigned int model);
extern int func_80036388_36F88(void *parent, void *child);
extern int func_800363F4_36FF4(void *parent, void *sibling);
extern void func_800196F0_1A2F0(unsigned int command);
extern void func_80018CA0_198A0(void *model, void *record);
extern int func_8003674C_3734C(void *record);
extern int func_80036798_37398(void *record);

static NativePoolChunk *s_model_chunks;
static NativePoolChunk *s_task_chunks;
static unsigned int s_model_extra, s_task_extra;

/* Explicit32-bit native links also make layout-faithful host fixtures possible. */
static void *read_pointer(const void *record, unsigned int offset)
{
#ifdef ANCHOR_MODEL_POOL_HOST_TEST
    return anchor_pool_test_decode_pointer(*(const unsigned int *)((const unsigned char *)record + offset));
#else
    return (void *)(unsigned long)*(const unsigned int *)((const unsigned char *)record + offset);
#endif
}
static void write_pointer(void *record, unsigned int offset, const void *value)
{
#ifdef ANCHOR_MODEL_POOL_HOST_TEST
    *(unsigned int *)((unsigned char *)record + offset) = anchor_pool_test_encode_pointer(value);
#else
    *(unsigned int *)((unsigned char *)record + offset) = (unsigned int)(unsigned long)value;
#endif
}

static int contains_chunk(const NativePoolChunk *chunk, const void *pointer)
{
    unsigned long address = (unsigned long)pointer;
    for (; chunk; chunk = chunk->next)
    {
        unsigned long first = (unsigned long)chunk->records;
        unsigned long size = chunk->count * chunk->record_size;
        if (address >= first && address - first < size)
            return 1;
    }
    return 0;
}

int anchor_remote_model_pool_contains(const void *pointer)
{
    return pointer && (contains_chunk(s_model_chunks, pointer) || contains_chunk(s_task_chunks, pointer));
}

static NativePoolChunk *allocate_chunk(unsigned int count, unsigned int record_size)
{
    NativePoolChunk *chunk = recomp_alloc(sizeof(NativePoolChunk) + count * record_size);
#ifndef ANCHOR_MODEL_POOL_HOST_TEST
    /* The runtime heap is CPU-mapped KSEG0 memory (512MiB), not RSP storage.
     * Reject invalid runtime allocation results as well as ordinary null. */
    unsigned int address = (unsigned int)(unsigned long)chunk;
    if (address < 0x80001000u || address >= 0xa0000000u)
        return 0;
#endif
    if (!chunk)
        return 0;
    chunk->count = count;
    chunk->record_size = record_size;
    chunk->next = 0;
    return chunk;
}

static void attach_model_chunk(NativePoolChunk *chunk)
{
    unsigned int i;
    for (i = 0; i < chunk->count; ++i)
    {
        void *record = chunk->records + i * MODEL_RECORD_SIZE;
        /* The native reset preserves +0 (free-list next), resets all other
         *0x98 bytes, and marks the record retired kind2 (byte4=0x82). */
        write_pointer(record, 0, D_8016DAC0_16E6C0[2]);
        func_8000A5C4_B1C4(record);
        D_8016DAC0_16E6C0[2] = record;
    }
}

static int ensure_model_capacity(unsigned int needed)
{
    unsigned int active = (unsigned short)D_8016DAD8_16E6D8[2];
    unsigned int capacity = STOCK_MODEL_COUNT + s_model_extra;
    if (active > MODEL_COUNT_MAX || needed > MODEL_COUNT_MAX - active)
        return 0;
    while (capacity < active || capacity - active < needed || !D_8016DAC0_16E6C0[2])
    {
        unsigned int count = MODEL_CHUNK_COUNT;
        NativePoolChunk *chunk;
        if (capacity >= MODEL_COUNT_MAX)
            return 0;
        if (count > MODEL_COUNT_MAX - capacity)
            count = MODEL_COUNT_MAX - capacity;
        chunk = allocate_chunk(count, MODEL_RECORD_SIZE);
        if (!chunk)
            return 0;
        chunk->next = s_model_chunks;
        s_model_chunks = chunk;
        s_model_extra += count;
        capacity += count;
        attach_model_chunk(chunk);
    }
    D_8006D350_6DF50[2] = (unsigned short)capacity;
    return 1;
}

/* Native reset replaces the free head and active counter; it cannot see our
 * heap storage. Reinitialize and reconnect every retained chunk exactly once. */
RECOMP_HOOK_RETURN("func_80035BF8_367F8")
void anchor_model_pool_reset(void)
{
    NativePoolChunk *chunk;
    for (chunk = s_model_chunks; chunk; chunk = chunk->next)
        attach_model_chunk(chunk);
    D_8006D350_6DF50[2] = (unsigned short)(STOCK_MODEL_COUNT + s_model_extra);
}

static void attach_task_chunk(NativePoolChunk *chunk)
{
    unsigned int i;
    for (i = 0; i < chunk->count; ++i)
    {
        void *task = chunk->records + i * TASK_RECORD_SIZE;
        func_80034A10_35610(task);
        write_pointer(task, 0, read_pointer(D_8006D328_6DF28, 8));
        write_pointer(D_8006D328_6DF28, 8, task);
    }
}

RECOMP_HOOK_RETURN("func_8003488C_3548C")
void anchor_task_pool_reset(void)
{
    NativePoolChunk *chunk;
    for (chunk = s_task_chunks; chunk; chunk = chunk->next)
        attach_task_chunk(chunk);
}

RECOMP_HOOK("func_80034B58_35758")
void anchor_task_pool_grow(void)
{
    unsigned int count = TASK_CHUNK_COUNT;
    NativePoolChunk *chunk;
    if (read_pointer(D_8006D328_6DF28, 8) || STOCK_TASK_COUNT + s_task_extra >= TASK_COUNT_MAX)
        return;
    if (count > TASK_COUNT_MAX - STOCK_TASK_COUNT - s_task_extra)
        count = TASK_COUNT_MAX - STOCK_TASK_COUNT - s_task_extra;
    chunk = allocate_chunk(count, TASK_RECORD_SIZE);
    if (!chunk)
        return; /* The original allocator already returnsNULL safely. */
    chunk->next = s_task_chunks;
    s_task_chunks = chunk;
    s_task_extra += count;
    attach_task_chunk(chunk);
}

RECOMP_PATCH void *func_80035D8C_3698C(short kind)
{
    void *record;
    if (kind == 2 && !ensure_model_capacity(1))
        return 0;
    record = D_8016DAC0_16E6C0[kind];
    if (!record)
        return 0;
    D_8016DAC0_16E6C0[kind] = read_pointer(record, 0);
    write_pointer(record, 0, 0);
    ((unsigned char *)record)[4] &= 0x7fu;
    ++D_8016DAD8_16E6D8[kind];
    return record;
}

RECOMP_PATCH void *func_80035EEC_36AEC(void *task, short kind, unsigned int count)
{
    void *first = 0;
    unsigned int allocated, needed = count & 0xffu;
    if (!task || (kind == 2 && needed && !ensure_model_capacity(needed)) ||
        (int)D_8006D350_6DF50[kind] - D_8016DAD8_16E6D8[kind] < (int)needed)
        return 0;
    for (allocated = 0; allocated < needed; ++allocated)
    {
        void *record = func_80035D8C_3698C(kind);
        if (!record)
            return 0;
        if (!first)
            first = record;
        if (read_pointer(task, 0x18))
            write_pointer(read_pointer(task, 0x1c), 0, record);
        else
            write_pointer(task, 0x18, record);
        write_pointer(task, 0x1c, record);
    }
    return first;
}

/* The front-inserting native chain allocator has the same pre-pop cap check. */
RECOMP_HOOK("func_80035DFC_369FC")
void anchor_model_front_allocation(void *task, short kind, unsigned int count)
{
    (void)task;
    if (kind == 2 && (count & 0xffu))
        (void)ensure_model_capacity(count & 0xffu);
}

RECOMP_PATCH void *func_80036448_37048(void)
{
    unsigned int active = D_8016DAB0_16E6B0 + 1u;
    D_8016DAB0_16E6B0 = (unsigned short)active;
    if (D_8016DAB2_16E6B2 < active)
        D_8016DAB2_16E6B2 = (unsigned short)active;
    if (D_8006D320_6DF20)
        func_80036058_36C58(D_8006D320_6DF20, 1);
    return func_80035D8C_3698C(2);
}

/* All three native cache constructors dereference the allocation result before
 * their callers can inspect it. Preserve successful native cache lookup/linking
 * and return safely when even the growable CPU pool is out of memory. */
static void *model_cache_link(void *model, void *parent, int sibling)
{
    unsigned int link_offset = sibling ? 0x74u : 0x78u;
    int model_index = ((signed char *)model)[sibling ? 5 : 4];
    unsigned int command = *(unsigned int *)((unsigned char *)model + model_index * 0x18) & 0x8fffffffu;
    void *first = read_pointer(parent, link_offset);
    void *record = first;
    while (record && command != (*(unsigned int *)((unsigned char *)record + 0x2c) & 0x8ffffffeu))
        record = read_pointer(record, 0x74);
    if (record)
    {
        if (record != first)
        {
            if (sibling) func_800363F4_36FF4(parent, record);
            else func_80036388_36F88(parent, record);
        }
        return record;
    }
    {
        void *ancestor = sibling ? read_pointer(parent, 0x94) : parent;
        if (ancestor)
            func_80019160_19D60(ancestor, command + 0x70000000u);
    }
    record = func_80036448_37048();
    if (!record)
        return 0;
    write_pointer(record, 0x94, sibling ? read_pointer(parent, 0x94) : parent);
    *(unsigned int *)((unsigned char *)record + 0x2c) = command + 0x70000000u;
    write_pointer(parent, link_offset, record);
    if (first)
        write_pointer(record, 0x74, first);
    return record;
}

RECOMP_PATCH void *func_80018F3C_19B3C(void *model, void *parent)
{
    return model_cache_link(model, parent, 0);
}
RECOMP_PATCH void *func_8001904C_19C4C(void *model, void *parent)
{
    return model_cache_link(model, parent, 1);
}
RECOMP_PATCH void func_80018B28_19728(void *model, void *unused, void *record)
{
    unsigned int command = *(unsigned int *)model;
    int child = ((signed char *)model)[4];
    int sibling;
    void *linked;
    (void)unused;
    if (!record)
    {
        record = func_80036448_37048();
        if (!record)
            return;
        write_pointer(record, 0x94, 0);
        write_pointer(D_801684A0_1690A0, 0x78, record);
    }
    /*80018B68's apparent replacement branch is unreachable: sltu produces
     *0/1 thenAND0x8ffffffe always0. Preserve the actual instructions. */
    *(unsigned int *)((unsigned char *)record + 0x2c) = command;
    if (command)
        func_800196F0_1A2F0(command);
    if (child)
    {
        linked = func_80018F3C_19B3C(model, record);
        if (linked)
            func_80018CA0_198A0((unsigned char *)model + child * 0x18, linked);
    }
    else if (read_pointer(record, 0x78))
        func_8003674C_3734C(record);
    sibling = ((signed char *)model)[5];
    if (sibling)
    {
        linked = func_8001904C_19C4C(model, record);
        if (linked)
            func_80018CA0_198A0((unsigned char *)model + sibling * 0x18, linked);
    }
    else if (read_pointer(record, 0x74))
        func_80036798_37398(record);
}
