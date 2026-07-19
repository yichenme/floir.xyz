#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/RarityScale.hh>
#include <Shared/Tilemap.hh>

#include <cmath>

struct PlayerBuffs {
    float extra_rot = 0;
    float extra_range = 0;
    float heal = 0;
    float vision_factor = 1;
    float extra_health = 0;
    float extra_damage = 0;
    float damage_factor = 1;
    float reload_factor = 1;
    uint8_t yinyang_count = 0;
    uint8_t is_poisonous = 0;
    uint8_t equip_flags = 0;
};

struct RotationCenter {
    float x;
    float y;
    float r;
};

static struct PlayerBuffs _get_petal_passive_buffs(Simulation *sim, Entity &player) {
    struct PlayerBuffs buffs = {0};
    if (player.has_component(kMob)) return buffs;
    player.set_equip_flags(0);
    player.damage_reflection = 0;
    player.poison_armor = 0;
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot const &slot = player.loadout[i];
        PetalID::T slot_petal_id = slot.get_petal_id();
        struct PetalData const &petal_data = PETAL_DATA[slot_petal_id];
        struct PetalAttributes const &attrs = petal_data.attributes;
        if (attrs.equipment != EquipmentFlags::kNone)
            player.set_equip_flags(player.get_equip_flags() | (1 << attrs.equipment));
        // FOV petals widen the view with rarity: Observer +75% per tier,
        // Antennae +50% per tier (view radius scales; vision_factor = 1/scale).
        if (attrs.vision_factor < 1.f) {
            float const rate = (slot_petal_id == PetalID::kObserver) ? 0.75f : 0.5f;
            buffs.vision_factor = std::min(buffs.vision_factor, 1.f / (1.f + rate * (slot.rarity + 1)));
        }
        buffs.extra_range = std::fmax(attrs.extra_range * (slot.rarity + 1), buffs.extra_range);   // Third Eye: +25 per rarity (base 25)
        buffs.extra_damage = std::fmax(buffs.extra_damage, attrs.extra_body_damage * rarity_pow3(slot.rarity));
        buffs.damage_factor *= attrs.extra_damage_factor;
        // Reload-factor reductions deepen one step per rarity (Golden Leaf: -5%
        // at Common, -5% more each tier up).
        {
            float rf = attrs.extra_reload_factor;
            if (rf < 1.f) rf = std::fmax(0.1f, 1.f - (1.f - rf) * (1 + slot.rarity));
            buffs.reload_factor *= rf;
        }
        if (slot_petal_id == PetalID::kYinYang)
            ++buffs.yinyang_count;
        if (!player.loadout[i].already_spawned) continue;
        if (slot_petal_id == PetalID::kLeaf)
            buffs.heal += attrs.constant_heal * rarity_pow3(slot.rarity) / TPS;
        else if (slot_petal_id == PetalID::kYucca && BitMath::at(player.input, InputFlags::kDefending) && !BitMath::at(player.input, InputFlags::kAttacking))
            buffs.heal += attrs.constant_heal * rarity_pow3(slot.rarity) / TPS;
        buffs.extra_rot += attrs.extra_rotation_speed * (1 + 0.4f * slot.rarity);   // Faster: +0.2 rad/s per rarity (base 0.5)
        buffs.extra_health += attrs.extra_health * rarity_pow3(slot.rarity);   // flower-HP buff x3/rarity (Cactus)
        player.damage_reflection = std::fmax(player.damage_reflection, attrs.damage_reflection > 0 ? attrs.damage_reflection + 0.05f * slot.rarity : 0);   // Salt: +5% per rarity
        player.poison_armor = std::fmax(player.poison_armor, attrs.poison_armor * rarity_pow3(slot.rarity) / TPS);   // Lotus x3/rarity
        if (slot_petal_id == PetalID::kPoisonCactus)
            buffs.is_poisonous = 1;
    }
    return buffs;
}

