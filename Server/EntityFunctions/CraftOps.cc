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
// click -- the loop just caps itself by availability). Each attempt requires
// 5 of (type, rarity): it consumes those 5, rolls the flat chance, and on
// success produces 1 petal of rarity+1. Regardless of success or failure, an
// EXTRA random 1-4 petals are then destroyed (capped at what's left), so
// every attempt burns 5 + (1..4) of the stack.
void try_craft(Client *client, PetalID::T type, uint8_t rarity, uint32_t amount) {
    if (client == nullptr || client->username.empty()) return;
    if (type == PetalID::kNone || type >= PetalID::kNumPetals) return;
    if (rarity >= RarityID::kSuper) return;   // Super isn't craftable
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
        count -= 5;                       // the 5 fed into this attempt
        if (frand() < chance) { ++crafted; any_success = true; }
        uint32_t extra = 1 + (uint32_t)(frand() * 4.f);   // 1..4 always lost on top
        if (extra > count) extra = (uint32_t)count;       // capped at what remains
        count -= extra;
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

    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kCraftResult);
    writer.write<uint8_t>(type);
    writer.write<uint8_t>(out_rarity);
    writer.write<uint32_t>(crafted);
    writer.write<uint32_t>(remaining);
    writer.write<uint8_t>(any_success ? 1 : 0);
    client->send_packet(writer.packet, writer.at - writer.packet);

    if (out_rarity == RarityID::kSuper && crafted > 0 && client->game != nullptr) {
        std::string name = client->username;
        if (client->alive()) {
            Simulation *sim = &client->game->simulation;
            Entity &camera = sim->get_ent(client->camera);
            if (sim->ent_exists(camera.get_player())) {
                std::string const nm = sim->get_ent(camera.get_player()).get_name();
                if (!nm.empty()) name = nm;
            }
        }
        std::string const msg = "A super " + std::string(PETAL_DATA[type].name)
            + " has been crafted by " + name + "!";
        client->game->system_message(SystemMsgKind::kSysSuper, msg);
    }
}

}
