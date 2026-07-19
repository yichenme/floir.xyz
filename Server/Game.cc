#include <Server/Game.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>
#include <Server/Squad.hh>

#include <algorithm>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Map.hh>

static void _update_client(Simulation *sim, Client *client) {
    if (client == nullptr) return;
    if (!client->verified) return;
    if (sim == nullptr) return;
    if (!sim->ent_exists(client->camera)) return;
    std::set<EntityID> in_view;
    std::vector<EntityID> deletes;
    in_view.insert(client->camera);
    Entity &camera = sim->get_ent(client->camera);
    if (sim->ent_exists(camera.get_player())) 
        in_view.insert(camera.get_player());
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kClientUpdate);
    writer.write<EntityID>(client->camera);
    sim->spatial_hash.query(camera.get_camera_x(), camera.get_camera_y(),
    960 / camera.get_fov() + 50, 540 / camera.get_fov() + 50,
    [&](Simulation *, Entity &ent){
        in_view.insert(ent.id);
    });

    // Squadmates' camera + flower are always synced, regardless of distance,
    // so the squad HP bars and the [squad] nametag work anywhere on the map.
    uint32_t const squad_id = camera.get_squad_id();
    if (squad_id != 0) {
        for (EntityID const &m : Squad::members(squad_id)) {
            if (!sim->ent_exists(m)) continue;
            in_view.insert(m);
            Entity &mcam = sim->get_ent(m);
            if (sim->ent_exists(mcam.get_player()))
                in_view.insert(mcam.get_player());
        }
    }

    for (EntityID const &i: client->in_view) {
        if (!in_view.contains(i)) {
            writer.write<EntityID>(i);
            deletes.push_back(i);
        }
    }

    for (EntityID const &i : deletes)
        client->in_view.erase(i);

    writer.write<EntityID>(NULL_ENTITY);
    //upcreates
    for (EntityID id: in_view) {
        DEBUG_ONLY(assert(sim->ent_exists(id));)
        Entity &ent = sim->get_ent(id);
        uint8_t create = !client->in_view.contains(id);
        writer.write<EntityID>(id);
        writer.write<uint8_t>(create | (ent.pending_delete << 1));
        ent.write(&writer, BitMath::at(create, 0));
        client->in_view.insert(id);
    }
    writer.write<EntityID>(NULL_ENTITY);
    //write arena stuff
    writer.write<uint8_t>(!client->seen_arena);
    sim->arena_info.write(&writer, !client->seen_arena);
    client->seen_arena = 1;
    client->send_packet(writer.packet, writer.at - writer.packet);
}

GameInstance::GameInstance() : simulation(), clients(), team_manager(&simulation) {}

void GameInstance::init() {
    for (uint32_t i = 0; i < ENTITY_CAP / 2; ++i)
        Map::spawn_random_mob(&simulation, frand() * ARENA_WIDTH, frand() * ARENA_HEIGHT);
    #ifdef GAMEMODE_TDM
    team_manager.add_team(ColorID::kBlue);
    team_manager.add_team(ColorID::kRed);
    #endif
}

void GameInstance::tick() {
    simulation.tick();
    for (Client *client : clients)
        _update_client(&simulation, client);
    simulation.post_tick();
    // Flush alive players' progress to disk on a fixed cadence so a restart
    // (deploy) mid-run doesn't discard everything gained since their last death.
    if (++save_counter >= 15 * TPS) {
        save_counter = 0;
        persist_alive_progress();
    }
    // Recompute the leaderboard-#1 Mjolnir holder about once a second.
    if (++mjolnir_counter >= TPS) {
        mjolnir_counter = 0;
        update_mjolnir_ownership();
    }
}

