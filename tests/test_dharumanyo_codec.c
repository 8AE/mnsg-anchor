#include "utils/anchor_dharumanyo_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char encoded[ANCHOR_DHARUMANYO_STATE_JSON_SIZE];
static char response[ANCHOR_DHARUMANYO_STATUS_JSON_SIZE];
static AnchorDharumanyoNativeSnapshot native;
static AnchorDharumanyoStatus decoded;

static void wrap(const char *state, const char *hits)
{
    snprintf(response, sizeof(response),
        "{\"role\":1,\"owner\":10,\"term\":2,\"revision\":7,\"paused\":0,"
        "\"encounter\":[10,11,12],\"state\":%s,\"hits\":%s}", state, hits);
}

int main(void)
{
    unsigned int i, j;
    char small[4] = {'a', 'b', 'c', 'd'};
    for (i = 0; i < ANCHOR_DHARUMANYO_ROOT_WORDS; ++i)
        native.root[i] = 0xffffffffu - i;
    for (i = 0; i < ANCHOR_DHARUMANYO_CARRIER_WORDS; ++i)
        native.carrier[i] = i;
    native.tick = 1234;
    native.projectile_serial = 9876;
    native.projectile_count = ANCHOR_DHARUMANYO_MAX_PROJECTILES;
    for (i = 0; i < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++i)
        for (j = 0; j < ANCHOR_DHARUMANYO_PROJECTILE_WORDS; ++j)
            native.projectile[i][j] = 0xffffffffu - i - j;
    /* Preserve the native special orientation through the C/Python bridge. */
    native.projectile[0][DHAR_PROJECTILE_YAW] = 0x8000u;
    assert(anchor_dharumanyo_state_encode(&native, encoded, sizeof(encoded)));
    assert(strlen(encoded) < ANCHOR_DHARUMANYO_STATE_JSON_SIZE);
    wrap(encoded, "[[1,2,3,4,1],[5,6,7,8,1]]");
    assert(anchor_dharumanyo_status_decode(response, &decoded));
    assert(decoded.has_state &&
           decoded.state.projectile_count == ANCHOR_DHARUMANYO_MAX_PROJECTILES &&
           decoded.hit_count == 2);
    assert(memcmp(&native, &decoded.state, sizeof(native)) == 0);
    assert(!anchor_dharumanyo_state_encode(&native, small, 3));
    assert(small[0] == 0 && small[3] == 'd');
    native.projectile_count = ANCHOR_DHARUMANYO_MAX_PROJECTILES + 1;
    assert(!anchor_dharumanyo_state_encode(&native, encoded, sizeof(encoded)));
    native.projectile_count = 0;
    assert(anchor_dharumanyo_state_encode(&native, encoded, sizeof(encoded)));
    wrap(encoded, "[]");
    assert(anchor_dharumanyo_status_decode(response, &decoded));
    assert(decoded.state.projectile_count == 0);
    wrap("null", "[]");
    assert(anchor_dharumanyo_status_decode(response, &decoded) && !decoded.has_state);
    wrap(encoded, "[[1,2,3,4,2]]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap(encoded, "[[1,2,3,0,1]]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap(encoded, "[[1,2,3,4,1],]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap(encoded, "[]");
    strcat(response, "x");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    assert(!anchor_dharumanyo_status_decode("{\"role\":1,\"role\":2}", &decoded));
    wrap("{\"r\":[4294967296]}", "[]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap("{\"r\":[-1]}", "[]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap("{\"r\":[01]}", "[]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap("{\"r\":[1.0]}", "[]");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    wrap(encoded, "[]");
    response[strlen(response) - 1] = ',';
    strcat(response, "\"owner\":999}");
    assert(!anchor_dharumanyo_status_decode(response, &decoded));
    puts("Dharumanyo codec tests passed");
}
