#include <Server/Spawn.hh>

#include <Server/EntityFunctions.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>

#include <Shared/Binary.hh>
#include <Shared/Map.hh>
#include <Shared/RarityScale.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/TalentData.hh>
#include <Shared/Tilemap.hh>

#include <cmath>
#include <vector>

uint8_t inherited_spawn_rarity(Entity const &parent) {
    if (BitMath::at(parent.flags, EntityFlags::kSpawnedFromZone))
        return roll_spawn_rarity((uint8_t)MAP_DATA[parent.zone].difficulty);
    if (parent.has_component(kMob)) return parent.get_mob_rarity();
    return RarityID::kCommon;
}

Entity &alloc_drop(Simulation *sim, PetalID::T drop_id, uint8_t rarity, uint32_t owner) {
    DEBUG_ONLY(assert(drop_id < PetalID::kNumPetals);)
    PetalTracker::add_petal(sim, drop_id);
    Entity &drop = sim->alloc_ent();
    drop.add_component(kPhysics);
    drop.set_radius(25);
    drop.set_angle(frand() * 0.2 - 0.1);
    drop.friction = 0.25;

    drop.add_component(kRelations);
    drop.set_team(NULL_ENTITY);

    drop.add_component(kDrop);
    drop.set_drop_id(drop_id);
    drop.set_drop_rarity(rarity);
    drop.set_drop_owner(owner);   // camera id of the sole looter (0 = shared)
    drop.set_drop_count(1);       // >1 for stacked drops (Unique's 10x Ultra)
    entity_set_despawn_tick(drop, 10 * (2 + rarity) * TPS);
    drop.immunity_ticks = TPS / 3;
    return drop;
}

static Entity &__alloc_mob(
    Simulation *sim, MobID::T mob_id, float x, float y, 
    EntityID const team, uint8_t rarity, std::function<void(Entity &)> on_spawn
) {
    DEBUG_ONLY(assert(mob_id < MobID::kNumMobs);)
    struct MobData const &data = MOB_DATA[mob_id];
    float seed = frand();
    Entity &mob = sim->alloc_ent();
    mob.mob_damage.clear();   // fresh per-player loot-damage tally

    mob.add_component(kPhysics);
    mob.set_radius(data.radius.get_single(seed));
    mob.set_angle(frand() * 2 * M_PI);
    mob.set_x(x);
    mob.set_y(y);
    mob.friction = DEFAULT_FRICTION;
    // Explicit per-mob mass (Shared/StaticData.cc), not derived from radius/
    // rarity -- stays exactly what's specified regardless of size.
    mob.mass = data.attributes.mass;
    if (mob_id == MobID::kAntHole)
        BitMath::set(mob.flags, EntityFlags::kNoFriendlyCollision);
    // Server-side dormancy (florr-style): a wild mob no camera can see is culled,
    // and its AI + motion are skipped -- it just sits with its coords/hp until a
    // player comes near and wakes it. This keeps the huge unobserved population
    // (empty / cleared zones) nearly free. Segmented mobs (centipedes) are
    // excluded: culling on the head would freeze a body whose tail is on-screen.
    if (data.attributes.segments <= 1)
        BitMath::set(mob.flags, EntityFlags::kHasCulling);

    mob.add_component(kRelations);
    mob.set_team(team);

    mob.add_component(kMob);
    mob.set_mob_id(mob_id);
    mob.set_mob_rarity(rarity);
    mob.set_radius(mob.get_radius() * mob_size_mult(rarity));
    // The pre-spawn terrain check only knew the base radius; scaling by rarity
    // (and petal/summon spawns that place a mob wherever the summoner is) can
    // leave a body clipping into a wall. Push the spawn out of any solid terrain
    // so mobs -- e.g. a wandering Sandstorm -- never start embedded in a block.
    {
        float sx = mob.get_x(), sy = mob.get_y();
        Tilemap::push_circle(sx, sy, mob.get_radius());
        mob.set_x(sx);
        mob.set_y(sy);
    }

    mob.add_component(kHealth);
    float hp_m = mob_hp_mult(rarity);
    float dmg_m = mob_body_damage_mult(rarity);
    float arm_m = mob_armor_mult(rarity);
    mob.health = mob.max_health = data.health.get_single(seed) * hp_m;
    mob.damage = data.damage * dmg_m;
    mob.armor = data.attributes.armor * arm_m;
    mob.poison_damage = data.attributes.poison_damage;
    mob.set_health_ratio(1);

    mob.detection_radius = data.attributes.aggro_radius;
    mob.score_reward = (uint32_t)(data.xp * mob_xp_mult(rarity) + 0.5f);

    mob.add_component(kName);
    mob.set_name(data.name);

    mob.base_entity = mob.id;
    if (mob_id == MobID::kDigger) {
        mob.add_component(kFlower);
        mob.set_angle(0);
        mob.set_color(ColorID::kGray);
    }
    if (on_spawn) 
        on_spawn(mob);
    return mob;
}

