#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ANCHOR_MODEL_POOL_HOST_TEST
#include "../src/global_patches/anchor_remote_model_pool.c"

void *D_8016DAC0_16E6C0[8];
short D_8016DAD8_16E6D8[8];
unsigned short D_8006D350_6DF50[8];
void *D_8006D320_6DF20;
unsigned short D_8016DAB0_16E6B0, D_8016DAB2_16E6B2;
unsigned char D_8006D328_6DF28[24] __attribute__((aligned(8)));
void *D_801684A0_1690A0;
static const void *pointers[65536];
static unsigned int pointer_count;
static int fail_alloc, allocation_count, geometry_calls, reclaim_calls;
static unsigned char stock[STOCK_MODEL_COUNT][MODEL_RECORD_SIZE] __attribute__((aligned(8)));

unsigned int anchor_pool_test_encode_pointer(const void *pointer)
{
    unsigned int i;
    if (!pointer) return 0;
    for (i = 1; i <= pointer_count; ++i)
        if (pointers[i] == pointer) return i;
    assert(pointer_count + 1 < sizeof(pointers) / sizeof(pointers[0]));
    pointers[++pointer_count] = pointer;
    return pointer_count;
}
void *anchor_pool_test_decode_pointer(unsigned int pointer)
{
    assert(pointer <= pointer_count);
    return (void *)pointers[pointer];
}
void *recomp_alloc(unsigned long size)
{
    if (fail_alloc) return 0;
    ++allocation_count;
    return malloc(size);
}
void func_8000A5C4_B1C4(void *record)
{
    unsigned int next = *(unsigned int *)record;
    memset(record, 0, MODEL_RECORD_SIZE);
    *(unsigned int *)record = next;
    ((unsigned char *)record)[4] = 0x82;
    ((unsigned char *)record)[5] = 5;
    ((unsigned char *)record)[0x65] = 1;
}
void *func_80034A10_35610(void *task)
{
    memset(task, 0, TASK_RECORD_SIZE);
    return task;
}
int func_80036058_36C58(void *task, int count) { (void)task; (void)count; return 0; }
int func_80019160_19D60(void *parent, unsigned int model)
{
    assert(parent); (void)model; ++reclaim_calls; return 0;
}
static int move_to_front(void *parent, void *record, unsigned int offset)
{
    void *first = read_pointer(parent, offset);
    void *previous = first;
    while (read_pointer(previous, 0x74) != record) previous = read_pointer(previous, 0x74);
    write_pointer(previous, 0x74, read_pointer(record, 0x74));
    write_pointer(record, 0x74, first);
    write_pointer(parent, offset, record);
    return 1;
}
int func_80036388_36F88(void *parent, void *child) { return move_to_front(parent, child, 0x78); }
int func_800363F4_36FF4(void *parent, void *sibling) { return move_to_front(parent, sibling, 0x74); }
void func_800196F0_1A2F0(unsigned int command) { (void)command; ++geometry_calls; }
void func_80018CA0_198A0(void *model, void *record) { assert(model && record); ++geometry_calls; }
int func_8003674C_3734C(void *record) { write_pointer(record, 0x78, 0); return 1; }
int func_80036798_37398(void *record) { write_pointer(record, 0x74, 0); return 1; }

