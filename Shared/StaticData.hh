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
// the last match). Each biome is cut into 7 difficulty bands (Common..Ultra,
// see RarityID) along one axis; Garden bands are emitted first, then Jungle,
// then Desert, so later biomes win ties on overlapping edges.
inline std::array const MAP_DATA = std::to_array<struct ZoneDefinition>({
    {
        .left = 0, .top = 0, .right = 12500.f * 1 / 7, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 0, .color = 0xff58c05c, .name = "Garden · Common"
    },
    {
        .left = 12500.f * 1 / 7, .top = 0, .right = 12500.f * 2 / 7, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 1, .color = 0xff50ae53, .name = "Garden · Uncommon"
    },
    {
        .left = 12500.f * 2 / 7, .top = 0, .right = 12500.f * 3 / 7, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 2, .color = 0xff489d4b, .name = "Garden · Rare"
    },
    {
        .left = 12500.f * 3 / 7, .top = 0, .right = 12500.f * 4 / 7, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3, .color = 0xff408c43, .name = "Garden · Epic"
    },
    {
        .left = 12500.f * 4 / 7, .top = 0, .right = 12500.f * 5 / 7, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 4, .color = 0xff387a3a, .name = "Garden · Legendary"
    },
    {
        .left = 12500.f * 5 / 7, .top = 0, .right = 12500.f * 6 / 7, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 5, .color = 0xff306932, .name = "Garden · Mythic"
    },
    {
        .left = 12500.f * 6 / 7, .top = 0, .right = 12500, .bottom = 14000,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 510000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xff28582a, .name = "Garden · Ultra"
    },
    {
        .left = 12500, .top = 0, .right = 25000, .bottom = 25500.f * 1 / 7,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 0, .color = 0xff3a8f4a, .name = "Jungle · Common"
    },
    {
        .left = 12500, .top = 25500.f * 1 / 7, .right = 25000, .bottom = 25500.f * 2 / 7,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 1, .color = 0xff348243, .name = "Jungle · Uncommon"
    },
    {
        .left = 12500, .top = 25500.f * 2 / 7, .right = 25000, .bottom = 25500.f * 3 / 7,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 2, .color = 0xff2f753c, .name = "Jungle · Rare"
    },
    {
        .left = 12500, .top = 25500.f * 3 / 7, .right = 25000, .bottom = 25500.f * 4 / 7,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3, .color = 0xff2a6836, .name = "Jungle · Epic"
    },
    {
        .left = 12500, .top = 25500.f * 4 / 7, .right = 25000, .bottom = 25500.f * 5 / 7,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 4, .color = 0xff255b2f, .name = "Jungle · Legendary"
    },
    {
        .left = 12500, .top = 25500.f * 5 / 7, .right = 25000, .bottom = 25500.f * 6 / 7,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 5, .color = 0xff1f4e28, .name = "Jungle · Mythic"
    },
    {
        .left = 12500, .top = 25500.f * 6 / 7, .right = 25000, .bottom = 25500,
        .density = 1, .drop_multiplier = 0.1,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kRock, 80000 },
            { MobID::kBeetle, 50000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xff1a4122, .name = "Jungle · Ultra"
    },
    {
        .left = 0, .top = 14000, .right = 14000, .bottom = 14000 + 11500.f * 1 / 7,
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
        .difficulty = 0, .color = 0xffe6c98a, .name = "Desert · Common"
    },
    {
        .left = 0, .top = 14000 + 11500.f * 1 / 7, .right = 14000, .bottom = 14000 + 11500.f * 2 / 7,
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
        .difficulty = 1, .color = 0xffd1b67d, .name = "Desert · Uncommon"
    },
    {
        .left = 0, .top = 14000 + 11500.f * 2 / 7, .right = 14000, .bottom = 14000 + 11500.f * 3 / 7,
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
        .difficulty = 2, .color = 0xffbca471, .name = "Desert · Rare"
    },
    {
        .left = 0, .top = 14000 + 11500.f * 3 / 7, .right = 14000, .bottom = 14000 + 11500.f * 4 / 7,
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
        .difficulty = 3, .color = 0xffa79264, .name = "Desert · Epic"
    },
    {
        .left = 0, .top = 14000 + 11500.f * 4 / 7, .right = 14000, .bottom = 14000 + 11500.f * 5 / 7,
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
        .difficulty = 4, .color = 0xff938058, .name = "Desert · Legendary"
    },
    {
        .left = 0, .top = 14000 + 11500.f * 5 / 7, .right = 14000, .bottom = 14000 + 11500.f * 6 / 7,
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
        .difficulty = 5, .color = 0xff7e6e4b, .name = "Desert · Mythic"
    },
    {
        .left = 0, .top = 14000 + 11500.f * 6 / 7, .right = 14000, .bottom = 25500,
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
        .difficulty = 6, .color = 0xff695c3f, .name = "Desert · Ultra"
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