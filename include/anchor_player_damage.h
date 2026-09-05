#ifndef ANCHOR_PLAYER_DAMAGE_H
#define ANCHOR_PLAYER_DAMAGE_H

/* Run ordinary native hit intake on the real local player. Returns 1 when
 * the hit caused a native reaction (including an armour-blocked hit), or 0
 * when the player cannot currently accept a hit. */
int anchor_player_damage_apply(float hit_x, float hit_y, float hit_z);

#endif
