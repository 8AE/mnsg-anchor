#include <assert.h>
#include <stdio.h>
#include "utils/texture_cache.h"

int main(void)
{
    MnsgTextureCacheEntry entries[2] = {{7, 1000, 0}, {8, 1, 0}};
    int i;
    assert(mnsg_texture_cache_find(entries, 2, 7) == 0);
    assert(mnsg_texture_cache_find(entries, 2, 0) == -1);
    assert(mnsg_texture_cache_victim(entries, 2, 100) == -1);
    for (i = 0; i < 999; ++i)
        mnsg_texture_cache_release(&entries[0], 20);
    assert(entries[0].references == 1);
    assert(mnsg_texture_cache_victim(entries, 2, 100) == -1);
    mnsg_texture_cache_release(&entries[0], 65535);
    assert(mnsg_texture_cache_victim(entries, 2, 65535) == -1);
    assert(mnsg_texture_cache_victim(entries, 2, 0) == -1);
    assert(mnsg_texture_cache_victim(entries, 2, 1) == 0);
    mnsg_texture_cache_release(&entries[0], 1);
    assert(entries[0].references == 0);
    assert(entries[0].released_frame == 65535);
    entries[1].resource = 0;
    entries[1].references = 0;
    assert(mnsg_texture_cache_victim(entries, 2, 1) == 1);
    puts("texture cache tests passed");
    return 0;
}