// Announce Super/Unique WILD mob spawns (team == NULL_ENTITY excludes player
// summons, whose rarity is clamped below Super anyway). One message per mob.
static void _announce_spawn(Entity &mob) {
    uint8_t const rarity = mob.get_mob_rarity();
    if (rarity < RarityID::kSuper) return;
    if (!(mob.get_team() == NULL_ENTITY)) return;
    if (!(mob.get_parent() == NULL_ENTITY)) return;
    // Pollen is a Bumble Bee projectile, not a real mob encounter -- never
    // worth a spawn announcement even at high rarity.
    if (mob.get_mob_id() == MobID::kPollen) return;
    bool const uniq = rarity >= RarityID::kUnique;
    std::string const msg = std::string("A ") + (uniq ? "Unique " : "Super ")
        + MOB_DATA[mob.get_mob_id()].name + " has spawned!";
    Server::game.system_message(uniq ? SystemMsgKind::kSysUnique : SystemMsgKind::kSysSuper, msg);
}

Entity &alloc_mob(
    Simulation *sim, MobID::T mob_id, float x, float y,
    EntityID const team, uint8_t rarity, std::function<void(Entity &)> on_spawn
) {
    struct MobData const &data = MOB_DATA[mob_id];
    if (data.attributes.segments <= 1) {
        Entity &ent = __alloc_mob(sim, mob_id, x, y, team, rarity, on_spawn);
        if (mob_id == MobID::kAntHole) {
            std::vector<MobID::T> const spawns = {
                MobID::kBabyAnt, MobID::kBabyAnt, MobID::kBabyAnt,
                MobID::kWorkerAnt, MobID::kWorkerAnt, MobID::kSoldierAnt,
                MobID::kQueenAnt
            };
            for (MobID::T mob_id : spawns) {
                Vector rand = Vector::rand(ent.get_radius() * 2);
                Entity &ant = __alloc_mob(sim, mob_id, x + rand.x, y + rand.y, team, rarity, on_spawn);
                ant.set_parent(ent.id);
            }
        }
        _announce_spawn(ent);
        return ent;
    }
    else {
        // Validate the WHOLE chain (using the mob's max possible body radius --
        // conservative but simple, since each segment's actual rolled radius
        // isn't known until __alloc_mob creates it) against walls/water/castle/
        // tunnels before creating any entities. Post-hoc push_circle per segment
        // (the old approach) could still leave a body visibly overlapping
        // terrain it was buried in; retry from a fresh nearby head point instead.
        float const seg_radius = data.radius.upper;
        auto blocked = [](float px, float py, float rad) {
            return Tilemap::solid_circle(px, py, rad) || Tilemap::tunnel_circle(px, py, rad);
        };
        std::vector<float> chain_x, chain_y, angles;
        float hx = x, hy = y;
        for (uint32_t attempt = 0; attempt < 8; ++attempt) {
            chain_x.clear();
            chain_y.clear();
            angles.clear();
            float cx = hx, cy = hy;
            float cangle = frand() * 2 * M_PI;
            bool ok = !blocked(cx, cy, seg_radius);
            if (ok) {
                chain_x.push_back(cx);
                chain_y.push_back(cy);
                angles.push_back(cangle);
                for (uint32_t i = 1; i < data.attributes.segments; ++i) {
                    float nangle = cangle + frand() * 0.1f - 0.05f;
                    float nx = cx - 2 * seg_radius * cosf(nangle);
                    float ny = cy - 2 * seg_radius * sinf(nangle);
                    if (blocked(nx, ny, seg_radius)) { ok = false; break; }
                    chain_x.push_back(nx);
                    chain_y.push_back(ny);
                    angles.push_back(nangle);
                    cx = nx; cy = ny; cangle = nangle;
                }
            }
            if (ok && chain_x.size() == data.attributes.segments) break;
            hx = x + (frand() - 0.5f) * 500;
            hy = y + (frand() - 0.5f) * 500;
        }
        // Still short after every retry (rare, dense terrain): fill out the
        // remaining segments anyway so the mob spawns at all -- push_circle
        // per-segment below still un-embeds each one from its actual terrain.
        while (chain_x.size() < data.attributes.segments) {
            float cx = chain_x.empty() ? x : chain_x.back();
            float cy = chain_y.empty() ? y : chain_y.back();
            float cangle = angles.empty() ? frand() * 2 * M_PI : angles.back();
            float nangle = cangle + frand() * 0.1f - 0.05f;
            chain_x.push_back(cx - 2 * seg_radius * cosf(nangle));
            chain_y.push_back(cy - 2 * seg_radius * sinf(nangle));
            angles.push_back(nangle);
        }

        Entity &head = __alloc_mob(sim, mob_id, chain_x[0], chain_y[0], team, rarity, on_spawn);
        head.set_angle(angles[0]);
        Entity *curr = &head;
        for (uint32_t i = 1; i < data.attributes.segments; ++i) {
            Entity &seg = __alloc_mob(sim, mob_id, chain_x[i], chain_y[i], team, rarity, on_spawn);
            seg.add_component(kSegmented);
            seg.seg_head = curr->id;
            seg.set_angle(angles[i]);
            // The chain above validated against the conservative max radius;
            // still push each segment to hug its own (smaller, actual rolled)
            // radius exactly.
            float sx = seg.get_x(), sy = seg.get_y();
            Tilemap::push_circle(sx, sy, seg.get_radius());
            seg.set_x(sx);
            seg.set_y(sy);
            curr = &seg;
        }
        _announce_spawn(head);
        return head;
    }
}