static uint32_t _get_petal_rotation_count(Simulation *sim, Entity &player) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot const &slot = player.loadout[i];
        struct PetalData const &petal_data = PETAL_DATA[slot.get_petal_id()];
        if (petal_data.attributes.clump_radius > 0)
            ++count;
        else {
            for (uint32_t j = 0; j < slot.size(); ++j) {
                if (!sim->ent_alive(slot.petals[j].ent_id))
                    ++count;
                else if (!BitMath::at(sim->get_ent(slot.petals[j].ent_id).flags, EntityFlags::kIsDetached))
                    ++count;
            }
        }
    }
    return count;
}

static RotationCenter const _get_petal_rotation_center(Simulation *sim, Entity const &player) {
    RotationCenter rotation_center = { 
        .x = player.get_x(), 
        .y = player.get_y(), 
        .r = player.get_radius() 
    };
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot const &slot = player.loadout[i];
        if (slot.get_petal_id() != PetalID::kMoon) continue;
        for (uint32_t j = 0; j < slot.size(); ++j) {
            LoadoutPetal const &petal_slot = slot.petals[j];
            if (!sim->ent_alive(petal_slot.ent_id)) continue;
            Entity &moon = sim->get_ent(petal_slot.ent_id);
            rotation_center.x = moon.get_x();
            rotation_center.y = moon.get_y();
            rotation_center.r = moon.get_radius();
            return rotation_center;
        }
    }
    return rotation_center;
}

