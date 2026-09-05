#include "utils/texture_cache.h"

int mnsg_texture_cache_find(const MnsgTextureCacheEntry *entries, int count,
                             unsigned int resource)
{
    int i;
    if (!resource)
        return -1;
    for (i = 0; i < count; ++i)
        if (entries[i].resource == resource)
            return i;
    return -1;
}

int mnsg_texture_cache_victim(const MnsgTextureCacheEntry *entries, int count,
                               unsigned short frame)
{
    int i;
    int oldest = -1;
    unsigned int age = 0;
    for (i = 0; i < count; ++i)
    {
        unsigned int elapsed;
        if (entries[i].references)
            continue;
        if (!entries[i].resource)
            return i;
        elapsed = (unsigned short)(frame - entries[i].released_frame);
        if (elapsed >= 2u && (oldest < 0 || elapsed > age))
        {
            oldest = i;
            age = elapsed;
        }
    }
    return oldest;
}

void mnsg_texture_cache_release(MnsgTextureCacheEntry *entry,
                                 unsigned short frame)
{
    if (entry->references && --entry->references == 0)
        entry->released_frame = frame;
}
