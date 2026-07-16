#pragma once

#include <Shared/StaticDefinitions.hh>

#include <array>
#include <cstdint>

extern uint32_t const MAX_LEVEL;
extern uint32_t const TPS;

extern float const PETAL_DISABLE_DELAY;
extern float const PLAYER_ACCELERATION;
extern float const DEFAULT_FRICTION;
extern float const SUMMON_RETREAT_RADIUS;
extern float const DIGGER_SPAWN_CHANCE;

extern float const BASE_FLOWER_RADIUS;
extern float const BASE_PETAL_ROTATION_SPEED;
extern float const BASE_FOV;
extern float const BASE_HEALTH;
extern float const BASE_BODY_DAMAGE;

extern std::array<struct PetalData, PetalID::kNumPetals> const PETAL_DATA;
extern std::array<struct MobData, MobID::kNumMobs> const MOB_DATA;

// Biomes on the 25000x25500 arena, in override order (get_zone_from_pos picks
// the last match). Zone 0 is the grasslands fallback that fills the middle;
// tundra, desert and jungle overlay on top for their AABBs.
inline std::array const MAP_DATA = std::to_array<struct ZoneDefinition>({
    {
        .left = 0, .top = 2500, .right = 25000, .bottom = 15000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 500000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kBoulder, 10000 },
            { MobID::kMassiveLadybug, 200 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 0, .color = 0xff58c05c, .name = "Grasslands"
    },
    {
        .left = 0, .top = 0, .right = 25000, .bottom = 2500,
        .density = 1, .drop_multiplier = 0.2,
        .spawns = {
            { MobID::kBoulder, 200000 },
            { MobID::kRock, 200000 },
            { MobID::kDarkLadybug, 100000 },
            { MobID::kBee, 50000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 1, .color = 0xff8a5a3c, .name = "Tundra"
    },
    {
        .left = 12500, .top = 5000, .right = 25000, .bottom = 25500,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kBoulder, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kMassiveBeetle, 2000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 2, .color = 0xff3a8f4a, .name = "Jungle"
    },
    {
        .left = 0, .top = 14000, .right = 14000, .bottom = 25500,
        .density = 1, .drop_multiplier = 0.15,
        .spawns = {
            { MobID::kCactus, 400000 },
            { MobID::kBeetle, 100000 },
            { MobID::kSandstorm, 50000 },
            { MobID::kScorpion, 50000 },
            { MobID::kDesertCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kShinyLadybug, 1000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3, .color = 0xffe6c98a, .name = "Desert"
    }
});

std::array const ANTHOLE_SPAWNS = std::to_array<StaticArray<MobID::T, 3>>({
    {MobID::kBabyAnt},
    {MobID::kWorkerAnt,MobID::kBabyAnt},
    {MobID::kWorkerAnt,MobID::kWorkerAnt},
    {MobID::kSoldierAnt,MobID::kWorkerAnt},
    {MobID::kBabyAnt,MobID::kWorkerAnt,MobID::kSoldierAnt},
    {MobID::kWorkerAnt,MobID::kSoldierAnt},
    {MobID::kSoldierAnt,MobID::kWorkerAnt,MobID::kWorkerAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt},
    {MobID::kQueenAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt,MobID::kSoldierAnt}
});

extern std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_CHANCES;

extern uint32_t score_to_pass_level(uint32_t);
extern uint32_t score_to_level(uint32_t);
extern uint32_t level_to_score(uint32_t);
extern uint32_t loadout_slots_at_level(uint32_t);

extern float hp_at_level(uint32_t);