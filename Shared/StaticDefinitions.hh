#pragma once

#include <Helpers/Array.hh>
#include <Helpers/Math.hh>

#include <cstdint>

// Arena footprint tracks Shared/Tilemap.hh: GRID_W * CELL_SIZE and GRID_H * CELL_SIZE.
inline uint32_t const ARENA_WIDTH = 25000;
inline uint32_t const ARENA_HEIGHT = 25500;

inline uint32_t const MAX_SLOT_COUNT = 10;
inline uint32_t const LEVELS_PER_EXTRA_SLOT = 15;
inline uint32_t const LEADERBOARD_SIZE = 10;
inline uint32_t const MAX_PETALS_IN_CLUMP = 5;
inline uint32_t const MAX_DIFFICULTY = 6;
inline uint32_t const MAX_DROPS_PER_MOB = 6;

namespace DamageType {
    enum : uint8_t {
        kContact,
        kPoison,
        kReflect,
        kLightning   // Mjölnir: ignores armor (matches neither armor branch in inflict_damage)
    };
}

namespace PetalID {
    typedef uint8_t T;
    enum : T {
        kNone,
        kBasic,
        kLight,
        kHeavyLegacy, // retired: merged into kHeavy (was "Heaviest"); kept only
                      // to preserve petal-ID numbering for saved accounts.
        kStinger,
        kLeaf,
        kTwin,
        kRose,
        kIris,
        kMissile,
        kDandelion,
        kBubble,
        kFaster,
        kRock,
        kCactus,
        kWeb,
        kWing,
        kPeas,
        kSand,
        kPincer,
        kDahlia, // "Triple Rose", displayed as "Dahlia"
        kTriplet,
        kAntEgg,
        kBlueIris, // removed from game; kept to preserve saved petal-ID numbering

        kPollen,
        kGrapes, // formerly kPoisonPeas ("Purple Peas"), now "Grapes"
        kBeetleEgg,
        kAzalea, // "Triangled Rose"; removed from game (numbering kept for accounts)
        kStick,
        kTringer,
        kTriweb, // "Triple Web"; removed from game (numbering kept for accounts)
        kAntennae,
        kTricac, // "Triple Cactus"; removed from game (numbering kept for accounts)
        kHeavy, // formerly "Heaviest"; the old kHeavy is now kHeavyLegacy.
        kThirdEye,
        kObserver,
        kPoisonCactus, // "Purple Cactus"; removed from game (numbering kept for accounts)
        kSalt,
        kUniqueBasic,
        kSquare,
        kMoon,
        kLotus,
        kCutter,
        kYinYang,
        kYggdrasil,
        kRice,
        kBone,
        kYucca,
        kCorn,
        kGoldenLeaf,
        kMjolnir,
        kMagnet,
        kNumPetals
    };
};

namespace MobID {
    typedef uint8_t T;
    enum : T {
        kBabyAnt,
        kWorkerAnt,
        kSoldierAnt,
        kBee,
        kLadybug,
        kBeetle,
        kDarkLadybug,
        kHornet,
        kCactus,
        kRock,
        kCentipede,
        kEvilCentipede,
        kDesertCentipede,
        kSandstorm,
        kScorpion,
        kSpider,
        kAntHole,
        kQueenAnt,
        kShinyLadybug,
        kSquare,
        kDigger,
        kMoon,
        // Appended (never reorder -- mob ids are persisted in the kill gallery).
        kBumbleBee,
        kDandelion,
        kPollen,     // the pushable floor particle dropped by Bumble Bee / Pollen petal
        kNumMobs
    };
};

namespace RarityID {
    enum {
        kCommon,
        kUncommon,
        kRare,
        kEpic,
        kLegendary,
        kMythic,
        kUltra,
        kSuper,
        kUnique,
        kNumRarities
    };
};

namespace ColorID {
    enum {
        kYellow,
        kGray,
        kBlue,
        kRed,
        kNumColors
    };
};

namespace AIState {
    enum {
        kIdle,
        kIdleMoving,
        kReturning,
        kBasicAggro
    };
};

namespace EntityFlags {
    enum {
        kIsDespawning,
        kNoFriendlyCollision,
        kDieOnParentDeath,
        kSpawnedFromZone,
        kNoDrops,
        kHasCulling,
        kIsCulled,
        kCPUControlled,
        kIsDetached,
        // Skips physical push in collision (still deals/takes damage) so the
        // body overlays creatures instead of shoving them -- used by Stick's
        // summoned Sandstorms.
        kNoPush
    };
};

namespace FaceFlags {
    enum {
        kAttacking,
        kDefending,
        kPoisoned,
        kDandelioned,
        kDeadEyes,
        kSquareEyes
    };
};

namespace EquipmentFlags {
    enum {
        kThirdEye,
        kAntennae,
        kObserver,
        kCutter,
        kNone
    };
};

namespace InputFlags {
    enum {
        kAttacking,
        kDefending
    };
}

struct PoisonDamage {
    float damage;
    float time;
};

struct PetalAttributes {
    enum {
        kPassiveRot,
        kNoRot,
        kFollowRot
    };
    float clump_radius = 0;
    float secondary_reload = 0;
    float constant_heal = 0;
    float burst_heal = 0;
    float mass = 0.1;
    float armor = 0;
    float poison_armor = 0;
    float dandelion_inflict_seconds = 0;
    float slow_inflict_seconds = 0;
    float vision_factor = 1;
    float extra_body_damage = 0;
    float extra_rotation_speed = 0;
    float extra_range = 0;
    float extra_health = 0;
    float damage_reflection = 0;
    float extra_damage_factor = 1;
    float extra_reload_factor = 1;
    float magnet_range = 0;
    struct PoisonDamage poison_damage;
    uint8_t defend_only = 0;
    uint8_t fixed_orbit = 0;
    float icon_angle = 0;
    uint8_t split_projectile = 0;
    uint8_t rotation_style = kPassiveRot;
    uint8_t spawns = MobID::kNumMobs;
    uint8_t spawn_count = 0;
    uint8_t equipment = EquipmentFlags::kNone;
};

struct PetalData {
    char const *name;
    char const *description;
    float health;
    float damage;
    float radius;
    float reload;
    uint8_t count;
    uint8_t rarity;
    struct PetalAttributes attributes;
};

struct MobAttributes {
    float aggro_radius = 500;
    uint8_t segments = 1;
    uint8_t stationary;
    struct PoisonDamage poison_damage;
    float armor = 1;
    float missile_damage = 0;
    // Collision mass (mirrors PetalAttributes.mass): drives push/knockback via
    // the mass-ratio calc in Collision.cc. Explicit per-mob, not derived from
    // radius/rarity, so it stays exactly what's specified regardless of size.
    float mass = 1.5f;
};

struct MobData {
    char const *name;
    char const *description;
    uint8_t rarity;
    RangeValue health;
    float damage;
    RangeValue radius;
    uint32_t xp = 1;
    StaticArray<PetalID::T, MAX_DROPS_PER_MOB> drops;
    struct MobAttributes attributes;
};

struct SpawnChance {
    MobID::T id;
    float chance;
};

struct ZoneDefinition {
    float left;
    float top;
    float right;
    float bottom;
    float density;
    float drop_multiplier;
    StaticArray<struct SpawnChance, MobID::kNumMobs> spawns;
    uint32_t difficulty;
    uint32_t color;
    char const *name;
};
