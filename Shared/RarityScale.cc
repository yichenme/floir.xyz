#include <Shared/RarityScale.hh>
#include <Helpers/Math.hh>
#include <algorithm>
#include <cmath>

// Plain 3x per tier, compounding. Used identically by petals AND mobs (HP,
// damage, XP) -- every rarity step up is exactly 3x the step before it, all
// the way from Common to Unique. No special-cased jump at any tier.
float rarity_pow3(uint8_t rarity) {
    return std::pow(3.f, (float)rarity);
}

// Mob damage/XP scaling: same flat 3x/tier curve as HP (mob_hp_mult) and
// petals (rarity_pow3).
float mob_rarity_mult(uint8_t rarity) {
    return rarity_pow3(rarity);
}
// Flat craft chance: 64% at Common->Uncommon, halving each tier up
// (32/16/8/4/2%), 1% at Ultra->Super, 0 at/above Super (uncraftable). The
// pity/attempt-scaling system was intentionally removed -- these are the
// fixed per-tier odds the crafting page displays and rolls against.
float craft_success_chance(uint8_t rarity) {
    if (rarity >= RarityID::kSuper) return 0.f;
    return 0.64f * std::pow(0.5f, (float)rarity);
}
float extra_vision_bonus(uint8_t rarity) {
    static float const B[RarityID::kNumRarities] = {
        0.10f, 0.20f, 0.35f, 0.50f, 0.75f, 1.00f, 1.75f, 2.50f, 6.00f
    };
    uint8_t const r = rarity < RarityID::kNumRarities ? rarity : (uint8_t)(RarityID::kNumRarities - 1);
    return B[r];
}
float petal_hp_mult(uint8_t r) { return rarity_pow3(r); }
float petal_damage_mult(uint8_t r) { return rarity_pow3(r); }
float mob_body_damage_mult(uint8_t r) { return mob_rarity_mult(r); }
float mob_xp_mult(uint8_t r) { return mob_rarity_mult(r); }
float mob_armor_mult(uint8_t r) {
    return rarity_pow3(r > RarityID::kUltra ? RarityID::kUltra : r);
}
// Per-tier mob HP step (cumulative product), NOT a flat 3x -- each transition
// has its own multiplier: Common->Uncommon 3.75, Uncommon->Rare 3.6,
// Rare->Epic 4, Epic->Legendary 6, Legendary->Mythic 9.75, Mythic->Ultra
// 810/13 (~62.31), Ultra->Super 200/9 (~22.22), Super->Unique 7.5 -- nerfed
// to 50% of the original 15 (a prior request had briefly 10x'd this to 150,
// since reverted and halved down from the original per explicit follow-up).
// Only the Unique tier changes, every tier at Super and below is untouched.
static float const HP_STEP[8] = {
    3.75f, 3.6f, 4.f, 6.f, 9.75f, 810.f / 13.f, 200.f / 9.f, 7.5f
};
float mob_hp_mult(uint8_t r) {
    float m = 1.f;
    for (uint8_t i = 0; i < r && i < 8; ++i) m *= HP_STEP[i];
    return m;
}
float mob_size_mult(uint8_t r) {
    // 1.4x per rarity tier, compounding (1.0x at Common, ~14.8x at Unique). No
    // cap: the adaptive-stride terrain collision (Shared/Tilemap.hh push_circle)
    // bounds per-mob cost, so large high-rarity mobs stay performant.
    return std::pow(1.4f, (float)r);
}

// Rarity of a dropped item given the MOB's rarity, or DROP_NOTHING for no drop.
// (Unique mobs roll the Super row 10x -- handled by the caller.)
uint8_t roll_drop_rarity(uint8_t mob_rarity) {
    // A dropped item is either the mob's own rarity ("same") or one tier lower,
    // with the same-rarity odds shrinking sharply as rarity climbs. Unique is
    // never a drop rarity (Super mobs cap the ladder at Super/Ultra).
    float const r = frand();
    switch (mob_rarity) {
        case RarityID::kCommon:    return RarityID::kCommon;
        case RarityID::kUncommon:  return r < 0.64f ? RarityID::kUncommon  : RarityID::kCommon;
        case RarityID::kRare:      return r < 0.32f ? RarityID::kRare      : RarityID::kUncommon;
        case RarityID::kEpic:      return r < 0.16f ? RarityID::kEpic      : RarityID::kRare;
        case RarityID::kLegendary: return r < 0.08f ? RarityID::kLegendary : RarityID::kEpic;
        case RarityID::kMythic:    return r < 0.04f ? RarityID::kMythic    : RarityID::kLegendary;
        case RarityID::kUltra:     return r < 0.02f ? RarityID::kUltra     : RarityID::kMythic;
        case RarityID::kSuper:     return r < 0.0001f ? RarityID::kSuper   : RarityID::kUltra;
        // Unique's drop is handled specially in Death.cc (0.1% Super, else a
        // single 10x-Ultra stack), so this row is unused for Unique mobs.
        case RarityID::kUnique:    return r < 0.001f ? RarityID::kSuper     : RarityID::kUltra;
        default:                   return DROP_NOTHING;
    }
}
uint8_t roll_spawn_rarity(uint8_t band) {
    // band = Common..Ultra (0..6)
    if (band >= RarityID::kUltra) {
        uint8_t r = (frand() < 0.25f) ? RarityID::kMythic : RarityID::kUltra;
        if (r == RarityID::kUltra && frand() < 0.01f) {
            r = RarityID::kSuper;
            if (frand() < 0.01f) r = RarityID::kUnique;
        }
        return r;
    }
    // Mostly spawn at the zone's own rarity; only a small fraction bump one tier
    // higher, so higher-rarity mobs stay rare and don't crowd low-rarity zones.
    if (frand() < 0.90f) return band;
    return (uint8_t)(band + 1);
}
