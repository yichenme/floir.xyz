#pragma once

#include <Server/TeamManager.hh>

#include <Shared/Simulation.hh>

#include <set>

class Client;

class GameInstance {
    std::set<Client *> clients;
    TeamManager team_manager;
public:
    Simulation simulation;
    GameInstance();
    void init();
    void tick();
    void add_client(Client *);
    void remove_client(Client *);
    // Resolves the Client owning a camera without a second registry; used by
    // Collision/Death to reach account state from ECS entities.
    Client *client_for_camera(EntityID const &);
    // Same, but matches by raw entity id (mob_damage / drop-owner keys store the
    // camera's uint16 id without its generation hash).
    Client *client_for_camera_id(EntityID::id_type);
    // Send one packet to every connected client (e.g. a chat broadcast).
    void broadcast(uint8_t const *packet, size_t len);
};