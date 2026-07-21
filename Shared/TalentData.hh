#pragma once
#include <cstdint>

// Two talent trees, each with one purchasable rank per rarity tier
// (Common..Unique -- 9 tiers, matching RarityID::kNumRarities). Rank 0 means
// nothing bought yet; rank N means the first N tiers are owned.
namespace TalentTree {
    enum T : uint8_t {
        kHealth,
        kReload,
        kNumTrees
    };
}

uint8_t const TALENT_MAX_RANK = 9;

// Multiplier applied to a petal's base max_health, indexed by health rank (0-9).
float talent_health_mult(uint8_t rank);

// Multiplier applied to reload TIME (lower = faster reload), indexed by
// reload rank (0-9).
float talent_reload_mult(uint8_t rank);

// TP cost of buying exactly rank `rank` (1-9) in `tree`; 0 for rank 0 or
// out-of-range.
uint32_t talent_rank_cost(TalentTree::T tree, uint8_t rank);

// Total TP spent owning `rank` ranks in `tree` (sum of talent_rank_cost 1..rank).
uint32_t talent_cumulative_cost(TalentTree::T tree, uint8_t rank);
