#include <Server/EntityFunctions.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>

EntityID find_nearest_enemy(Simulation *simulation, Entity const &entity, float radius, bool mobs_only) {
    // Aggro-scan throttle: this spatial-hash query is the dominant AI tick
    // cost (run per targetless mob), so stagger it by entity id and only
    // actually scan once every TPS/2 ticks (0.5s) instead of TPS/5 (0.2s) --
    // ~2.5x fewer queries. A targetless mob just notices a nearby enemy up to
    // 0.5s later; once it has a target it stops scanning entirely.
    if ((entity.id.id - entity.lifetime) % (TPS / 2) != 0) return NULL_ENTITY;
    if (entity.immunity_ticks > 0) return NULL_ENTITY;
    EntityID ret;
    float min_dist = radius;
    simulation->spatial_hash.query(entity.get_x(), entity.get_y(), radius, radius, [&](Simulation *sim, Entity &ent){
        if (!sim->ent_alive(ent.id)) return;
        if (ent.get_team() == entity.get_team()) return;
        if (ent.immunity_ticks > 0) return;
        if (mobs_only) {
            if (!ent.has_component(kMob)) return;
        } else {
            if (!ent.has_component(kMob) && !ent.has_component(kFlower)) return;
            // A dead player is an inert corpse: never a valid target, so mobs and
            // summons don't swarm/farm it.
            if (ent.has_component(kFlower) && !ent.has_component(kMob) && ent.get_dead()) return;
            // A player hidden in a tunnel can't be newly acquired as a target.
            if (ent.has_component(kFlower) && !ent.has_component(kMob) && ent.hidden) return;
        }
        if (sim->ent_alive(entity.get_parent())) {
            Entity &parent = sim->get_ent(entity.get_parent());
            float dist = Vector(ent.get_x()-parent.get_x(),ent.get_y()-parent.get_y()).magnitude();
            if (dist > SUMMON_RETREAT_RADIUS) return;
        }
        float dist = Vector(ent.get_x()-entity.get_x(),ent.get_y()-entity.get_y()).magnitude();
        if (dist < min_dist) { min_dist = dist; ret = ent.id; }
    });
    return ret;
}
