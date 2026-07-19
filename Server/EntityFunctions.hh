#pragma once

#include <Shared/EntityDef.hh>

class Simulation;
class Entity;
class EntityID;

void inflict_damage(Simulation *, EntityID const, EntityID const, float, uint8_t);
void inflict_heal(Simulation *, Entity &, float);

void entity_on_death(Simulation *, Entity const &);

// A real player (kFlower && !kMob) becomes an immobile synced corpse instead of
// being deleted on lethal damage. Runs the one-time death bookkeeping (via
// entity_on_death), clears petals, and sets Flower.dead.
void enter_player_dead_state(Simulation *, Entity &);

EntityID find_nearest_enemy(Simulation *, Entity const &, float, bool mobs_only = false);

void entity_set_despawn_tick(Entity &, game_tick_t);
void entity_clear_references(Simulation *, Entity &);
