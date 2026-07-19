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
    // Ticks since the last Mjolnir-ownership recompute (see update_mjolnir_ownership).
    uint32_t mjolnir_counter = 0;
public:
    Simulation simulation;
    GameInstance();
    void init();
    void tick();
    // Persist every alive, logged-in player's peak progress + loadout to their
    // account, so a server restart/redeploy can't wipe an in-progress run
    // (progress is otherwise only written on flower death). Called on the
    // periodic in-tick cadence (see Game.cc) and directly on SIGTERM/SIGINT
    // (see Wasm.cc) so a deploy's process restart can't lose the window since
    // the last periodic flush.
    void persist_alive_progress();
    void add_client(Client *);
    void remove_client(Client *);
    // Tear down the in-game session bound to this client's camera: delete any
    // live flower, drop its loot claims, and reset the camera's progress +
    // loadout to a fresh-account baseline. Called on logout AND on a new
    // authentication so a flower/loadout can never straddle two accounts on
    // the same socket (the cross-account loadout-leak / dupe exploit).
    void reset_session(Client *);
    // Grant the Mjolnir petal to the current level-leaderboard #1 (alive) and
    // revoke it from anyone who is no longer #1. Mjolnir is transient (lives
    // only on the live flower loadout, never persisted), so losing #1 makes it
    // vanish from the loadout. Run on a periodic cadence from tick().
    void update_mjolnir_ownership();
    // Resolves the Client owning a camera without a second registry; used by
    // Collision/Death to reach account state from ECS entities.
    Client *client_for_camera(EntityID const &);
    // Same, but matches by raw entity id (mob_damage / drop-owner keys store the
    // camera's uint16 id without its generation hash).
    Client *client_for_camera_id(EntityID::id_type);
    // Looks up a connected client by account username (case-sensitive, exact
    // match); used by the "/squad-invite <username>" chat command.
    Client *client_for_username(std::string const &);
    // Send one packet to every connected client (e.g. a chat broadcast).
    void broadcast(uint8_t const *packet, size_t len);
    // Broadcast a coloured system line into everyone's chat (kind = SystemMsgKind).
    void system_message(uint8_t kind, std::string const &text);
    // Disconnect any OTHER connection logged into the same account (multi-tab
    // kick): the newest tab to enter the game wins.
    void kick_other_sessions(Client const *keep, std::string const &username);
};