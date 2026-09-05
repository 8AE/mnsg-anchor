#ifndef MNSG_JSON_UTILS_H
#define MNSG_JSON_UTILS_H

/** A small bounded writer for the compact JSON objects used by Anchor. */
typedef struct
{
    char *cursor;
    char *end;
    int has_entries;
    int valid;
    int finished;
} MnsgJsonObjectWriter;

/* Advance through an array of objects, skipping braces inside JSON strings.
 * end points at the object's closing brace, which callers may temporarily
 * replace with NUL to bound field lookup; restore it before freeing the JSON. */
char *mnsg_json_next_object(char **cursor, char **end);

/* Find a field's value, skipping quoted string contents; key is unquoted.
 * To restrict lookup to one object, terminate it at the boundary above. */
const char *mnsg_json_find_value(const char *json, const char *key);

/* Copy a bounded display label, retaining the roster's legacy escape handling
 * and truncation behavior. Always terminate out when out_size is nonzero. */
void mnsg_json_copy_display_string(const char *json, const char *key,
                                   char *out, unsigned int out_size);

/** Return non-zero when the first matching string field equals expected. */
int mnsg_json_string_equals(const char *json, const char *key,
                            const char *expected);

/** Read the first matching string field anywhere in the JSON document. */
int mnsg_json_get_string(const char *json, const char *key,
                         char *out, unsigned int out_size);

/** Read the first matching signed 32-bit integer field in the document. */
int mnsg_json_get_s32(const char *json, const char *key, signed int *out);

/** Read the first matching unsigned 32-bit integer field in the document. */
int mnsg_json_get_u32(const char *json, const char *key, unsigned int *out);

/** Read and range-check the first matching unsigned 16-bit integer field. */
int mnsg_json_get_u16(const char *json, const char *key, unsigned short *out);

/** Begin a compact JSON object in buffer. */
void mnsg_json_writer_begin(MnsgJsonObjectWriter *writer,
                            char *buffer, unsigned int buffer_size);

/** Add a quoted string property. Control characters are rejected. */
int mnsg_json_writer_add_string(MnsgJsonObjectWriter *writer,
                                const char *key, const char *value);

/** Add a signed decimal integer property. */
int mnsg_json_writer_add_s32(MnsgJsonObjectWriter *writer,
                             const char *key, signed int value);

/** Add an unsigned decimal integer without losing high color/flag bits. */
int mnsg_json_writer_add_u32(MnsgJsonObjectWriter *writer,
                             const char *key, unsigned int value);

/** Close and null-terminate the object. */
int mnsg_json_writer_finish(MnsgJsonObjectWriter *writer);

#endif
