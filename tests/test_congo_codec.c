#include "utils/anchor_congo_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char encoded[ANCHOR_CONGO_STATE_JSON_SIZE];
static char response[ANCHOR_CONGO_STATUS_JSON_SIZE];
static AnchorCongoNativeSnapshot native;
static AnchorCongoStatus decoded;

static void wrap(const char *state, const char *hits)
{
    snprintf(response, sizeof(response),
        "{\"role\":1,\"owner\":10,\"term\":2,\"revision\":7,\"paused\":0,"
        "\"encounter\":[10,11,12],\"state\":%s,\"hits\":%s}", state, hits);
}
int main(void)
{
    unsigned i, j;
    char small[4] = {'a','b','c','d'};
    for(i=0;i<24;i++) native.root[i]=0xffffffffu-i;
    for(i=0;i<6;i++) for(j=0;j<4;j++) native.part[i][j]=i*4+j;
    native.tick=1234; native.spin_serial=9876; native.flame_count=32;
    for(i=0;i<32;i++) for(j=0;j<7;j++) native.flame[i][j]=0xffffffffu-i-j;
    assert(anchor_congo_state_encode(&native,encoded,sizeof(encoded)));
    assert(strlen(encoded)<4096);
    wrap(encoded,"[[1,2,3,4,8],[5,6,7,8,1]]");
    assert(anchor_congo_status_decode(response,&decoded));
    assert(decoded.has_state && decoded.state.flame_count==32 && decoded.hit_count==2);
    assert(memcmp(&native,&decoded.state,sizeof(native))==0);
    assert(!anchor_congo_state_encode(&native,small,3));
    assert(small[0]==0 && small[3]=='d');
    native.flame_count=33;
    assert(!anchor_congo_state_encode(&native,encoded,sizeof(encoded)));
    native.flame_count=0;
    assert(anchor_congo_state_encode(&native,encoded,sizeof(encoded)));
    wrap(encoded,"[]");
    assert(anchor_congo_status_decode(response,&decoded));
    assert(decoded.state.flame_count==0);
    wrap("null","[]");
    assert(anchor_congo_status_decode(response,&decoded) && !decoded.has_state);
    wrap(encoded,"[[1,2,3,4,9]]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap(encoded,"[[1,2,3,0,1]]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap(encoded,"[[1,2,3,4,1],]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap(encoded,"[]");
    strcat(response,"x");
    assert(!anchor_congo_status_decode(response,&decoded));
    assert(!anchor_congo_status_decode("{\"role\":1,\"role\":2}",&decoded));
    wrap("{\"r\":[4294967296]}","[]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap("{\"r\":[-1]}","[]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap("{\"r\":[01]}","[]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap("{\"r\":[1.0]}","[]");
    assert(!anchor_congo_status_decode(response,&decoded));
    wrap(encoded,"[]");
    response[strlen(response)-1]=',';
    strcat(response,"\"owner\":999}");
    assert(!anchor_congo_status_decode(response,&decoded));
    puts("Congo codec tests passed");
}
