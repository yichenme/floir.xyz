#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/RarityScale.hh>
#include <Shared/TalentData.hh>
#include <Shared/Tilemap.hh>

#include <cmath>

struct PlayerBuffs {
    float extra_rot = 0;
    float extra_range = 0;
    float magnet_range = 0;
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
    // Reload talent: a flat account-wide multiplier on top of every petal's
    // own reload_factor contributions below, read off the owning camera (the
    // only place it's cached -- see Client.cc's kClientSpawn and
    // TalentOps::try_buy for where it gets written).
    if (sim->ent_alive(player.get_parent()))
        buffs.reload_factor *= talent_reload_mult(sim->get_ent(player.get_parent()).get_talent_reload_rank());
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot const &slot = player.loadout[i];
        PetalID::T slot_petal_id = slot.get_petal_id();
        struct PetalData const &petal_data = PETAL_DATA[slot_petal_id];
        struct PetalAttributes const &attrs = petal_data.attributes;
        if (attrs.equipment != EquipmentFlags::kNone)
            player.set_equip_flags(player.get_equip_flags() | (1 << attrs.equipment));
        // FOV petals widen the view by an explicit per-rarity extra-vision
        // bonus: Common +10%, Uncommon +20%, Rare +35%, Epic +50%,
        // Legendary +75%, Mythic +100%, Ultra +175%, Super +250%,
        // Unique +600% (view radius scales by 1+bonus, vision_factor =
        // 1/(1+bonus)). The camera FOV floor below is this table's own
        // minimum (Unique -> 1/7) so no tier saturates early -- an arbitrary
        // floor higher than the true minimum is what caused the earlier
        // "no vision difference between rarities" bug.
        if (attrs.vision_factor < 1.f)
            buffs.vision_factor = std::min(buffs.vision_factor, 1.f / (1.f + extra_vision_bonus(slot.rarity)));
        buffs.extra_range = std::fmax(attrs.extra_range * (slot.rarity + 1), buffs.extra_range);   // Third Eye: +25 per rarity (base 25)
        buffs.magnet_range = std::fmax(buffs.magnet_range, attrs.magnet_range * (slot.rarity + 1));   // Magnet: +150 per rarity (base 150)
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
    // Petals always orbit the player. (Moon used to be an orbiting petal that
    // other petals rotated around; it's now a free-roaming summoned mob, so
    // there's no petal-as-centre special case anymore.)
    (void)sim;
    return RotationCenter {
        .x = player.get_x(),
        .y = player.get_y(),
        .r = player.get_radius()
    };
}

// Magnet: pulls nearby drops the player is actually eligible to collect
// to their position in EXACTLY 0.1s of continuous pull, regardless of how
// far away the drop started -- not a constant speed (a distant drop would
// otherwise take longer). Achieving a fixed total duration needs to know
// how long a given drop has already been mid-pull, so unlike most of this
// file's other per-tick nudges this one needs a little persistent state per
// drop: magnet_pull_ticks (elapsed ticks since it started, 0 = not
// currently being pulled) and the position it started from
// (magnet_pull_start_x/y), captured once on the first tick it's grabbed.
// Every tick after that just linearly interpolates from that frozen start
// point to the player's CURRENT position (so it also tracks a moving
// player), reaching exactly the player's position the instant elapsed
// ticks hits PULL_TICKS. A drop that leaves range or becomes ineligible
// (owner change) has its counter reset so a later re-entry starts a fresh
// 0.1s pull instead of resuming a stale one; a drop that leaves the
// spatial-hash query box entirely (range shrinks, e.g. Magnet unequipped)
// simply stops being visited and keeps a stale counter until it's captured
// again -- harmless, since it only ever shortens a future pull slightly.
static uint32_t const PULL_TICKS = (uint32_t) std::lround(0.25 * TPS);   // exactly 0.25s
static void _apply_magnet_pull(Simulation *sim, Entity &player, float range) {
    if (range <= 0) return;
    if (!sim->ent_alive(player.get_parent())) return;
    uint32_t const owner_id = player.get_parent().id;
    float const px = player.get_x(), py = player.get_y();
    sim->spatial_hash.query(px, py, range, range, [&](Simulation *, Entity &ent) {
        if (!ent.has_component(kDrop) || ent.pending_delete) return;
        uint32_t const drop_owner = ent.get_drop_owner();
        if (drop_owner != 0 && drop_owner != owner_id) { ent.magnet_pull_ticks = 0; return; }
        Vector d(px - ent.get_x(), py - ent.get_y());
        float const dist = d.magnitude();
        if (dist > range || dist < 1.0f) { ent.magnet_pull_ticks = 0; return; }
        if (ent.magnet_pull_ticks == 0) {
            ent.magnet_pull_start_x = ent.get_x();
            ent.magnet_pull_start_y = ent.get_y();
        }
        if (ent.magnet_pull_ticks < PULL_TICKS) ++ent.magnet_pull_ticks;
        float const ratio = (float) ent.magnet_pull_ticks / (float) PULL_TICKS;
        ent.set_x(ent.magnet_pull_start_x + (px - ent.magnet_pull_start_x) * ratio);
        ent.set_y(ent.magnet_pull_start_y + (py - ent.magnet_pull_start_y) * ratio);
    });
}

