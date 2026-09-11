#include "utils/anchor_impact_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char encoded[ANCHOR_IMPACT_STATE_JSON_SIZE];
static char response[ANCHOR_IMPACT_STATUS_JSON_SIZE];
static AnchorImpactNativeSnapshot native;
static AnchorImpactStatus decoded;

static void wrap(const char *state, const char *hits)
{
    snprintf(response, sizeof(response),
        "{\"role\":1,\"owner\":10,\"term\":2,\"revision\":7,\"paused\":0,"
        "\"encounter\":[10,11,12],\"state\":%s,\"hits\":%s}", state, hits);
}

int main(void)
{
    unsigned int i;
    char small[4] = {'a', 'b', 'c', 'd'};
    for (i = 0; i < ANCHOR_IMPACT_ROOT_WORDS; ++i)
        native.root[i] = 0xffffffffu - i;
    native.root[IMP_BOSS_HP] = 2000;
    native.root[IMP_AMMO] = 100;
    native.root[IMP_MECH_HP] = 999;
    native.encounter = 3;
    native.stage = 0x0222;
    assert(anchor_impact_state_encode(&native, encoded, sizeof(encoded)));
    assert(strlen(encoded) < ANCHOR_IMPACT_STATE_JSON_SIZE);
    wrap(encoded, "[[1,2,3,4,7],[5,6,7,8,255]]");
    assert(anchor_impact_status_decode(response, &decoded));
    assert(decoded.has_state && decoded.hit_count == 2);
    assert(memcmp(&native, &decoded.state, sizeof(native)) == 0);
    assert(!anchor_impact_state_encode(&native, small, 3));
    assert(small[0] == 0 && small[3] == 'd');
    assert(!anchor_impact_state_encode(0, small, sizeof(small)));

    wrap(encoded, "[]");
    assert(anchor_impact_status_decode(response, &decoded));
    assert(decoded.state.encounter == 3 && decoded.state.stage == 0x0222);

    wrap("null", "[]");
    assert(anchor_impact_status_decode(response, &decoded) &&
           !decoded.has_state);

    /* Malformed hit rows and states are rejected before native use. */
    wrap(encoded, "[[1,2,3,4,0]]");
    assert(!anchor_impact_status_decode(response, &decoded));
    wrap(encoded, "[[1,2,3,4,256]]");
    assert(!anchor_impact_status_decode(response, &decoded));
    wrap(encoded, "[[1,2,3,4,7],]");
    assert(!anchor_impact_status_decode(response, &decoded));
    wrap("{\"r\":[1],\"k\":1,\"s\":544}", "[]");
    assert(!anchor_impact_status_decode(response, &decoded));
    wrap("{\"r\":[],\"k\":1}", "[]");
    assert(!anchor_impact_status_decode(response, &decoded));
    wrap("{\"r\":[4294967296],\"k\":1,\"s\":544}", "[]");
    assert(!anchor_impact_status_decode(response, &decoded));
    wrap("{\"r\":[-1],\"k\":1,\"s\":544}", "[]");
    assert(!anchor_impact_status_decode(response, &decoded));

    wrap(encoded, "[]");
    strcat(response, "x");
    assert(!anchor_impact_status_decode(response, &decoded));
    assert(!anchor_impact_status_decode("{\"role\":1,\"role\":2}", &decoded));

    puts("Impact codec tests passed");
    return 0;
}
