#include <Client/Ui/InGame/Loadout.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/StaticData.hh>

#include <Client/Game.hh>

#include <Shared/RarityScale.hh>
#include <Shared/TalentData.hh>

#include <format>

using namespace Ui;

Element *Ui::UiLoadout::petal_tooltips[PetalID::kNumPetals][RarityID::kNumRarities] = {{nullptr}};

static float get_reload_factor() {
    if (!Game::alive()) return 1;
    // Final Reload = Original x product over equipped Golden Leaves of
    // (1 - reduction), where each leaf's reduction scales with its rarity
    // (matches Server/Process/Flower.cc). Multiplying per leaf gives the
    // (1 - r)^(#leaves) behaviour. The Reload talent is a further flat
    // account-wide multiplier, matching how Flower.cc folds it into
    // buffs.reload_factor server-side.
    float factor = talent_reload_mult(Game::talent_reload_rank);
    Entity &player = Game::simulation.get_ent(Game::player_id);
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        float rf = PETAL_DATA[player.get_loadout_ids(i)].attributes.extra_reload_factor;
        if (rf < 1.f) {
            rf = 1.f - (1.f - rf) * (1 + player.get_loadout_rarities(i));
            if (rf < 0.1f) rf = 0.1f;
        }
        factor *= rf;
    }
    return factor;
}

static float get_damage_factor() {
    if (!Game::alive()) return 1;
    float factor = 1;
    Entity &player = Game::simulation.get_ent(Game::player_id);
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        factor *= PETAL_DATA[player.get_loadout_ids(i)].attributes.extra_damage_factor;
    }
    return factor;
}

