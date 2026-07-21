#include <Server/Process.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <algorithm>

// 20% padding beyond the visible screen (florr-style, matches the identical
// pad in Game.cc's _update_client) instead of a flat pixel margin, so a mob
// wakes from dormancy a beat before it actually scrolls into view. The
// ABSOLUTE padding added is capped: at max de-zoom (Unique Antennae, fov
// floor 1/7 -> 960/fov ~= 6720) a naive 20% relative pad adds ~1344 world
// units on top of an already-huge radius, waking a much larger fraction of
// the map's mob population than intended and tanking tick time -- reported
// as the game freezing while Unique Antennae is equipped. Capping the pad's
// absolute size keeps the florr-style easing at normal zoom without letting
// it runaway at extreme zoom-out.
constexpr float CULL_PAD_FACTOR = 1.2f;
constexpr float CULL_PAD_MAX = 300.f;

void tick_culling_behavior(Simulation *sim, Entity &ent) {
    float fov = fclamp(ent.get_fov(), BASE_FOV * 0.1, BASE_FOV);
    float const rx = 960 / fov, ry = 540 / fov;
    float const px = std::min(rx * (CULL_PAD_FACTOR - 1.f), CULL_PAD_MAX);
    float const py = std::min(ry * (CULL_PAD_FACTOR - 1.f), CULL_PAD_MAX);
    sim->spatial_hash.query(ent.get_camera_x(), ent.get_camera_y(), rx + px, ry + py, [](Simulation *, Entity &ent) {
        BitMath::unset(ent.flags, EntityFlags::kIsCulled);
    });
}
