#include <Client/StaticData.hh>

std::array<uint32_t, RarityID::kNumRarities> const RARITY_COLORS = {
    0xff7eef6d, // common
    0xffffe65d, // uncommon
    0xff4d52e3, // rare
    0xff861fde, // epic
    0xffde1f1f, // legendary
    0xff1fdbde, // mythic
    0xffde1f65, // ultra
    0xff2affa3, // super
    0xff555555, // unique
};

std::array<uint32_t, ColorID::kNumColors> const FLOWER_COLORS = {
    0xffffe763, 0xff999999, 0xff689ce2, 0xffec6869
};

std::array<char const *, RarityID::kNumRarities> const RARITY_NAMES = {
    "Common",
    "Uncommon",
    "Rare",
    "Epic",
    "Legendary",
    "Mythic",
    "Ultra",
    "Super",
    "Unique"
};

std::array<char const, MAX_SLOT_COUNT> const SLOT_KEYBINDS = 
    {'1','2','3','4','5','6','7','8'};
