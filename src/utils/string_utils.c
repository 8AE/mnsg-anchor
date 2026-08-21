#include "utils/string_utils.h"

static int is_ascii_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

const char *mnsg_string_find(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return 0;
    if (!*needle)
        return haystack;

    for (; *haystack; ++haystack)
    {
        const char *hay = haystack;
        const char *wanted = needle;

        while (*wanted && *hay == *wanted)
        {
            ++hay;
            ++wanted;
        }
        if (!*wanted)
            return haystack;
    }
    return 0;
}

int mnsg_string_equal(const char *left, const char *right)
{
    if (!left || !right)
        return left == right;

    while (*left && *left == *right)
    {
        ++left;
        ++right;
    }
    return *left == *right;
}

int mnsg_string_starts_with(const char *text, const char *prefix)
{
    if (!text || !prefix)
        return 0;

    while (*prefix)
    {
        if (*text++ != *prefix++)
            return 0;
    }
    return 1;
}

int mnsg_string_is_blank(const char *text)
{
    if (!text)
        return 1;

    while (*text)
    {
        if (!is_ascii_space(*text++))
            return 0;
    }
    return 1;
}

int mnsg_parse_s32(const char **cursor, signed int *out)
{
    const char *start;
    const char *p;
    unsigned int magnitude = 0;
    unsigned int limit;
    int negative = 0;

    if (!cursor || !*cursor || !out)
        return 0;

    start = *cursor;
    p = start;
    if (*p == '-')
    {
        negative = 1;
        ++p;
    }
    if (*p < '0' || *p > '9')
        return 0;

    limit = negative ? 2147483648u : 2147483647u;
    while (*p >= '0' && *p <= '9')
    {
        unsigned int digit = (unsigned int)(*p - '0');

        if (magnitude > (limit - digit) / 10u)
        {
            *cursor = start;
            return 0;
        }
        magnitude = magnitude * 10u + digit;
        ++p;
    }

    if (negative)
    {
        if (magnitude == 2147483648u)
            *out = (-2147483647 - 1);
        else
            *out = -(signed int)magnitude;
    }
    else
    {
        *out = (signed int)magnitude;
    }
    *cursor = p;
    return 1;
}

signed int mnsg_string_to_s32(const char *text, signed int fallback)
{
    const char *cursor = text;
    signed int value;

    if (!cursor)
        return fallback;
    while (is_ascii_space(*cursor))
        ++cursor;
    if (!mnsg_parse_s32(&cursor, &value))
        return fallback;
    while (is_ascii_space(*cursor))
        ++cursor;
    return *cursor ? fallback : value;
}

char *mnsg_string_append_s32(char *out, signed int value)
{
    char digits[10];
    int count = 0;
    unsigned int magnitude;

    if (value < 0)
    {
        *out++ = '-';
        magnitude = 0u - (unsigned int)value;
    }
    else
    {
        magnitude = (unsigned int)value;
    }

    do
    {
        digits[count++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u);

    while (count-- > 0)
        *out++ = digits[count];
    return out;
}

void mnsg_string_write_s32(char *out, signed int value)
{
    char *end = mnsg_string_append_s32(out, value);
    *end = '\0';
}
