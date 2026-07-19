#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/ScrollContainer.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Ui/Extern.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/StaticData.hh>

#include <Shared/RarityScale.hh>
#include <Shared/StackFormat.hh>
#include <Shared/StaticData.hh>

#include <Helpers/Math.hh>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace Ui;

namespace {
    // Current selection: a specific (type, rarity) inventory stack. Crafting
    // always consumes exactly 5 -- one pentagon's worth -- per attempt, so
    // there's no amount to accumulate beyond that.
    PetalID::T g_sel_type = PetalID::kNone;
    uint8_t g_sel_rarity = 0;

    void clear_selection() { g_sel_type = PetalID::kNone; }

    uint64_t owned_count(PetalID::T type, uint8_t rarity) {
        for (PetalStack const &s : Game::inventory_stacks)
            if (s.type == type && s.rarity == rarity) return s.count;
        return 0;
    }

    bool craftable(uint8_t rarity, uint64_t count) {
        return rarity < RarityID::kSuper && count >= 5;
    }

    // Roll/reveal animation state, driven by Craft button clicks and the
    // kCraftResult that arrives shortly after.
    enum AnimState { kAnimIdle, kAnimRolling, kAnimReveal };
    AnimState g_anim = kAnimIdle;
    double g_anim_started = 0;
    double g_roll_request_at = 0;   // so we only react to a result AFTER our own request

    float const ROLL_MS = 900, REVEAL_MS = 900;

    // Pentagon layout for the 5 craft slots: index 0 is top, then clockwise
    // (upper-right, lower-right, lower-left, upper-left).
    float const PENTAGON_R = 44;
    void pentagon_pos(int i, float &x, float &y) {
        float const theta = (float) i * (2 * M_PI / 5);   // 0 = top, increases clockwise
        x = PENTAGON_R * sinf(theta);
        y = -PENTAGON_R * cosf(theta);
    }

    float ease_in_out(float t) { return t * t * (3 - 2 * t); }

    class CraftIcon final : public Element {
    public:
        CraftIcon() : Element(140, 40, {
            .fill = 0xff5a9fdb, .line_width = 5, .round_radius = 3,
            .should_render = [](){ return Game::alive(); },
            .h_justify = Style::Left, .v_justify = Style::Bottom
        }) {}
        void on_render(Renderer &ctx) override {
            Element::on_render(ctx);
            ctx.draw_text("Craft", { .fill = 0xffffffff, .size = 18 });
        }
        void on_event(uint8_t event) override {
            if (event != kClick) return;
            Ui::panel_open = Ui::panel_open == Panel::kCraft ? Panel::kNone : Panel::kCraft;
        }
    };

    // One (type,rarity) cell in the recipe grid: icon + owned count, greyed
    // and non-interactive when uncraftable (Super+, or fewer than 5 owned).
    class RecipeCell final : public Element {
    public:
        PetalID::T type;
        uint8_t rarity;
        RecipeCell(PetalID::T t, uint8_t r) : Element(40, 40, { .round_radius = 4 }), type(t), rarity(r) {}
        void on_render(Renderer &ctx) override {
            uint64_t const count = owned_count(type, rarity);
            bool const can = craftable(rarity, count);
            bool const selected = g_sel_type == type && g_sel_rarity == rarity;
            if (selected) {
                ctx.set_fill(0x80ffffff);
                ctx.begin_path();
                ctx.round_rect(-width / 2, -height / 2, width, height, 4);
                ctx.fill();
            }
            ctx.set_global_alpha(can ? 1.0f : 0.35f);
            draw_loadout_background(ctx, type, 1, 1, rarity);
            ctx.set_global_alpha(1.0f);
            std::string const t = format_stack_count(count);
            if (!t.empty()) {
                RenderContext c(&ctx);
                ctx.translate(width / 2 - 10, -height / 2 + 8);
                ctx.draw_text(t.c_str(), { .fill = 0xffffffff, .size = 11 });
            }
        }
        void on_event(uint8_t event) override {
            if (event != kClick) return;
            uint64_t const count = owned_count(type, rarity);
            if (!craftable(rarity, count)) return;
            if (g_anim != kAnimIdle) return;   // can't reselect mid-animation
            g_sel_type = type;
            g_sel_rarity = rarity;
        }
    };

