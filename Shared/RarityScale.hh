#pragma once
#include <Shared/StaticDefinitions.hh>
#include <cstdint>
float rarity_pow3(uint8_t rarity);
float petal_hp_mult(uint8_t rarity);
float petal_damage_mult(uint8_t rarity);
float mob_body_damage_mult(uint8_t rarity);
float mob_xp_mult(uint8_t rarity);
float mob_armor_mult(uint8_t rarity);
float mob_hp_mult(uint8_t rarity);
uint8_t roll_spawn_rarity(uint8_t band_difficulty);
