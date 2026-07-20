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
    // Current selection: a specific (type, rarity) inventory stack.
    PetalID::T g_sel_type = PetalID::kNone;
    uint8_t g_sel_rarity = 0;
    bool g_craft_all = false;

    void clear_selection() {
        g_sel_type = PetalID::kNone;
        g_craft_all = false;
    }

    uint64_t owned_count(PetalID::T type, uint8_t rarity) {
        for (PetalStack const &s : Game::inventory_stacks)
            if (s.type == type && s.rarity == rarity) return s.count;
        return 0;
    }

    // Pity counter for a stack: rises with each failed 5-petal attempt at
    // that (type,rarity), resets on any success -- see Shared/RarityScale.cc
    // craft_success_chance. Reading it straight from the synced inventory
    // stacks (rather than tracking it separately client-side) means it can
    // never drift from what the server actually used for the last roll.
    uint32_t attempt_for(PetalID::T type, uint8_t rarity) {
        for (PetalStack const &s : Game::inventory_stacks)
            if (s.type == type && s.rarity == rarity) return s.craft_attempt;
        return 0;
    }

    bool craftable(uint8_t rarity, uint64_t count) {
        return rarity < RarityID::kSuper && count >= 5;
    }

    // Drop selection when the stack is gone or no longer craftable; keep it
    // after reveal so the Craft button can retry (5 or all per g_craft_all).
    void sync_selection_after_craft() {
        if (g_sel_type == PetalID::kNone) return;
        uint64_t const count = owned_count(g_sel_type, g_sel_rarity);
        if (!craftable(g_sel_rarity, count)) {
            clear_selection();
            return;
        }
    }

    // Roll/reveal animation state, driven by Craft button clicks and the
    // kCraftResult that arrives shortly after.
    enum AnimState { kAnimIdle, kAnimRolling, kAnimReveal };
    AnimState g_anim = kAnimIdle;
    double g_anim_started = 0;
    double g_roll_request_at = 0;   // so we only react to a result AFTER our own request

    float const ROLL_MS = 900, REVEAL_MS = 900;

    void begin_craft(PetalID::T type, uint8_t rarity, uint32_t amount) {
        g_sel_type = type;
        g_sel_rarity = rarity;
        Game::send_craft(type, rarity, amount);
        g_anim = kAnimRolling;
        g_anim_started = Game::timestamp;
        g_roll_request_at = Game::timestamp;
    }

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
        RecipeCell(PetalID::T t, uint8_t r) : Element(36, 36, { .round_radius = 4 }), type(t), rarity(r) {}
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
            if (g_anim != kAnimIdle) return;

            bool const alt_held = Input::keys_held.contains('\x12');

            if (alt_held) {
                bool const shift_held = Input::keys_held.contains('\x10');
                uint64_t const amount = shift_held ? count : 5;
                begin_craft(type, rarity, (uint32_t) amount);
            } else {
                g_sel_type = type;
                g_sel_rarity = rarity;
                g_craft_all = Input::keys_held.contains('\x10');
            }
        }
    };

    // A flat grid of every owned (type, rarity) stack.
    // Ordered highest-rarity-first, then by petal name, matching the inventory's
    // display order so the two panels read consistently.
    int const CRAFT_COLUMNS = 6;
    Element *make_recipe_grid() {
        Element *grid = new VContainer({}, 8, 6, {});
        struct Cell { PetalID::T type; uint8_t rarity; };
        std::vector<Cell> cells;
        for (PetalStack const &s : Game::inventory_stacks)
            if (s.count > 0)
                cells.push_back({ s.type, s.rarity });
        std::sort(cells.begin(), cells.end(), [](Cell const &a, Cell const &b) {
            if (a.rarity != b.rarity) return a.rarity > b.rarity;
            return std::string(PETAL_DATA[a.type].name) < std::string(PETAL_DATA[b.type].name);
        });
        for (size_t i = 0; i < cells.size();) {
            Element *row = new HContainer({}, 0, 6, { .v_justify = Style::Top });
            for (int j = 0; j < CRAFT_COLUMNS && i < cells.size(); ++j, ++i)
                row->add_child(new RecipeCell(cells[i].type, cells[i].rarity));
            row->refactor();
            // Fixed row width so short final rows stay left-aligned instead of
            // centering (40px cells, 6px gaps), same idea as the inventory grid.
            row->width = CRAFT_COLUMNS * 36 + (CRAFT_COLUMNS - 1) * 6;
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
            static bool was_craft_open = false;
            bool const craft_open = Ui::panel_open == Panel::kCraft;
            if (was_craft_open && !craft_open) clear_selection();
            was_craft_open = craft_open;

            bool const has_selection = g_sel_type != PetalID::kNone;

            // Transition rolling -> reveal once ROLL_MS has passed AND a
            // result for OUR request has actually arrived.
            if (g_anim == kAnimRolling) {
                bool const result_in = Game::last_craft_result.received_at >= g_roll_request_at;
                if (Game::timestamp - g_anim_started > ROLL_MS && result_in) {
                    g_anim = kAnimReveal;
                    g_anim_started = Game::timestamp;
                }
            } else if (g_anim == kAnimReveal) {
                // A successful reveal doesn't auto-dismiss -- the crafted
                // petal has to be manually clicked to claim it (see
                // on_event below). A failed reveal still auto-clears after
                // REVEAL_MS since there's nothing to claim.
                if (!Game::last_craft_result.any_success && Game::timestamp - g_anim_started > REVEAL_MS) {
                    g_anim = kAnimIdle;
                    sync_selection_after_craft();
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
        }
        // Click-to-claim: the crafted petal was already granted server-side
        // the instant the result came back (sync_inventory_update ran as
        // part of try_craft), so this click doesn't move any petals -- it
        // only dismisses the reveal and re-arms the pentagon for the next
        // attempt, mirroring the reference project's manual-collect step.
        void on_event(uint8_t event) override {
            if (event != kClick) return;
            if (g_anim != kAnimReveal || !Game::last_craft_result.any_success) return;
            g_anim = kAnimIdle;
            sync_selection_after_craft();
        }
    };

    // Set once by make_craft_panel; CraftChance recolors it every frame to
    // match the OUTPUT rarity (g_sel_rarity+1) you're about to craft into,
    // same idea as the reference project's rarity-tinted craft button.
    Element *g_craft_btn = nullptr;

    class CraftChance final : public Element {
    public:
        CraftChance() : Element(160, 36, {}) {}
        void on_render(Renderer &ctx) override {
            if (g_craft_btn != nullptr) {
                uint8_t const out_rarity = g_sel_type == PetalID::kNone
                    ? RarityID::kCommon : (uint8_t)(g_sel_rarity + 1);
                g_craft_btn->style.fill = RARITY_COLORS[out_rarity];
            }
            if (g_anim == kAnimReveal) {
                bool const success = Game::last_craft_result.any_success;
                uint32_t const survived = std::min<uint32_t>(4, (uint32_t) Game::last_craft_result.remaining);
                std::string const text = success
                    ? "Crafted!"
                    : "Failed -- " + std::to_string(survived) + " petal" + (survived == 1 ? "" : "s") + " left";
                ctx.draw_text(text.c_str(), { .fill = success ? 0xff75dd34u : 0xffcc3333u, .size = 14 });
                if (success) {
                    RenderContext c(&ctx);
                    ctx.translate(0, 18);
                    ctx.draw_text("Click to claim", { .fill = 0xffdddddd, .size = 11 });
                }
                return;
            }
            if (g_sel_type == PetalID::kNone) {
                ctx.draw_text("Select a stack", { .fill = 0xffcccccc, .size = 14 });
                return;
            }
            uint32_t const attempt = attempt_for(g_sel_type, g_sel_rarity);
            float const chance = craft_success_chance(g_sel_rarity, attempt) * 100.0f;
            std::string const text = format_pct(chance) + " success";
            ctx.draw_text(text.c_str(), { .fill = 0xffffffff, .size = 14 });
            RenderContext c(&ctx);
            ctx.translate(0, 18);
            ctx.draw_text(("Attempt " + std::to_string(attempt + 1)).c_str(), { .fill = 0xffaaaaaa, .size = 11 });
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
            uint64_t const count = owned_count(g_sel_type, g_sel_rarity);
            if (g_sel_type == PetalID::kNone || count < 5) return;
            uint64_t const amount = g_craft_all ? count : 5;
            begin_craft(g_sel_type, g_sel_rarity, (uint32_t) amount);
        }, nullptr,
        { .fill = 0xff5a9fdb, .line_width = 4, .round_radius = 4 }
    );
    g_craft_btn = craft_btn;
    Element *craft_actions = new VContainer({
        craft_btn,
        new CraftChance()
    }, 0, 7, {});
    Element *craft_row = new HContainer({
        controls,
        craft_actions
    }, 0, 12, {});

    class CraftPanel final : public VContainer {
    public:
        using VContainer::VContainer;
    };

    Element *elt = new CraftPanel(std::vector<Element *>{
        new Ui::StaticText(22, "Craft"),
        craft_row,
        new Ui::StaticParagraph(300, 13, "Combine 5 of the same petal to craft an upgrade", {}),
        new Ui::StaticParagraph(300, 13, "Failure will destroy 1-4 petals", {}),
        grid
    }, 15, 10, {
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        // Slide up from below on open / down on close, same as the inventory
        // panel. They're mutually exclusive (panel_open enum), so sharing the
        // bottom-left corner never shows both at once.
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - (float) elt->animation) * 2 * elt->height);
        },
        .should_render = [](){ return Ui::panel_open == Panel::kCraft && Game::alive(); },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    // Bottom-left, sliding up above the Craft button (button: x=10, y=-60,
    // height 40) -- same corner and behaviour as the Inventory panel.
    elt->x = 10;
    elt->y = -110;
    return elt;
}
