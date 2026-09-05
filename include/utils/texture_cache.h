#ifndef MNSG_TEXTURE_CACHE_H
#define MNSG_TEXTURE_CACHE_H

/* Renderer-visible textures remain immutable while referenced by a model or
 * either of the native double-buffered display lists. Entries describe fixed
 * original-RDRAM pages; the number of players sharing a page is unrestricted. */
typedef struct MnsgTextureCacheEntry
{
    unsigned int resource;
    unsigned int references;
    unsigned short released_frame;
} MnsgTextureCacheEntry;

int mnsg_texture_cache_find(const MnsgTextureCacheEntry *entries, int count,
                             unsigned int resource);
int mnsg_texture_cache_victim(const MnsgTextureCacheEntry *entries, int count,
                               unsigned short frame);
void mnsg_texture_cache_release(MnsgTextureCacheEntry *entry,
                                 unsigned short frame);

#endif