void GameInstance::update_mjolnir_ownership() {
    // Find the highest-score ALIVE player (level leaderboard #1).
    Client *top = nullptr; uint32_t best = 0;
    for (Client *c : clients) {
        if (!c->alive()) continue;
        Entity &cam = simulation.get_ent(c->camera);
        if (!simulation.ent_alive(cam.get_player())) continue;
        uint32_t const sc = simulation.get_ent(cam.get_player()).get_score();
        if (top == nullptr || sc > best) { best = sc; top = c; }
    }
    for (Client *c : clients) {
        if (!c->alive()) continue;
        Entity &cam = simulation.get_ent(c->camera);
        if (!simulation.ent_alive(cam.get_player())) continue;
        Entity &fl = simulation.get_ent(cam.get_player());
        int has = -1;
        for (uint32_t i = 0; i < 2 * fl.get_loadout_count(); ++i)
            if (fl.get_loadout_ids(i) == PetalID::kMjolnir) { has = (int)i; break; }
        bool const should = (c == top);
        if (should && has < 0) {
            // Grant: prefer an empty slot (any row) so we don't bump an equipped
            // petal; fall back to the last secondary slot.
            uint32_t slot = 2 * fl.get_loadout_count() - 1;
            for (uint32_t i = 0; i < 2 * fl.get_loadout_count(); ++i)
                if (fl.get_loadout_ids(i) == PetalID::kNone) { slot = i; break; }
            PetalTracker::remove_petal(&simulation, fl.get_loadout_ids(slot));
            fl.set_loadout_ids(slot, PetalID::kMjolnir);
            fl.set_loadout_rarities(slot, RarityID::kUnique);
            PetalTracker::add_petal(&simulation, PetalID::kMjolnir);
            std::string const nm = fl.get_name().empty() ? c->username : fl.get_name();
            system_message(SystemMsgKind::kSysUnique, nm + " is now the Unique Mjolnir owner!");
        } else if (!should && has >= 0) {
            // Revoke: blank every Mjolnir slot (it just vanishes from the loadout).
            for (uint32_t i = 0; i < 2 * fl.get_loadout_count(); ++i)
                if (fl.get_loadout_ids(i) == PetalID::kMjolnir) {
                    fl.set_loadout_ids(i, PetalID::kNone);
                    fl.set_loadout_rarities(i, 0);
                    PetalTracker::remove_petal(&simulation, PetalID::kMjolnir);
                }
        }
    }
}

void GameInstance::persist_alive_progress() {
    bool dirty = false;
    for (Client *client : clients) {
        if (client == nullptr || !client->logged_in || client->username.empty())
            continue;
        if (!simulation.ent_alive(client->camera))
            continue;
        Entity &camera = simulation.get_ent(client->camera);
        if (!camera.has_component(kCamera) || !simulation.ent_alive(camera.get_player()))
            continue;
        Entity &flower = simulation.get_ent(camera.get_player());
        // Peak score only ever grows (no death loss), so the live flower's
        // current score is the peak worth banking.
        uint32_t score = std::max(flower.get_score(), camera.get_respawn_score());
        uint32_t level = score_to_level(score);
        if (level < 1) level = 1;
        if (level > MAX_LEVEL) {
            level = MAX_LEVEL;
            score = std::min(score, level_to_score(MAX_LEVEL));
        }
        // Bank on the camera too so an immediate death can't regress it, then
        // write through to the account (mirrors the flower-death path).
        camera.set_respawn_score(score);
        camera.set_respawn_level(level);
        AccountDB::write_progress(client->username, level, score);
        InventoryOps::persist_account_petals(client, flower);
        dirty = true;
    }
    if (dirty) AccountDB::save();
}

void GameInstance::add_client(Client *client) {
    DEBUG_ONLY(assert(client->game != this);)
    if (client->game != nullptr)
        client->game->remove_client(client);
    client->game = this;
    clients.insert(client);
    Entity &ent = simulation.alloc_ent();
    ent.add_component(kCamera);
    ent.add_component(kRelations);
    #ifdef GAMEMODE_TDM
    EntityID team = team_manager.get_random_team();
    ent.set_team(team);
    ent.set_color(simulation.get_ent(team).get_color());
    ++simulation.get_ent(team).player_count;
    #else
    ent.set_team(ent.id);
    ent.set_color(ColorID::kYellow); 
    #endif
    
    ent.set_fov(BASE_FOV);
    ent.set_respawn_level(1);
    ent.set_respawn_score(level_to_score(1));
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i) {
        ent.set_inventory(i, PetalID::kBasic);
        ent.set_inventory_rarity(i, PETAL_DATA[PetalID::kBasic].rarity);
    }
    if (frand() < 0.001 && PetalTracker::get_count(&simulation, PetalID::kUniqueBasic) == 0) {
        ent.set_inventory(0, PetalID::kUniqueBasic);
        ent.set_inventory_rarity(0, PETAL_DATA[PetalID::kUniqueBasic].rarity);
    }
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i)
        PetalTracker::add_petal(&simulation, ent.get_inventory(i));
    client->camera = ent.id;
    client->seen_arena = 0;
}

