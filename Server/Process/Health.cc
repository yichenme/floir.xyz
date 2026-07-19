#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>

#include <iostream>

void tick_health_behavior(Simulation *sim, Entity &ent) {
    ent.set_damaged(0);
    if (ent.poison_ticks > 0 && !ent.has_component(kPetal)) {
        ent.poison_ticks--;
        inflict_damage(sim, ent.poison_dealer, ent.id, ent.poison_inflicted, DamageType::kPoison);
        if (ent.poison_ticks % (TPS / 2) != 0) ent.set_damaged(0);
    } else {
        ent.poison_inflicted = 0;
        ent.poison_dealer = NULL_ENTITY;
    }
    if (ent.dandy_ticks > 0) --ent.dandy_ticks;
    if (ent.health <= 0) {
        // A real player becomes an immobile corpse instead of vanishing; a corpse
        // (dead flower) is already handled and must NOT be deleted here -- only
        // kLeave/cleanup removes it. Everything else dies normally.
        if (ent.has_component(kFlower) && !ent.has_component(kMob) && !ent.get_dead())
            enter_player_dead_state(sim, ent);
        else if (!(ent.has_component(kFlower) && ent.get_dead()))
            sim->request_delete(ent.id);
    }
    if (ent.max_health == 0) return;
    ent.set_health_ratio(ent.health / ent.max_health);
}