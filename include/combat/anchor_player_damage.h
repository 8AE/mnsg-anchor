#ifndef ANCHOR_PLAYER_DAMAGE_H
#define ANCHOR_PLAYER_DAMAGE_H

/* Run ordinary native hit intake on the real local player. Returns 1 when
 * the hit caused a native reaction (including an armour-blocked hit), or 0
 * when the player cannot currently accept a hit. */
int anchor_player_damage_apply(float hit_x, float hit_y, float hit_z, int damage);
/* Reports only native hit HP loss, before No Hit's additional death penalty. */
int anchor_player_damage_apply_with_loss(float hit_x, float hit_y, float hit_z,
                                         int damage, unsigned int *health_loss);
/* Native capped healing for the guarded, living local Ebisumaru. */
unsigned int anchor_player_damage_life_steal(unsigned int amount);

#endif
