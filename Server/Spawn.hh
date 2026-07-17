#pragma once

#include <Shared/Entity.hh>

#include <functional>

class Simulation;

Entity &alloc_drop(Simulation *, PetalID::T, uint8_t rarity);
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