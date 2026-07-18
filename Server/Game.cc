#include <Server/Game.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>

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
        camera.set_respawn_level((uint8_t)level);
        AccountDB::write_progress(client->username, (uint8_t)level, score);
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

void GameInstance::broadcast(uint8_t const *packet, size_t len) {
    for (Client *client : clients)
        client->send_packet(packet, len);
}

void GameInstance::remove_client(Client *client) {
    DEBUG_ONLY(assert(client->game == this);)
    clients.erase(client);
    if (simulation.ent_exists(client->camera)) {
        Entity &c = simulation.get_ent(client->camera);
        if (simulation.ent_exists(c.get_team()))
            --simulation.get_ent(c.get_team()).player_count;
        if (simulation.ent_exists(c.get_player()))
            simulation.request_delete(c.get_player());
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            PetalTracker::remove_petal(&simulation, c.get_inventory(i));
        simulation.request_delete(client->camera);
    }
    client->game = nullptr;
}