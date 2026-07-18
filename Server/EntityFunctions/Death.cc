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
#include <Shared/RarityScale.hh>
#include <Shared/Simulation.hh>
#include <Shared/Tilemap.hh>

#include <algorithm>
#include <iostream>
#include <utility>

// One dropped item: petal kind, its rolled rarity, and a stack count (>1 only
// for a Unique mob's 10x-Ultra drop, which lands as a single stacked entity).
struct DropSpec { PetalID::T id; uint8_t rarity; uint8_t count; };

static void _alloc_drops(Simulation *sim, std::vector<DropSpec> &drops, float x, float y, uint32_t owner) {
    // Keep only one Unique-tier petal of a kind in the world at a time.
    for (size_t i = drops.size(); i > 0; --i) {
        PetalID::T const id = drops[i - 1].id;
        if (PETAL_DATA[id].rarity == RarityID::kUnique && PetalTracker::get_count(sim, id) > 0)
            drops.erase(drops.begin() + (i - 1));
    }
    // Nudge the spawn point out of any wall/water the mob died inside, so loot
    // never materialises embedded in terrain (motion keeps it out afterwards).
    float sx = x, sy = y;
    Tilemap::push_circle(sx, sy, 25);
    size_t const count = drops.size();
    for (size_t i = 0; i < count; ++i) {
        Entity &drop = alloc_drop(sim, drops[i].id, drops[i].rarity, owner);
        drop.set_drop_count(drops[i].count < 1 ? 1 : drops[i].count);
        drop.set_x(sx);
        drop.set_y(sy);
        if (count > 1)
            drop.velocity.unit_normal(i * 2 * M_PI / count).set_magnitude(25);
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
            uint8_t const mob_rarity = ent.get_mob_rarity();
            // Individual loot: a player loots a mob only if they dealt at least a
            // rarity-based fraction of its max HP, and only the top-damage players
            // loot, capped by rarity. Each eligible player gets their OWN drop set
            // (owned drops are visible/collectable only by that player).
            float threshold_pct = 0.05f;   // Common..Ultra
            uint32_t max_looters = 4;
            if (mob_rarity == RarityID::kSuper)       { threshold_pct = 0.01f;  max_looters = 25; }
            else if (mob_rarity == RarityID::kUnique) { threshold_pct = 0.005f; max_looters = 100; }
            float const threshold = ent.max_health * threshold_pct;
            std::vector<std::pair<uint16_t, float>> looters;
            for (auto const &kv : ent.mob_damage)
                if (kv.second >= threshold) looters.push_back({kv.first, kv.second});
            std::sort(looters.begin(), looters.end(),
                [](auto const &a, auto const &b){ return a.second > b.second; });
            if (looters.size() > max_looters) looters.resize(max_looters);
            bool credited_kill = false;
            for (auto const &looter : looters) {
                std::vector<DropSpec> drops;
                // Every item in the mob's drop list drops (at a rolled rarity),
                // so a kill yields the full loot set. A Unique mob instead gives
                // each item as a single 10x-Ultra stack (99.9%) or one Super
                // (0.1%) -- no more rolling the Super row ten times.
                for (uint32_t i = 0; i < mob_data.drops.size(); ++i) {
                    if (mob_rarity == RarityID::kUnique) {
                        if (frand() < 0.001f)
                            drops.push_back({mob_data.drops[i], (uint8_t)RarityID::kSuper, 1});
                        else
                            drops.push_back({mob_data.drops[i], (uint8_t)RarityID::kUltra, 10});
                    } else {
                        uint8_t const rar = roll_drop_rarity(mob_rarity);
                        if (rar != DROP_NOTHING)
                            drops.push_back({mob_data.drops[i], rar, 1});
                    }
                }
                _alloc_drops(sim, drops, ent.get_x(), ent.get_y(), looter.first);
                // Credit this mob (at its rarity) to each eligible looter's
                // account for the Mob Gallery kill tally. The gallery is only
                // viewable at the title screen, so the client copy is pushed on
                // login and on flower death (below), not per-kill.
                Client *lc = Server::game.client_for_camera_id(looter.first);
                if (lc != nullptr && !lc->username.empty()) {
                    AccountDB::add_kill(lc->username, (uint8_t)ent.get_mob_id(), mob_rarity);
                    credited_kill = true;
                }
            }
            if (credited_kill) AccountDB::save();
            // Announce Super/Unique mob kills in everyone's chat (coloured, no
            // label). With a squad, name every squad member (see Squad); solo,
            // just the killer.
            if (mob_rarity >= RarityID::kSuper) {
                std::string killer_name = "someone";
                if (sim->ent_exists(ent.last_damaged_by)) {
                    EntityID kid = sim->get_ent(ent.last_damaged_by).base_entity;
                    if (sim->ent_alive(kid) && sim->get_ent(kid).has_component(kName)
                        && !sim->get_ent(kid).get_name().empty())
                        killer_name = sim->get_ent(kid).get_name();
                }
                bool const uniq = mob_rarity >= RarityID::kUnique;
                std::string const msg = std::string("A ") + (uniq ? "unique " : "super ")
                    + MOB_DATA[ent.get_mob_id()].name + " has been killed by " + killer_name + "!";
                Server::game.system_message(uniq ? SystemMsgKind::kSysUnique : SystemMsgKind::kSysSuper, msg);
            }
        }
        if (ent.get_mob_id() == MobID::kAntHole &&
            BitMath::at(ent.flags, EntityFlags::kSpawnedFromZone) &&
            frand() < DIGGER_SPAWN_CHANCE) {
            // The digger is the killer's ALLY -- it must inherit the killing
            // flower's team so it hunts nearby wild mobs instead of chasing the
            // player. Resolve the flower via the damager's base_entity (robust
            // even when the finishing petal has despawned). No owner -> no
            // digger, rather than a team-less one that would target players.
            EntityID owner = NULL_ENTITY;
            if (sim->ent_exists(ent.last_damaged_by))
                owner = sim->get_ent(ent.last_damaged_by).base_entity;
            // Only spawn the ally digger for a player kill: if a wild mob felled
            // the hole, its base_entity has a NULL team and the digger would end
            // up hunting flowers -- the very bug we're fixing.
            if (sim->ent_alive(owner) && sim->get_ent(owner).has_component(kFlower)) {
                Entity &of = sim->get_ent(owner);
                Entity &digger = alloc_mob(sim, MobID::kDigger, ent.get_x(), ent.get_y(),
                    of.get_team(), inherited_spawn_rarity(ent));
                // Parent to the owner so it stays near the player (retreat radius)
                // and only engages mobs within range, like a proper summon.
                digger.set_parent(owner);
            }
        }

    } else if (ent.has_component(kPetal)) {
        if (ent.get_petal_id() == PetalID::kWeb || ent.get_petal_id() == PetalID::kTriweb)
            alloc_web(sim, 100, ent);
    } else if (ent.has_component(kFlower)) {
        // No death loss: petals leave the ECS world here, but nothing is
        // dropped or reshuffled — they're persisted onto the account instead
        // (see InventoryOps::persist_account_petals) and restored unchanged
        // on next spawn via InventoryOps::apply_account_loadout_to_camera.
        for (uint32_t i = 0; i < 2 * ent.get_loadout_count(); ++i) {
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
                AccountDB::write_progress(username, respawn_level, respawn_score);
                AccountDB::save();
            }
        }
        // Returning to the title screen: refresh the Mob Gallery kill tally the
        // client will show (kills accrued during this life are now persisted).
        InventoryOps::sync_kills_update(client);
    } else if (ent.has_component(kDrop)) {
        if (BitMath::at(ent.flags, EntityFlags::kIsDespawning))
            PetalTracker::remove_petal(sim, ent.get_drop_id());
    }
}