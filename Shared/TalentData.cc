#include <Shared/TalentData.hh>

namespace {
    // Index 0 = no ranks bought yet; index 1..9 = value after buying that
    // many ranks (one per rarity tier, Common..Unique). Values match the
    // reference talent table exactly (percent of base petal health / percent
    // reload-time reduction).
    float const HEALTH_PCT[TALENT_MAX_RANK + 1] = {
        100.f, 115.f, 132.25f, 152.09f, 174.9f, 201.14f, 231.31f, 266.f, 305.9f, 404.56f
    };
    float const RELOAD_REDUCTION_PCT[TALENT_MAX_RANK + 1] = {
        0.f, 10.f, 19.f, 27.1f, 34.39f, 47.51f, 58.01f, 66.41f, 73.13f, 81.19f
    };
    // Cost to buy rank i (1-indexed, i.e. COST[0] is the cost of rank 1).
    uint32_t const HEALTH_COST[TALENT_MAX_RANK] = { 2, 4, 6, 8, 10, 12, 14, 16, 18 };
    uint32_t const RELOAD_COST[TALENT_MAX_RANK] = { 2, 6, 10, 12, 16, 20, 24, 28, 32 };
}

float talent_health_mult(uint8_t rank) {
    if (rank > TALENT_MAX_RANK) rank = TALENT_MAX_RANK;
    return HEALTH_PCT[rank] / 100.f;
}

float talent_reload_mult(uint8_t rank) {
    if (rank > TALENT_MAX_RANK) rank = TALENT_MAX_RANK;
    return 1.f - RELOAD_REDUCTION_PCT[rank] / 100.f;
}

uint32_t talent_rank_cost(TalentTree::T tree, uint8_t rank) {
    if (rank < 1 || rank > TALENT_MAX_RANK) return 0;
    return (tree == TalentTree::kHealth ? HEALTH_COST : RELOAD_COST)[rank - 1];
}

uint32_t talent_cumulative_cost(TalentTree::T tree, uint8_t rank) {
    uint32_t total = 0;
    for (uint8_t r = 1; r <= rank && r <= TALENT_MAX_RANK; ++r)
        total += talent_rank_cost(tree, r);
    return total;
}
