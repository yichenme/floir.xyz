#include <Shared/RarityScale.hh>
#include <Helpers/Math.hh>
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
