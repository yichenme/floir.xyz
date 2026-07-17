#include <Server/EntityFunctions.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/Game.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>

#include <iostream>

static void _alloc_drops(Simulation *sim, std::vector<PetalID::T> &success_drops, float x, float y, uint8_t rarity) {
    #ifdef DEBUG
    for (PetalID::T id : success_drops)
        assert(id != PetalID::kNone && id < PetalID::kNumPetals);
    #endif
    size_t count = success_drops.size();
    for (size_t i = count; i > 0; --i) {
        PetalID::T drop_id = success_drops[i - 1];
        if (PETAL_DATA[drop_id].rarity == RarityID::kUnique && PetalTracker::get_count(sim, drop_id) > 0) {
            success_drops[i - 1] = success_drops[count - 1];
            --count;
            success_drops.pop_back();
        }
    }
    DEBUG_ONLY(assert(success_drops.size() == count);)
    if (count > 1) {
        for (size_t i = 0; i < count; ++i) {
            Entity &drop = alloc_drop(sim, success_drops[i], rarity);
            drop.set_x(x);
            drop.set_y(y);
            drop.velocity.unit_normal(i * 2 * M_PI / count).set_magnitude(25);
        }
    } else if (count == 1) {
        Entity &drop = alloc_drop(sim, success_drops[0], rarity);
        drop.set_x(x);
        drop.set_y(y);
    }
}

static void _add_score(Simulation *sim, EntityID const killer_id, Entity const &target) {
    if (!sim->ent_exists(killer_id)) return;
    Entity &killer = sim->get_ent(killer_id);
    if (killer.has_component(kScore)) {
        killer.set_score(killer.get_score() + target.score_reward);
        if (target.has_component(kMob))
            killer.set_mobs_killed(killer.get_mobs_killed() + 1);
        // Continuously bank the peak level on the camera so it persists across
        // deaths and can only ever grow (XP is never lost on respawn).
        if (sim->ent_exists(killer.get_parent())) {
            Entity &cam = sim->get_ent(killer.get_parent());
            if (cam.has_component(kCamera)) {
                uint32_t lvl = score_to_level(killer.get_score());
                if (lvl > cam.get_respawn_level()) cam.set_respawn_level(lvl);
            }
        }
    }
}

void entity_on_death(Simulation *sim, Entity const &ent) {
    //don't do on_death for any despawned entity
    uint8_t natural_despawn = BitMath::at(ent.flags, EntityFlags::kIsDespawning) && ent.despawn_tick == 0;
    if (ent.score_reward > 0 && sim->ent_exists(ent.last_damaged_by) && !natural_despawn) {
        EntityID killer_id = sim->get_ent(ent.last_damaged_by).base_entity;
        _add_score(sim, killer_id, ent);
    }
    if (ent.has_component(kFlower) && sim->ent_alive(ent.get_parent())) {
        Entity &camera = sim->get_ent(ent.get_parent());
        EntityID killer_id = sim->ent_exists(ent.last_damaged_by) ?
            sim->get_ent(ent.last_damaged_by).base_entity : NULL_ENTITY;
        if (sim->ent_alive(killer_id)) {
            Entity const &killer = sim->get_ent(killer_id);
            if (killer.has_component(kName)) camera.set_killed_by(killer.get_name());
            else camera.set_killed_by("");
        } else if (ent.poison_ticks > 0) camera.set_killed_by("Poison");
        else camera.set_killed_by("");
    }
    if (ent.has_component(kMob)) {
        if (BitMath::at(ent.flags, EntityFlags::kSpawnedFromZone))
            Map::remove_mob(sim, ent.zone);
        if (!natural_despawn && !(BitMath::at(ent.flags, EntityFlags::kNoDrops))) {
            struct MobData const &mob_data = MOB_DATA[ent.get_mob_id()];
            std::vector<PetalID::T> success_drops = {};
            StaticArray<float, MAX_DROPS_PER_MOB> const &drop_chances = MOB_DROP_CHANCES[ent.get_mob_id()];
            for (uint32_t i = 0; i < mob_data.drops.size(); ++i) 
                if (frand() < drop_chances[i]) success_drops.push_back(mob_data.drops[i]);
            _alloc_drops(sim, success_drops, ent.get_x(), ent.get_y(), ent.get_mob_rarity());
        }
        if (ent.get_mob_id() == MobID::kAntHole && 
            BitMath::at(ent.flags, EntityFlags::kSpawnedFromZone) && 
            frand() < DIGGER_SPAWN_CHANCE) { 
            EntityID team = NULL_ENTITY;
            if (sim->ent_exists(ent.last_damaged_by))
                team = sim->get_ent(ent.last_damaged_by).get_team();
            alloc_mob(sim, MobID::kDigger, ent.get_x(), ent.get_y(), team, inherited_spawn_rarity(ent));
        }

    } else if (ent.has_component(kPetal)) {
        if (ent.get_petal_id() == PetalID::kWeb || ent.get_petal_id() == PetalID::kTriweb)
            alloc_web(sim, 100, ent);
    } else if (ent.has_component(kFlower)) {
        // No death loss: petals leave the ECS world here, but nothing is
        // dropped or reshuffled — they're persisted onto the account instead
        // (see InventoryOps::persist_account_petals) and restored unchanged
        // on next spawn via InventoryOps::apply_account_loadout_to_camera.
        for (uint32_t i = 0; i < ent.get_loadout_count() + MAX_SLOT_COUNT; ++i) {
            DEBUG_ONLY(assert(ent.get_loadout_ids(i) < PetalID::kNumPetals));
            PetalTracker::remove_petal(sim, ent.get_loadout_ids(i));
        }
        // ent.get_parent() (the camera) may already be gone on disconnect, in
        // which case there's no live Client; fall back to the account_name
        // that was stamped onto the flower at spawn so persistence still
        // happens for a disconnecting player.
        Client *client = sim->ent_alive(ent.get_parent()) ?
            Server::game.client_for_camera(ent.get_parent()) : nullptr;
        InventoryOps::persist_account_petals(client, sim->get_ent(ent.id));
        if (!sim->ent_alive(ent.get_parent()))
            return;
        Entity &camera = sim->get_ent(ent.get_parent());
        // No death loss: preserve the EXACT peak score ever reached, so even
        // partial XP within a level survives death (respawn restores this score,
        // not the level's base). Score only grows, never drops.
        uint32_t respawn_score = std::max(ent.get_score(), camera.get_respawn_score());
        uint32_t respawn_level = score_to_level(respawn_score);
        if (respawn_level < 1) respawn_level = 1;
        if (respawn_level > MAX_LEVEL) {
            respawn_level = MAX_LEVEL;
            respawn_score = std::min(respawn_score, level_to_score(MAX_LEVEL));
        }
        camera.set_respawn_score(respawn_score);
        camera.set_respawn_level(respawn_level);
        // Persist the peak level/XP onto the account too, so progress survives
        // across sessions (a fresh connection), not just across deaths within
        // one connection (camera.respawn_level alone only lives as long as the
        // camera entity does). Same username fallback as persist_account_petals.
        {
            std::string const username = client != nullptr ? client->username : ent.get_account_name();
            if (!username.empty()) {
                AccountDB::write_progress(username, (uint8_t)respawn_level, respawn_score);
                AccountDB::save();
            }
        }
    } else if (ent.has_component(kDrop)) {
        if (BitMath::at(ent.flags, EntityFlags::kIsDespawning))
            PetalTracker::remove_petal(sim, ent.get_drop_id());
    }
}