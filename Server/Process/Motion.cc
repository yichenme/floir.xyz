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
    // Players and mobs collide with impassable terrain (resolved after the move).
    bool const terrain_collide = ent.has_component(kFlower) || ent.has_component(kMob);
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
        int steps = std::max(1, (int)std::ceil(dist / 50.0f));
        float cx = prev_x, cy = prev_y;
        for (int s = 1; s <= steps; ++s) {
            cx = prev_x + (tx - prev_x) * s / steps;
            cy = prev_y + (ty - prev_y) * s / steps;
            Tilemap::push_circle(cx, cy, r);
        }
        ent.set_x(cx);
        ent.set_y(cy);
    }
    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}