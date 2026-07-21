#include <Server/Process.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

// 20% padding beyond the visible screen (florr-style, matches the identical
// pad in Game.cc's _update_client) instead of a flat pixel margin, so a mob
// wakes from dormancy a beat before it actually scrolls into view.
constexpr float CULL_PAD_FACTOR = 1.2f;

void tick_culling_behavior(Simulation *sim, Entity &ent) {
    float fov = fclamp(ent.get_fov(), BASE_FOV * 0.1, BASE_FOV);
    sim->spatial_hash.query(ent.get_camera_x(), ent.get_camera_y(), 960 / fov * CULL_PAD_FACTOR, 540 / fov * CULL_PAD_FACTOR, [](Simulation *, Entity &ent) {
        BitMath::unset(ent.flags, EntityFlags::kIsCulled);
    });
}
