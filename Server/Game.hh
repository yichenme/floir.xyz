#pragma once

#include <Server/TeamManager.hh>

#include <Shared/Simulation.hh>

#include <set>

class Client;

class GameInstance {
    std::set<Client *> clients;
    TeamManager team_manager;
    // Ticks since the last periodic progress flush (see persist_alive_progress).
    uint32_t save_counter = 0;
    // Persist every alive, logged-in player's peak progress + loadout to their
    // account, so a server restart/redeploy can't wipe an in-progress run
    // (progress is otherwise only written on flower death).
    void persist_alive_progress();
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
    // Broadcast a coloured system line into everyone's chat (kind = SystemMsgKind).
    void system_message(uint8_t kind, std::string const &text);
    // Disconnect any OTHER connection logged into the same account (multi-tab
    // kick): the newest tab to enter the game wins.
    void kick_other_sessions(Client const *keep, std::string const &username);
};