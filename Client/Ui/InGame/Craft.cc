#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/InGame/Loadout.hh>
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

#include <cmath>
#include <string>
#include <vector>

using namespace Ui;

// Crafting page. Rectangular panel split into two roughly equal halves: LEFT
// is the same grid as the real Inventory panel (Inventory.cc) -- one slot per
// owned stack, 5 columns, 60px slots -- wired for selection instead of
// drag-to-equip, and FILTERED to stacks of 5+ (fewer can never be crafted, so
// they're just left out instead of cluttering the list); RIGHT is the craft
// UI, a 5-box pentagon preview + success % + Craft button + result.
// Uncraftable stacks (Super or Unique rarity) still render in black/white/
// gray instead of a translucent icon. Hovering a grid slot, or anywhere over
// the pentagon boxes, shows the same name/rarity/description/damage/health
// tooltip used everywhere else (Ui::UiLoadout::petal_tooltips).
//
// Clicking a slot never crafts by itself -- it only selects (type, rarity)
// and previews what a Craft click would consume: a plain click shows 1 in
// each of the 5 pentagon boxes (5 total = one attempt), a shift-click splits
// the whole owned stack evenly across the 5 boxes. Only clicking the Craft
// button actually sends the request.
//
// Economics live server-side (Server/EntityFunctions/CraftOps.cc): each
// attempt consumes 5 of the selected stack, rolls craft_success_chance (flat
// per-rarity, 64% halving per tier, 0 at/above Ultra -- Super and Unique are
// never reachable through crafting), on success yields 1 of rarity+1, and
// ALWAYS destroys an extra random 1-4 on top.
namespace {
    PetalID::T g_sel_type = PetalID::kNone;
    uint8_t g_sel_rarity = 0;
    // Set by a shift-click on a grid cell: true = the whole stack is queued
    // (previewed split across the 5 pentagon boxes), false = a plain click,
    // one attempt's worth (5 total, 1 per box). Only the Craft button actually
    // sends anything to the server -- selecting never crafts by itself.
    bool g_craft_all = false;

    // Brief post-craft result banner.
    double g_result_at = 0;
    float const RESULT_MS = 1400;

    void clear_selection() { g_sel_type = PetalID::kNone; g_sel_rarity = 0; g_craft_all = false; }

    uint64_t owned_count(PetalID::T type, uint8_t rarity) {
        for (PetalStack const &s : Game::inventory_stacks)
            if (s.type == type && s.rarity == rarity) return s.count;
        return 0;
    }

    bool craftable(uint8_t rarity, uint64_t count) {
        return rarity < RarityID::kUltra && count >= 5;
    }

    // amount==1 -> single attempt; amount==owned -> "craft all" (server caps).
    void do_craft(uint32_t amount) {
        if (!craftable(g_sel_rarity, owned_count(g_sel_type, g_sel_rarity))) return;
        Game::send_craft(g_sel_type, g_sel_rarity, amount);
        g_result_at = Game::timestamp;
    }

    // ---- Left side: the SAME grid as the real Inventory panel (Inventory.cc)
    // -- one slot per owned stack, 5 columns, 60px slots, pre-allocated up to
    // a display cap with should_render hiding unused slots. This is a
    // selection grid, not drag-to-equip, so clicking selects (type, rarity)
    // for crafting instead of equipping to the loadout.
    //
    // Unlike the real inventory, this list is FILTERED to stacks with 5+
    // (fewer can never be crafted, so showing them just clutters the list).
    // Filtering means the slot's `index` no longer maps 1:1 onto
    // Game::inventory_display_order, so a separate g_craft_order cache holds
    // the filtered real-stack indices, refreshed only when
    // Game::inventory_version actually changes (cheap check every frame).
    uint32_t const CRAFT_DISPLAY_CAP = 200;
    uint32_t const CRAFT_COLUMNS = 5;
    float const CRAFT_SLOT_SIZE = 60.0f;

    std::vector<uint32_t> g_craft_order;
    uint32_t g_craft_order_version = (uint32_t) -1;

    void refresh_craft_order() {
        if (g_craft_order_version == Game::inventory_version) return;
        g_craft_order_version = Game::inventory_version;
        g_craft_order.clear();
        for (uint32_t real : Game::inventory_display_order)
            if (Game::inventory_stacks[real].count >= 5)
                g_craft_order.push_back(real);
    }

