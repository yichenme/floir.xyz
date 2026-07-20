#pragma once

#include <Shared/StaticDefinitions.hh>

#include <cstdint>

class Client;

// Turns 5 inventory petals of one rarity into (a chance at) 1 of the next
// rarity up. Session-only mechanic on top of the account inventory
// (Shared/PetalItem.hh PetalStack); no synced entity state involved. Flat
// success chance lives in Shared/RarityScale.hh (craft_success_chance) so the
// client's displayed percentage can't drift from what actually gets rolled.
namespace CraftOps {
    // Runs up to `amount` craft ATTEMPTS against `client`'s account inventory
    // (1 = single craft; the owned count = "craft all"). Each attempt consumes
    // 5 of (type, rarity), rolls the flat chance (success -> +1 petal at
    // rarity+1), and ALWAYS destroys an extra random 1-4 on top, until the
    // requested attempts run out or fewer than 5 remain. Sends kCraftResult
    // (and an inventory resync), plus a system message if a Super was crafted.
    // No-ops silently on invalid input (amount<1, rarity uncraftable, or fewer
    // than 5 owned).
    void try_craft(Client *client, PetalID::T type, uint8_t rarity, uint32_t amount);
}
