#ifndef ANCHOR_ITEM_RECONCILE_H
#define ANCHOR_ITEM_RECONCILE_H

/* Pure save/cache decisions. The caller owns native writes and side effects.
 * An absent snapshot key must not call this helper: it acknowledges nothing. */
typedef struct AnchorItemReconcile
{
    int value;
    int cached;
    int changed;
    int acknowledged;
} AnchorItemReconcile;

/* Preserve the existing field rules: take-max accepts only a greater value;
 * one-shot accepts any nonzero value only while the local value is zero.
 * A stale/different incoming value must not hide an unsent local gain. A
 * non-durable live merge also preserves an existing dirty gain when it raises
 * the local value, making this client publish the merged value later. */
AnchorItemReconcile anchor_item_reconcile_field(int local, int cached,
                                               int incoming, int take_max,
                                               int source_is_durable);

/* Packed flags normalize nonzero to one and never clear local progress. */
AnchorItemReconcile anchor_item_reconcile_flag(int local, int cached,
                                              int incoming,
                                              int source_is_durable);

#endif