void tick_player_behavior(Simulation *sim, Entity &player) {
    if (player.pending_delete) return;
    DEBUG_ONLY(assert(player.max_health > 0);)
    PlayerBuffs const buffs = _get_petal_passive_buffs(sim, player);
    float health_ratio = player.health / player.max_health;
    if (!player.has_component(kMob)) {
        player.max_health = hp_at_level(score_to_level(player.get_score())) + buffs.extra_health;
        player.damage = BASE_BODY_DAMAGE + buffs.extra_damage;
    }
    player.health = health_ratio * player.max_health;
    if (buffs.heal > 0)
        inflict_heal(sim, player, buffs.heal);
    if (buffs.is_poisonous)
        player.poison_damage = {10.0, 2};
    else
        player.poison_damage = {0, 0};
    
    float rot_pos = 0;
    uint32_t rotation_count = _get_petal_rotation_count(sim, player);
    RotationCenter const rotation_center = _get_petal_rotation_center(sim, player);
    //maybe use delta mode for face flags?
    player.set_face_flags(0);

    if (sim->ent_alive(player.get_parent())) {
        Entity &camera = sim->get_ent(player.get_parent());
        camera.set_fov(BASE_FOV * buffs.vision_factor);
    }

    DEBUG_ONLY(assert(player.get_loadout_count() <= MAX_SLOT_COUNT);)
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot &slot = player.loadout[i];
        //player.set_loadout_ids(i, slot.id);
        //other way around. loadout_ids should dictate loadout
        if (slot.get_petal_id() != player.get_loadout_ids(i) || slot.rarity != player.get_loadout_rarities(i))
            slot.update_id(sim, player.get_loadout_ids(i), player.get_loadout_rarities(i));
        PetalID::T slot_petal_id = slot.get_petal_id();
        struct PetalData const &petal_data = PETAL_DATA[slot_petal_id];
        DEBUG_ONLY(assert(petal_data.count <= MAX_PETALS_IN_CLUMP);)

        if (slot_petal_id == PetalID::kNone || petal_data.count == 0)
            continue;
        float min_reload = 1;
        // Worst-case (lowest) health among this slot's currently-alive petal
        // instances, mirroring how min_reload takes the least-ready instance.
        // Stays at 1 (no darken) if every instance is mid-reload right now --
        // the reload wedge already covers that case on its own.
        float min_health = 1;
        for (uint32_t j = 0; j < slot.size(); ++j) {
            LoadoutPetal &petal_slot = slot.petals[j];
            if (!sim->ent_alive(petal_slot.ent_id)) {
                petal_slot.ent_id = NULL_ENTITY;
                game_tick_t reload_time = (petal_data.reload * TPS) * buffs.reload_factor;
                // Yggdrasil's cooldown is divided by 3 each rarity up.
                if (slot_petal_id == PetalID::kYggdrasil)
                    reload_time = (game_tick_t)(reload_time / rarity_pow3(player.get_loadout_rarities(i)));
                // Bubble: primary reload 2.0s at Common, -0.25s per rarity (min 0.1s).
                else if (slot_petal_id == PetalID::kBubble)
                    reload_time = (game_tick_t)(std::fmax(0.1f, 2.0f - 0.25f * player.get_loadout_rarities(i)) * TPS);
                if (!slot.already_spawned) reload_time += TPS;
                float this_reload = reload_time == 0 ? 1 : (float) petal_slot.reload / reload_time;
                min_reload = std::min(min_reload, this_reload);
                if (petal_slot.reload >= reload_time) {
                    petal_slot.ent_id = alloc_petal(sim, slot_petal_id, player, player.get_loadout_rarities(i)).id;
                    sim->get_ent(petal_slot.ent_id).damage *= buffs.damage_factor;
                    sim->get_ent(petal_slot.ent_id).set_x(rotation_center.x);
                    sim->get_ent(petal_slot.ent_id).set_y(rotation_center.y);
                    petal_slot.reload = 0;
                    slot.already_spawned = 1;
                } 
                else
                    ++petal_slot.reload;
            }
            if (sim->ent_alive(petal_slot.ent_id)) {
                Entity &petal = sim->get_ent(petal_slot.ent_id);
                if (petal.has_component(kHealth))
                    min_health = std::min(min_health, (float) petal.get_health_ratio());
                //only do this if petal not despawning
                if (petal.has_component(kPetal) &&
                    !BitMath::at(petal.flags, EntityFlags::kIsDespawning) &&
                    !BitMath::at(petal.flags, EntityFlags::kIsDetached)) {
                    //petal rotation behavior
                    Vector wanting;
                    Vector delta(rotation_center.x - petal.get_x(), rotation_center.y - petal.get_y());
                    if (rotation_count > 0)
                        wanting.unit_normal(2 * M_PI * rot_pos / rotation_count + player.heading_angle);

                    float range = rotation_center.r + 40;
                    if (BitMath::at(player.input, InputFlags::kAttacking)) { 
                        if (petal_data.attributes.defend_only == 0) 
                            range = rotation_center.r + 100 + buffs.extra_range; 
                        if (petal.get_petal_id() == PetalID::kWing) {
                            float wave = sinf((float) petal.lifetime / (0.4 * TPS));
                            wave = wave * wave;
                            range += wave * 120;
                        }
                    }
                    else if (BitMath::at(player.input, InputFlags::kDefending)) 
                        range = rotation_center.r + 15;
                    wanting *= range;
                    if (petal_data.attributes.clump_radius > 0) {
                        Vector secondary;
                        secondary.unit_normal(2 * M_PI * j / petal_data.count + player.heading_angle * 0.2)
                        .set_magnitude(petal_data.attributes.clump_radius);
                        wanting += secondary;
                    }
                    wanting += delta;
                    wanting *= 0.5f;
                    petal.acceleration = wanting;
                    game_tick_t sec_reload_ticks = petal_data.attributes.secondary_reload * TPS;
                    if (petal_data.attributes.spawns != MobID::kNumMobs &&
                        petal.secondary_reload > sec_reload_ticks) {
                        uint8_t spawn_id = petal_data.attributes.spawns;
                        // Summons scale with the PETAL's rarity (petals aren't
                        // mobs, so inherited_spawn_rarity would force Common).
                        uint8_t const summon_rarity = petal.get_petal_rarity();
                        Entity &mob = alloc_mob(sim, spawn_id, petal.get_x(), petal.get_y(), petal.get_team(), summon_rarity, [&](Entity &mob){
                            mob.set_parent(player.id);
                            mob.set_color(player.get_color());
                            mob.base_entity = player.id;
                            BitMath::set(mob.flags, EntityFlags::kDieOnParentDeath);
                            BitMath::set(mob.flags, EntityFlags::kNoDrops);
                            BitMath::set(mob.flags, EntityFlags::kIsDetached);
                            mob.set_is_summon(1);
                            // Fixed summon stats scaled x3 per rarity (not the
                            // wild-mob HP curve), so a summon matches its petal
                            // tooltip exactly. See summon_base_* in StaticData.
                            float const s = rarity_pow3(summon_rarity);
                            mob.max_health = mob.health = summon_base_health(spawn_id) * s;
                            mob.damage = summon_base_damage(spawn_id) * s;
                            mob.set_health_ratio(1);
                            // Summons grow gentler than wild mobs: 1.2x/tier
                            // instead of the 1.4x mob_size_mult already applied,
                            // so a high-rarity summon isn't oversized. Divide out
                            // the 1.4x scaling and re-apply 1.2x.
                            mob.set_radius(mob.get_radius() / mob_size_mult(summon_rarity)
                                           * powf(1.2f, (float)summon_rarity));
                            // The petal spawns the summon at its own position; if
                            // the player is hugging a wall that spot can be inside
                            // terrain. Push the summon out so it never starts
                            // embedded in a block.
                            float px = mob.get_x(), py = mob.get_y();
                            Tilemap::push_circle(px, py, mob.get_radius());
                            mob.set_x(px);
                            mob.set_y(py);
                            // Stick's Sandstorms overlay creatures (no hitbox push)
                            // while still dealing their contact damage.
                            if (petal.get_petal_id() == PetalID::kStick)
                                BitMath::set(mob.flags, EntityFlags::kNoPush);
                        });
                    
                        if (petal_data.attributes.spawn_count == 0) {
                            petal_slot.ent_id = mob.id;
                            sim->request_delete(petal.id);
                            break;
                        } else {
                            entity_set_despawn_tick(mob, sec_reload_ticks * petal_data.attributes.spawn_count);
                            petal.secondary_reload = 0;
                            //needed
                            mob.set_parent(petal.id);
                            mob.base_entity = player.id;
                        }
                    }
                } else {
                    //if petal is a mob, or detached (IsDespawning)
                    if (BitMath::at(petal.flags, EntityFlags::kIsDespawning))
                        petal_slot.ent_id = NULL_ENTITY;
                    if (BitMath::at(petal.flags, EntityFlags::kIsDetached))
                        --rot_pos;
                }
            }
            //spread out
            if (petal_data.attributes.clump_radius == 0) ++rot_pos;
        }
        //clump
        if (petal_data.attributes.clump_radius > 0) ++rot_pos;
        player.set_loadout_reloads(i, min_reload * 255);
        player.set_loadout_healths(i, min_health * 255);
    };
    if (BitMath::at(player.input, InputFlags::kAttacking)) 
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kAttacking));
    else if (BitMath::at(player.input, InputFlags::kDefending))
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kDefending));
    if (player.poison_ticks > 0)
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kPoisoned));
    if (player.dandy_ticks > 0)
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kDandelioned));
    if (buffs.yinyang_count != MAX_SLOT_COUNT) {
        switch (buffs.yinyang_count % 3) {
            case 0:
                player.heading_angle += (BASE_PETAL_ROTATION_SPEED + buffs.extra_rot) / TPS;
                break;
            case 1:
                player.heading_angle -= (BASE_PETAL_ROTATION_SPEED + buffs.extra_rot) / TPS;
                break;
            default:
                break;
        }
    } else 
        player.heading_angle += 10 * (BASE_PETAL_ROTATION_SPEED + buffs.extra_rot) / TPS;
}