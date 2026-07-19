#include <Shared/StaticData.hh>

#include <cmath>

uint32_t const MAX_LEVEL = 999;
uint32_t const TPS = 20;

//these two are based on the base game's 20 TPS
float const PLAYER_ACCELERATION = 5.0f;
float const DEFAULT_FRICTION = 1.0f/3.0f;

float const PETAL_DISABLE_DELAY = 45.0f; //seconds
float const SUMMON_RETREAT_RADIUS = 600.0f;
float const DIGGER_SPAWN_CHANCE = 0.25f;

float const BASE_FLOWER_RADIUS = 25.0f;
float const BASE_PETAL_ROTATION_SPEED = 2.5f;
float const BASE_FOV = 0.9f;
float const BASE_HEALTH = 100.0f;
float const BASE_BODY_DAMAGE = 25.0f;

std::array<struct PetalData, PetalID::kNumPetals> const PETAL_DATA = {{
    {
        .name = "None",
        .description = "How can you see this?",
        .health = 0.0, 
        .damage = 0.0,
        .radius = 0.0,
        .reload = 1.0,
        .count = 0,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Basic",
        .description = "A nice petal, not too strong but not too weak",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Light",
        .description = "Weaker than most petals, but reloads very quickly. Higher rarities gain more dots",
        .health = 5.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 0.5,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Heavy",
        .description = "Very resilient and deals more damage, but reloads very slowly",
        .health = 20.0,
        .damage = 20.0,
        .radius = 12.0,
        .reload = 4.5,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Stinger",
        .description = "It really hurts, but it's really fragile. Higher rarities gain more stingers",
        .health = 1.0,
        .damage = 100.0,
        .radius = 7.0,
        .reload = 10.0,
        .count = 1,
        .rarity = RarityID::kUncommon,
        .attributes = {
            // Gather the 3 (Mythic) / 5 (Ultra+) stingers into ONE tight cluster
            // that orbits as a single petal, instead of spreading them around
            // the flower (same clumping Cactus uses).
            .clump_radius = 9,
            // Point each stinger's sharp tip toward the cluster centre (inward).
            .icon_angle = M_PI,
            // Follow-rotation instead of the default free spin, so the in-world
            // stinger holds a stable orientation that matches its loadout icon
            // (it used to spin independently and look detached from the icon).
            .rotation_style = PetalAttributes::kFollowRot
        }
    },
    {
        .name = "Leaf",
        .description = "Gathers energy from the sun to passively heal your flower",
        .health = 10.0,
        .damage = 8.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kUncommon,
        .attributes = {
            .constant_heal = 3,
            .icon_angle = -1
        }
    },
    {
        .name = "Twin",
        .description = "Why stop at one? Why not TWO?!",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.0,
        .count = 2,
        .rarity = RarityID::kUncommon,
        .attributes = {}
    },
    {
        .name = "Rose",
        .description = "Its healing properties are amazing. Not so good at combat though",
        .health = 1.67,
        .damage = 1.67,
        .radius = 10.0,
        .reload = 3.5,
        .count = 1,
        .rarity = RarityID::kUncommon,
        .attributes = {
            .secondary_reload = 1.0,
            .burst_heal = 3.5,
            .defend_only = 1
        }
    },
    {
        .name = "Iris",
        .description = "Very poisonous, but takes a while to do its work",
        .health = 5.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 6.0,
        .count = 1,
        .rarity = RarityID::kUncommon,
        .attributes = {
            .poison_damage = {
                .damage = 15.0,
                .time = 3.0
            }
        }
    },
    {
        .name = "Missile",
        .description = "You can actually shoot this one",
        .health = 10.0,
        .damage = 35.0,
        .radius = 10.0,
        .reload = 3.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 1.0, 
            .defend_only = 1,
            .icon_angle = 1,
            .rotation_style = PetalAttributes::kFollowRot 
        }
    },
    {
        .name = "Dandelion",
        .description = "Its interesting properties prevent healing effects on affected units",
        .health = 10.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .icon_angle = 1,
            .rotation_style = PetalAttributes::kFollowRot 
        }
    },
    {
        .name = "Bubble",
        .description = "You can right click to pop it and propel your flower",
        .health = 1.0,
        .damage = 0.0,
        .radius = 12.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 0.7,
            .defend_only = 1,
        }
    },
    {
        .name = "Faster",
        .description = "It's so light it makes your other petals spin faster",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 0.5,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .extra_rotation_speed = 0.5
        }
    },
    {
        .name = "Rock",
        .description = "Even more durable, but slower to recharge",
        .health = 80.0,
        .damage = 10.0,
        .radius = 12.0,
        .reload = 10.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {}
    },
    {
        .name = "Cactus",
        .description = "Not very strong, but somehow increases your maximum health",
        .health = 15.0,
        .damage = 7.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .extra_health = 30
        }
    },
    {
        .name = "Web",
        .description = "It's really sticky",
        .health = 0.8,
        .damage = 0.5,
        .radius = 10.0,
        .reload = 3.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }
    },
    {
        .name = "Wing",
        .description = "It comes and goes",
        .health = 15.0,
        .damage = 15.0,
        .radius = 10.0,
        .reload = 1.25,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .icon_angle = 1,
        }
    },
    {
        .name = "Peas",
        .description = "4 in 1 deal",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.4,
        .count = 4,
        .rarity = RarityID::kRare,
        .attributes = {
            .clump_radius = 8,
            .secondary_reload = 0.5,
            .defend_only = 1,
            .split_projectile = 1
        }
    },
    {
        .name = "Sand",
        .description = "It's coarse, rough, and gets everywhere",
        .health = 5.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 1.5,
        .count = 4,
        .rarity = RarityID::kRare,
        .attributes = {
            .clump_radius = 10,
        }
    },
    {
        .name = "Pincer",
        .description = "Stuns and poisons targets for a short duration",
        .health = 15.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .slow_inflict_seconds = 2,
            .poison_damage = {
                .damage = 5.0,
                .time = 3.0
            },
            .icon_angle = 0.7
        }
    },
    {
        .name = "Dahlia",
        .description = "Its healing properties are amazing. Not so good at combat though",
        .health = 5.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 3.5,
        .count = 3,
        .rarity = RarityID::kRare,
        .attributes = { 
            .clump_radius = 10,
            .secondary_reload = 1.0,
            .burst_heal = 2.0,
            .defend_only = 1
        }
    },
    {
        .name = "Triplet",
        .description = "How about THREE?!",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.0,
        .count = 3,
        .rarity = RarityID::kEpic,
        .attributes = {}
    },
    {
        .name = "Egg",
        .description = "Something interesting might pop out of this",
        .health = 1.0,
        .damage = 0.0,
        .radius = 12.5,
        .reload = 60.0,
        .count = 2,
        .rarity = RarityID::kEpic,
        .attributes = {
            .secondary_reload = 2.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kSoldierAnt,
            // 2 eggs (count) x 2 concurrent each -> ~4 Soldier Ants alive.
            .spawn_count = 2
        }
    },
    {
        .name = "Iris",
        .description = "Deals its effects quicker than traditional irises",
        .health = 10.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 5.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = { 
            .poison_damage = {
                .damage = 15.0,
                .time = 4.0
            }
        }
    },
    {
        .name = "Pollen",
        .description = "Asthmatics beware",
        .health = 5.0,
        .damage = 10.0,
        .radius = 7.0,
        .reload = 1.0,
        // One particle at a time: on attack the single pollen detaches and is
        // left on the floor for a few seconds (see Server/Process/Petal.cc).
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .secondary_reload = 0.5,
            .defend_only = 1
        }
    },
    {
        .name = "Grapes",
        .description = "4 in 1 deal, now with a secret ingredient: poison",
        .health = 5.0,
        .damage = 4.0,
        .radius = 7.0,
        .reload = 1.5,
        .count = 4,
        .rarity = RarityID::kEpic,
        .attributes = {
            .clump_radius = 8,
            .secondary_reload = 0.5,
            .poison_damage = {
                .damage = 5.0,
                .time = 3.0
            },
            .defend_only = 1,
            .split_projectile = 1,
        }
    },
    {
        .name = "Egg",
        .description = "Something interesting might pop out of this",
        .health = 1.0,
        .damage = 0.0,
        .radius = 15.0,
        .reload = 60.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .secondary_reload = 2.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kBeetle
        }
    },
    {
        .name = "Rose",
        .description = "Extremely powerful rose, almost unheard of",
        .health = 5.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 3.5,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = { 
            .secondary_reload = 1.0,
            .burst_heal = 22,
            .defend_only = 1
        }
    },
    {
        .name = "Stick",
        .description = "Harnesses the power of the wind",
        .health = 1.0,
        .damage = 0.0,
        .radius = 15.0,
        .reload = 4.0,
        .count = 1,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .secondary_reload = 5.0,
            .defend_only = 1,
            .icon_angle = 1,
            .spawns = MobID::kSandstorm,
            .spawn_count = 4
        }
    },
    {
        .name = "Stinger",
        .description = "It really hurts, but it's really fragile",
        .health = 5.0,
        .damage = 35.0,
        .radius = 7.0,
        .reload = 4.5,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10
        }
    },
    {
        .name = "Web",
        .description = "It's really sticky",
        .health = 10.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 3.0,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10,
            .secondary_reload = 0.5,
            .defend_only = 1,
        }
    },
    {
        .name = "Antennae",
        .description = "Allows your flower to sense foes from farther away",
        .health = 0.0,
        .damage = 0.0,
        .radius = 12.5,
        .reload = 0.0,
        .count = 0,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .vision_factor = 0.6,
            .equipment = EquipmentFlags::kAntennae 
        }
    },
    {
        .name = "Cactus",
        .description = "Not very strong, but somehow increases your maximum health",
        .health = 15.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10,
            .extra_health = 40,
        }
    },
    {
        .name = "Heavy",
        .description = "This thing is so heavy that nothing gets in the way",
        .health = 20.0,
        .damage = 20.0,
        .radius = 12.0,
        .reload = 5.5,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            // Mass is never rarity-scaled (see alloc_petal), so this push
            // stays constant across rarities by construction -- only the
            // health/damage/reload above scale with rarity.
            .mass = 70
        }
    },
    {
        .name = "Third Eye",
        .description = "Allows your flower to extend petals further out",
        .health = 0.0,
        .damage = 0.0,
        .radius = 10.0,
        .reload = 0.0,
        .count = 0,
        .rarity = RarityID::kMythic,
        .attributes = {
            .extra_range = 25,
            .equipment = EquipmentFlags::kThirdEye
        }
    },
    {
        .name = "Observer",
        .description = "The one who sees all",
        .health = 0.0,
        .damage = 0.0,
        .radius = 12.5,
        .reload = 0.0,
        .count = 0,
        .rarity = RarityID::kMythic,
        .attributes = {
            .vision_factor = 0.25,
            .equipment = EquipmentFlags::kObserver
        }
    },
    {
        .name = "Cactus",
        .description = "Turns your flower poisonous. Enemies will take poison damage on contact",
        .health = 15.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .extra_health = 20,
            .poison_damage = {
                .damage = 1.0,
                .time = 5.0
            }
        }
    },
    {
        .name = "Salt",
        .description = "Reflects some damage dealt to the flower. Does not stack with itself",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .damage_reflection = 0.10
        }
    },
    {
        .name = "Basic",
        .description = "Something incredibly rare and useless",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {}
    },
    {
        .name = "Square",
        .description = "This shape... it looks familiar...",
        .health = 10.0,
        .damage = 10.0,
        .radius = 15.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {
            .icon_angle = M_PI / 4 + 1,
            .spawns = MobID::kSquare
        }
    },
    {
        .name = "Moon",
        .description = "A heavy moon rock that roams around you. Bump it with Bubble to send it flying.",
        .health = 100.0,
        .damage = 10.0,
        .radius = 50.0,
        .reload = 5.0,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {
            // Moon is no longer an orbiting petal: it summons a free-roaming,
            // player-owned Moon MOB (spawn_count 0 -> the mob replaces the petal,
            // like Beetle Egg). Wandering/heaviness/size live on that mob.
            .secondary_reload = 0.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kMoon
        }
    },
    {
        .name = "Lotus",
        .description = "Absorbs some poison damage taken by the flower",
        .health = 5.0,
        .damage = 0.0,
        .radius = 12.0,
        .reload = 6.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .poison_armor = 14,
            .icon_angle = 0.1
        }
    },
    {
        .name = "Cutter",
        .description = "Increases the flower's body damage",
        .health = 0.0,
        .damage = 0.0,
        .radius = 40.0,
        .reload = 2.5,
        .count = 0,
        .rarity = RarityID::kEpic,
        .attributes = {
            .extra_body_damage = 10,
            .equipment = EquipmentFlags::kCutter
        }
    },
    {
        .name = "Yin Yang",
        .description = "Alters the flower's petal rotation in interesting ways",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {}
    },
    {
        .name = "Yggdrasil",
        .description = "Revives a dead flower on contact",
        .health = 5.0,
        .damage = 0.0,
        .radius = 12.0,
        .reload = 300.0,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {
            .defend_only = 1,
            .icon_angle = M_PI
        }
    },
    {
        .name = "Rice",
        .description = "Spawns instantly, but not very strong",
        .health = 5.0,
        .damage = 8.0,
        .radius = 13.0,
        .reload = 0.05,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .icon_angle = 0.7
        }
    },
    {
        .name = "Bone",
        .description = "Sturdy",
        .health = 30.0,
        .damage = 10.0,
        .radius = 12.0,
        .reload = 3.0,
        .count = 1,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .armor = 10,
            .icon_angle = 1
        }
    },
    {
        .name = "Yucca",
        .description = "Heals the flower, but only while in the defensive position",
        .health = 10.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kUncommon,
        .attributes = {
            .constant_heal = 10,
            .icon_angle = -1
        }
    },
    {
        .name = "Corn",
        .description = "Takes a long time to spawn, but has a lot of health",
        .health = 1000.0,
        .damage = 2.0,
        .radius = 16.0,
        .reload = 7.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .icon_angle = 0.5
        }
    },
    {
        .name = "Leaf",
        .description = "Reloads all your petals faster",
        .health = 10.0,
        .damage = 8.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .extra_reload_factor = 0.95,
            .icon_angle = -1
        }
    },
    {
        .name = "Mjolnir",
        .description = "The hammer of thunder. Only the level leaderboard's number one may wield it.",
        .health = 328050.0,
        .damage = 328050.0,
        .radius = 28.0,
        .reload = 7.0,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {
            .mass = 40
        }
    },
    {
        .name = "Magnet",
        .description = "Pulls nearby drops toward you and widens your pickup range. Stays fixed in place -- doesn't pull in on defend or push out on attack",
        .health = 30.0,
        .damage = 1.0,
        .radius = 10.0,
        .reload = 5.0,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {
            .magnet_range = 150,   // +150 per rarity tier
            .fixed_orbit = 1
        }
    },
}};

