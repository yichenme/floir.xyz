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
};
