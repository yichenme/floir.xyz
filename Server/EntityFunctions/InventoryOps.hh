#pragma once

#include <Shared/PetalItem.hh>
#include <Shared/StaticDefinitions.hh>

#include <cstdint>
#include <vector>

class Client;
class Entity;
class Simulation;

// Account-backed inventory mutations: store/equip/pickup/persist/sync. Keeps
// Server/Client.cc, Process/Collision.cc, and EntityFunctions/Death.cc as
// thin dispatchers; this module owns the (type,rarity) stack rules on top of
// Server/Account/Database.hh (AccountDB). Entity loadout fields remain the
// in-simulation source of truth for what's currently equipped.
namespace InventoryOps {
    void add_stack(std::vector<PetalStack> &inv, PetalID::T type, uint8_t rarity, uint64_t n = 1);
    bool take_one(std::vector<PetalStack> &inv, uint32_t index, PetalItem &out);

    // client may be null (e.g. CPU-controlled cameras have no account); calls
    // that would touch account state become no-ops in that case.
    void store_from_loadout(Client *client, Entity &player, uint8_t static_pos);
    void equip_to_loadout(Client *client, Entity &player, uint32_t inv_index, uint8_t static_pos);
    void pickup_drop(Simulation *sim, Client *client, Entity &player, Entity &drop);

    void sync_inventory_update(Client *client);
    void persist_account_petals(Client *client, Entity &player);
    void apply_account_loadout_to_camera(Client *client, Entity &camera);
}
