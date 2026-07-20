#include <Server/EntityFunctions.hh>

#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/StaticDefinitions.hh>

void enter_player_dead_state(Simulation *sim, Entity &flower) {
    // Run the exact death-moment bookkeeping the flower would have gotten on
    // deletion: killer XP + kill credit, kill message, loot-claim purge, and the
    // one-time progress + loadout persist. entity_on_death early-returns for an
    // already-dead corpse (set_dead below), so the later kLeave/cleanup delete
    // never repeats any of it -- persistence happens exactly once.
    entity_on_death(sim, flower);
    // The corpse stays alive, so its orbiting petals won't self-delete on a dead
    // parent (tick_petal_behavior only culls petals whose parent is gone). Remove
    // them explicitly; tick_player_behavior early-outs while dead so the loadout
    // never spawns replacements.
    for (uint32_t i = 0; i < flower.get_loadout_count(); ++i) {
        LoadoutSlot &slot = flower.loadout[i];
        for (uint32_t j = 0; j < slot.size(); ++j)
            if (sim->ent_alive(slot.petals[j].ent_id))
                sim->request_delete(slot.petals[j].ent_id);
        slot.reset();
    }
    flower.set_dead(1);
    flower.health = 0;
    flower.set_health_ratio(0);
    flower.input = 0;
    flower.acceleration.set(0, 0);
    flower.velocity.set(0, 0);
    flower.collision_velocity.set(0, 0);
    // Stop lingering status effects from ticking on the corpse (poison would
    // otherwise keep re-inflicting and drive health_ratio negative each tick).
    flower.poison_ticks = 0;
    flower.slow_ticks = 0;
    flower.dandy_ticks = 0;
    // kDeadEyes drives the corpse face; kDefending unsmiles the mouth (client).
    flower.set_face_flags((1 << FaceFlags::kDeadEyes) | (1 << FaceFlags::kDefending));
    // tick_player_behavior skips the buff recompute (incl. camera.set_fov)
    // entirely while dead, so an equipped Antennae/Observer's widened FOV
    // would otherwise stay frozen at its last pre-death value and keep
    // "working" on the corpse. Reset it to the unbuffed base here.
    if (sim->ent_alive(flower.get_parent())) {
        Entity &camera = sim->get_ent(flower.get_parent());
        camera.set_fov(BASE_FOV);
    }
}
