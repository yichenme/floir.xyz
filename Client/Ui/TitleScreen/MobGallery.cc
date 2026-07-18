#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/ScrollContainer.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Ui/Extern.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/StaticData.hh>

#include <Shared/RarityScale.hh>

#include <algorithm>
#include <cstring>
#include <string>

using namespace Ui;

GalleryMob::GalleryMob(MobID::T id, float w) :
    Element(w,w,{ .fill=0xff5a9fdb, .stroke_hsv=1, .line_width=3, .round_radius=6, .v_justify=Style::Top }), id(id) {}

void GalleryMob::on_render(Renderer &ctx) {
    Element::on_render(ctx);
    ctx.begin_path();
    ctx.round_rect(-width / 2, -height / 2, width, height, style.round_radius);
    ctx.clip();
    struct MobData const &data = MOB_DATA[id];
    if (id != MobID::kDigger)
        ctx.rotate(-3*M_PI/4);
    if (id == MobID::kBeetle)
        ctx.translate(-5,0);
    float radius = (data.radius.upper + data.radius.lower) / 2;
    if (radius > width * 0.5) ctx.scale(0.5 * width / radius);
    ctx.scale(0.5);
    draw_static_mob(id, ctx, { .radius = radius, .flower_attrs = { .color = ColorID::kGray } });
    if (data.attributes.segments > 1) {
        ctx.translate(-2 * radius, 0);
        draw_static_mob(id, ctx, { .radius = radius, .flags = 1<<1, .flower_attrs = { .color = ColorID::kGray } });
    }
}

// Conditional distribution over dropped-item rarities for a given MOB rarity,
// mirroring Shared/RarityScale.cc roll_drop_rarity (per-item; DROP_NOTHING is
// the leftover mass, so for low-rarity mobs the row sums below 1).
static void drop_rarity_dist(uint8_t mob_rarity, float out[RarityID::kNumRarities]) {
    for (int i = 0; i < RarityID::kNumRarities; ++i) out[i] = 0;
    switch (mob_rarity) {
        case RarityID::kCommon:    out[RarityID::kCommon] = 1.00f; break;
        case RarityID::kUncommon:  out[RarityID::kUncommon] = 0.64f; out[RarityID::kCommon] = 0.36f; break;
        case RarityID::kRare:      out[RarityID::kRare] = 0.32f; out[RarityID::kUncommon] = 0.68f; break;
        case RarityID::kEpic:      out[RarityID::kEpic] = 0.16f; out[RarityID::kRare] = 0.84f; break;
        case RarityID::kLegendary: out[RarityID::kLegendary] = 0.08f; out[RarityID::kEpic] = 0.92f; break;
        case RarityID::kMythic:    out[RarityID::kMythic] = 0.04f; out[RarityID::kLegendary] = 0.96f; break;
        case RarityID::kUltra:     out[RarityID::kUltra] = 0.02f; out[RarityID::kMythic] = 0.98f; break;
        case RarityID::kSuper:
        case RarityID::kUnique:     out[RarityID::kSuper] = 0.01f; out[RarityID::kUltra] = 0.99f; break;
    }
}

static Element *make_mob_drops(MobID::T id, uint8_t rarity) {
    Element *elt = new Ui::HContainer({}, 0, 6, { .h_justify = Style::Left });
    struct MobData const &data = MOB_DATA[id];
    float dist[RarityID::kNumRarities];
    drop_rarity_dist(rarity, dist);
    for (uint32_t i = 0; i < data.drops.size(); ++i) {
        // Each drop shows the sub-rarities it can roll at this mob rarity, with
        // the chance for each (highest rarity first).
        Element *col = new Ui::VContainer({}, 0, 4, { .h_justify = Style::Left });
        for (int r = RarityID::kNumRarities - 1; r >= 0; --r) {
            if (dist[r] <= 0) continue;
            col->add_child(new Ui::VContainer({
                new GalleryPetal(data.drops[i], 42, (uint8_t) r),
                new StaticText(11, format_pct(dist[r] * 100))
            }, 0, 4, { .h_justify = Style::Left }));
        }
        col->refactor();
        elt->add_child(col);
    }
    return elt;
}

