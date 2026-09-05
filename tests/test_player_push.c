#include "anchor_remote_collision.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void no_push(const AnchorCollisionBody *local,
                    const AnchorCollisionBody *remote, float x, float z)
{
    AnchorCollisionVec3 push = anchor_collision_push(local, remote, x, z);
    assert(push.x == 0.0f && push.y == 0.0f && push.z == 0.0f);
}

int main(void)
{
    AnchorCollisionBody local = {{10.02f, 0.0f, 0.0f}, 5.0f, 20.0f};
    AnchorCollisionBody remote = {{0.0f, 0.0f, 0.0f}, 5.0f, 20.0f};
    AnchorCollisionVec3 push;
    AnchorCollisionVec3 reverse;

    push = anchor_collision_push(&local, &remote, 2.0f, 0.0f);
    assert(fabsf(push.x - 1.0f) < 0.0001f && push.z == 0.0f);
    reverse = anchor_collision_push(&remote, &local, -2.0f, 0.0f);
    assert(fabsf(reverse.x + push.x) < 0.0001f);

    /* Releasing, retreating or sliding alongside a contact applies no force. */
    no_push(&local, &remote, 0.0f, 0.0f);
    no_push(&local, &remote, -3.0f, 0.0f);
    no_push(&local, &remote, 0.0f, 3.0f);
    no_push(&local, &remote, 500.0f, 0.0f);
    no_push(&local, &remote, NAN, 0.0f);

    local.position.x = 10.3f;
    no_push(&local, &remote, 2.0f, 0.0f);
    local.position.x = 10.02f;
    local.position.y = 20.0f;
    no_push(&local, &remote, 2.0f, 0.0f);
    local.position.y = -20.0f;
    no_push(&local, &remote, 2.0f, 0.0f);
    local.position.y = 0.0f;

    push = anchor_collision_push(&local, &remote, 10.0f, 0.0f);
    assert(fabsf(push.x - 1.5f) < 0.0001f);
    local.radius = remote.radius = 1.25f;
    local.position.x = 2.52f;
    push = anchor_collision_push(&local, &remote, 2.0f, 0.0f);
    assert(fabsf(push.x - 0.375f) < 0.0001f);
    local.position.x = 0.0f;
    no_push(&local, &remote, 2.0f, 0.0f);
    puts("Player pushing tests passed");
    return 0;
}
