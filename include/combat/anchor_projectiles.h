#ifndef ANCHOR_PROJECTILES_H
#define ANCHOR_PROJECTILES_H

#define ANCHOR_PROJECTILE_REMOTE_MAX 64
#define ANCHOR_PROJECTILE_BATCH_MAX 16
#define ANCHOR_PROJECTILE_JSON_SIZE 512

/* One throw, not a stream of display state. Velocity is world units per
 * native 30 Hz tick times 100; rotations retain signed billboard sentinels. */
typedef struct AnchorProjectileSpawn
{
    int id, kind;
    int x100, y100, z100;
    int vx100, vy100, vz100;
    int rx, ry, rz;
    int scale100000;
} AnchorProjectileSpawn;

typedef struct AnchorProjectileRemote
{
    AnchorProjectileSpawn spawn;
    int cid, session, epoch, age_ms;
} AnchorProjectileRemote;

int anchor_projectile_spawn_valid(const AnchorProjectileSpawn *spawn);
int anchor_projectile_spawn_encode(const AnchorProjectileSpawn *spawn,
                                    char *out, unsigned int capacity);
int anchor_projectile_spawns_decode(const char *json, AnchorProjectileRemote *out,
                                     int capacity);

#endif
