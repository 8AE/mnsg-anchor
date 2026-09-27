#ifndef ANCHOR_NAMEPLATE_BUDGET_H
#define ANCHOR_NAMEPLATE_BUDGET_H

/* Reserve at most one quarter of the remaining original RDRAM for two-bank
 * nameplate data; leave room for the native scene and render scratch. */
int anchor_nameplate_arena_plan(unsigned int cursor,
                                unsigned int bytes_per_plate,
                                unsigned int *start,
                                unsigned int *capacity,
                                unsigned int *end);

#endif