    // Rows grouped by petal TYPE; the row count is exactly the number of
    // distinct petal kinds currently in the inventory (loadout doesn't
    // count -- Game::inventory_stacks only ever reflects the account
    // inventory, never equipped petals).
    Element *make_recipe_grid() {
        Element *grid = new VContainer({}, 10, 8, {});
        std::vector<PetalID::T> types;
        for (PetalStack const &s : Game::inventory_stacks)
            if (std::find(types.begin(), types.end(), s.type) == types.end())
                types.push_back(s.type);
        std::sort(types.begin(), types.end(), [](PetalID::T a, PetalID::T b) {
            return std::string(PETAL_DATA[a].name) < std::string(PETAL_DATA[b].name);
        });
        for (PetalID::T type : types) {
            Element *row = new HContainer({}, 0, 6, { .v_justify = Style::Top });
            for (uint8_t r = 0; r < RarityID::kNumRarities; ++r) {
                if (owned_count(type, r) == 0) continue;
                row->add_child(new RecipeCell(type, r));
            }
            row->refactor();
            grid->add_child(row);
        }
        return grid;
    }

    class CraftGrid final : public ScrollContainer {
    public:
        CraftGrid() : ScrollContainer(make_recipe_grid(), 220) {}
        // Inventory contents change over time (new petals, crafts, drops) --
        // rebuild the grouping fresh each time the panel becomes visible
        // rather than trying to patch it incrementally.
        void on_render(Renderer &ctx) override {
            static uint32_t last_version = (uint32_t)-1;
            if (last_version != Game::inventory_version) {
                last_version = Game::inventory_version;
                children[0] = make_recipe_grid();
                children[0]->parent = this;
            }
            ScrollContainer::on_render(ctx);
        }
    };

    // The 5-box pentagon + chance%, driving the roll/reveal animation.
    // Idle: all 5 boxes sit at their pentagon positions showing the selected
    // petal (blank if nothing selected). Rolling: all 5 spin in place while
    // sliding toward the center, clockwise. Reveal (success): the merged
    // center box becomes the crafted petal's icon, the other 4 vanish (they
    // WERE the 5 that became the 1). Reveal (failure): the boxes slide back
    // out to their pentagon spots -- however many of the original 5 survived
    // (Game::last_craft_result.remaining, 0-4 on a failed round) show the
    // original petal again, the rest go blank (lost).
    class CraftControls final : public Element {
    public:
        CraftControls() : Element(200, 185, {}) {}
        void on_render(Renderer &ctx) override {
            bool const has_selection = g_sel_type != PetalID::kNone;
            uint64_t const count = has_selection ? owned_count(g_sel_type, g_sel_rarity) : 0;
            float const chance = has_selection ? craft_success_chance(g_sel_rarity) * 100.0f : 0;

            // Transition rolling -> reveal once ROLL_MS has passed AND a
            // result for OUR request has actually arrived.
            if (g_anim == kAnimRolling) {
                bool const result_in = Game::last_craft_result.received_at >= g_roll_request_at;
                if (Game::timestamp - g_anim_started > ROLL_MS && result_in) {
                    g_anim = kAnimReveal;
                    g_anim_started = Game::timestamp;
                }
            } else if (g_anim == kAnimReveal) {
                if (Game::timestamp - g_anim_started > REVEAL_MS) {
                    g_anim = kAnimIdle;
                    clear_selection();
                }
            }

            bool const success = Game::last_craft_result.any_success;
            // On a failed round, 1-4 of the fed petals survive uncomsumed
            // (see Server/EntityFunctions/CraftOps.cc); those slots keep
            // showing the original petal, the rest (lost) go blank.
            uint32_t const survived = std::min<uint32_t>(4, (uint32_t) Game::last_craft_result.remaining);

            {
                RenderContext c(&ctx);
                // -20 (not -35): the top pentagon box (center at -20-44, top at
                // -84) now sits just inside the element's top edge (-92) instead
                // of overflowing into the paragraph above it.
                ctx.translate(0, -20);
                for (int i = 0; i < 5; ++i) {
                    RenderContext c2(&ctx);
                    float px, py;
                    pentagon_pos(i, px, py);

                    bool draw_box = true, draw_icon = has_selection, dim = false;
                    float box_scale = 1.0f;

                    if (g_anim == kAnimRolling) {
                        float const t = ease_in_out(fclamp((float) ((Game::timestamp - g_anim_started) / ROLL_MS), 0, 1));
                        px = lerp(px, 0.0f, t);
                        py = lerp(py, 0.0f, t);
                        ctx.translate(px, py);
                        ctx.rotate(Game::timestamp / 150.0 + i * (2 * M_PI / 5));
                        box_scale = lerp(1.0f, 0.6f, t);
                    } else if (g_anim == kAnimReveal) {
                        if (success) {
                            // All 5 merged into the one result -- only draw
                            // the center slot, with a little pop-in scale.
                            if (i != 0) { draw_box = false; draw_icon = false; }
                            else {
                                float const t = fclamp((float) ((Game::timestamp - g_anim_started) / 250.0), 0, 1);
                                box_scale = lerp(0.6f, 1.05f, ease_in_out(t)) - 0.05f * sinf(t * (float) M_PI);
                                ctx.translate(0, 0);
                            }
                        } else {
                            float const t = ease_in_out(fclamp((float) ((Game::timestamp - g_anim_started) / REVEAL_MS), 0, 1));
                            px = lerp(0.0f, px, t);
                            py = lerp(0.0f, py, t);
                            ctx.translate(px, py);
                            draw_icon = (uint32_t) i < survived;
                            dim = !draw_icon;
                        }
                    } else {
                        ctx.translate(px, py);
                    }

                    if (draw_box) {
                        ctx.scale(box_scale);
                        float const slot_w = 40;
                        // Dark blue, matching the scrollbar/panel chrome
                        // instead of the old brown dirt-slot look.
                        ctx.set_fill(0xff1c3a52);
                        ctx.begin_path();
                        ctx.round_rect(-slot_w / 2, -slot_w / 2, slot_w, slot_w, 8);
                        ctx.fill();
                        if (draw_icon) {
                            ctx.set_global_alpha(dim ? 0.3f : 1.0f);
                            ctx.scale(0.7f);
                            if (g_anim == kAnimReveal && success)
                                draw_loadout_background(ctx, Game::last_craft_result.type, 1, 1, Game::last_craft_result.out_rarity);
                            else
                                draw_loadout_background(ctx, g_sel_type, 1, 1, g_sel_rarity);
                            ctx.set_global_alpha(1.0f);
                        }
                    }
                }
            }

            RenderContext c(&ctx);
            ctx.translate(0, 62);
            if (g_anim == kAnimReveal) {
                std::string const t = success
                    ? "Crafted!"
                    : "Failed -- " + std::to_string(survived) + " petal" + (survived == 1 ? "" : "s") + " left";
                ctx.draw_text(t.c_str(), { .fill = success ? 0xff75dd34u : 0xffcc3333u, .size = 16 });
            } else if (has_selection) {
                std::string const t = format_pct(chance) + " success chance (" + std::to_string(count) + " owned)";
                ctx.draw_text(t.c_str(), { .fill = 0xffffffff, .size = 14 });
            } else {
                ctx.draw_text("Select a petal below (5 needed)", { .fill = 0xffcccccc, .size = 13 });
            }
        }
    };
}

