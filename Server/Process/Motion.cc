#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/Tilemap.hh>

#include <algorithm>
#include <cmath>

constexpr float BASE_TPS = 20;

void tick_entity_motion(Simulation *sim, Entity &ent) {
    if (ent.pending_delete) return;
    // Dormant (culled) mobs skip motion entirely -- combined with the AI skip in
    // tick_ai_behavior, an unobserved mob costs almost nothing (it just holds its
    // position/HP until a camera comes near and un-culls it). Only kHasCulling
    // mobs ever carry kIsCulled, so players/petals/drops are never affected.
    if (BitMath::at(ent.flags, EntityFlags::kIsCulled)) return;
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
        // Coarse swept-path reject: if no solid cell (and, for mobs, no tunnel
        // cell) lies anywhere near the whole prev->cur segment -- bbox padded by
        // the move distance so the entire swept circle is covered -- there is
        // nothing to resolve. Skip the sub-step loop AND the post-move rejection
        // checks entirely. On open terrain (most non-culled mobs, away from
        // walls) this makes terrain collision nearly free; it was the bulk of
        // per-tick motion cost. The precise sub-cell work below is unchanged for
        // anything actually near terrain, so behavior is identical.
        bool const _maybe_terrain =
            Tilemap::_near_solid_cells(tx, ty, r + dist) ||
            (ent.has_component(kMob) && Tilemap::_near_tunnel_cells(tx, ty, r + dist));
        if (_maybe_terrain) {
        // Step must be <= this body's radius so the swept circle overlaps any
        // wall it crosses (no thin-wall skip); the floor bounds the step count.
        float step = std::max(4.0f, r * 0.8f);
        int steps = std::max(1, (int)std::ceil(dist / step));
        // Hard cap the sub-step count. A body moving a normal per-tick distance
        // needs only a handful of steps; a count in the hundreds/thousands only
        // happens for an anomalous single-tick jump (spawn/teleport/runaway
        // knockback), where each step runs a full push_circle scan -- that was
        // the ~356ms motion spike (a whole-frame freeze) seen in profiling. 64
        // swept sub-steps still covers any legitimate fast move without letting
        // one entity blow the entire tick budget.
        if (steps > 64) steps = 64;
        float sdx = (tx - prev_x) / steps, sdy = (ty - prev_y) / steps;
        float cx = prev_x, cy = prev_y;
        // Mobs (not players/drops) also treat tunnels as impassable -- they
        // can't walk into/through one to "cross", just bump and slide along
        // it like a wall. Players are exempt: entering is exactly what makes
        // them hidden.
        bool const mob_tunnel_block = ent.has_component(kMob);
        // Advance from the RESOLVED position each step and re-resolve, so a fast
        // burst (bubble, knockback) can't tunnel through / into a wall -- or,
        // for mobs, hop clean across a narrow tunnel strip in one move without
        // ever landing on a sampled point inside it.
        for (int s = 0; s < steps; ++s) {
            cx += sdx;
            cy += sdy;
            Tilemap::push_circle(cx, cy, r);
            if (mob_tunnel_block)
                Tilemap::tunnel_push_circle(cx, cy, r);
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
        // Same rejection fallback as above, but for the tunnel wall mask: a mob
        // whose resolved spot still reads as tunnel (adaptive-stride miss, or
        // simply started outside one) is shoved back rather than let through.
        if (mob_tunnel_block && Tilemap::tunnel_wall_circle(cx, cy, r) && !Tilemap::tunnel_wall_circle(prev_x, prev_y, r)) {
            cx = prev_x;
            cy = prev_y;
        }
        ent.set_x(cx);
        ent.set_y(cy);
        }   // end if (_maybe_terrain)
    }
    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}