#include <Server/EntityFunctions.hh>

#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>

#include <cmath>

static bool _yggdrasil_revival_clause(Simulation *sim, Entity &player) {
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        if (!player.loadout[i].already_spawned) continue;
        if (player.loadout[i].get_petal_id() != PetalID::kYggdrasil) continue;
        player.set_loadout_ids(i, PetalID::kNone);
        player.set_loadout_rarities(i, 0);
        return true;
    }
    return false;
}

void inflict_damage(Simulation *sim, EntityID const atk_id, EntityID const def_id, float amt, uint8_t type) {
    if (amt <= 0) return;
    if (!sim->ent_alive(def_id)) return;
    Entity &defender = sim->get_ent(def_id);
    if (!defender.has_component(kHealth)) return;
    DEBUG_ONLY(assert(!defender.pending_delete);)
    DEBUG_ONLY(assert(defender.has_component(kHealth));)
    if (defender.immunity_ticks > 0) return;
    if (type == DamageType::kContact) amt -= defender.armor;
    else if (type == DamageType::kPoison) amt -= defender.poison_armor;
    if (amt <= 0) return;
    //if (amt <= defender.armor) return;
    float old_health = defender.health;
    defender.set_damaged(1);
    defender.health = fclamp(defender.health - amt, 0, defender.health);  
    float damage_dealt = old_health - defender.health;
    // Individual-loot: attribute damage on a mob to the attacking player's
    // camera (persistent identity), so on death we can pick the eligible looters.
    if (damage_dealt > 0 && defender.has_component(kMob) && sim->ent_alive(atk_id)) {
        Entity &atk = sim->get_ent(atk_id);
        if (sim->ent_alive(atk.base_entity)) {
            Entity &owner = sim->get_ent(atk.base_entity);
            if (owner.has_component(kFlower) && sim->ent_alive(owner.get_parent()))
                defender.mob_damage[owner.get_parent().id] += damage_dealt;
        }
    }
    //ant hole spawns
    //floor start, ceil end
    if (defender.has_component(kMob) && defender.get_mob_id() == MobID::kAntHole) {
        uint32_t const num_waves = ANTHOLE_SPAWNS.size() - 1;
        uint32_t start = ceilf((defender.max_health - old_health) / defender.max_health * num_waves);
        uint32_t end = ceilf((defender.max_health - defender.health) / defender.max_health * num_waves);
        if (defender.health <= 0) end = num_waves + 1;
        for (uint32_t i = start; i < end; ++i) {
            for (MobID::T mob_id : ANTHOLE_SPAWNS[i]) {
                Entity &child = alloc_mob(
                    sim, mob_id, 
                    defender.get_x(), defender.get_y(), 
                    defender.get_team(), inherited_spawn_rarity(defender), [](Entity &mob) {
                    BitMath::set(mob.flags, EntityFlags::kHasCulling);
                });
                child.set_parent(defender.id);
                child.target = defender.target;
            }
        }
    }
    /* yggdrasil revive clause
    if (defender.health == 0 && defender.has_component(kFlower)) {
        if (_yggdrasil_revival_clause(sim, defender)) {
            defender.health = defender.max_health * 0.25;
            defender.immunity_ticks = 1.0 * TPS;
        }
    }
    */
    if (!sim->ent_exists(atk_id)) return;
    Entity &attacker = sim->get_ent(atk_id);

    if (type != DamageType::kReflect && defender.damage_reflection > 0)
        inflict_damage(sim, def_id, attacker.base_entity, damage_dealt * defender.damage_reflection, DamageType::kReflect);
    
    if (!sim->ent_alive(atk_id)) return;

    if (defender.slow_ticks < attacker.slow_inflict)
        defender.slow_ticks = attacker.slow_inflict;
    
    if (attacker.has_component(kPetal)) {
        switch (attacker.get_petal_id()) {
            case PetalID::kDandelion:
                defender.dandy_ticks = 10 * TPS;
                break;
            default:
                break;
        }
    }

    if (attacker.has_component(kPetal)) {
        if (!sim->ent_alive(defender.target))
            defender.target = attacker.base_entity;
        
    } else {
        if (!sim->ent_alive(defender.target))
            defender.target = atk_id;
    }
    defender.last_damaged_by = attacker.base_entity;

    if (type == DamageType::kContact && defender.poison_ticks < attacker.poison_damage.time * TPS) {
        defender.poison_ticks = attacker.poison_damage.time * TPS;
        defender.poison_inflicted = attacker.poison_damage.damage / TPS;
        defender.poison_dealer = defender.last_damaged_by;
    }
}

void inflict_heal(Simulation *sim, Entity &ent, float amt) {
    DEBUG_ONLY(assert(ent.has_component(kHealth));)
    if (ent.pending_delete || ent.health <= 0) return;
    if (ent.dandy_ticks > 0) return;
    ent.health = fclamp(ent.health + amt, 0, ent.max_health);
}