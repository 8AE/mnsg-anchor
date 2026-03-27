#ifndef __RECOMPUTILS_H__
#define __RECOMPUTILS_H__

#include "modding.h"

RECOMP_IMPORT("*", void *recomp_alloc(unsigned long size));
RECOMP_IMPORT("*", void recomp_free(void *memory));
RECOMP_IMPORT("*", int recomp_printf(const char *fmt, ...));
/* Returns the absolute path of this mod's .nrm file as a heap-allocated string.
 * The caller must free the result with recomp_free(). Used by repy_api.h. */
RECOMP_IMPORT("*", unsigned char *recomp_get_mod_file_path());

/* Provide NULL for use without standard library headers. */
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif
