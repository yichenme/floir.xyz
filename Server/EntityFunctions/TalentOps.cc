#include <Server/EntityFunctions/TalentOps.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/TalentData.hh>

namespace TalentOps {

// Buys exactly the NEXT rank in `tree` (Common..Unique, one rank per rarity
// tier) if the account has enough unspent TP (1 per 5 levels, shared across
// both trees). Doesn't require a live player -- level/xp comes straight from
// AccountDB::read_progress, not the live entity -- but if a camera exists
// (the common case, since the talent panel only opens in-game) its live
// rank fields are updated too so the effect applies immediately without a
// respawn; see Spawn.cc/Flower.cc for where those fields actually get read.
void try_buy(Client *client, uint8_t tree) {
    if (client == nullptr || client->username.empty()) return;
    if (tree >= TalentTree::kNumTrees) return;

    uint8_t health_rank = 0, reload_rank = 0;
    AccountDB::read_talents(client->username, health_rank, reload_rank);
    uint8_t &target_rank = tree == TalentTree::kHealth ? health_rank : reload_rank;
    if (target_rank >= TALENT_MAX_RANK) return;   // already maxed

    uint32_t level = 0, xp = 0;
    AccountDB::read_progress(client->username, level, xp);
    uint32_t const total_tp = level / 5;
    uint32_t const spent = talent_cumulative_cost(TalentTree::kHealth, health_rank)
                          + talent_cumulative_cost(TalentTree::kReload, reload_rank);
    uint32_t const cost = talent_rank_cost((TalentTree::T)tree, target_rank + 1);
    if (spent + cost > total_tp) return;   // not enough unspent TP

    ++target_rank;
    AccountDB::write_talents(client->username, health_rank, reload_rank);
    AccountDB::save();

    if (client->game != nullptr) {
        Simulation *sim = &client->game->simulation;
        if (sim->ent_exists(client->camera)) {
            Entity &camera = sim->get_ent(client->camera);
            camera.set_talent_health_rank(health_rank);
            camera.set_talent_reload_rank(reload_rank);
        }
    }

    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kTalentUpdate);
    writer.write<uint8_t>(health_rank);
    writer.write<uint8_t>(reload_rank);
    writer.write<uint32_t>(total_tp);
    client->send_packet(writer.packet, writer.at - writer.packet);
}

}
