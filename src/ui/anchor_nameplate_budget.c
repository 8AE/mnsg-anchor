#include "anchor_nameplate_budget.h"

int anchor_nameplate_arena_plan(unsigned int cursor,
                                unsigned int bytes_per_plate,
                                unsigned int *start,
                                unsigned int *capacity,
                                unsigned int *end)
{
    unsigned int aligned, available, count;
    if (!start || !capacity || !end || !bytes_per_plate ||
        cursor < 0x1000u || cursor >= 0x800000u)
        return 0;
    aligned = (cursor + 15u) & ~15u;
    if (aligned >= 0x800000u)
        return 0;
    available = 0x800000u - aligned;
    count = (available / 4u) / bytes_per_plate;
    if (!count)
        return 0;
    *start = aligned;
    *capacity = count;
    *end = aligned + count * bytes_per_plate;
    return 1;
}
