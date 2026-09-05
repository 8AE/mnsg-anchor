#include "utils/array_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int fail_alloc;
static int allocations;
static int releases;

void *recomp_alloc(unsigned long size)
{
    ++allocations;
    return fail_alloc ? 0 : malloc(size);
}

void recomp_free(void *memory)
{
    ++releases;
    free(memory);
}

int main(void)
{
    unsigned int *items = 0;
    unsigned int *previous;
    int capacity = 0;
    int old_capacity;
    int i;

    assert(mnsg_array_reserve((void **)&items, &capacity, 0, sizeof(*items)));
    assert(items == 0 && capacity == 0 && allocations == 0);
    assert(mnsg_array_reserve((void **)&items, &capacity, 33, sizeof(*items)));
    assert(capacity >= 33);
    for (i = 0; i < capacity; ++i)
    {
        assert(items[i] == 0);
        items[i] = (unsigned int)(i + 1);
    }
    previous = items;
    old_capacity = capacity;
    fail_alloc = 1;
    assert(!mnsg_array_reserve((void **)&items, &capacity, 100, sizeof(*items)));
    assert(items == previous && capacity == old_capacity && releases == 0);
    fail_alloc = 0;
    assert(mnsg_array_reserve((void **)&items, &capacity, 100, sizeof(*items)));
    assert(capacity >= 100 && releases == 1);
    for (i = 0; i < capacity; ++i)
        assert(items[i] == (i < old_capacity ? (unsigned int)(i + 1) : 0u));
    previous = items;
    old_capacity = capacity;
    assert(mnsg_array_reserve((void **)&items, &capacity, 1, sizeof(*items)));
    assert(items == previous && capacity == old_capacity);
    assert(!mnsg_array_reserve((void **)&items, &capacity, -1, sizeof(*items)));
    assert(!mnsg_array_reserve((void **)&items, &capacity, 0x7fffffff, sizeof(*items)));
    assert(!mnsg_array_reserve((void **)&items, &capacity, 1, 0));
    assert(!mnsg_array_reserve(0, &capacity, 1, sizeof(*items)));
    assert(!mnsg_array_reserve((void **)&items, 0, 1, sizeof(*items)));
    assert(items == previous && capacity == old_capacity && releases == 1);
    recomp_free(items);
    puts("dynamic array growth, initialization, overflow, and failure tests passed");
    return 0;
}
