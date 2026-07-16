#include <Server/EntityFunctions/InventoryOps.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>

#include <array>
#include <string>

namespace InventoryOps {

void add_stack(std::vector<PetalStack> &inv, PetalID::T type, uint8_t rarity, uint64_t n) {
    if (type == PetalID::kNone || n == 0) return;
    for (PetalStack &stack : inv) {
        if (stack.type == type && stack.rarity == rarity) {
            stack.count += n;
            return;
        }
    }
    inv.push_back(PetalStack{ type, rarity, n });
}

bool take_one(std::vector<PetalStack> &inv, uint32_t index, PetalItem &out) {
    if (index >= inv.size()) return false;
    PetalStack &stack = inv[index];
    if (stack.count == 0) return false;
    out.type = stack.type;
    out.rarity = stack.rarity;
    if (--stack.count == 0) inv.erase(inv.begin() + index);
    return true;
}

namespace {
    std::vector<PetalStack> load_inventory(Client *client) {
        std::vector<PetalStack> inv;
        AccountDB::read_inventory(client->username, inv);
        return inv;
    }

    void save_inventory(Client *client, std::vector<PetalStack> const &inv) {
        AccountDB::write_inventory(client->username, inv);
        AccountDB::save();
    }
}

void store_from_loadout(Client *client, Entity &player, uint8_t static_pos) {
    if (client == nullptr) return;
    if (static_pos >= player.get_loadout_count() + MAX_SLOT_COUNT) return;
    PetalID::T const id = player.get_loadout_ids(static_pos);
    if (id == PetalID::kNone) return;
    uint8_t const rarity = player.get_loadout_rarities(static_pos);
    PetalTracker::remove_petal(&client->game->simulation, id);
    player.set_loadout_ids(static_pos, PetalID::kNone);
    player.set_loadout_rarities(static_pos, 0);

    std::vector<PetalStack> inv = load_inventory(client);
    add_stack(inv, id, rarity, 1);
    save_inventory(client, inv);
    sync_inventory_update(client);
}

void equip_to_loadout(Client *client, Entity &player, uint32_t inv_index, uint8_t static_pos) {
    if (client == nullptr) return;
    if (static_pos >= player.get_loadout_count() + MAX_SLOT_COUNT) return;
    std::vector<PetalStack> inv = load_inventory(client);
    PetalItem taken;
    if (!take_one(inv, inv_index, taken)) return;

    Simulation *sim = &client->game->simulation;
    PetalID::T const old_id = player.get_loadout_ids(static_pos);
    uint8_t const old_rarity = player.get_loadout_rarities(static_pos);
    // Covers both kEquipPetal (slot empty) and kInventorySwap (slot occupied):
    // whatever was equipped goes back into the stack it came from.
    if (old_id != PetalID::kNone) {
        add_stack(inv, old_id, old_rarity, 1);
        PetalTracker::remove_petal(sim, old_id);
    }
    PetalTracker::add_petal(sim, taken.type);
    player.set_loadout_ids(static_pos, taken.type);
    player.set_loadout_rarities(static_pos, taken.rarity);

    save_inventory(client, inv);
    sync_inventory_update(client);
}

void pickup_drop(Simulation *sim, Client *client, Entity &player, Entity &drop) {
    PetalID::T const id = drop.get_drop_id();
    uint8_t const rarity = drop.get_drop_rarity();
    if (player.has_component(kScore))
        player.set_petals_collected(player.get_petals_collected() + 1);
    for (uint32_t i = 0; i < player.get_loadout_count() + MAX_SLOT_COUNT; ++i) {
        if (player.get_loadout_ids(i) != PetalID::kNone) continue;
        player.set_loadout_ids(i, id);
        player.set_loadout_rarities(i, rarity);
        drop.set_x(player.get_x());
        drop.set_y(player.get_y());
        BitMath::unset(drop.flags, EntityFlags::kIsDespawning);
        sim->request_delete(drop.id);
        //peaceful transfer: petal stays world-tracked, just moves into the loadout
        return;
    }
    // Loadout full: never reject the pickup. Overflow into the account
    // inventory instead (no-op without a logged-in account to store it on).
    if (client == nullptr) return;
    std::vector<PetalStack> inv = load_inventory(client);
    add_stack(inv, id, rarity, 1);
    save_inventory(client, inv);
    sync_inventory_update(client);
    PetalTracker::remove_petal(sim, id);
    drop.set_x(player.get_x());
    drop.set_y(player.get_y());
    BitMath::unset(drop.flags, EntityFlags::kIsDespawning);
    sim->request_delete(drop.id);
}

void sync_inventory_update(Client *client) {
    if (client == nullptr) return;
    std::vector<PetalStack> const inv = load_inventory(client);
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kInventoryUpdate);
    writer.write<uint32_t>(inv.size());
    for (PetalStack const &stack : inv) {
        writer.write<uint8_t>(stack.type);
        writer.write<uint8_t>(stack.rarity);
        writer.write<uint64_t>(stack.count);
    }
    client->send_packet(writer.packet, writer.at - writer.packet);
}

void persist_account_petals(Client *client, Entity &player) {
    std::string const username = client != nullptr ? client->username : player.get_account_name();
    if (username.empty()) return;
    std::array<PetalItem, 2 * MAX_SLOT_COUNT> items{};
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        items[i].type = player.get_loadout_ids(i);
        items[i].rarity = player.get_loadout_rarities(i);
    }
    AccountDB::write_loadout(username, items.data());
    AccountDB::save();
}

void apply_account_loadout_to_camera(Client *client, Entity &camera) {
    if (client == nullptr) return;
    std::array<PetalItem, 2 * MAX_SLOT_COUNT> items{};
    AccountDB::read_loadout(client->username, items.data());
    Simulation *sim = &client->game->simulation;
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        PetalTracker::remove_petal(sim, camera.get_inventory(i));
        camera.set_inventory(i, items[i].type);
        camera.set_inventory_rarity(i, items[i].rarity);
        PetalTracker::add_petal(sim, items[i].type);
    }
}

}
