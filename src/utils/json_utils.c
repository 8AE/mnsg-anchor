#include "utils/json_utils.h"
#include "utils/string_utils.h"

static int is_json_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int is_json_value_end(char c)
{
    return c == '\0' || c == ',' || c == '}' || c == ']';
}

/* Preserve the old packet parser's whole-document lookup semantics: compact
 * team state may place progression fields inside a nested `state` object. */
static const char *find_value(const char *json, const char *key)
{
    const char *p;

    if (!json || !key || !*key)
        return 0;

    for (p = json; *p; ++p)
    {
        const char *candidate;
        const char *wanted;

        if (*p != '"')
            continue;
        candidate = p + 1;
        wanted = key;
        while (*wanted && *candidate == *wanted)
        {
            ++candidate;
            ++wanted;
        }
        if (*wanted || *candidate != '"')
            continue;
        ++candidate;
        while (is_json_space(*candidate))
            ++candidate;
        if (*candidate++ != ':')
            continue;
        while (is_json_space(*candidate))
            ++candidate;
        return candidate;
    }
    return 0;
}

int mnsg_json_string_equals(const char *json, const char *key,
                            const char *expected)
{
    const char *value = find_value(json, key);

    if (!value || !expected || *value++ != '"')
        return 0;
    while (*expected && *value == *expected)
    {
        ++value;
        ++expected;
    }
    return !*expected && *value == '"';
}

int mnsg_json_get_string(const char *json, const char *key,
                         char *out, unsigned int out_size)
{
    const char *value = find_value(json, key);
    unsigned int written = 0;

    if (!value || !out || out_size == 0 || *value++ != '"')
        return 0;

    while (*value && *value != '"')
    {
        char c = *value++;

        if (c == '\\')
        {
            c = *value++;
            if (c == 'b')
                c = '\b';
            else if (c == 'f')
                c = '\f';
            else if (c == 'n')
                c = '\n';
            else if (c == 'r')
                c = '\r';
            else if (c == 't')
                c = '\t';
            else if (c != '"' && c != '\\' && c != '/')
                return 0;
        }
        if (written + 1u >= out_size)
        {
            out[0] = '\0';
            return 0;
        }
        out[written++] = c;
    }
    if (*value != '"')
    {
        out[0] = '\0';
        return 0;
    }
    out[written] = '\0';
    return 1;
}

int mnsg_json_get_s32(const char *json, const char *key, signed int *out)
{
    const char *value = find_value(json, key);

    if (!value || !mnsg_parse_s32(&value, out))
        return 0;
    while (is_json_space(*value))
        ++value;
    return is_json_value_end(*value);
}

int mnsg_json_get_u32(const char *json, const char *key, unsigned int *out)
{
    const char *value = find_value(json, key);
    unsigned int parsed = 0;

    if (!value || !out || *value < '0' || *value > '9')
        return 0;
    while (*value >= '0' && *value <= '9')
    {
        unsigned int digit = (unsigned int)(*value - '0');

        if (parsed > (4294967295u - digit) / 10u)
            return 0;
        parsed = parsed * 10u + digit;
        ++value;
    }
    while (is_json_space(*value))
        ++value;
    if (!is_json_value_end(*value))
        return 0;
    *out = parsed;
    return 1;
}

int mnsg_json_get_u16(const char *json, const char *key, unsigned short *out)
{
    unsigned int value;

    if (!out || !mnsg_json_get_u32(json, key, &value) || value > 0xFFFFu)
        return 0;
    *out = (unsigned short)value;
    return 1;
}

static int writer_append_char(MnsgJsonObjectWriter *writer, char c)
{
    if (!writer || !writer->valid || writer->finished ||
        !writer->cursor || !writer->end || writer->cursor >= writer->end - 1)
    {
        if (writer)
            writer->valid = 0;
        return 0;
    }
    *writer->cursor++ = c;
    return 1;
}

static int writer_append_text(MnsgJsonObjectWriter *writer, const char *text)
{
    if (!writer)
        return 0;
    if (!text)
    {
        writer->valid = 0;
        return 0;
    }
    while (*text)
    {
        if (!writer_append_char(writer, *text++))
            return 0;
    }
    return 1;
}

static int writer_append_quoted(MnsgJsonObjectWriter *writer, const char *text)
{
    if (!writer)
        return 0;
    if (!text)
    {
        writer->valid = 0;
        return 0;
    }
    if (!writer_append_char(writer, '"'))
        return 0;
    while (*text)
    {
        unsigned char c = (unsigned char)*text++;

        if (c == '"' || c == '\\')
        {
            if (!writer_append_char(writer, '\\'))
                return 0;
        }
        else if (c < 0x20u)
        {
            writer->valid = 0;
            return 0;
        }
        if (!writer_append_char(writer, (char)c))
            return 0;
    }
    return writer_append_char(writer, '"');
}

static int writer_begin_entry(MnsgJsonObjectWriter *writer, const char *key)
{
    if (!writer || !writer->valid || writer->finished)
        return 0;
    if (writer->has_entries && !writer_append_char(writer, ','))
        return 0;
    if (!writer_append_quoted(writer, key) ||
        !writer_append_char(writer, ':'))
        return 0;
    return 1;
}

void mnsg_json_writer_begin(MnsgJsonObjectWriter *writer,
                            char *buffer, unsigned int buffer_size)
{
    if (!writer)
        return;

    writer->cursor = buffer;
    writer->end = buffer ? buffer + buffer_size : 0;
    writer->has_entries = 0;
    writer->valid = buffer && buffer_size >= 3u;
    writer->finished = 0;
    if (buffer && buffer_size > 0u)
        buffer[0] = '\0';
    if (writer->valid)
        writer_append_char(writer, '{');
}

int mnsg_json_writer_add_string(MnsgJsonObjectWriter *writer,
                                const char *key, const char *value)
{
    if (!writer_begin_entry(writer, key) ||
        !writer_append_quoted(writer, value))
        return 0;
    writer->has_entries = 1;
    return 1;
}

int mnsg_json_writer_add_s32(MnsgJsonObjectWriter *writer,
                             const char *key, signed int value)
{
    char value_text[12];

    if (!writer_begin_entry(writer, key))
        return 0;
    mnsg_string_write_s32(value_text, value);
    if (!writer_append_text(writer, value_text))
        return 0;
    writer->has_entries = 1;
    return 1;
}

int mnsg_json_writer_finish(MnsgJsonObjectWriter *writer)
{
    if (!writer || !writer->valid || writer->finished ||
        !writer_append_char(writer, '}'))
        return 0;
    *writer->cursor = '\0';
    writer->finished = 1;
    return 1;
}