static Ui::Element *make_petal_stat_container(PetalID::T id, uint8_t rarity) {
    std::vector<Ui::Element *> stats = {new Ui::Element(0,10)};
    struct PetalData const &petal_data = PETAL_DATA[id];
    struct PetalAttributes const &attrs = petal_data.attributes;
    // Health and damage scale with the petal's rarity (mirrors Server/Spawn.cc,
    // which applies these exact multipliers when the petal is spawned).
    // Mjolnir's data values are its exact stats (flat, no rarity scaling) --
    // mirror Server/Spawn.cc's alloc_petal so the tooltip matches in-game.
    bool const flat_stats = id == PetalID::kMjolnir;
    float const hp_mult = flat_stats ? 1.0f : petal_hp_mult(rarity);
    float const dmg_mult = flat_stats ? 1.0f : petal_damage_mult(rarity);
    // Specials that scale x3 per rarity in-game (armor, poison, flower-HP, body
    // damage, poison-absorb, heal) use this so the info box tracks the rarity.
    float const mult = rarity_pow3(rarity);
    // Pure summoner petals (eggs, Stick) and Lotus hide their own HP/damage.
    bool const is_summoner = attrs.spawns != MobID::kNumMobs && attrs.defend_only;
    bool const hide_combat = is_summoner || id == PetalID::kLotus;
    // Stinger and Bubble stay at 1 HP regardless of rarity.
    float const eff_hp_mult = (id == PetalID::kStinger || id == PetalID::kBubble) ? 1.0f : hp_mult;
    if (petal_data.health > 0 && !hide_combat) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Health:", { .fill = 0xff77ff77 }),
            new Ui::DynamicText(12, [&petal_data, eff_hp_mult](){
                // Petal Health talent is a flat account-wide multiplier on top
                // of the rarity scaling above (matches Spawn.cc's alloc_petal).
                return format_number(petal_data.health * eff_hp_mult * talent_health_mult(Game::talent_health_rank));
            })
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (petal_data.damage > 0 && !hide_combat) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Damage:", { .fill = 0xffff7777 }),
            new Ui::DynamicText(12, [&petal_data, dmg_mult](){
                return format_number(petal_data.damage * dmg_mult * get_damage_factor());
            })
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.armor > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Armor:", { .fill = 0xff777777 }),
            new Ui::StaticText(12, format_number(attrs.armor * mult))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.damage_reflection > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Damage Reflection:", { .fill = 0xff777777 }),
            new Ui::StaticText(12, format_pct((attrs.damage_reflection + 0.05f * rarity) * 100))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    // Heal scales with rarity (x3 per tier, mirrors Server/Process/Flower.cc
    // and Petal.cc which apply rarity_pow3 to the heal amounts).
    float const heal_mult = rarity_pow3(rarity);
    if (attrs.constant_heal > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Heal:", { .fill = 0xffff96cb }),
            new Ui::StaticText(12, format_number(attrs.constant_heal * heal_mult) + "/s")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.burst_heal > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Heal:", { .fill = 0xffff96cb }),
            new Ui::StaticText(12, format_number(attrs.burst_heal * heal_mult))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.poison_damage.damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Poison:", { .fill = 0xffce76db }),
            new Ui::StaticText(12, format_number(attrs.poison_damage.time * attrs.poison_damage.damage * mult) + " (" + format_number(attrs.poison_damage.damage * mult) + "/s)")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_health > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Flower Health:", { .fill = 0xff77ff77 }),
            new Ui::StaticText(12, format_number(attrs.extra_health * mult))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_body_damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Body Damage:", { .fill = 0xffff7777 }),
            new Ui::StaticText(12, format_number(attrs.extra_body_damage * mult))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.poison_armor > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Poison Armor:", { .fill = 0xffce76db }),
            new Ui::StaticText(12, format_number(attrs.poison_armor * mult) + "/s")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_rotation_speed > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Rotation Speed:", { .fill = 0xffcde23b }),
            new Ui::StaticText(12, "+" + format_number(attrs.extra_rotation_speed * (1 + 0.4f * rarity)) + " rad/s")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.spawns != MobID::kNumMobs && attrs.spawn_count == 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Contents:", { .fill = 0xffd2eb34 }),
            new Ui::StaticText(12, (petal_data.count > 1 ? format_number(petal_data.count) + "x " : "") + (MOB_DATA[attrs.spawns].name))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.spawns != MobID::kNumMobs && attrs.spawn_count > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Spawns:", { .fill = 0xffd2eb34 }),
            new Ui::StaticText(12, format_number(attrs.spawn_count) + "x " + (MOB_DATA[attrs.spawns].name))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    // Summon stats for spawning petals (eggs, Stick, Square). HP/damage scale
    // x3 per rarity, matching the petal's rarity.
    if (attrs.spawns != MobID::kNumMobs) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Summon HP:", { .fill = 0xff77ff77 }),
            new Ui::StaticText(12, format_number(summon_base_health(attrs.spawns) * mult))
        }, 0, 5, { .h_justify = Style::Left }));
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Summon Damage:", { .fill = 0xffff7777 }),
            new Ui::StaticText(12, format_number(summon_base_damage(attrs.spawns) * mult))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.vision_factor < 1) {
        // Matches the server's actual FOV widening exactly (both call
        // extra_vision_bonus): Common +10% .. Unique +600%.
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Extra Vision:", { .fill = 0xffcde23b }),
            new Ui::StaticText(12, "+" + format_pct(100 * extra_vision_bonus(rarity)))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_range > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Attack Range:", { .fill = 0xffcde23b }),
            new Ui::StaticText(12, "+" + format_number(attrs.extra_range * (rarity + 1)))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.magnet_range > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Pickup Range:", { .fill = 0xffcde23b }),
            new Ui::StaticText(12, "+" + format_number(attrs.magnet_range * (rarity + 1)))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    // Damage/reload multipliers: show whether they raise (+) or lower (-) the
    // stat (Golden Leaf, e.g., is -5% reload). format_pct only takes positives,
    // so pass the magnitude and prefix the sign.
    if (attrs.extra_damage_factor != 1) {
        float const pct = 100 * (attrs.extra_damage_factor - 1);
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Damage Factor:", { .fill = 0xffff7777 }),
            new Ui::StaticText(12, (pct > 0 ? "+" : "-") + format_pct(pct > 0 ? pct : -pct))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_reload_factor != 1) {
        // Reduction deepens per rarity (mirrors Server/Process/Flower.cc):
        // Golden Leaf -5% -> -5*(rarity+1)%.
        float factor = attrs.extra_reload_factor;
        if (factor < 1) factor = 1 - (1 - factor) * (1 + rarity);
        float const pct = 100 * (factor - 1);
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Reload Factor:", { .fill = 0xff7777ff }),
            new Ui::StaticText(12, (pct > 0 ? "+" : "-") + format_pct(pct > 0 ? pct : -pct))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    return new Ui::VContainer(stats, 0, 2, { .h_justify = Style::Left });
}

static void make_petal_tooltip(PetalID::T id, uint8_t rarity) {
    Element *tooltip = new Ui::VContainer({
        new Ui::HFlexContainer(
            new Ui::StaticText(20, PETAL_DATA[id].name, { .fill = 0xffffffff, .h_justify = Style::Left }),
            new Ui::DynamicText(16, [=](){
                float reload = PETAL_DATA[id].reload * get_reload_factor();
                // Yggdrasil's cooldown is divided by 3 each rarity up.
                if (id == PetalID::kYggdrasil) reload /= rarity_pow3(rarity);
                float secondary = PETAL_DATA[id].attributes.secondary_reload;
                // Bubble reload table: -0.25s primary / -0.1s secondary per tier.
                if (id == PetalID::kBubble) {
                    reload = 2.0f - 0.25f * rarity; if (reload < 0.1f) reload = 0.1f;
                    secondary = rarity >= RarityID::kUnique ? 0.f : 0.7f - 0.1f * rarity;
                    if (secondary < 0.1f && secondary > 0) secondary = 0.1f;
                }
                // Show both reload phases with their own "s" (e.g. 0.1s + 0.1s),
                // no reload icon.
                if (reload == 0) return std::string("");
                if (secondary == 0)
                    return std::format("{:.1f}s", reload);
                return std::format("{:.1f}s + {:.1f}s", reload, secondary);
            }, { .fill = 0xffffffff, .v_justify = Style::Top }),
            5, 10, {}
        ),
        new Ui::StaticText(14, RARITY_NAMES[rarity], { .fill = RARITY_COLORS[rarity], .h_justify = Style::Left }),
        new Ui::Element(0,8),
        new Ui::StaticText(12, PETAL_DATA[id].description, { .fill = 0xffffffff, .h_justify = Style::Left }),
        make_petal_stat_container(id, rarity)
    }, 5, 2);
    tooltip->style.fill = 0x80000000;
    tooltip->style.round_radius = 6;
    tooltip->refactor();
    Ui::UiLoadout::petal_tooltips[id][rarity] = tooltip;
}

void Ui::make_petal_tooltips() {
    for (PetalID::T i = 0; i < PetalID::kNumPetals; ++i)
        for (uint8_t r = 0; r < RarityID::kNumRarities; ++r)
            make_petal_tooltip(i, r);
}