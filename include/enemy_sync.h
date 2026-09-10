#ifndef MNSG_ENEMY_SYNC_H
#define MNSG_ENEMY_SYNC_H

/* Handle one packet already dequeued by item_sync's sole Anchor poller.
 * Returns non-zero when the packet belongs to enemy synchronization. */
int enemy_sync_handle_packet(const char *json);

/* Flush compact outgoing enemy state and retry failed sends. */
void enemy_sync_update(void);

/* End networking while preserving the currently loaded room's actor roster. */
void enemy_sync_disconnect(void);

/* Clear the current occupied-room epoch and all pending network work. */
void enemy_sync_reset(void);

#endif
