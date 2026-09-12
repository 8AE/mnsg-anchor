/* Host pointer slots keep the native 32-bit record layout on 64-bit hosts. */
#ifndef IMPACT_TEST_POINTERS_H
#define IMPACT_TEST_POINTERS_H
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef struct { const void *base; unsigned int offset; void *value; } ImpactTestPtr;
static ImpactTestPtr test_ptrs[512];
static unsigned int test_ptr_count;
static void **test_ptr(const void *base, unsigned int offset)
{
    unsigned int i;
    for (i = 0; i < test_ptr_count; ++i)
        if (test_ptrs[i].base == base && test_ptrs[i].offset == offset)
            return &test_ptrs[i].value;
    assert(test_ptr_count < 512);
    test_ptrs[test_ptr_count].base = base;
    test_ptrs[test_ptr_count].offset = offset;
    return &test_ptrs[test_ptr_count++].value;
}
#define TP(p, o) (*test_ptr((p), (o)))
#define TU16(p, o) (*(unsigned short *)((unsigned char *)(p) + (o)))
#define TU32(p, o) (*(unsigned int *)((unsigned char *)(p) + (o)))
#define TF32(p, o) (*(float *)((unsigned char *)(p) + (o)))
#endif
