#ifndef ANCHOR_FLAG_CATALOG_H
#define ANCHOR_FLAG_CATALOG_H

typedef struct
{
    const char *key;
    const char *display;
    int force_val;
} AnchorFlagEntry;

int anchor_flag_catalog_count(void);
const AnchorFlagEntry *anchor_flag_catalog_get(int index);
const char *anchor_flag_catalog_find_display(const char *key);

#endif
