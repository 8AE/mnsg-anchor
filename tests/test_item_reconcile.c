#include "../src/utils/anchor_item_reconcile.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

typedef struct TestField
{
    int value;
    int cached;
    int take_max;
    int packets;
} TestField;

static AnchorItemReconcile receive(TestField *field, int incoming,
                                   int source_is_durable)
{
    AnchorItemReconcile result = anchor_item_reconcile_field(
        field->value, field->cached, incoming, field->take_max,
        source_is_durable);
    field->value = result.value;
    field->cached = result.cached;
    return result;
}

/* Model the existing gain-only monitor and a deferred send budget. */
static void monitor(TestField *field, int can_send)
{
    int gain;
    if (field->value == field->cached)
        return;
    gain = field->take_max ? field->value > field->cached : field->value != 0;
    if (gain)
    {
        if (!can_send)
            return;
        field->packets++;
    }
    field->cached = field->value;
}

static void stale_snapshot_preserves_local_pickup(void)
{
    TestField moon = {1, 0, 0, 0};
    TestField unrelated = {1, 0, 0, 0};
    AnchorItemReconcile result;

    monitor(&moon, 0);
    result = receive(&moon, 0, 1);
    assert(!result.changed && !result.acknowledged);
    assert(moon.value == 1 && moon.cached == 0);
    monitor(&moon, 0);
    result = receive(&moon, 0, 1);
    assert(!result.acknowledged && moon.cached == 0);
    monitor(&moon, 1);
    monitor(&moon, 1);
    assert(moon.packets == 1 && moon.cached == 1);

    /* A partial snapshot never acknowledges a key absent from that packet. */
    assert(unrelated.cached == 0);
    monitor(&unrelated, 1);
    assert(unrelated.packets == 1);
}

static void matching_snapshot_acknowledges_without_echo(void)
{
    TestField moon = {1, 0, 0, 0};
    TestField remote = {0, 0, 0, 0};
    AnchorItemReconcile result = receive(&moon, 1, 1);
    assert(!result.changed && result.acknowledged && moon.cached == 1);
    monitor(&moon, 1);
    assert(moon.packets == 0);

    result = receive(&remote, 1, 1);
    assert(result.changed && result.acknowledged);
    assert(remote.value == 1 && remote.cached == 1);
    result = receive(&remote, 1, 1);
    assert(!result.changed && result.acknowledged);
    monitor(&remote, 1);
    assert(remote.packets == 0);
}

static void field_value_rules(void)
{
    TestField count = {5, 2, 1, 0};
    TestField one_shot = {2, 0, 0, 0};
    AnchorItemReconcile result = receive(&count, 4, 1);
    assert(!result.changed && !result.acknowledged);
    assert(count.value == 5 && count.cached == 2);
    monitor(&count, 1);
    assert(count.packets == 1);
    result = receive(&count, 9, 1);
    assert(result.changed && result.acknowledged);
    assert(count.value == 9 && count.cached == 9);
    monitor(&count, 1);
    assert(count.packets == 1);

    result = receive(&one_shot, 1, 1);
    assert(!result.changed && !result.acknowledged);
    assert(one_shot.value == 2 && one_shot.cached == 0);
    monitor(&one_shot, 1);
    assert(one_shot.packets == 1);

    result = anchor_item_reconcile_field(INT_MAX, 0, INT_MAX - 1, 1, 1);
    assert(!result.changed && !result.acknowledged && result.cached == 0);
    result = anchor_item_reconcile_field(0, 0, 0, 0, 1);
    assert(!result.changed && result.acknowledged && result.value == 0);
}

static void packed_flag_rules(void)
{
    AnchorItemReconcile result = anchor_item_reconcile_flag(8, 0, 0, 1);
    assert(result.value == 1 && result.cached == 0);
    assert(!result.changed && !result.acknowledged);

    result = anchor_item_reconcile_flag(8, 0, 32, 1);
    assert(result.value == 1 && result.cached == 1);
    assert(!result.changed && result.acknowledged);

    result = anchor_item_reconcile_flag(0, 0, 32, 1);
    assert(result.value == 1 && result.cached == 1);
    assert(result.changed && result.acknowledged);

    result = anchor_item_reconcile_flag(1, 8, 0, 1);
    assert(result.value == 1 && result.cached == 1);
    assert(!result.changed && !result.acknowledged);
}

static void live_snapshot_does_not_acknowledge_dirty_local_gain(void)
{
    TestField moon = {1, 0, 0, 0};
    TestField remote = {0, 0, 0, 0};
    AnchorItemReconcile result;

    result = receive(&moon, 1, 0);
    assert(!result.changed && !result.acknowledged);
    assert(moon.value == 1 && moon.cached == 0);
    monitor(&moon, 1);
    assert(moon.packets == 1 && moon.cached == 1);

    /* A value newly learned from a live peer still becomes the local
       baseline; the original sender remains responsible for durability. */
    result = receive(&remote, 1, 0);
    assert(result.changed && result.acknowledged);
    assert(remote.value == 1 && remote.cached == 1);
    monitor(&remote, 1);
    assert(remote.packets == 0);
}

static void live_snapshot_preserves_dirty_gain_when_raising_it(void)
{
    TestField count = {5, 2, 1, 0};
    TestField clean = {5, 5, 1, 0};
    AnchorItemReconcile result;

    result = receive(&count, 9, 0);
    assert(result.changed && !result.acknowledged);
    assert(count.value == 9 && count.cached == 2);
    monitor(&count, 1);
    assert(count.packets == 1 && count.cached == 9);

    /* A clean recipient still accepts live progress as its baseline so a
       single broadcaster does not fan out one echo from every teammate. */
    result = receive(&clean, 9, 0);
    assert(result.changed && result.acknowledged);
    assert(clean.value == 9 && clean.cached == 9);
    monitor(&clean, 1);
    assert(clean.packets == 0);
}

static void live_deferred_completion_stays_dirty(void)
{
    AnchorItemReconcile result = anchor_item_reconcile_flag(1, 0, 1, 0);

    /* Models native Moon completion after a live-only A4 was deferred. */
    assert(!result.changed && !result.acknowledged);
    assert(result.value == 1 && result.cached == 0);
}

int main(void)
{
    stale_snapshot_preserves_local_pickup();
    matching_snapshot_acknowledges_without_echo();
    field_value_rules();
    packed_flag_rules();
    live_snapshot_does_not_acknowledge_dirty_local_gain();
    live_snapshot_preserves_dirty_gain_when_raising_it();
    live_deferred_completion_stays_dirty();
    puts("Item reconciliation and stale snapshot tests passed");
    return 0;
}