Client *GameInstance::client_for_camera(EntityID const &camera) {
    for (Client *client : clients)
        if (client->camera == camera) return client;
    return nullptr;
}

Client *GameInstance::client_for_camera_id(EntityID::id_type id) {
    for (Client *client : clients)
        if (client->camera.id == id) return client;
    return nullptr;
}

Client *GameInstance::client_for_username(std::string const &username) {
    for (Client *client : clients)
        if (client->logged_in && client->username == username) return client;
    return nullptr;
}

void GameInstance::broadcast(uint8_t const *packet, size_t len) {
    for (Client *client : clients)
        client->send_packet(packet, len);
}

void GameInstance::system_message(uint8_t kind, std::string const &text) {
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kSystemMessage);
    writer.write<uint8_t>(kind);
    writer.write<std::string>(text);
    broadcast(writer.packet, writer.at - writer.packet);
}

void GameInstance::kick_other_sessions(Client const *keep, std::string const &username) {
    if (username.empty()) return;
    // Copy first: disconnect() mutates the client set as it removes them.
    std::vector<Client *> doomed;
    for (Client *client : clients)
        if (client != keep && client->logged_in && client->username == username)
            doomed.push_back(client);
    for (Client *client : doomed)
        client->disconnect(CloseReason::kServer, "Logged in from another tab");
}

void GameInstance::remove_client(Client *client) {
    DEBUG_ONLY(assert(client->game == this);)
    clients.erase(client);
    if (simulation.ent_exists(client->camera)) {
        Squad::leave(&simulation, client->camera);
        Entity &c = simulation.get_ent(client->camera);
        if (simulation.ent_exists(c.get_team()))
            --simulation.get_ent(c.get_team()).player_count;
        if (simulation.ent_exists(c.get_player()))
            simulation.request_delete(c.get_player());
        // Drop this camera's loot claims so a disconnected player can't be
        // credited when a mob they damaged later dies.
        uint32_t const cam_id = client->camera.id;
        simulation.for_each<kMob>([cam_id](Simulation *, Entity &m){ m.mob_damage.erase(cam_id); });
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            PetalTracker::remove_petal(&simulation, c.get_inventory(i));
        simulation.request_delete(client->camera);
    }
    client->game = nullptr;
}

void GameInstance::reset_session(Client *client) {
    if (!simulation.ent_exists(client->camera)) return;
    Entity &cam = simulation.get_ent(client->camera);
    // Kill any live flower -> the client returns to the title screen. This is
    // the core of the fix: a flower (and the loadout it holds) must never
    // survive an account switch on the same socket.
    if (simulation.ent_exists(cam.get_player()))
        simulation.request_delete(cam.get_player());
    // Drop this camera's loot claims -- a switched-away identity shouldn't be
    // credited when a mob it damaged later dies.
    uint32_t const cam_id = client->camera.id;
    simulation.for_each<kMob>([cam_id](Simulation *, Entity &m){ m.mob_damage.erase(cam_id); });
    // Reset progress to a fresh-account baseline; the real account's saved peak
    // is re-applied from the DB on the next spawn (kClientSpawn's read_progress).
    // Without this, the previous account's inflated level/score would persist on
    // the camera and get written back into the NEW account's DB record.
    cam.set_respawn_level(1);
    cam.set_respawn_score(level_to_score(1));
    // Reset the camera loadout/inventory arrays to defaults (mirrors add_client),
    // keeping PetalTracker's global counts balanced.
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        PetalTracker::remove_petal(&simulation, cam.get_inventory(i));
        cam.set_inventory(i, PetalID::kNone);
        cam.set_inventory_rarity(i, 0);
    }
    for (uint32_t i = 0; i < loadout_slots_at_level(1); ++i) {
        cam.set_inventory(i, PetalID::kBasic);
        cam.set_inventory_rarity(i, PETAL_DATA[PetalID::kBasic].rarity);
        PetalTracker::add_petal(&simulation, cam.get_inventory(i));
    }
}