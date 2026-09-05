#include "utils/array_utils.h"

#ifndef MNSG_ARRAY_UTILS_HOST_TEST
#include "recomputils.h"
#else
extern void *recomp_alloc(unsigned long size);
extern void recomp_free(void *memory);
#endif

int mnsg_array_reserve(void **data, int *capacity, int needed,
                       unsigned int element_size)
{
    unsigned int limit;
    unsigned int grown;
    unsigned int old_bytes;
    unsigned int bytes;
    unsigned int i;
    volatile unsigned char *replacement;
    const volatile unsigned char *previous;

    if (!data || !capacity || needed < 0 || *capacity < 0 || !element_size ||
        (*capacity && !*data))
        return 0;
    limit = 0x7fffffffu / element_size;
    if ((unsigned int)needed > limit || (unsigned int)*capacity > limit)
        return 0;
    if (needed <= *capacity)
        return 1;
    grown = *capacity ? (unsigned int)*capacity : 1u;
    while (grown < (unsigned int)needed)
    {
        if (grown > limit / 2u)
        {
            grown = (unsigned int)needed;
            break;
        }
        grown *= 2u;
    }
    bytes = grown * element_size;
    old_bytes = (unsigned int)*capacity * element_size;
    replacement = recomp_alloc(bytes);
    if (!replacement)
        return 0;
    previous = *data;
    /* Volatile accesses prevent freestanding MIPS builds from introducing
     * unavailable memcpy/memset library calls. */
    for (i = 0; i < old_bytes; ++i)
        replacement[i] = previous[i];
    for (; i < bytes; ++i)
        replacement[i] = 0;
    if (*data)
        recomp_free(*data);
    *data = (void *)replacement;
    *capacity = (int)grown;
    return 1;
}
