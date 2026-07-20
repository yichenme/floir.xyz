#include <Server/EntityFunctions.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/Game.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>
#include <Server/Squad.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/RarityScale.hh>
#include <Shared/Simulation.hh>
#include <Shared/Tilemap.hh>

#include <algorithm>
#include <iostream>
#include <unordered_map>
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

// Grant `reward` XP to one flower and bank its camera's peak level. count_kill
// bumps mobs_killed (only the actual killer gets that, not squad-mates).
static void _grant_score(Simulation *sim, Entity &flower, uint32_t reward, bool count_kill) {
    if (!flower.has_component(kScore)) return;
    flower.set_score(flower.get_score() + reward);
    if (count_kill) flower.set_mobs_killed(flower.get_mobs_killed() + 1);
    // Continuously bank the peak level on the camera so it persists across
    // deaths and can only ever grow (XP is never lost on respawn).
    if (sim->ent_exists(flower.get_parent())) {
        Entity &cam = sim->get_ent(flower.get_parent());
        if (cam.has_component(kCamera)) {
            uint32_t lvl = score_to_level(flower.get_score());
            if (lvl > cam.get_respawn_level()) cam.set_respawn_level(lvl);
        }
    }
}

static void _add_score(Simulation *sim, EntityID const killer_id, Entity const &target) {
    if (!sim->ent_exists(killer_id)) return;
    Entity &killer = sim->get_ent(killer_id);
    if (!killer.has_component(kScore)) return;

    // Squads share XP: every kill grants its FULL reward to each living squad
    // member's flower (so a squad levels together), not just the killer. The
    // killer alone gets the mobs_killed credit. Unsquadded players (squad_id
    // 0) fall through to the single-award path -- identical to before.
    uint32_t squad_id = 0;
    if (sim->ent_exists(killer.get_parent())) {
        Entity const &kcam = sim->get_ent(killer.get_parent());
        if (kcam.has_component(kCamera)) squad_id = kcam.get_squad_id();
    }
    bool const is_mob_kill = target.has_component(kMob);
    if (squad_id != 0) {
        for (EntityID const member_cam_id : Squad::members(squad_id)) {
            if (!sim->ent_exists(member_cam_id)) continue;
            Entity &mcam = sim->get_ent(member_cam_id);
            if (!mcam.has_component(kCamera)) continue;
            EntityID const flower_id = mcam.get_player();
            if (!sim->ent_alive(flower_id)) continue;
            Entity &mflower = sim->get_ent(flower_id);
            _grant_score(sim, mflower, target.score_reward, is_mob_kill && mflower.id == killer.id);
        }
        return;
    }
    _grant_score(sim, killer, target.score_reward, is_mob_kill);
}

// The flower (base_entity) that dealt the MOST damage to `ent` -- used for
// kill reward + kill-message credit instead of the finishing blow. Falls back
// to last_damaged_by when there's no recorded damage (poison / self-death).
static EntityID _top_damage_killer(Simulation *sim, Entity const &ent) {
    uint32_t best_cam = 0; float best = -1.f;
    for (auto const &kv : ent.mob_damage)
        if (kv.second > best) { best = kv.second; best_cam = kv.first; }
    if (best_cam != 0) {
        Client *c = Server::game.client_for_camera_id(best_cam);
        if (c != nullptr && sim->ent_exists(c->camera)) {
            Entity &cam = sim->get_ent(c->camera);
            if (sim->ent_alive(cam.get_player())) return cam.get_player();
        }
    }
    if (sim->ent_exists(ent.last_damaged_by))
        return sim->get_ent(ent.last_damaged_by).base_entity;
    return NULL_ENTITY;
}

