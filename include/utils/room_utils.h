#ifndef MNSG_ROOM_UTILS_H
#define MNSG_ROOM_UTILS_H

/**
 * Return non-zero when a user-facing room ID contains a private value.
 * Whitespace and the reserved `mnsg-` namespace by itself are invalid.
 */
int mnsg_room_id_is_valid(const char *room_id);

#endif
