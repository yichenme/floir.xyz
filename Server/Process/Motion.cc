#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/Tilemap.hh>

#include <algorithm>
#include <cmath>

constexpr float BASE_TPS = 20;

void tick_entity_motion(Simulation *sim, Entity &ent) {
    if (ent.pending_delete) return;
    if (ent.slow_ticks > 0) {
        ent.speed_ratio *= 0.5;
        --ent.slow_ticks;
    }
    // Players, mobs and drops collide with impassable terrain (resolved after
    // the move). Drops are included so loot can't come to rest inside a blocked
    // tile: it gets pushed out and settles beside the block instead.
    bool const terrain_collide = ent.has_component(kFlower) || ent.has_component(kMob) || ent.has_component(kDrop);
    float const prev_x = ent.get_x();
    float const prev_y = ent.get_y();
    float const dt = (BASE_TPS / TPS);
    if (ent.friction <= 0) {
        Vector const add = ent.velocity * dt + ent.acceleration * (0.5 * dt * dt);
        ent.velocity += ent.acceleration * dt;
        ent.set_x(ent.get_x() + add.x + ent.collision_velocity.x);
        ent.set_y(ent.get_y() + add.y + ent.collision_velocity.y);
    }
    else if (ent.friction >= 1) {
        ent.velocity.set(0,0);
        ent.set_x(ent.get_x() + ent.acceleration.x * dt + ent.collision_velocity.x);
        ent.set_y(ent.get_y() + ent.acceleration.y * dt + ent.collision_velocity.y);
    }
    else {
        float const f = 1 - ent.friction;
        Vector const term_vel = ent.acceleration * (ent.speed_ratio / ent.friction);
        Vector const v = ent.velocity - term_vel;
        Vector const add = term_vel * dt + v * ((std::powf(f, dt) - 1) / std::logf(f));
        ent.set_x(ent.get_x() + add.x + ent.collision_velocity.x);
        ent.set_y(ent.get_y() + add.y + ent.collision_velocity.y);
        ent.velocity = term_vel + v * (std::powf(f, dt));
    }
    ent.velocity += ent.collision_velocity * 0.5;
    if (!ent.has_component(kPetal) && !ent.has_component(kWeb)) {
        ent.set_x(fclamp(ent.get_x(), ent.get_radius(), ARENA_WIDTH - ent.get_radius()));
        ent.set_y(fclamp(ent.get_y(), ent.get_radius(), ARENA_HEIGHT - ent.get_radius()));
    }
    // Circle-vs-tile terrain collision. Resolve the body out of every blocked
    // cell its radius overlaps, so it stops exactly when the model touches the
    // wall/water/etc. (not the grid line) and slides along edges. Handles any
    // approach angle; an entity whose center is inside a blocked cell is pushed
    // out along the shallowest axis so it can escape.
    if (terrain_collide) {
        float const r = ent.get_radius();
        // Sub-step from the pre-move position so a big move/knockback can't
        // tunnel through a thin wall; resolve exactly against tile polygons.
        float tx = ent.get_x(), ty = ent.get_y();
        float dist = std::hypot(tx - prev_x, ty - prev_y);
        // Step must be <= this body's radius so the swept circle overlaps any
        // wall it crosses (no thin-wall skip); the floor bounds the step count.
        float step = std::max(4.0f, r * 0.8f);
        int steps = std::max(1, (int)std::ceil(dist / step));
        float sdx = (tx - prev_x) / steps, sdy = (ty - prev_y) / steps;
        float cx = prev_x, cy = prev_y;
        // Advance from the RESOLVED position each step and re-resolve, so a fast
        // burst (bubble, knockback) can't tunnel through / into a wall.
        for (int s = 0; s < steps; ++s) {
            cx += sdx;
            cy += sdy;
            Tilemap::push_circle(cx, cy, r);
        }
        // push_circle samples large bodies coarsely (an adaptive stride caps its
        // cost -- see Tilemap), so a big high-rarity mob can end a step still
        // overlapping a block it should have been stopped by. If the resolved
        // spot is still solid, reject the move and stay at the last valid
        // position, so mobs are actually blocked instead of drifting through.
        // Gated by the cheap near-solid pre-check, so open-terrain bodies and
        // fully-resolved small mobs/players pay nothing extra.
        if (Tilemap::_near_solid_cells(cx, cy, r) && Tilemap::solid_circle(cx, cy, r)
            && !Tilemap::solid_circle(prev_x, prev_y, r)) {
            cx = prev_x;
            cy = prev_y;
        }
        ent.set_x(cx);
        ent.set_y(cy);
    }
    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}