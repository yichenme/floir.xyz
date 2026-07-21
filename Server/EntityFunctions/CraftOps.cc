#include <Server/EntityFunctions/CraftOps.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/RarityScale.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <Helpers/Math.hh>

#include <vector>

namespace CraftOps {

// `amount` is the number of craft ATTEMPTS the client asked for (1 for a
// single click, or the owned count for a "craft all" double-click / shift-
// click -- the loop just caps itself by availability). Each attempt rolls the
// flat chance: on SUCCESS it consumes EXACTLY 5 and produces 1 petal of
// rarity+1; on FAILURE it does NOT touch the base 5 at all -- it only loses a
// random 1-4 (capped at what's left). So a run like 15 owned might go
// fail(-4)->11, success(-5)->6, fail(-1)->5, success(-5)->0, ending with 2
// crafted -- each attempt's cost depends on its own outcome, not a flat 5+extra
// every time.
void try_craft(Client *client, PetalID::T type, uint8_t rarity, uint32_t amount) {
    if (client == nullptr || client->username.empty()) return;
    if (type == PetalID::kNone || type >= PetalID::kNumPetals) return;
    if (rarity >= RarityID::kUnique) return;   // Unique+ isn't craftable (final tier)
    if (amount < 1) return;

    std::vector<PetalStack> inv;
    AccountDB::read_inventory(client->username, inv);
    int32_t idx = -1;
    for (size_t i = 0; i < inv.size(); ++i) {
        if (inv[i].type == type && inv[i].rarity == rarity) { idx = (int32_t)i; break; }
    }
    if (idx < 0 || inv[(size_t)idx].count < 5) return;   // need at least one attempt's worth

    float const chance = craft_success_chance(rarity);
    uint64_t count = inv[(size_t)idx].count;
    uint32_t attempts_left = amount;
    uint32_t crafted = 0;
    bool any_success = false;
    while (attempts_left > 0 && count >= 5) {
        if (frand() < chance) {
            count -= 5;                   // success: exactly the 5 fed in
            ++crafted;
            any_success = true;
        } else {
            uint32_t extra = 1 + (uint32_t)(frand() * 4.f);   // failure: only 1-4 lost
            if (extra > count) extra = (uint32_t)count;       // capped at what remains
            count -= extra;
        }
        --attempts_left;
    }
    uint32_t const remaining = (uint32_t)count;
    inv[(size_t)idx].count = count;
    if (inv[(size_t)idx].count == 0) inv.erase(inv.begin() + idx);
    uint8_t const out_rarity = rarity + 1;
    if (crafted > 0) InventoryOps::add_stack(inv, type, out_rarity, crafted);

    AccountDB::write_inventory(client->username, inv);
    AccountDB::save();
    InventoryOps::sync_inventory_update(client);

    // Announce every Super/Unique craft success individually -- a batched
    // "craft all" landing 10 successes gets 10 separate messages, not one
    // summarizing line, matching how Spawn.cc's _announce_spawn treats every
    // Super+ wild mob spawn as its own announcement.
    if (crafted > 0 && out_rarity >= RarityID::kSuper) {
        bool const uniq = out_rarity >= RarityID::kUnique;
        Simulation *sim = &client->game->simulation;
        std::string name = "A player";
        if (sim->ent_exists(client->camera)) {
            EntityID const player_id = sim->get_ent(client->camera).get_player();
            if (sim->ent_exists(player_id)) {
                std::string const nm = sim->get_ent(player_id).get_name();
                if (!nm.empty()) name = nm;
            }
        }
        std::string const msg = name + " has crafted a " + (uniq ? "Unique " : "Super ")
            + PETAL_DATA[type].name + "!";
        for (uint32_t i = 0; i < crafted; ++i)
            Server::game.system_message(uniq ? SystemMsgKind::kSysUnique : SystemMsgKind::kSysSuper, msg);
    }

    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kCraftResult);
    writer.write<uint8_t>(type);
    writer.write<uint8_t>(out_rarity);
    writer.write<uint32_t>(crafted);
    writer.write<uint32_t>(remaining);
    writer.write<uint8_t>(any_success ? 1 : 0);
    client->send_packet(writer.packet, writer.at - writer.packet);
}

}