static void boot(void)
{
    int i;
    memset(D_8016DAC0_16E6C0, 0, sizeof(D_8016DAC0_16E6C0));
    memset(D_8016DAD8_16E6D8, 0, sizeof(D_8016DAD8_16E6D8));
    for (i = STOCK_MODEL_COUNT - 1; i >= 0; --i)
    {
        write_pointer(stock[i], 0, D_8016DAC0_16E6C0[2]);
        func_8000A5C4_B1C4(stock[i]);
        D_8016DAC0_16E6C0[2] = stock[i];
    }
    D_8016DAB0_16E6B0 = D_8016DAB2_16E6B2 = 0;
    anchor_model_pool_reset();
    memset(D_8006D328_6DF28, 0, sizeof(D_8006D328_6DF28));
    anchor_task_pool_reset();
    fail_alloc = 0;
    geometry_calls = reclaim_calls = 0;
}
static unsigned int free_count(void)
{
    void *record = D_8016DAC0_16E6C0[2];
    unsigned int count = 0;
    while (record)
    {
        assert(++count <= STOCK_MODEL_COUNT + s_model_extra); /* Detect cycles. */
        assert(((unsigned char *)record)[4] == 0x82);
        assert(((unsigned char *)record)[0x74] == 0);
        assert(read_pointer(record, 0x94) == 0);
        record = read_pointer(record, 0);
    }
    return count;
}
static void native_release(void *record)
{
    write_pointer(record, 0, D_8016DAC0_16E6C0[2]);
    D_8016DAC0_16E6C0[2] = record;
    --D_8016DAD8_16E6D8[2];
    func_8000A5C4_B1C4(record);
}
static void test_growth_reuse_reset_and_membership(void)
{
    void *records[1200];
    int i, round, allocations;
    boot();
    for (i = 0; i < 1200; ++i)
    {
        records[i] = func_80035D8C_3698C(2);
        assert(records[i]);
        assert(((uintptr_t)records[i] & 7u) == 0);
        assert(((unsigned char *)records[i])[4] == 2);
        assert(read_pointer(records[i], 0) == 0);
    }
    assert(D_8016DAD8_16E6D8[2] == 1200 && D_8006D350_6DF50[2] >= 1200);
    assert(anchor_remote_model_pool_contains(records[1199]));
    assert(anchor_remote_model_pool_contains((unsigned char *)records[1199] + 0x94));
    assert(!anchor_remote_model_pool_contains(s_model_chunks));
    assert(!anchor_remote_model_pool_contains(s_model_chunks->records + s_model_chunks->count * MODEL_RECORD_SIZE));
    assert(!anchor_remote_model_pool_contains(stock[0]));
    allocations = allocation_count;
    for (round = 0; round < 5; ++round)
    {
        for (i = 0; i < 1200; ++i) native_release(records[i]);
        assert(D_8016DAD8_16E6D8[2] == 0);
        assert(free_count() == STOCK_MODEL_COUNT + s_model_extra);
        for (i = 0; i < 1200; ++i) assert((records[i] = func_80035D8C_3698C(2)) != 0);
    }
    assert(allocation_count == allocations);
    /* Native scene reset invalidates all old records before reconnecting chunks. */
    boot();
    assert(free_count() == STOCK_MODEL_COUNT + s_model_extra);
    assert(D_8016DAD8_16E6D8[2] == 0);
}
static void test_chain_and_native_other_kinds(void)
{
    unsigned char task[TASK_RECORD_SIZE] __attribute__((aligned(8))) = {0};
    unsigned char other[MODEL_RECORD_SIZE] __attribute__((aligned(8))) = {0};
    void *first, *second, *record;
    int i;
    boot();
    first = func_80035EEC_36AEC(task, 2, 200);
    assert(first == read_pointer(task, 0x18));
    second = func_80035EEC_36AEC(task, 2, 100);
    assert(second && D_8016DAD8_16E6D8[2] == 300);
    record = first;
    for (i = 0; i < 300; ++i) { assert(record); record = read_pointer(record, 0); }
    assert(!record && !read_pointer(read_pointer(task, 0x1c), 0));
    assert(!func_80035EEC_36AEC(0, 2, 1));
    other[4] = 0x83;
    D_8016DAC0_16E6C0[3] = other;
    D_8006D350_6DF50[3] = 1;
    assert(func_80035D8C_3698C(3) == other);
    assert(other[4] == 3 && D_8016DAD8_16E6D8[3] == 1);
    assert(!func_80035D8C_3698C(3));
}
static void exhaust_and_fail(void)
{
    unsigned int capacity = STOCK_MODEL_COUNT + s_model_extra;
    while ((unsigned short)D_8016DAD8_16E6D8[2] < capacity)
        assert(func_80035D8C_3698C(2));
    fail_alloc = 1;
}
static void test_oom_and_signed_counter_limit(void)
{
    unsigned char parent[MODEL_RECORD_SIZE] __attribute__((aligned(8))) = {0};
    unsigned char ancestor[MODEL_RECORD_SIZE] __attribute__((aligned(8))) = {0};
    unsigned char model[0x48] __attribute__((aligned(8))) = {0};
    unsigned char original[MODEL_RECORD_SIZE];
    boot();
    write_pointer(parent, 0x94, ancestor);
    model[4] = 1; model[5] = 2;
    *(unsigned int *)(model + 0x18) = 0x09000020;
    *(unsigned int *)(model + 0x30) = 0x09000030;
    memcpy(original, parent, sizeof(parent));
    D_801684A0_1690A0 = parent;
    exhaust_and_fail();
    assert(!func_80018F3C_19B3C(model, parent));
    assert(!func_8001904C_19C4C(model, parent));
    func_80018B28_19728(model, 0, 0);
    assert(memcmp(parent, original, sizeof(parent)) == 0);
    assert(geometry_calls == 0);
    assert(!func_80035EEC_36AEC(parent, 2, 12));
    D_8016DAD8_16E6D8[2] = 32767;
    assert(!func_80035D8C_3698C(2));
    assert(D_8016DAD8_16E6D8[2] == 32767);
}
static void test_model_cache_lookup_and_links(void)
{
    unsigned char parent[MODEL_RECORD_SIZE] __attribute__((aligned(8))) = {0};
    unsigned char ancestor[MODEL_RECORD_SIZE] __attribute__((aligned(8))) = {0};
    unsigned char model[0x48] __attribute__((aligned(8))) = {0};
    void *a, *b, *sibling;
    boot();
    write_pointer(parent, 0x94, ancestor);
    model[4] = 1; model[5] = 2;
    *(unsigned int *)(model + 0x18) = 0x09000020;
    *(unsigned int *)(model + 0x30) = 0x09000030;
    a = func_80018F3C_19B3C(model, parent);
    assert(a && read_pointer(parent, 0x78) == a && read_pointer(a, 0x94) == parent);
    assert(func_80018F3C_19B3C(model, parent) == a && D_8016DAD8_16E6D8[2] == 1);
    *(unsigned int *)(model + 0x18) = 0x09000040;
    b = func_80018F3C_19B3C(model, parent);
    assert(b && read_pointer(parent, 0x78) == b && read_pointer(b, 0x74) == a);
    *(unsigned int *)(model + 0x18) = 0x09000020;
    assert(func_80018F3C_19B3C(model, parent) == a);
    assert(read_pointer(parent, 0x78) == a && read_pointer(a, 0x74) == b && !read_pointer(b, 0x74));
    sibling = func_8001904C_19C4C(model, parent);
    assert(sibling && read_pointer(sibling, 0x94) == ancestor && read_pointer(parent, 0x74) == sibling);
}
static void test_task_growth_reset_and_failure(void)
{
    void *tasks[300];
    int i, allocations;
    boot();
    for (i = 0; i < 300; ++i)
    {
        anchor_task_pool_grow();
        tasks[i] = read_pointer(D_8006D328_6DF28, 8);
        assert(tasks[i] && anchor_remote_model_pool_contains((unsigned char *)tasks[i] + 4));
        assert(((uintptr_t)tasks[i] & 15u) == 0);
        write_pointer(D_8006D328_6DF28, 8, read_pointer(tasks[i], 0));
        write_pointer(tasks[i], 4, tasks[i]);
    }
    allocations = allocation_count;
    memset(D_8006D328_6DF28, 0, sizeof(D_8006D328_6DF28));
    anchor_task_pool_reset();
    for (i = 0; i < 300; ++i)
    {
        void *task = read_pointer(D_8006D328_6DF28, 8);
        assert(task && !read_pointer(task, 4));
        write_pointer(D_8006D328_6DF28, 8, read_pointer(task, 0));
    }
    assert(allocation_count == allocations);
    write_pointer(D_8006D328_6DF28, 8, 0);
    fail_alloc = 1;
    anchor_task_pool_grow();
    assert(!read_pointer(D_8006D328_6DF28, 8));
}
int main(void)
{
    test_growth_reuse_reset_and_membership();
    test_chain_and_native_other_kinds();
    test_model_cache_lookup_and_links();
    test_oom_and_signed_counter_limit();
    test_task_growth_reset_and_failure();
    puts("Expandable native model/task pool tests passed");
    return 0;
}
