#include "utils/room_utils.h"

static int is_ascii_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

int mnsg_room_id_is_valid(const char *room_id)
{
    const char *start;
    const char *end;
    unsigned int length;

    if (!room_id)
        return 0;

    start = room_id;
    while (is_ascii_space(*start))
        ++start;
    end = start;
    while (*end)
        ++end;
    while (end > start && is_ascii_space(end[-1]))
        --end;

    length = (unsigned int)(end - start);
    if (length == 0u)
        return 0;
    if (length == 5u &&
        start[0] == 'm' && start[1] == 'n' && start[2] == 's' &&
        start[3] == 'g' && start[4] == '-')
        return 0;
    return 1;
}
