#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/Tilemap.hh>

#include <cmath>

constexpr float BASE_TPS = 20;

void tick_entity_motion(Simulation *sim, Entity &ent) {
    if (ent.pending_delete) return;
    if (ent.slow_ticks > 0) {
        ent.speed_ratio *= 0.5;
        --ent.slow_ticks;
    }
    // Players and mobs collide with impassable terrain. Capture the pre-move
    // position so we can revert blocked axes for wall-sliding below.
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
    // Edge-based axis-separated terrain collision: sample the leading edge of
    // the body (center +/- radius in the move direction) so the entity stops
    // when its *body* touches a blocked cell, not when its center crosses the
    // grid line. Reverting per-axis lets it slide along walls. Only blocks
    // moves *into* new blocked cells, so anything already inside one can leave.
    if (terrain_collide) {
        float const r = ent.get_radius();
        float nx = ent.get_x();
        float ny = ent.get_y();
        float const edge_x = nx + (nx > prev_x ? r : (nx < prev_x ? -r : 0));
        if (Tilemap::blocks_movement(Tilemap::terrain_at(edge_x, prev_y)))
            nx = prev_x;
        float const edge_y = ny + (ny > prev_y ? r : (ny < prev_y ? -r : 0));
        if (Tilemap::blocks_movement(Tilemap::terrain_at(nx, edge_y)))
            ny = prev_y;
        ent.set_x(nx);
        ent.set_y(ny);
    }
    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}