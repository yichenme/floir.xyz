#pragma once

#include <Shared/StaticDefinitions.hh>

#include <cstdint>

class Client;

// Turns 5 inventory petals of one rarity into 1 of the next rarity up.
// Session-only mechanic on top of the account inventory (Shared/PetalItem.hh
// PetalStack); no synced entity state involved. Success chance itself lives
// in Shared/RarityScale.hh (craft_success_chance) so the client's displayed
// percentage can't drift from what actually gets rolled here.
namespace CraftOps {
    // Validates and executes a craft request against `client`'s account
    // inventory: loops rounds -- each success consumes 5 and produces 1
    // petal at rarity+1, each failure loses a random 1-4 -- until fewer than
    // 5 of the selected amount remain. Sends kCraftResult (and an inventory
    // resync) back to the client, and a system message if a Super was
    // crafted. No-ops silently on invalid input (amount<5, rarity
    // uncraftable, or fewer than `amount` actually owned).
    void try_craft(Client *client, PetalID::T type, uint8_t rarity, uint32_t amount);
}