    class CraftStackSlot final : public Element {
    public:
        uint32_t index;
        CraftStackSlot(uint32_t idx) : Element(CRAFT_SLOT_SIZE, CRAFT_SLOT_SIZE, { .h_justify = Style::Left }), index(idx) {
            style.should_render = [this](){ refresh_craft_order(); return index < g_craft_order.size(); };
        }
        void on_render(Renderer &ctx) override {
            refresh_craft_order();
            if (index >= g_craft_order.size()) return;
            PetalStack const &stack = Game::inventory_stacks[g_craft_order[index]];
            bool const selected = g_sel_type == stack.type && g_sel_rarity == stack.rarity;
            if (selected) {
                ctx.set_fill(0x60ffffff);
                ctx.begin_path();
                ctx.round_rect(-width / 2, -height / 2, width, height, width / 20);
                ctx.fill();
            }
            bool const can = craftable(stack.rarity, stack.count);
            {
                RenderContext c(&ctx);
                // Uncraftable stacks (Super/Unique rarity, or fewer than 5
                // owned) render in black/white/gray instead of a translucent
                // full-color icon -- a flat color filter forces every
                // fill/stroke toward mid-gray.
                if (!can) ctx.add_color_filter(0xff888888, 1.0f);
                draw_loadout_background(ctx, stack.type, 1, 1, stack.rarity);
            }
            std::string const t = format_stack_count(stack.count);
            if (!t.empty()) {
                RenderContext c(&ctx);
                ctx.translate(width / 2 - 12, -height / 2 + 10);
                ctx.draw_text(t.c_str(), { .fill = 0xffffffff, .size = 13 });
            }
        }
        // Click ONLY selects -- it never crafts by itself. A plain click
        // previews one attempt (5 total, shown as 1 in each of the 5 pentagon
        // boxes); shift-click previews the whole stack split evenly across
        // the 5 boxes. Either way, the Craft button is what actually sends
        // the request. Hovering shows the same name/rarity/description/
        // damage/health tooltip used on every other petal slot in the game.
        void on_event(uint8_t event) override {
            refresh_craft_order();
            if (index >= g_craft_order.size()) { rendering_tooltip = 0; return; }
            PetalStack const &stack = Game::inventory_stacks[g_craft_order[index]];
            if (event != kFocusLost) {
                rendering_tooltip = 1;
                tooltip = Ui::UiLoadout::petal_tooltips[stack.type][stack.rarity];
            } else {
                rendering_tooltip = 0;
            }
            if (event != kClick) return;
            g_sel_type = stack.type;
            g_sel_rarity = stack.rarity;
            g_craft_all = Input::keys_held.contains('\x10');
        }
    };

    Element *make_craft_grid() {
        Element *grid = new VContainer({}, 10, 10, {});
        for (uint32_t i = 0; i < CRAFT_DISPLAY_CAP;) {
            uint32_t const start = i;
            Element *row = new HContainer({}, 0, 10, { .v_justify = Style::Top });
            for (uint32_t j = 0; j < CRAFT_COLUMNS && i < CRAFT_DISPLAY_CAP; ++j, ++i)
                row->add_child(new CraftStackSlot(i));
            row->refactor();
            row->width = CRAFT_COLUMNS * CRAFT_SLOT_SIZE + (CRAFT_COLUMNS - 1) * 10;
            row->style.should_render = [start](){ refresh_craft_order(); return start < g_craft_order.size(); };
            grid->add_child(row);
        }
        return new ScrollContainer(grid, 260);
    }

    // ---- Right side: 5-box pentagon craft preview + chance + Craft button +
    // result ----
    float const PENT_CELL = 56.0f, PENT_RADIUS = 95.0f;

    // 5 boxes arranged in a point-up pentagon (one top, two upper-flanking,
    // two lower-flanking -- the same angular spacing as a 5-petal flower).
    // Selecting a grid cell doesn't craft; it just fills these boxes as a
    // preview of what a Craft click will actually consume: a plain click
    // shows 1 per box (5 total, one attempt), a shift-click shows the whole
    // owned stack split evenly across the 5 boxes. All 5 boxes show the same
    // petal, so hovering anywhere over this element shows one tooltip for the
    // selected (type, rarity) -- the same info box every other petal slot in
    // the game shows on hover.
    class PentagonCraft final : public Element {
    public:
        PentagonCraft() : Element(2 * (PENT_RADIUS + PENT_CELL / 2), 2 * (PENT_RADIUS + PENT_CELL / 2), {}) {}
        void on_event(uint8_t event) override {
            if (event != kFocusLost && g_sel_type != PetalID::kNone) {
                rendering_tooltip = 1;
                tooltip = Ui::UiLoadout::petal_tooltips[g_sel_type][g_sel_rarity];
            } else {
                rendering_tooltip = 0;
            }
        }
        void on_render(Renderer &ctx) override {
            static bool was_open = false;
            bool const open = Ui::panel_open == Panel::kCraft;
            if (was_open && !open) clear_selection();
            was_open = open;

            // Result banner overrides the pentagon briefly after a craft.
            if (g_result_at > 0 && Game::timestamp - g_result_at < RESULT_MS
                && Game::last_craft_result.received_at >= g_result_at) {
                bool const ok = Game::last_craft_result.any_success;
                std::string const txt = ok
                    ? ("Crafted " + std::to_string(Game::last_craft_result.crafted) + "!")
                    : std::string("Failed");
                ctx.draw_text(txt.c_str(), { .fill = ok ? 0xff75dd34u : 0xffcc3333u, .size = 20 });
                return;
            }

            uint64_t const owned = g_sel_type == PetalID::kNone ? 0 : owned_count(g_sel_type, g_sel_rarity);
            uint64_t const per_box = g_sel_type == PetalID::kNone ? 0 : (g_craft_all ? owned / 5 : 1);

            for (int i = 0; i < 5; ++i) {
                float const angle = -(float) M_PI / 2 + i * (2.0f * (float) M_PI / 5);
                RenderContext c(&ctx);
                ctx.translate(PENT_RADIUS * cosf(angle), PENT_RADIUS * sinf(angle));
                // Pure black at the same opacity as the grid's scrollbar
                // (Ui::ScrollBar uses 0x40000000), no stroke -- matches its
                // flat, single-tone look.
                ctx.set_fill(0x40000000);
                ctx.begin_path();
                ctx.round_rect(-PENT_CELL / 2, -PENT_CELL / 2, PENT_CELL, PENT_CELL, PENT_CELL / 10);
                ctx.fill();
                if (g_sel_type == PetalID::kNone) continue;
                {
                    RenderContext c2(&ctx);
                    ctx.scale(PENT_CELL / 60.0f);
                    draw_loadout_background(ctx, g_sel_type, 1, 1, g_sel_rarity);
                }
                if (per_box > 0) {
                    RenderContext c3(&ctx);
                    ctx.translate(PENT_CELL / 2 - 11, -PENT_CELL / 2 + 10);
                    ctx.draw_text(format_stack_count(per_box).c_str(), { .fill = 0xffffffff, .size = 12 });
                }
            }
        }
    };

