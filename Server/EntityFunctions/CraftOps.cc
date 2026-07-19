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

void try_craft(Client *client, PetalID::T type, uint8_t rarity, uint32_t amount) {
    if (client == nullptr || client->username.empty()) return;
    if (type == PetalID::kNone || type >= PetalID::kNumPetals) return;
    if (rarity >= RarityID::kSuper) return;   // Super isn't craftable
    if (amount < 5) return;

    std::vector<PetalStack> inv;
    AccountDB::read_inventory(client->username, inv);
    int32_t idx = -1;
    for (size_t i = 0; i < inv.size(); ++i) {
        if (inv[i].type == type && inv[i].rarity == rarity) { idx = (int32_t)i; break; }
    }
    if (idx < 0 || inv[(size_t)idx].count < amount) return;   // can't select more than owned

    float const chance = craft_success_chance(rarity);
    uint32_t remaining = amount;
    uint32_t crafted = 0;
    bool any_success = false;
    while (remaining >= 5) {
        if (frand() < chance) {
            remaining -= 5;
            ++crafted;
            any_success = true;
        } else {
            uint32_t loss = 1 + (uint32_t)(frand() * 4.f);
            if (loss > remaining) loss = remaining;   // defensive; can't trigger (remaining>=5 > loss<=4)
            remaining -= loss;
        }
    }
    uint32_t const consumed = amount - remaining;
    inv[(size_t)idx].count -= consumed;
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