void tick_player_behavior(Simulation *sim, Entity &player) {
    if (player.pending_delete) return;
    // A dead player is a frozen corpse: no input, no movement, no petals, no
    // buffs -- just the dead face. Petals were already despawned on enter-dead.
    if (player.get_dead()) {
        player.input = 0;
        player.acceleration.set(0, 0);
        player.set_face_flags((1 << FaceFlags::kDeadEyes) | (1 << FaceFlags::kDefending));
        return;
    }
    DEBUG_ONLY(assert(player.max_health > 0);)
    PlayerBuffs const buffs = _get_petal_passive_buffs(sim, player);
    float health_ratio = player.health / player.max_health;
    if (!player.has_component(kMob)) {
        player.max_health = hp_at_level(score_to_level(player.get_score())) + buffs.extra_health;
        player.damage = BASE_BODY_DAMAGE + buffs.extra_damage;
    }
    player.health = health_ratio * player.max_health;
    if (buffs.magnet_range > 0)
        _apply_magnet_pull(sim, player, buffs.magnet_range);
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
        // Clamped here (not just at the network query in Game.cc's
        // _update_client) so the fov synced to the client -- which drives the
        // client's own render zoom -- always matches the radius the server
        // actually queries/sends entities for. Without this, a high-rarity
        // Antennae/Observer would visually zoom out further than what got
        // queried, so mobs inside the visible-but-unqueried gap silently never
        // rendered. 1/7 is the vision-bonus table's own minimum (Unique
        // Antennae, +600% -> 1/(1+6)) -- matching the floor to that exactly
        // means it only ever engages at the top rarity, not several tiers
        // early.
        camera.set_fov(fclamp(BASE_FOV * buffs.vision_factor, BASE_FOV * (1.f / 7.f), BASE_FOV));
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
                    // Magnet stays at the neutral orbit distance regardless of
                    // attack/defend -- it's meant to sit in place widening pickup
                    // range, not swing in and out like combat petals.
                    if (!petal_data.attributes.fixed_orbit) {
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
                    }
                    wanting *= range;
                    if (petal_data.attributes.clump_radius > 0 && slot.size() > 1) {
                        // Cluster the group into a small ring exactly like the
                        // loadout icon (draw_static_petal): divide the circle by
                        // the ACTUAL instance count (slot.size(), e.g. 3/5 for a
                        // Mythic/Ultra stinger), not the static PETAL_DATA.count,
                        // so in-game penta/trio stingers sit together like the icon
                        // instead of collapsing onto one point.
                        Vector secondary;
                        secondary.unit_normal(2 * M_PI * j / slot.size() + player.heading_angle * 0.2)
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
                            float const size_mult = spawn_id == MobID::kMoon ? 1.1f : 1.2f;
                            mob.set_radius(mob.get_radius() / mob_size_mult(summon_rarity)
                                           * powf(size_mult, (float)summon_rarity));
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