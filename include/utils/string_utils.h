#ifndef MNSG_STRING_UTILS_H
#define MNSG_STRING_UTILS_H

/** Locate the first occurrence of needle in haystack, or return 0. */
const char *mnsg_string_find(const char *haystack, const char *needle);

/** Return non-zero when both null-terminated strings are equal. */
int mnsg_string_equal(const char *left, const char *right);

/** Return non-zero when text starts with prefix. */
int mnsg_string_starts_with(const char *text, const char *prefix);

/** Return non-zero when text is null, empty, or only ASCII whitespace. */
int mnsg_string_is_blank(const char *text);

/**
 * Parse one signed decimal integer at *cursor and advance past its digits.
 * The cursor is unchanged on invalid input or 32-bit overflow.
 */
int mnsg_parse_s32(const char **cursor, signed int *out);

/** Parse a complete decimal string, returning fallback when it is invalid. */
signed int mnsg_string_to_s32(const char *text, signed int fallback);

/** Append a signed decimal integer and return the first unwritten byte. */
char *mnsg_string_append_s32(char *out, signed int value);

/** Write a null-terminated signed decimal integer. */
void mnsg_string_write_s32(char *out, signed int value);

#endif
