#include <Shared/RarityScale.hh>
#include <Helpers/Math.hh>
#include <algorithm>
#include <cmath>

static float const HP_STEP[8] = {5,5,5,5,10,45,30,5};

float rarity_pow3(uint8_t rarity) {
    return std::pow(3.f, (float)rarity);
}
float petal_hp_mult(uint8_t r) { return rarity_pow3(r); }
float petal_damage_mult(uint8_t r) { return rarity_pow3(r); }
float mob_body_damage_mult(uint8_t r) { return rarity_pow3(r); }
float mob_xp_mult(uint8_t r) { return rarity_pow3(r); }
float mob_armor_mult(uint8_t r) {
    return rarity_pow3(r > RarityID::kUltra ? RarityID::kUltra : r);
}
float mob_hp_mult(uint8_t r) {
    float m = 1.f;
    for (uint8_t i = 0; i < r && i < 8; ++i) m *= HP_STEP[i];
    return m;
}
float mob_size_mult(uint8_t r) {
    // 1.5x per rarity tier, compounding (1.0x at Common, ~25.6x at Unique). No
    // cap: the adaptive-stride terrain collision (Shared/Tilemap.hh push_circle)
    // bounds per-mob cost, so large high-rarity mobs stay performant.
    return std::pow(1.5f, (float)r);
}

// Rarity of a dropped item given the MOB's rarity, or DROP_NOTHING for no drop.
// (Unique mobs roll the Super row 10x -- handled by the caller.)
uint8_t roll_drop_rarity(uint8_t mob_rarity) {
    float const r = frand();
    switch (mob_rarity) {
        case RarityID::kCommon:    return r < 0.60f ? RarityID::kCommon : DROP_NOTHING;
        case RarityID::kUncommon:  return r < 0.40f ? RarityID::kCommon
                                        : (r < 0.80f ? RarityID::kUncommon : DROP_NOTHING);
        case RarityID::kRare:      return r < 0.10f ? RarityID::kRare
                                        : (r < 0.80f ? RarityID::kUncommon : RarityID::kCommon);
        case RarityID::kEpic:      return r < 0.10f ? RarityID::kEpic
                                        : (r < 0.80f ? RarityID::kRare : RarityID::kUncommon);
        case RarityID::kLegendary: return r < 0.10f ? RarityID::kLegendary
                                        : (r < 0.80f ? RarityID::kEpic : RarityID::kRare);
        case RarityID::kMythic:    return r < 0.05f ? RarityID::kMythic
                                        : (r < 0.80f ? RarityID::kLegendary : RarityID::kEpic);
        case RarityID::kUltra:     return r < 0.05f ? RarityID::kUltra
                                        : (r < 0.80f ? RarityID::kMythic : RarityID::kLegendary);
        case RarityID::kSuper:     return r < 0.75f ? RarityID::kUltra : RarityID::kMythic;
        case RarityID::kUnique:    return r < 0.75f ? RarityID::kUltra : RarityID::kMythic;
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
    if (frand() < 0.75f) return band;
    return (uint8_t)(band + 1);
}
