#ifndef MNSG_ARRAY_UTILS_H
#define MNSG_ARRAY_UTILS_H

/* Grow a mod-owned array geometrically and zero new elements. On failure,
 * leave both data and capacity unchanged. No pointer into data may survive a
 * successful growth; native callback records must use stable separate storage.
 * The caller owns the allocation and releases it with recomp_free(). */
int mnsg_array_reserve(void **data, int *capacity, int needed,
                       unsigned int element_size);

#endif
