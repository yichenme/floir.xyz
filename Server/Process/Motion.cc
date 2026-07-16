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
        float const cs = Tilemap::COLL_CELL;
        for (uint32_t pass = 0; pass < 2; ++pass) {
            float x = ent.get_x();
            float y = ent.get_y();
            int32_t c0 = (int32_t)std::floor((x - r) / cs);
            int32_t c1 = (int32_t)std::floor((x + r) / cs);
            int32_t r0 = (int32_t)std::floor((y - r) / cs);
            int32_t r1 = (int32_t)std::floor((y + r) / cs);
            for (int32_t rr = r0; rr <= r1; ++rr)
            for (int32_t cc = c0; cc <= c1; ++cc) {
                if (!Tilemap::solid_at(cc * cs + 1, rr * cs + 1)) continue;
                float const cellL = cc * cs, cellT = rr * cs;
                float const cellR = cellL + cs, cellB = cellT + cs;
                float const nearX = fclamp(x, cellL, cellR);
                float const nearY = fclamp(y, cellT, cellB);
                float dx = x - nearX, dy = y - nearY;
                float d2 = dx * dx + dy * dy;
                if (d2 >= r * r) continue;
                if (d2 > 0.0001f) {
                    float d = std::sqrt(d2);
                    float push = r - d;
                    ent.set_x(x + dx / d * push);
                    ent.set_y(y + dy / d * push);
                } else {
                    // Center inside the cell: eject along the nearest face.
                    float const dl = x - cellL, dr = cellR - x;
                    float const dt = y - cellT, db = cellB - y;
                    float const m = std::min(std::min(dl, dr), std::min(dt, db));
                    if (m == dl) ent.set_x(cellL - r);
                    else if (m == dr) ent.set_x(cellR + r);
                    else if (m == dt) ent.set_y(cellT - r);
                    else ent.set_y(cellB + r);
                }
                x = ent.get_x();
                y = ent.get_y();
            }
        }
    }
    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}