static Element *make_mob_stat_container(MobID::T id, uint8_t rarity) {
    std::vector<Ui::Element *> stats;
    struct MobData const &mob_data = MOB_DATA[id];
    struct MobAttributes const &attrs = mob_data.attributes;
    // Stats scale with the mob's rarity, mirroring Server/Spawn.cc alloc_mob.
    float const hp_m = mob_hp_mult(rarity);
    float const dmg_m = mob_body_damage_mult(rarity);
    float const arm_m = mob_armor_mult(rarity);
    float const mid_hp = (mob_data.health.lower + mob_data.health.upper) / 2.0f;
    stats.push_back(new Ui::HContainer({
        new Ui::StaticText(12, "Health:", { .fill = 0xff77ff77 }),
        new Ui::StaticText(12, format_number(mid_hp * hp_m))
    }, 0, 5, { .h_justify = Style::Left }));
    stats.push_back(new Ui::HContainer({
        new Ui::StaticText(12, "Body Damage:", { .fill = 0xffff7777 }),
        new Ui::StaticText(12, format_number(mob_data.damage * dmg_m))
    }, 0, 5, { .h_justify = Style::Left }));
    if (attrs.missile_damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Missile Damage:", { .fill = 0xffff7777 }),
            new Ui::StaticText(12, format_number(attrs.missile_damage * dmg_m))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    // Armor is always shown (0 when the mob has none).
    stats.push_back(new Ui::HContainer({
        new Ui::StaticText(12, "Armor:", { .fill = 0xff777777 }),
        new Ui::StaticText(12, format_number(attrs.armor * arm_m))
    }, 0, 5, { .h_justify = Style::Left }));
    if (attrs.poison_damage.damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(12, "Poison:", { .fill = 0xffce76db }),
            new Ui::StaticText(12, format_number(attrs.poison_damage.damage * attrs.poison_damage.time) + " (" + format_number(attrs.poison_damage.damage) + "/s)")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    stats.push_back(new Ui::HContainer({
        new Ui::StaticText(12, "XP:", { .fill = 0xff7777ff }),
        new Ui::StaticText(12, format_score((uint64_t)(mob_data.xp * mob_xp_mult(rarity) + 0.5f)))
    }, 0, 5, { .h_justify = Style::Left }));
    return new Ui::VContainer(stats, 0, 2, { .h_justify = Style::Left });
}

static Element *make_mob_card(MobID::T id, uint8_t rarity) {
    Element *elt = new Ui::VContainer({
        new Ui::Element(300,0),
        new Ui::HFlexContainer(
            new Ui::VContainer({
                // Live kill count so the info box updates as the tally grows.
                new Ui::DynamicText(18, [id, rarity](){
                    return std::string(MOB_DATA[id].name) + "  x" + format_score(Game::mob_kills[id][rarity]);
                }, { .fill = 0xffffffff, .h_justify = Style::Left }),
                new Ui::StaticText(14, RARITY_NAMES[rarity], { .fill = RARITY_COLORS[rarity], .h_justify = Style::Left }),
                new Ui::Element(0,2),
                new Ui::StaticParagraph(220, 14, MOB_DATA[id].description, { .h_justify = Style::Left })
            }, 0, 5),
            new GalleryMob(id, 60),
            10, 10
        ),
        new Ui::Element(0,10),
        make_mob_stat_container(id, rarity),
        new Ui::Element(0,10),
        make_mob_drops(id, rarity)
    }, 10, 0, { .fill = 0x33000000, .stroke_hsv = 1, .line_width = 3, .round_radius = 6, .v_justify = Style::Top, .no_animation = 1 });
    return elt;
}

// One grid cell: a mob at a single rarity. Rarity-coloured with the kill count
// when killed, a dim placeholder otherwise (so columns stay aligned by rarity).
namespace {
    class GalleryMobCell final : public Element {
    public:
        MobID::T id;
        uint8_t rarity;
        Element *card = nullptr;   // tooltip, built lazily on first hover
        GalleryMobCell(MobID::T id, uint8_t rarity, float w) :
            Element(w, w, { .round_radius = w / 12, .v_justify = Style::Top }), id(id), rarity(rarity) {}

        void on_render(Renderer &ctx) override {
            uint64_t const kills = Game::mob_kills[id][rarity];
            if (kills == 0) {
                ctx.set_fill(0x22ffffff);
                ctx.begin_path();
                ctx.round_rect(-width / 2, -height / 2, width, height, style.round_radius);
                ctx.fill();
                return;
            }
            ctx.set_fill(Renderer::HSV(RARITY_COLORS[rarity], 0.8));
            ctx.begin_path();
            ctx.round_rect(-width / 2, -height / 2, width, height, style.round_radius);
            ctx.fill();
            ctx.set_fill(RARITY_COLORS[rarity]);
            ctx.begin_path();
            float const inner = width * 0.84f;
            ctx.round_rect(-inner / 2, -inner / 2, inner, inner, style.round_radius);
            ctx.fill();
            {
                RenderContext c(&ctx);
                ctx.begin_path();
                ctx.round_rect(-inner / 2, -inner / 2, inner, inner, style.round_radius);
                ctx.clip();
                struct MobData const &data = MOB_DATA[id];
                if (id != MobID::kDigger) ctx.rotate(-3 * M_PI / 4);
                float radius = (data.radius.upper + data.radius.lower) / 2;
                if (radius > width * 0.5) ctx.scale(0.5 * width / radius);
                ctx.scale(0.42);
                draw_static_mob(id, ctx, { .radius = radius, .flower_attrs = { .color = ColorID::kGray } });
            }
            std::string const txt = "x" + format_score(kills);
            RenderContext c(&ctx);
            ctx.translate(width / 2 - 3 - 3.2f * txt.size(), -height / 2 + 9);
            ctx.draw_text(txt.c_str(), { .fill = 0xffffffff, .size = 12 });
        }

        void on_event(uint8_t event) override {
            if (event != kFocusLost && Game::mob_kills[id][rarity] > 0) {
                if (card == nullptr) card = make_mob_card(id, rarity);
                rendering_tooltip = 1;
                tooltip = card;
            } else
                rendering_tooltip = 0;
        }
    };
}

// True if the player has killed this mob at any rarity (drives row visibility).
static bool mob_killed_any(MobID::T id) {
    for (uint8_t r = 0; r < RarityID::kNumRarities; ++r)
        if (Game::mob_kills[id][r] > 0) return true;
    return false;
}

static bool any_kills() {
    for (MobID::T m = 0; m < MobID::kNumMobs; ++m)
        if (mob_killed_any(m)) return true;
    return false;
}

static Element *make_scroll() {
    Element *elt = new Ui::VContainer({}, 0, 6, {});
    MobID::T id_list[MobID::kNumMobs];
    for (MobID::T i = 0; i < MobID::kNumMobs; ++i)
        id_list[i] = i;
    std::sort(id_list, id_list + MobID::kNumMobs, [](MobID::T a, MobID::T b) {
        if (MOB_DATA[a].rarity != MOB_DATA[b].rarity) return MOB_DATA[a].rarity < MOB_DATA[b].rarity;
        return strcmp(MOB_DATA[a].name, MOB_DATA[b].name) < 0;
    });
    // One row per mob, a cell per rarity. Rows for un-killed mobs drop out of
    // layout so only killed mobs appear.
    for (MobID::T k = 0; k < MobID::kNumMobs; ++k) {
        MobID::T const id = id_list[k];
        Element *row = new Ui::HContainer({}, 0, 3, { .v_justify = Style::Top });
        for (uint8_t r = 0; r < RarityID::kNumRarities; ++r)
            row->add_child(new GalleryMobCell(id, r, 32));
        row->refactor();
        row->style.should_render = [id](){ return mob_killed_any(id); };
        elt->add_child(row);
    }
    // Empty-state hint when nothing has been killed yet.
    elt->add_child(new Ui::StaticText(14, "Kill mobs to fill your gallery", {
        .fill = 0xffffffff,
        .should_render = [](){ return !any_kills(); }
    }));
    return new Ui::ScrollContainer(elt, 320);
}

Element *Ui::make_mob_gallery() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Mob Gallery"),
        make_scroll()
    }, 15, 10, {
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kMobs && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::mob_gallery = elt;
    return elt;
}
