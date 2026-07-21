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

// Minimum level to be granted the leaderboard-#1 Mjolnir. Below this nobody
// holds it, regardless of leaderboard rank.
static uint32_t const MJOLNIR_MIN_LEVEL = 125;

static void _update_client(Simulation *sim, Client *client) {
    if (client == nullptr) return;
    if (!client->verified) return;
    if (sim == nullptr) return;
    if (!sim->ent_exists(client->camera)) return;
    // These three are REUSED across every client every tick instead of being
    // allocated fresh each call. At 100 players that was 100 std::set + vector
    // allocations per tick, which both cost CPU and ratcheted the Emscripten
    // linear heap (it never shrinks, so transient churn permanently raised RSS
    // -- the "memory build-up"). in_view_mark tags each entity SLOT with the
    // (hash+1) of the in-view entity occupying it, giving O(1) full-EntityID
    // membership (so a recycled slot with a new hash is still detected as a
    // delete, not a false match). Only touched slots are reset at the end.
    static std::vector<EntityID> in_view;
    static std::vector<EntityID> deletes;
    static std::vector<uint32_t> in_view_mark(ENTITY_CAP, 0);
    in_view.clear();
    deletes.clear();
    auto add_view = [&](EntityID e){
        if (e == NULL_ENTITY) return;
        uint32_t const tag = (uint32_t) e.hash + 1;
        if (in_view_mark[e.id] == tag) return;   // this exact entity already added
        in_view_mark[e.id] = tag;
        in_view.push_back(e);
    };
    add_view(client->camera);
    Entity &camera = sim->get_ent(client->camera);
    if (sim->ent_exists(camera.get_player()))
        add_view(camera.get_player());
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kClientUpdate);
    writer.write<EntityID>(client->camera);
    // camera.get_fov() is already clamped where it's set (Flower.cc) so the
    // client's render zoom and this query's radius always agree -- see the
    // comment there for why (a high-rarity Antennae/Observer used to zoom the
    // client out further than what got queried, so mobs in the gap silently
    // never rendered).
    // Query rect padded 20% beyond the visible screen (florr-style) instead of
    // a flat pixel margin, so the pad scales with zoom -- an entity crossing
    // into view is already synced a beat early regardless of FOV.
    sim->spatial_hash.query(camera.get_camera_x(), camera.get_camera_y(),
    960 / camera.get_fov() * 1.2f, 540 / camera.get_fov() * 1.2f,
    [&](Simulation *, Entity &ent){
        add_view(ent.id);
    });

    // Squadmates' camera + flower are always synced, regardless of distance,
    // so the squad HP bars and the [squad] nametag work anywhere on the map.
    uint32_t const squad_id = camera.get_squad_id();
    if (squad_id != 0) {
        for (EntityID const &m : Squad::members(squad_id)) {
            if (!sim->ent_exists(m)) continue;
            add_view(m);
            Entity &mcam = sim->get_ent(m);
            if (sim->ent_exists(mcam.get_player()))
                add_view(mcam.get_player());
        }
    }

    for (EntityID const &i: client->in_view) {
        if (in_view_mark[i.id] != (uint32_t) i.hash + 1) {   // not this exact entity in view
            writer.write<EntityID>(i);
            deletes.push_back(i);
        }
    }

    for (EntityID const &i : deletes)
        client->in_view.erase(i);

    writer.write<EntityID>(NULL_ENTITY);
    //upcreates
    for (EntityID const &id: in_view) {
        DEBUG_ONLY(assert(sim->ent_exists(id));)
        Entity &ent = sim->get_ent(id);
        uint8_t create = !client->in_view.contains(id);
        writer.write<EntityID>(id);
        writer.write<uint8_t>(create | (ent.pending_delete << 1));
        ent.write(&writer, BitMath::at(create, 0));
        client->in_view.insert(id);
    }
    writer.write<EntityID>(NULL_ENTITY);
    // Reset only the slots we touched, keeping the array allocated for next call.
    for (EntityID const &e : in_view) in_view_mark[e.id] = 0;
    //write arena stuff
    writer.write<uint8_t>(!client->seen_arena);
    sim->arena_info.write(&writer, !client->seen_arena);
    client->seen_arena = 1;
    client->send_packet(writer.packet, writer.at - writer.packet);
}

GameInstance::GameInstance() : simulation(), clients(), team_manager(&simulation) {}

void GameInstance::init() {
    // Seed the map to the steady-state mob target (not ENTITY_CAP/2). Every mob
    // ticks every frame on the single game thread, so this count is the main
    // determinant of how many players the server can carry.
    for (uint32_t i = 0; i < MOB_TARGET; ++i)
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
    // Mjolnir requires at least level 125: below that nobody holds it, even the
    // leaderboard #1.
    if (top != nullptr && score_to_level(best) < MJOLNIR_MIN_LEVEL) top = nullptr;
    for (Client *c : clients) {
        if (!c->alive()) continue;
        Entity &cam = simulation.get_ent(c->camera);
        if (!simulation.ent_alive(cam.get_player())) continue;
        Entity &fl = simulation.get_ent(cam.get_player());
        int has = -1;
        for (uint32_t i = 0; i < 2 * fl.get_loadout_count(); ++i)
            if (fl.get_loadout_ids(i) == PetalID::kMjolnir) { has = (int)i; break; }
        bool const has_anywhere = has >= 0 || InventoryOps::has_in_inventory(c, PetalID::kMjolnir);
        bool const should = (c == top);
        if (should && !has_anywhere) {
            // Grant into the inventory, never directly into a loadout slot --
            // forcing it into an occupied slot used to silently destroy
            // whatever petal was equipped there (every full-loadout top player
            // hit this, since an empty slot essentially never existed). The
            // player can equip it themselves like any other petal, which
            // properly swaps the bumped petal back into the inventory instead
            // of deleting it.
            InventoryOps::grant_to_inventory(c, PetalID::kMjolnir, RarityID::kUnique);
            std::string const nm = fl.get_name().empty() ? c->username : fl.get_name();
            system_message(SystemMsgKind::kSysUnique, nm + " is now the Unique Mjolnir owner!");
        } else if (!should && has_anywhere) {
            // Revoke: blank every equipped Mjolnir slot (it just vanishes from
            // the loadout) and purge any unequipped copy sitting in the
            // inventory -- it's a transient leaderboard reward, not something
            // that should persist once ownership changes.
            for (uint32_t i = 0; i < 2 * fl.get_loadout_count(); ++i)
                if (fl.get_loadout_ids(i) == PetalID::kMjolnir) {
                    fl.set_loadout_ids(i, PetalID::kNone);
                    fl.set_loadout_rarities(i, 0);
                    PetalTracker::remove_petal(&simulation, PetalID::kMjolnir);
                }
            InventoryOps::remove_from_inventory_by_type(c, PetalID::kMjolnir);
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