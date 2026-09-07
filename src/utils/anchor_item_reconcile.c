#include "anchor_item_reconcile.h"

AnchorItemReconcile anchor_item_reconcile_field(int local, int cached,
                                               int incoming, int take_max,
                                               int source_is_durable)
{
    AnchorItemReconcile result;
    int local_dirty_gain;

    local_dirty_gain = take_max ? local > cached
                                : local != 0 && local != cached;
    result.changed = take_max ? incoming > local : local == 0 && incoming != 0;
    result.value = result.changed ? incoming : local;
    /* A live merge is not stored by Anchor.  A clean client can accept it as
       its baseline without echoing, but an already-dirty client must retain
       responsibility for publishing the merged value through SET_FLAG. */
    if (result.changed)
        result.acknowledged = source_is_durable || !local_dirty_gain;
    else
        result.acknowledged = source_is_durable &&
                              incoming == result.value;
    result.cached = result.acknowledged ? result.value : cached;
    return result;
}

AnchorItemReconcile anchor_item_reconcile_flag(int local, int cached,
                                              int incoming,
                                              int source_is_durable)
{
    return anchor_item_reconcile_field(local != 0, cached != 0,
                                       incoming != 0, 0,
                                       source_is_durable);
}
