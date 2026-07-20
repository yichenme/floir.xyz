#pragma once
#include <Shared/StaticDefinitions.hh>
#include <cstdint>
struct PetalItem {
    PetalID::T type = PetalID::kNone;
    uint8_t rarity = 0;
};
struct PetalStack {
    PetalID::T type = PetalID::kNone;
    uint8_t rarity = 0;
    uint64_t count = 0;
    // Craft pity counter for this (type,rarity) stack: increments on every
    // failed 5-petal craft attempt, resets to 0 on any success. Raises the
    // stack's own craft_success_chance the more it's failed. Persisted
    // alongside count so it survives across sessions.
    uint32_t craft_attempt = 0;
};