Entity &alloc_player(Simulation *sim, EntityID const team) {
    Entity &player = sim->alloc_ent();

    player.add_component(kPhysics);
    player.set_radius(BASE_FLOWER_RADIUS);
    player.friction = DEFAULT_FRICTION;
    player.mass = 1;

    player.add_component(kFlower);

    player.add_component(kRelations);
    player.set_team(team);

    player.add_component(kHealth);
    player.health = player.max_health = BASE_HEALTH;
    player.set_health_ratio(1);
    player.damage = BASE_BODY_DAMAGE;
    player.immunity_ticks = 1.0 * TPS;

    player.add_component(kScore);

    player.add_component(kName);
    player.set_nametag_visible(1);

    player.base_entity = player.id;
    return player;
}

Entity &alloc_petal(Simulation *sim, PetalID::T petal_id, Entity const &parent, uint8_t rarity) {
    DEBUG_ONLY(assert(petal_id < PetalID::kNumPetals);)
    struct PetalData const &petal_data = PETAL_DATA[petal_id];
    Entity &petal = sim->alloc_ent();
    petal.add_component(kPhysics);
    petal.set_x(parent.get_x());
    petal.set_y(parent.get_y());
    petal.set_radius(petal_data.radius);
    if (petal_data.attributes.rotation_style == PetalAttributes::kPassiveRot)
        petal.set_angle(frand() * 2 * M_PI);
    // (Moon is no longer a detached orbiting petal -- it summons a Moon mob.)
    petal.mass = petal_data.attributes.mass;
    petal.friction = DEFAULT_FRICTION * 1.5;
    petal.add_component(kRelations);
    petal.set_parent(parent.id);
    petal.set_team(parent.get_team());
    petal.set_color(parent.get_color());
    petal.add_component(kPetal);
    petal.set_petal_id(petal_id);
    petal.set_petal_rarity(rarity);
    petal.set_split_projectile(petal_data.attributes.split_projectile);
    petal.add_component(kHealth);
    // Stinger and Bubble are always 1 HP regardless of rarity. Mjolnir's data
    // values (328050) are its exact Unique stats -- it's a fixed leaderboard
    // reward that only ever exists at Unique, so it bypasses rarity scaling for
    // both HP and damage instead of being multiplied up from a Common base.
    bool const flat_stats = petal_id == PetalID::kMjolnir;
    float const hp_mult = (petal_id == PetalID::kStinger || petal_id == PetalID::kBubble || flat_stats) ? 1.0f : petal_hp_mult(rarity);
    // Petal Health talent: a flat account-wide multiplier, read off the
    // owning player's camera (parent here is the player; parent.get_parent()
    // is the camera -- see Client.hh's Camera fields and Client.cc's
    // kClientSpawn for where this gets cached). Mob-owned petals (parent is
    // a mob, not a player) have no camera parent so this is just a no-op 1.0x.
    float talent_mult = 1.0f;
    if (sim->ent_alive(parent.get_parent())) {
        Entity const &owner_camera = sim->get_ent(parent.get_parent());
        if (owner_camera.has_component(kCamera))
            talent_mult = talent_health_mult(owner_camera.get_talent_health_rank());
    }
    petal.health = petal.max_health = petal_data.health * hp_mult * talent_mult;
    petal.damage = petal_data.damage * (flat_stats ? 1.0f : petal_damage_mult(rarity));
    petal.set_health_ratio(1);
    // Poison and armor both scale x3 per rarity (Iris/Grapes/Pincer poison,
    // Bone armor, ...), matching the damage/HP multiplier.
    petal.poison_damage = petal_data.attributes.poison_damage;
    petal.poison_damage.damage *= rarity_pow3(rarity);
    petal.armor = petal_data.attributes.armor * rarity_pow3(rarity);
    petal.slow_inflict = TPS * petal_data.attributes.slow_inflict_seconds;

    if (parent.id == NULL_ENTITY) petal.base_entity = petal.id;
    else petal.base_entity = parent.id;
    return petal;
}

