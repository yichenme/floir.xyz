#pragma once
#include <Shared/StaticDefinitions.hh>
#include <cstdint>
float rarity_pow3(uint8_t rarity);
float mob_rarity_mult(uint8_t rarity);
float petal_hp_mult(uint8_t rarity);
float petal_damage_mult(uint8_t rarity);
float mob_body_damage_mult(uint8_t rarity);
float mob_xp_mult(uint8_t rarity);
float mob_armor_mult(uint8_t rarity);
float mob_hp_mult(uint8_t rarity);
float mob_size_mult(uint8_t rarity);
uint8_t roll_spawn_rarity(uint8_t band_difficulty);

// Drop-rarity table keyed on the mob's rarity; returns DROP_NOTHING for no drop.
constexpr uint8_t DROP_NOTHING = 255;
uint8_t roll_drop_rarity(uint8_t mob_rarity);

// Flat chance to turn 5 petals of `rarity` (Common=0..Ultra=6) into 1 of
// rarity+1: 64% at Common, halving per tier, 1% at Ultra, 0 at/above Super
// (not craftable). Shared so the server's actual roll and the client's
// displayed "?% success chance" can't drift apart.
float craft_success_chance(uint8_t rarity);