void entity_on_death(Simulation *sim, Entity const &ent) {
    // A real player already ran all of this at the moment it entered the dead
    // state (enter_player_dead_state calls us once, before set_dead). Deleting
    // the corpse afterwards (kLeave/cleanup/respawn) must not repeat the score
    // award, kill credit, or progress persist -- so bail out for a dead flower.
    if (ent.has_component(kFlower) && !ent.has_component(kMob) && ent.get_dead())
        return;
    //don't do on_death for any despawned entity
    uint8_t natural_despawn = BitMath::at(ent.flags, EntityFlags::kIsDespawning) && ent.despawn_tick == 0;
    if (ent.score_reward > 0 && !natural_despawn) {
        // Award the mob's XP to whoever dealt the most damage, not the finishing
        // blow. Non-mob entities (no damage tally) fall back to last_damaged_by.
        EntityID killer_id = ent.has_component(kMob)
            ? _top_damage_killer(sim, ent)
            : (sim->ent_exists(ent.last_damaged_by) ? sim->get_ent(ent.last_damaged_by).base_entity : NULL_ENTITY);
        if (sim->ent_alive(killer_id)) _add_score(sim, killer_id, ent);
    }
    if (ent.has_component(kFlower) && sim->ent_alive(ent.get_parent())) {
        Entity &camera = sim->get_ent(ent.get_parent());
        // Loot-claim damage is NOT purged here on death -- a Yggdrasil revive
        // flips the corpse back to alive on this same flower entity (see
        // Collision.cc's try_yggdrasil_revive) without ever calling
        // player_spawn, so its damage claims must survive. The purge instead
        // happens in player_spawn (Spawn.cc), which only runs on an actual
        // new-flower respawn.
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
            //
            // Squads loot as a unit: a squad's damage is pooled, its required
            // threshold scales with member count (threshold * squad size), and
            // if the squad qualifies every member loots -- if not, none do.
            // Unsquadded players are just squads of one, so this is the exact
            // same behaviour as before when nobody's in a squad.
            float threshold_pct = 0.05f;   // Common..Ultra
            uint32_t max_looters = 6;
            if (mob_rarity == RarityID::kSuper)       { threshold_pct = 0.01f;  max_looters = 25; }
            else if (mob_rarity == RarityID::kUnique) { threshold_pct = 0.005f; max_looters = 100; }
            float const threshold = ent.max_health * threshold_pct;
            struct LootGroup { std::vector<uint16_t> ids; float total_damage = 0; uint32_t squad_id = 0; };
            std::unordered_map<uint32_t, LootGroup> squad_groups;
            std::vector<LootGroup> groups;
            for (auto const &kv : ent.mob_damage) {
                uint32_t squad_id = 0;
                Client *dc = Server::game.client_for_camera_id(kv.first);
                if (dc != nullptr && sim->ent_exists(dc->camera))
                    squad_id = sim->get_ent(dc->camera).get_squad_id();
                if (squad_id == 0) {
                    LootGroup g; g.ids.push_back(kv.first); g.total_damage = kv.second;
                    groups.push_back(std::move(g));
                } else {
                    // Pool the squad's damage; the looter set is the FULL roster
                    // (filled below), not just whoever dealt damage.
                    LootGroup &g = squad_groups[squad_id];
                    g.squad_id = squad_id;
                    g.total_damage += kv.second;
                }
            }
            // A squad loots TOGETHER: every current (alive) squad member gets a
            // drop, even if they dealt no damage themselves.
            for (auto &kv : squad_groups) {
                LootGroup g = std::move(kv.second);
                for (EntityID const &m : Squad::members(g.squad_id)) {
                    if (!sim->ent_alive(m)) continue;
                    g.ids.push_back((uint16_t) m.id);
                }
                if (!g.ids.empty()) groups.push_back(std::move(g));
            }
            // Each qualifying group's total damage must clear threshold*group size.
            groups.erase(std::remove_if(groups.begin(), groups.end(), [&](LootGroup const &g) {
                return g.total_damage < threshold * (float)g.ids.size();
            }), groups.end());
            std::sort(groups.begin(), groups.end(),
                [](LootGroup const &a, LootGroup const &b){ return a.total_damage > b.total_damage; });
            // max_looters caps the number of qualifying GROUPS (a full squad
            // loots together as one unit, even if that puts more than
            // max_looters individual players on the ground).
            if (groups.size() > max_looters) groups.resize(max_looters);
            std::vector<std::pair<uint16_t, float>> looters;
            for (auto const &g : groups)
                for (uint16_t id : g.ids)
                    looters.push_back({id, g.total_damage});
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
                // Find the top total-damage dealer's camera (mirrors
                // _top_damage_killer) so we can read its squad.
                uint32_t best_cam = 0; float best = -1.f;
                for (auto const &kv : ent.mob_damage)
                    if (kv.second > best) { best = kv.second; best_cam = kv.first; }
                uint32_t killer_squad = 0;
                if (best_cam != 0) {
                    Client *c = Server::game.client_for_camera_id(best_cam);
                    if (c != nullptr && sim->ent_exists(c->camera)) {
                        Entity &cam = sim->get_ent(c->camera);
                        killer_squad = cam.get_squad_id();
                        if (sim->ent_alive(cam.get_player())
                            && sim->get_ent(cam.get_player()).has_component(kName)
                            && !sim->get_ent(cam.get_player()).get_name().empty())
                            killer_name = sim->get_ent(cam.get_player()).get_name();
                    }
                }
                // A squad kill is a team effort: name every alive squad member,
                // not just the top dealer ("A, B and C").
                if (killer_squad != 0) {
                    std::vector<std::string> names;
                    for (EntityID const &m : Squad::members(killer_squad)) {
                        if (!sim->ent_exists(m)) continue;
                        Entity &mcam = sim->get_ent(m);
                        if (!sim->ent_alive(mcam.get_player())) continue;
                        std::string const nm = sim->get_ent(mcam.get_player()).get_name();
                        names.push_back(nm.empty() ? std::string("someone") : nm);
                    }
                    if (!names.empty()) {
                        std::string joined;
                        for (size_t k = 0; k < names.size(); ++k) {
                            if (k > 0) joined += (k + 1 == names.size()) ? " and " : ", ";
                            joined += names[k];
                        }
                        killer_name = joined;
                    }
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
        // Persist ONTO THE FLOWER'S OWN account, stamped onto it at spawn --
        // never whatever account is currently bound to this socket. On a
        // same-socket account switch the old flower dies (deferred) AFTER the
        // camera has re-bound to the new account; keying persistence off the
        // live client would leak this flower's loadout + level into the new
        // account (the cross-account dupe / level-force bug).
        std::string const owner = ent.get_account_name();
        Client *client = sim->ent_alive(ent.get_parent()) ?
            Server::game.client_for_camera(ent.get_parent()) : nullptr;
        InventoryOps::persist_account_petals(client, sim->get_ent(ent.id));
        // Does the live camera still belong to THIS flower's account? If the
        // socket switched accounts, the camera is now someone else's -- we must
        // not write this dead flower's score/level onto it.
        bool const camera_is_owner =
            client != nullptr && !owner.empty() && client->username == owner;
        // No death loss: preserve the EXACT peak score ever reached, so even
        // partial XP within a level survives death (respawn restores this score,
        // not the level's base). Score only grows, never drops.
        uint32_t respawn_score = ent.get_score();
        if (camera_is_owner)
            respawn_score = std::max(respawn_score, sim->get_ent(ent.get_parent()).get_respawn_score());
        uint32_t respawn_level = score_to_level(respawn_score);
        if (respawn_level < 1) respawn_level = 1;
        if (respawn_level > MAX_LEVEL) {
            respawn_level = MAX_LEVEL;
            respawn_score = std::min(respawn_score, level_to_score(MAX_LEVEL));
        }
        // Bank onto the live camera only when it's still this account's camera.
        if (camera_is_owner) {
            Entity &camera = sim->get_ent(ent.get_parent());
            camera.set_respawn_score(respawn_score);
            camera.set_respawn_level(respawn_level);
        }
        // Persist the peak level/XP onto the flower's OWN account so progress
        // survives across sessions -- and lands on the RIGHT account even after
        // a same-socket switch.
        if (!owner.empty()) {
            AccountDB::write_progress(owner, respawn_level, respawn_score);
            AccountDB::save();
        }
        // Refresh the (owning) client's Mob Gallery kill tally, if they're still
        // the one on this socket.
        if (camera_is_owner)
            InventoryOps::sync_kills_update(client);
    } else if (ent.has_component(kDrop)) {
        if (BitMath::at(ent.flags, EntityFlags::kIsDespawning))
            PetalTracker::remove_petal(sim, ent.get_drop_id());
    }
}