Element *Ui::make_craft_button() {
    Element *elt = new CraftIcon();
    elt->x = 10;
    elt->y = -60;
    return elt;
}

Element *Ui::make_craft_panel() {
    Element *grid = new CraftGrid();
    Element *controls = new CraftControls();
    Element *craft_btn = new Ui::Button(100, 36, new Ui::StaticText(16, "Craft"),
        [](Element *e, uint8_t ev) {
            if (ev != Ui::kClick) return;
            if (g_anim != kAnimIdle) return;
            if (g_sel_type == PetalID::kNone || owned_count(g_sel_type, g_sel_rarity) < 5) return;
            Game::send_craft(g_sel_type, g_sel_rarity, 5);
            g_anim = kAnimRolling;
            g_anim_started = Game::timestamp;
            g_roll_request_at = Game::timestamp;
        }, nullptr,
        { .fill = 0xff5a9fdb, .line_width = 4, .round_radius = 4 }
    );

    class CraftPanel final : public VContainer {
    public:
        using VContainer::VContainer;
    };

    Element *elt = new CraftPanel(std::vector<Element *>{
        new Ui::StaticText(22, "Craft"),
        new Ui::StaticParagraph(300, 13, "Combine 5 of the same petal to craft an upgrade", {}),
        controls,
        craft_btn,
        grid
    }, 15, 10, {
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .should_render = [](){ return Ui::panel_open == Panel::kCraft && Game::alive(); },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom,
        // Hard show/hide: without this the panel lerps out over ~10 frames and,
        // because inventory anchors to the same corner, the two briefly overlap
        // and read as "both open at once".
        .no_animation = 1
    });
    // To the right of the icon (icon: x=10, width=140), not stacked above it.
    elt->x = 10 + 140 + 10;
    elt->y = -60;
    return elt;
}
