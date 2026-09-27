#include <assert.h>

#include "../src/ui/anchor_nameplate_budget.h"

int main(void)
{
    unsigned int start, capacity, end;
    assert(anchor_nameplate_arena_plan(0x500003u, 0x22a0u,
                                       &start, &capacity, &end));
    assert(start == 0x500010u);
    assert(capacity > 25u);
    assert(end <= 0x800000u);
    assert(end - start == capacity * 0x22a0u);
    assert(end - start <= (0x800000u - start) / 4u);
    assert(!anchor_nameplate_arena_plan(0x7ffff0u, 0x22a0u,
                                        &start, &capacity, &end));
    assert(!anchor_nameplate_arena_plan(0x800000u, 0x22a0u,
                                        &start, &capacity, &end));
    return 0;
}