Entity &alloc_web(Simulation *sim, float radius, Entity const &parent) {
    Entity &web = sim->alloc_ent();
    web.add_component(kPhysics);
    web.set_x(parent.get_x());
    web.set_y(parent.get_y());
    web.set_angle(frand() * 2 * M_PI);
    web.set_radius(radius);
    web.mass = 1.0;
    web.friction = 1.0;
    web.add_component(kRelations);
    web.set_team(parent.get_team());
    web.set_parent(parent.id);
    web.set_color(parent.get_color());
    web.add_component(kWeb);
    entity_set_despawn_tick(web, 10 * TPS);
    return web;
}

Entity &alloc_cpu_camera(Simulation *sim, EntityID const team) {
    Entity &ent = sim->alloc_ent();
    ent.add_component(kCamera);
    ent.add_component(kRelations);

    ent.set_fov(BASE_FOV);
    ent.set_respawn_level(frand() * 30);
    ent.set_respawn_score(level_to_score(ent.get_respawn_level()));
    ent.set_team(team);
    ent.set_color(ColorID::kGray);
    
    //need to auto add to petaltracker
    std::vector<PetalID::T> const inventory = {
        PetalID::kRose, PetalID::kBasic, PetalID::kBasic, 
        PetalID::kRose, PetalID::kBasic, PetalID::kBasic,
        PetalID::kRose, PetalID::kBasic, PetalID::kBasic
    };

    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i) {
        ent.set_inventory(i, inventory[i]);
        ent.set_inventory_rarity(i, PETAL_DATA[inventory[i]].rarity);
    }
    
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i)
        PetalTracker::add_petal(sim, ent.get_inventory(i));
    return ent;
}

void player_spawn(Simulation *sim, Entity &camera, Entity &player) {
    // A real respawn (new flower entity) forfeits loot claims from the
    // previous life -- purge this camera's damage from every live mob here,
    // NOT on death. A Yggdrasil revive never reaches this function (it flips
    // the corpse flower back to alive in-place), so it keeps its claims.
    uint32_t const cam_id = camera.id.id;
    sim->for_each<kMob>([cam_id](Simulation *, Entity &m){ m.mob_damage.erase(cam_id); });
    camera.set_player(player.id);
    player.set_parent(camera.id);
    player.set_color(camera.get_color());
    // Everyone spawns at the garden's marked first-join point (the unpainted
    // ellipse near the NW corner), on walkable ground clear of walls. Jitter
    // around the point and reject blocked cells.
    constexpr float SPAWN_CX = 6175.8f, SPAWN_CY = 2820.3f, SPAWN_JITTER = 1000;
    float spawn_x = SPAWN_CX;
    float spawn_y = SPAWN_CY;
    for (uint32_t i = 0; i < 40; ++i) {
        float jx = SPAWN_CX + (frand() - 0.5f) * 2 * SPAWN_JITTER;
        float jy = SPAWN_CY + (frand() - 0.5f) * 2 * SPAWN_JITTER;
        if (!Tilemap::solid_circle(jx, jy, BASE_FLOWER_RADIUS)) {
            spawn_x = jx;
            spawn_y = jy;
            break;
        }
    }
    camera.set_camera_x(spawn_x);
    camera.set_camera_y(spawn_y);
    player.set_x(spawn_x);
    player.set_y(spawn_y);
    player.set_score(camera.get_respawn_score());
    player.set_loadout_count(loadout_slots_at_level(camera.get_respawn_level()));
    player.health = player.max_health = hp_at_level(camera.get_respawn_level());
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        PetalID::T id = camera.get_inventory(i);
        uint8_t rarity = camera.get_inventory_rarity(i);
        LoadoutSlot &slot = player.loadout[i];
        player.set_loadout_ids(i, id);
        player.set_loadout_rarities(i, rarity);
        slot.update_id(sim, id, rarity);
        slot.force_reload();
    }

    for (uint32_t i = player.get_loadout_count(); i < 2 * player.get_loadout_count(); ++i) {
        player.set_loadout_ids(i, camera.get_inventory(i));
        player.set_loadout_rarities(i, camera.get_inventory_rarity(i));
    }

    //peaceful transfer, no petal tracking needed
    for (uint32_t i = 0; i < MAX_SLOT_COUNT * 2; ++i) {
        camera.set_inventory(i, PetalID::kNone);
        camera.set_inventory_rarity(i, 0);
    }
}