std::array<struct MobData, MobID::kNumMobs> const MOB_DATA = {{
    {
        .name = "Baby Ant",
        .description = "Weak and defenseless, but big dreams.",
        .rarity = RarityID::kCommon,
        .health = {10.0},
        .damage = 10.0,
        .radius = {14.0},
        .xp = 1,
        .drops = {
            PetalID::kLight, PetalID::kLeaf, PetalID::kRice
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Worker Ant",
        .description = "It's temperamental, probably from working all the time.",
        .rarity = RarityID::kCommon,
        .health = {25.0},
        .damage = 10.0,
        .radius = {14.0},
        .xp = 3,
        .drops = {
            PetalID::kLeaf, PetalID::kCorn, PetalID::kBone
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Soldier Ant",
        .description = "It's got wings and it's ready to use them.",
        .rarity = RarityID::kUncommon,
        .health = {40.0},
        .damage = 10.0,
        .radius = {14.0},
        .xp = 5,
        .drops = {
            PetalID::kWing, PetalID::kFaster
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Bee",
        .description = "It stings. Don't touch it.",
        .rarity = RarityID::kCommon,
        .health = {15.0},
        .damage = 50.0,
        .radius = {20.0},
        .xp = 4,
        .drops = {
            PetalID::kStinger, PetalID::kPollen
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Ladybug",
        .description = "Cute and harmless.",
        .rarity = RarityID::kCommon,
        .health = {25.0},
        .damage = 10.0,
        .radius = {30.0},
        .xp = 3,
        .drops = {
            PetalID::kLight, PetalID::kRose, PetalID::kBubble
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Beetle",
        .description = "It's hungry and flowers are its favorite meal.",
        .rarity = RarityID::kUncommon,
        .health = {40.0},
        .damage = 35.0,
        .radius = {35.0},
        .xp = 10,
        .drops = {
            PetalID::kSalt, PetalID::kBeetleEgg
        },
        .attributes = { .mass = 2.5f }
    },
    {
        .name = "Ladybug",
        .description = "Cute and harmless... if left unprovoked.",
        .rarity = RarityID::kUncommon,
        .health = {35.0},
        .damage = 10.0,
        .radius = {30.0},
        .xp = 5,
        .drops = {
            PetalID::kDahlia, PetalID::kYinYang
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Hornet",
        .description = "These aren't quite as nice as the little bees.",
        .rarity = RarityID::kUncommon,
        .health = {40.0},
        .damage = 40.0,
        .radius = {40.0},
        .xp = 12,
        .drops = {
            PetalID::kAntennae, PetalID::kMissile
        },
        .attributes = {
            .aggro_radius = 600,
            .missile_damage = 10,
            .mass = 2.5f
        }
    },
    {
        .name = "Cactus",
        .description = "This one's prickly, don't touch it either.",
        .rarity = RarityID::kCommon,
        .health = {25.0, 50.0},
        .damage = 30.0,
        .radius = {30.0, 60.0},
        .xp = 2,
        .drops = {
            PetalID::kCactus
        },
        .attributes = {
            .stationary = 1,
            .mass = 15000.0f
        }
    },
    {
        .name = "Rock",
        .description = "A rock. It doesn't do much.",
        .rarity = RarityID::kCommon,
        .health = {5.0, 15.0},
        .damage = 10.0,
        .radius = {10.0, 25.0},
        .xp = 1,
        .drops = {
            PetalID::kMoon, PetalID::kRock, PetalID::kHeavy
        },
        .attributes = {
            .stationary = 1,
            .mass = 15000.0f
        }
    },
    {
        .name = "Centipede",
        .description = "It's just there doing its thing.",
        .rarity = RarityID::kUncommon,
        .health = {50.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 2,
        .drops = {
            PetalID::kLeaf, PetalID::kPeas, PetalID::kGoldenLeaf
        },
        .attributes = {
            .segments = 10,
            .mass = 2.5f
        }
    },
    {
        .name = "Evil Centipede",
        .description = "This one loves flowers.",
        .rarity = RarityID::kRare,
        .health = {50.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 3,
        .drops = {
            PetalID::kIris, PetalID::kGrapes
        },
        .attributes = {
            .segments = 10,
            .poison_damage = {
                .damage = 5.0,
                .time = 2.0
            },
            .mass = 2.5f
        }
    },
    {
        .name = "Desert Centipede",
        .description = "It doesn't like it when you interrupt its run.",
        .rarity = RarityID::kRare,
        .health= {50.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 4,
        .drops = {
            PetalID::kSand, PetalID::kFaster, PetalID::kSalt, PetalID::kStick
        },
        .attributes = {
            .segments = 6,
            .mass = 2.5f
        }
    },
    {
        .name = "Sandstorm",
        .description = "Quite unpredictable.",
        .rarity = RarityID::kUncommon,
        .health = {30.0, 45.0},
        .damage = 40.0,
        .radius = {32.0, 48.0},
        .xp = 5,
        .drops = {
            PetalID::kSand, PetalID::kFaster, PetalID::kStick
        },
        .attributes = { .mass = 2.0f }
    },
    {
        .name = "Scorpion",
        .description = "This one stings, now with poison.",
        .rarity = RarityID::kUncommon,
        .health = {35.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 10,
        .drops = {
            PetalID::kLight, PetalID::kIris, PetalID::kPincer, PetalID::kLotus
        }, 
        .attributes = {
            .poison_damage = {
                .damage = 10.0,
                .time = 1.0
            },
            .mass = 2.5f
        }
    },
    {
        .name = "Spider",
        .description = "Spooky.",
        .rarity = RarityID::kUncommon,
        .health = {35.0},
        .damage = 10.0,
        .radius = {15.0},
        .xp = 8,
        .drops = {
            PetalID::kWeb, PetalID::kFaster, PetalID::kThirdEye
        },
        .attributes = {
            .poison_damage = {
                .damage = 3.75f,  // was 5.0; 0.75x
                .time = 3.0
            },
            .mass = 1.5f
        }
    },
    {
        .name = "Ant Hole",
        .description = "Ants go in, and come out. Can't explain that.",
        .rarity = RarityID::kRare,
        .health = {500.0},
        .damage = 10.0,
        .radius = {45.0},
        .xp = 25,
        .drops = {
            PetalID::kAntEgg, PetalID::kMagnet
        },
        .attributes = {
            .stationary = 1,
            .mass = 20000.0f
        }
    },
    {
        .name = "Queen Ant",
        .description = "You must have done something really bad if she's chasing you.",
        .rarity = RarityID::kRare,
        .health = {350.0},
        .damage = 10.0,
        .radius = {50.0},
        .xp = 15,
        .drops = {
            PetalID::kWing, PetalID::kAntEgg
        },
        .attributes = {
            .aggro_radius = 750,
            .mass = 3.0f
        }
    },
    {
        .name = "Ladybug",
        .description = "This one is shiny... I wonder what it could mean...",
        .rarity = RarityID::kEpic,
        .health = {25.0},
        .damage = 10.0,
        .radius = {30.0},
        .xp = 30,
        .drops = {
            PetalID::kDahlia, PetalID::kYggdrasil
        },
        .attributes = { .mass = 1.5f }
    },
    {
        .name = "Square",
        .description = "???",
        .rarity = RarityID::kUnique,
        .health = {20.0},
        .damage = 10.0,
        .radius = {40.0},
        .xp = 1,
        .drops = {
            PetalID::kSquare
        },
        .attributes = {
            .stationary = 1,
            .mass = 20000.0f
        }
    },
    {
        .name = "Digger",
        .description = "Friend or foe? You'll never know...",
        .rarity = RarityID::kEpic,
        .health = {250.0},
        .damage = 25.0,
        .radius = {40.0},
        .xp = 0,
        .drops = {
            PetalID::kCutter
        },
        .attributes = { .mass = 2.5f }
    },
    {
        // Moon: summoned only by the Moon petal (never wild-spawns; NoDrops via
        // the summon flags). Base stats; the summon scales HP/damage/size by the
        // petal's rarity (see Flower.cc + summon_base_*).
        .name = "Moon",
        .description = "A heavy moon rock summoned by the Moon petal.",
        .rarity = RarityID::kUnique,
        .health = {100.0},
        .damage = 10.0,
        .radius = {50.0},
        .xp = 0,
        .drops = {},
        .attributes = { .mass = 1.0f }
    },
    {
        // Bumble Bee: never aggros. Drifts in fast curvy arcs, turning at walls,
        // and lands a Pollen particle every 0.5s (spawned in tick_ai_behavior).
        .name = "Bumble Bee",
        .description = "A gentle bee that wanders and scatters pollen.",
        .rarity = RarityID::kCommon,
        .health = {25.0},
        .damage = 20.0,
        .radius = {22.0},
        .xp = 1,
        .drops = {
            PetalID::kPollen
        },
        .attributes = {
            .armor = 1,
            .mass = 1.75f
        }
    },
    {
        // Dandelion: a mostly-stationary body ringed by missiles. When the body
        // takes damage every missile fires outward; each missile is independently
        // destroyable (its own HP/hitbox). missile_damage carries the seed shot's
        // damage, mirroring Hornet.
        .name = "Dandelion",
        .description = "Strike the core and its seeds burst outward.",
        .rarity = RarityID::kCommon,
        .health = {25.0},
        .damage = 15.0,
        .radius = {28.0},
        .xp = 3,
        .drops = {
            PetalID::kDandelion
        },
        .attributes = {
            .stationary = 1,
            .armor = 1,
            .missile_damage = 10,
            .mass = 10000.0f
        }
    },
    {
        // Pollen: the small particle a Bumble Bee (or the Pollen petal) leaves on
        // the ground. NOT stationary -- stationary mobs get 10000 mass (immovable)
        // and this must stay PUSHABLE. Its AI does nothing, so it never self-moves;
        // it just sits and stings on contact until destroyed or it despawns.
        .name = "Pollen",
        .description = "A drifting mote of pollen. Mildly irritating.",
        .rarity = RarityID::kCommon,
        .health = {5.0},
        .damage = 10.0,
        .radius = {10.0},
        .xp = 0,
        .drops = {},
        .attributes = {
            .armor = 0,
            .mass = 1.0f
        }
    },
}};

std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_CHANCES = [](){
    std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> ret;
    double const RARITY_MULT[RarityID::kNumRarities] = {50000,15000,2500,100,10,2.5,1};
    double MOB_SPAWN_RATES[MobID::kNumMobs] = {0};
    double PETAL_AGGREGATE_DROPS[PetalID::kNumPetals] = {0};
    for (struct ZoneDefinition const &zone : MAP_DATA) {
        double total = 0;
        for (SpawnChance const &s : zone.spawns) total += s.chance;
        for (SpawnChance const &s : zone.spawns) {
            double base_chance = (s.chance * zone.drop_multiplier / total);
            MOB_SPAWN_RATES[s.id] += base_chance;
            if (s.id == MobID::kAntHole) {
                MOB_SPAWN_RATES[MobID::kDigger] += DIGGER_SPAWN_CHANCE * base_chance;
                for (auto const &spawn_wave : ANTHOLE_SPAWNS)
                    for (MobID::T spawn : spawn_wave)
                        MOB_SPAWN_RATES[spawn] += base_chance;
            }
        }
    }

    for (MobID::T id = 0; id < MobID::kNumMobs; ++id)
        for (PetalID::T const drop_id : MOB_DATA[id].drops) PETAL_AGGREGATE_DROPS[drop_id]++;

    double const BASE_NUM = MOB_SPAWN_RATES[MobID::kSquare];
    if (BASE_NUM <= 0) assert(!"Square mob must spawn in at least one zone");

    for (MobID::T id = 0; id < MobID::kNumMobs; ++id) {
        for (PetalID::T const drop_id : MOB_DATA[id].drops) {
            float chance = fclamp((BASE_NUM * RARITY_MULT[PETAL_DATA[drop_id].rarity]) / (PETAL_AGGREGATE_DROPS[drop_id] * MOB_SPAWN_RATES[id] * MOB_DATA[id].attributes.segments), 0, 1);
            ret[id].push(chance);
        }
    }
    return ret;
}();

float summon_base_health(uint8_t mob_id) {
    switch (mob_id) {
        case MobID::kSoldierAnt: return 50.f;
        case MobID::kBeetle:     return 250.f;
        case MobID::kSquare:     return 100.f;
        case MobID::kSandstorm:  return 30.f;
        default: return (MOB_DATA[mob_id].health.lower + MOB_DATA[mob_id].health.upper) / 2.f;
    }
}

float summon_base_damage(uint8_t mob_id) {
    switch (mob_id) {
        case MobID::kSoldierAnt: return 10.f;
        case MobID::kBeetle:     return 250.f;
        case MobID::kSquare:     return 250.f;
        case MobID::kSandstorm:  return 30.f;
        default: return MOB_DATA[mob_id].damage;
    }
}

uint32_t score_to_pass_level(uint32_t level) {
    // Levels 1-99 are UNCHANGED, so every existing player keeps the exact same
    // level and XP after the cap was raised to 999. The 1.06^(n-1) curve would
    // explode past 99 (L999 would need ~10^25 XP), so beyond 99 it continues as
    // a linear ramp from the L99 cost -- a long but reachable end-game grind.
    if (level < 100)
        return (uint32_t)(pow(1.06, level - 1) * level) + 3;
    double const cost99 = pow(1.06, 98) * 99 + 3;
    return (uint32_t)(cost99 + (double)(level - 99) * 2000.0);
}

uint32_t score_to_level(uint32_t score) {
    uint32_t level = 1;
    while (level < MAX_LEVEL) {
        uint32_t level_score = score_to_pass_level(level);
        if (score < level_score) break;
        score -= level_score;
        ++level;
    }
    return level;
}

uint32_t level_to_score(uint32_t level) {
    uint32_t score = 0;
    for (uint32_t i = 1; i < level; ++i)
        score += score_to_pass_level(i);
    return score;
}

uint32_t loadout_slots_at_level(uint32_t level) {
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    uint32_t ret = 5 + level / LEVELS_PER_EXTRA_SLOT;
    if (ret > MAX_SLOT_COUNT) return MAX_SLOT_COUNT;
    return ret;
}

float hp_at_level(uint32_t level) {
    if (level < 1) level = 1;
    if (level > 100) level = 100;
    return 200.0f * std::pow(std::pow(243.0f, 0.01f), (float)level - 1.0f);
}

uint32_t light_petal_count(uint8_t rarity) {
    // Fast (1) at Common; Twin (2) at Uncommon/Rare; Triplet (3) at
    // Epic/Legendary; 5 dots at Mythic and above.
    if (rarity >= RarityID::kMythic) return 5;
    if (rarity >= RarityID::kEpic) return 3;
    if (rarity >= RarityID::kUncommon) return 2;
    return 1;
}

uint32_t stinger_count(uint8_t rarity) {
    // Single stinger through Legendary; triple at Mythic; penta at Ultra+.
    if (rarity >= RarityID::kUltra) return 5;
    if (rarity >= RarityID::kMythic) return 3;
    return 1;
}