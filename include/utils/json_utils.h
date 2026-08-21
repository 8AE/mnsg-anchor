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

/** Close and null-terminate the object. */
int mnsg_json_writer_finish(MnsgJsonObjectWriter *writer);

#endif
