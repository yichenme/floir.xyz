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

// Base HP / body-damage of a PETAL-summoned mob (Ant Egg, Beetle Egg, Stick,
// Square). Kept separate from MOB_DATA so wild spawns keep their own balance;
// summons scale x3 per rarity from these bases. Both the server (actual summon
// stats) and the client (petal tooltip) read these so display == reality.
float summon_base_health(uint8_t mob_id);
float summon_base_damage(uint8_t mob_id);

// Garden map (27500x27500 arena): 20 hand-painted spawn zones from the Tiled
// 刷怪区域 layer, approximated as AABBs (get_zone_from_pos picks the last
// match). Ordered ascending by rarity so rarer, typically-smaller zones win
// ties against the larger, more-common AABBs they overlap.
inline std::array const MAP_DATA = std::to_array<struct ZoneDefinition>({
    {
        .left = 3875.0f, .top = 2359.4f, .right = 8609.4f, .bottom = 4140.6f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 0, .color = 0xff7eef6d, .name = "Garden · Common (Zone 12)"
    },
    {
        .left = 6843.8f, .top = 4250.0f, .right = 8554.7f, .bottom = 7031.2f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 0, .color = 0xff7eef6d, .name = "Garden · Common (Zone 13)"
    },
    {
        .left = 3421.9f, .top = 4382.8f, .right = 5031.2f, .bottom = 6234.4f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 0, .color = 0xff7eef6d, .name = "Garden · Common (Zone 14)"
    },
    {
        .left = 2875.0f, .top = 6382.8f, .right = 6632.8f, .bottom = 8109.4f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 1, .color = 0xffffe65d, .name = "Garden · Uncommon (Zone 15)"
    },
    {
        .left = 1885.4f, .top = 8265.6f, .right = 4708.3f, .bottom = 12562.5f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 1, .color = 0xffffe65d, .name = "Garden · Uncommon (Zone 16)"
    },
    {
        .left = 2984.4f, .top = 9390.6f, .right = 8062.5f, .bottom = 15437.5f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 2, .color = 0xff4d52e3, .name = "Garden · Rare (Zone 18)"
    },
    {
        .left = 8203.1f, .top = 2859.4f, .right = 18781.2f, .bottom = 10109.4f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3, .color = 0xff861fde, .name = "Garden · Epic (Zone 19)"
    },
    {
        .left = 8164.1f, .top = 10351.6f, .right = 15203.1f, .bottom = 12632.8f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3, .color = 0xff861fde, .name = "Garden · Epic (Zone 23)"
    },
    {
        .left = 18963.1f, .top = 2438.4f, .right = 25568.2f, .bottom = 8238.6f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 4, .color = 0xffde1f1f, .name = "Garden · Legendary (Zone 20)"
    },
    {
        .left = 15296.9f, .top = 11804.7f, .right = 21135.4f, .bottom = 15552.1f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 4, .color = 0xffde1f1f, .name = "Garden · Legendary (Zone 24)"
    },
    {
        .left = 16927.1f, .top = 5492.4f, .right = 23982.0f, .bottom = 10582.4f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 5, .color = 0xff1fdbde, .name = "Garden · Mythic (Zone 21)"
    },
    {
        .left = 14343.7f, .top = 12864.6f, .right = 25666.7f, .bottom = 20166.7f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 5, .color = 0xff1fdbde, .name = "Garden · Mythic (Zone 25)"
    },
    {
        .left = 6835.9f, .top = 13898.4f, .right = 16585.9f, .bottom = 18101.6f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 5, .color = 0xff1fdbde, .name = "Garden · Mythic (Zone 30)"
    },
    {
        .left = 1867.2f, .top = 17835.9f, .right = 7750.0f, .bottom = 25593.8f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 5, .color = 0xff1fdbde, .name = "Garden · Mythic (Zone 31)"
    },
    {
        .left = 17729.2f, .top = 19302.1f, .right = 23260.4f, .bottom = 25562.5f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xffde1f65, .name = "Garden · Ultra (Zone 26)"
    },
    {
        .left = 23197.9f, .top = 20354.2f, .right = 25604.2f, .bottom = 25604.2f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xffde1f65, .name = "Garden · Ultra (Zone 27)"
    },
    {
        .left = 13291.7f, .top = 18177.1f, .right = 18093.8f, .bottom = 25052.1f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xffde1f65, .name = "Garden · Ultra (Zone 28)"
    },
    {
        .left = 17739.6f, .top = 21583.3f, .right = 18989.6f, .bottom = 23104.2f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xffde1f65, .name = "Garden · Ultra (Zone 29)"
    },
    {
        .left = 7820.3f, .top = 18804.7f, .right = 11656.2f, .bottom = 25632.8f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xffde1f65, .name = "Garden · Ultra (Zone 32)"
    },
    {
        .left = 5375.0f, .top = 19312.5f, .right = 8687.5f, .bottom = 23093.8f,
        .density = 1, .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 126000 },
            { MobID::kLadybug, 126000 },
            { MobID::kBee, 126000 },
            { MobID::kBumbleBee, 90000 },
            { MobID::kDandelion, 70000 },
            { MobID::kBabyAnt, 106000 },
            { MobID::kSpider, 96000 },
            { MobID::kCentipede, 30000 },
            { MobID::kHornet, 60000 },
            { MobID::kAntHole, 8000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 6, .color = 0xffde1f65, .name = "Garden · Ultra (Zone 35)"
    },
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

// Light petal: number of orbiting dots by rarity (common 1 -> mythic+ 5).
extern uint32_t light_petal_count(uint8_t rarity);
extern uint32_t stinger_count(uint8_t rarity);