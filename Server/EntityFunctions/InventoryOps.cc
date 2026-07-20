#include <Server/EntityFunctions/InventoryOps.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>

#include <algorithm>
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
    if (static_pos >= 2 * player.get_loadout_count()) return;
    PetalID::T const id = player.get_loadout_ids(static_pos);
    if (id == PetalID::kNone) return;
    uint8_t const rarity = player.get_loadout_rarities(static_pos);
    PetalTracker::remove_petal(&client->game->simulation, id);
    player.set_loadout_ids(static_pos, PetalID::kNone);
    player.set_loadout_rarities(static_pos, 0);
    // Mjolnir can't be stored into the inventory (that would persist a transient
    // leaderboard reward): unequipping it just discards it.
    if (id == PetalID::kMjolnir) return;

    std::vector<PetalStack> inv = load_inventory(client);
    add_stack(inv, id, rarity, 1);
    save_inventory(client, inv);
    sync_inventory_update(client);
}

void grant_to_inventory(Client *client, PetalID::T type, uint8_t rarity, uint64_t n) {
    if (client == nullptr) return;
    std::vector<PetalStack> inv = load_inventory(client);
    add_stack(inv, type, rarity, n);
    save_inventory(client, inv);
    sync_inventory_update(client);
}

void remove_from_inventory_by_type(Client *client, PetalID::T type) {
    if (client == nullptr) return;
    std::vector<PetalStack> inv = load_inventory(client);
    size_t const before = inv.size();
    inv.erase(std::remove_if(inv.begin(), inv.end(),
        [type](PetalStack const &s){ return s.type == type; }), inv.end());
    if (inv.size() == before) return;
    save_inventory(client, inv);
    sync_inventory_update(client);
}

bool has_in_inventory(Client *client, PetalID::T type) {
    if (client == nullptr) return false;
    std::vector<PetalStack> inv = load_inventory(client);
    for (PetalStack const &s : inv)
        if (s.type == type) return true;
    return false;
}

void equip_to_loadout(Client *client, Entity &player, uint32_t inv_index, uint8_t static_pos) {
    if (client == nullptr) return;
    if (static_pos >= 2 * player.get_loadout_count()) return;
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
    uint8_t count = drop.get_drop_count();
    if (count < 1) count = 1;
    if (player.has_component(kScore))
        player.set_petals_collected(player.get_petals_collected() + count);
    // A stacked drop (Unique's 10x Ultra) can't sit in a single loadout slot, so
    // it goes straight to the account inventory as one stack.
    if (count > 1) {
        if (client != nullptr) {
            std::vector<PetalStack> inv = load_inventory(client);
            add_stack(inv, id, rarity, count);
            save_inventory(client, inv);
            sync_inventory_update(client);
        } else {
            // No account to store the stack (a CPU flower): equip what fits into
            // empty loadout slots so the pickup isn't wholly wasted.
            for (uint32_t i = 0; i < 2 * player.get_loadout_count() && count > 0; ++i) {
                if (player.get_loadout_ids(i) != PetalID::kNone) continue;
                player.set_loadout_ids(i, id);
                player.set_loadout_rarities(i, rarity);
                --count;
            }
        }
        PetalTracker::remove_petal(sim, id);
        drop.set_x(player.get_x());
        drop.set_y(player.get_y());
        BitMath::unset(drop.flags, EntityFlags::kIsDespawning);
        sim->request_delete(drop.id);
        return;
    }
    for (uint32_t i = 0; i < 2 * player.get_loadout_count(); ++i) {
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
        writer.write<uint32_t>(stack.craft_attempt);
    }
    client->send_packet(writer.packet, writer.at - writer.packet);
}

void sync_kills_update(Client *client) {
    if (client == nullptr || client->username.empty()) return;
    std::vector<AccountDB::KillEntry> kills;
    AccountDB::read_kills(client->username, kills);
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kKillsUpdate);
    writer.write<uint32_t>(kills.size());
    for (AccountDB::KillEntry const &k : kills) {
        writer.write<uint8_t>(k.mob);
        writer.write<uint8_t>(k.rarity);
        writer.write<uint64_t>(k.count);
    }
    client->send_packet(writer.packet, writer.at - writer.packet);
}

void persist_account_petals(Client *client, Entity &player) {
    // Key persistence off the flower's OWN account, stamped onto it at spawn --
    // never the socket's currently-bound username. On a same-socket account
    // switch the dying flower's camera has already re-bound to the NEW account;
    // keying off the live client would write this flower's loadout into the
    // wrong account (the cross-account leak / dupe exploit). account_name is
    // always set for a real player flower at spawn; the client fallback only
    // covers oddities where it somehow wasn't.
    std::string username = player.get_account_name();
    if (username.empty() && client != nullptr) username = client->username;
    if (username.empty()) return;
    std::array<PetalItem, 2 * MAX_SLOT_COUNT> items{};
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        items[i].type = player.get_loadout_ids(i);
        items[i].rarity = player.get_loadout_rarities(i);
        // Mjolnir is a transient leaderboard-#1 reward: never persist it to the
        // account, or it would survive losing #1 (and be duplicable).
        if (items[i].type == PetalID::kMjolnir) { items[i].type = PetalID::kNone; items[i].rarity = 0; }
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