    class CraftChance final : public Element {
    public:
        CraftChance() : Element(160, 18, {}) {}
        void on_render(Renderer &ctx) override {
            if (g_sel_type == PetalID::kNone) return;
            uint64_t const count = owned_count(g_sel_type, g_sel_rarity);
            if (g_sel_rarity >= RarityID::kUltra) {
                ctx.draw_text("Not craftable", { .fill = 0xffcc7777, .size = 14 });
                return;
            }
            float const chance = craft_success_chance(g_sel_rarity) * 100.0f;
            std::string txt = format_pct(chance) + " success";
            if (count < 5) txt = "Need 5+ (" + format_stack_count(count) + ")";
            ctx.draw_text(txt.c_str(), { .fill = 0xffffffff, .size = 14 });
        }
    };
}

Element *Ui::make_craft_button() {
    class CraftIcon final : public Element {
    public:
        CraftIcon() : Element(140, 40, {
            // Same warm tan/caramel as the crafting panel it opens.
            .fill = 0xffc9975b, .line_width = 5, .round_radius = 3,
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
    Element *elt = new CraftIcon();
    elt->x = 10;
    elt->y = -60;
    return elt;
}

Element *Ui::make_craft_panel() {
    Element *grid = make_craft_grid();
    // If nothing is craftable (no stack of 5+), drop the whole left side --
    // grid AND its scrollbar -- instead of showing an empty list with a
    // scrollbar that has nothing to scroll.
    grid->style.should_render = [](){ refresh_craft_order(); return !g_craft_order.empty(); };

    // Craft is the ONLY thing that actually crafts: the selection just set up
    // the (type, rarity) and whether the whole stack is queued (g_craft_all).
    Element *craft_btn = new Ui::Button(120, 40, new Ui::StaticText(17, "Craft"),
        [](Element *e, uint8_t ev) {
            if (ev != Ui::kClick) return;
            uint64_t const count = owned_count(g_sel_type, g_sel_rarity);
            if (!craftable(g_sel_rarity, count)) return;
            do_craft(g_craft_all ? (uint32_t)count : 1);
        }, nullptr,
        // Muted warm gray-tan, matching the reference panel's Craft button.
        { .fill = 0xff8a7860, .line_width = 4, .round_radius = 4 }
    );

    Element *right = new VContainer({
        new PentagonCraft(),
        new CraftChance(),
        craft_btn
    }, 6, 10, { .v_justify = Style::Middle });

    // h_justify Left on the row (and the text below) so the petal grid --
    // first child of the row -- hugs the panel's left edge instead of being
    // centred by the wider paragraph text. Left (grid) and right (craft UI)
    // are roughly equal width panel halves.
    Element *row = new HContainer({ grid, right }, 4, 16, { .h_justify = Style::Left, .v_justify = Style::Top });

    class CraftPanel final : public VContainer {
    public:
        using VContainer::VContainer;
    };

    Element *elt = new CraftPanel(std::vector<Element *>{
        new Ui::StaticText(22, "Craft", { .fill = 0xffffffff, .h_justify = Style::Left }),
        row,
        new Ui::StaticParagraph(500, 12, "Click a petal to select it (shift-click to queue the whole stack), then hit Craft: 5 become a chance at +1 rarity, and every attempt always loses 1-4 extra.", { .h_justify = Style::Left })
    }, 14, 8, {
        // Warm tan/caramel panel (border auto-derives a darker brown via
        // Style::stroke_hsv), matching the reference craft panel's page color.
        .fill = 0xffc9975b,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - (float) elt->animation) * 2 * elt->height);
        },
        .should_render = [](){ return Ui::panel_open == Panel::kCraft && Game::alive(); },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    elt->x = 10;
    elt->y = -110;
    return elt;
}
