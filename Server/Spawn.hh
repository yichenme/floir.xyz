#pragma once

#include <Shared/Entity.hh>

#include <functional>

class Simulation;

/* rarity for a mob spawned as a side effect of another entity (anthole
waves, digger spawns, queen ant reinforcements, petal-summoned mobs):
rolls off the parent's zone if it was itself a zone spawn, else inherits
the parent's mob rarity, else falls back to Common. */
uint8_t inherited_spawn_rarity(Entity const &parent);

Entity &alloc_drop(Simulation *, PetalID::T, uint8_t rarity, uint32_t owner = 0);
Entity &alloc_mob(
    Simulation *, MobID::T, float, float, 
    EntityID const, uint8_t rarity = 0,
    std::function<void(Entity &)> = nullptr
);
Entity &alloc_player(Simulation *, EntityID const);
Entity &alloc_petal(Simulation *, PetalID::T, Entity const &, uint8_t rarity);
Entity &alloc_web(Simulation *, float, Entity const &);
Entity &alloc_cpu_camera(Simulation *, EntityID const);

void player_spawn(Simulation *, Entity &, Entity &);