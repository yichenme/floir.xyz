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

#include <algorithm>
#include <string>
#include <vector>

using namespace Ui;

// Crafting page. Rectangular panel: LEFT half is the inventory laid out as one
// row per petal TYPE x 9 rarity columns (Common..Unique); RIGHT half is the
// craft UI (selected petal preview, success %, Craft button, result). Hovering
// a grid cell shows the same name/rarity/description/damage/health tooltip
// used everywhere else (Ui::UiLoadout::petal_tooltips). Click a left cell to
// select that (type, rarity). One Craft click = one attempt; a double-click,
// or shift+click, crafts the whole stack.
//
// Economics live server-side (Server/EntityFunctions/CraftOps.cc): each
// attempt consumes 5 of the selected stack, rolls craft_success_chance (flat
// per-rarity, 64% halving per tier, 0 at/above Ultra -- Super and Unique are
// never reachable through crafting), on success yields 1 of rarity+1, and
// ALWAYS destroys an extra random 1-4 on top.
namespace {
    PetalID::T g_sel_type = PetalID::kNone;
    uint8_t g_sel_rarity = 0;

    // Brief post-craft result banner.
    double g_result_at = 0;
    float const RESULT_MS = 1400;

    void clear_selection() { g_sel_type = PetalID::kNone; g_sel_rarity = 0; }

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

    // ---- Left grid: one row per owned type, 9 rarity columns ----
    float const CELL = 30.0f, CELL_GAP = 4.0f;

    // Same rounding proportion as the loadout slots (width/20).
    float const CELL_ROUND = CELL / 20.0f;

    class RaritySlot final : public Element {
    public:
        PetalID::T type;
        uint8_t rarity;
        RaritySlot(PetalID::T t, uint8_t r) : Element(CELL, CELL, { .round_radius = CELL_ROUND }), type(t), rarity(r) {}
        void on_render(Renderer &ctx) override {
            uint64_t const count = owned_count(type, rarity);
            bool const selected = g_sel_type == type && g_sel_rarity == rarity && count > 0;
            // Slot background tinted by rarity (muted normally, full brightness
            // when selected) -- same RARITY_COLORS palette used everywhere else
            // (tooltips, gallery, mob cards) so the grid reads consistently.
            ctx.set_fill(selected ? RARITY_COLORS[rarity] : Renderer::HSV(RARITY_COLORS[rarity], 0.45f));
            ctx.begin_path();
            ctx.round_rect(-CELL / 2, -CELL / 2, CELL, CELL, CELL_ROUND);
            ctx.fill();
            if (count == 0) return;   // empty rarity for this type: just the box
            bool const can = craftable(rarity, count);
            ctx.set_global_alpha(can ? 1.0f : 0.4f);
            {
                RenderContext c(&ctx);
                ctx.scale(CELL / 60.0f);
                draw_loadout_background(ctx, type, 1, 1, rarity);
            }
            ctx.set_global_alpha(1.0f);
            std::string const t = format_stack_count(count);
            if (!t.empty()) {
                RenderContext c(&ctx);
                ctx.translate(CELL / 2 - 8, -CELL / 2 + 7);
                ctx.draw_text(t.c_str(), { .fill = 0xffffffff, .size = 10 });
            }
        }
        // Click the petal to craft it directly: single click = one attempt,
        // shift-click or double-click = craft the whole stack. Non-craftable
        // cells (Super/Unique, or fewer than 5) just get selected so the right
        // panel can show why. Hovering shows the same name/rarity/description/
        // damage/health tooltip used on every other petal slot in the game.
        void on_event(uint8_t event) override {
            uint64_t const count = owned_count(type, rarity);
            if (event != kFocusLost && count > 0) {
                rendering_tooltip = 1;
                tooltip = Ui::UiLoadout::petal_tooltips[type][rarity];
            } else if (event == kFocusLost) {
                rendering_tooltip = 0;
            }
            if (event != kClick) return;
            if (count == 0) return;
            g_sel_type = type;
            g_sel_rarity = rarity;
            if (!craftable(rarity, count)) return;
            static double last_click = 0;
            static PetalID::T last_type = PetalID::kNone;
            static uint8_t last_rar = 0;
            bool const shift = Input::keys_held.contains('\x10');
            bool const dbl = (Game::timestamp - last_click) < 350.0 && last_type == type && last_rar == rarity;
            last_click = Game::timestamp; last_type = type; last_rar = rarity;
            do_craft((shift || dbl) ? (uint32_t)count : 1);
        }
    };

    Element *make_type_grid() {
        Element *grid = new VContainer({}, 6, CELL_GAP, {});
        // Distinct owned types, sorted by petal name for a stable layout.
        std::vector<PetalID::T> types;
        for (PetalStack const &s : Game::inventory_stacks) {
            if (s.count == 0) continue;
            if (std::find(types.begin(), types.end(), s.type) == types.end())
                types.push_back(s.type);
        }
        std::sort(types.begin(), types.end(), [](PetalID::T a, PetalID::T b) {
            return std::string(PETAL_DATA[a].name) < std::string(PETAL_DATA[b].name);
        });
        for (PetalID::T type : types) {
            Element *row = new HContainer({}, 0, CELL_GAP, { .v_justify = Style::Top });
            for (uint8_t rar = 0; rar < RarityID::kNumRarities; ++rar)
                row->add_child(new RaritySlot(type, rar));
            row->refactor();
            row->width = RarityID::kNumRarities * CELL + (RarityID::kNumRarities - 1) * CELL_GAP;
            grid->add_child(row);
        }
        return grid;
    }

    class CraftGrid final : public ScrollContainer {
    public:
        CraftGrid() : ScrollContainer(make_type_grid(), 260) {}
        void on_render(Renderer &ctx) override {
            static uint32_t last_version = (uint32_t)-1;
            if (last_version != Game::inventory_version) {
                last_version = Game::inventory_version;
                children[0] = make_type_grid();
                children[0]->parent = this;
                // Drop a selection whose stack no longer exists.
                if (g_sel_type != PetalID::kNone && owned_count(g_sel_type, g_sel_rarity) == 0)
                    clear_selection();
            }
            ScrollContainer::on_render(ctx);
        }
    };

    // ---- Right side: selected preview + name/rarity/stats + chance + Craft
    // button + result ----
    class CraftPreview final : public Element {
    public:
        CraftPreview() : Element(150, 150, {}) {}
        void on_render(Renderer &ctx) override {
            static bool was_open = false;
            bool const open = Ui::panel_open == Panel::kCraft;
            if (was_open && !open) clear_selection();
            was_open = open;

            // Result banner overrides the preview briefly after a craft.
            if (g_result_at > 0 && Game::timestamp - g_result_at < RESULT_MS
                && Game::last_craft_result.received_at >= g_result_at) {
                bool const ok = Game::last_craft_result.any_success;
                std::string const txt = ok
                    ? ("Crafted " + std::to_string(Game::last_craft_result.crafted) + "!")
                    : std::string("Failed");
                ctx.draw_text(txt.c_str(), { .fill = ok ? 0xff75dd34u : 0xffcc3333u, .size = 20 });
                return;
            }
            if (g_sel_type == PetalID::kNone) {
                ctx.draw_text("Select a petal", { .fill = 0xffcccccc, .size = 15 });
                return;
            }
            {
                RenderContext c(&ctx);
                ctx.scale(90.0f / 60.0f);
                draw_loadout_background(ctx, g_sel_type, 1, 1, g_sel_rarity);
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
    Element *grid = new CraftGrid();

    Element *craft_btn = new Ui::Button(120, 40, new Ui::StaticText(17, "Craft"),
        [](Element *e, uint8_t ev) {
            if (ev != Ui::kClick) return;
            uint64_t const count = owned_count(g_sel_type, g_sel_rarity);
            if (!craftable(g_sel_rarity, count)) return;
            // Single click = 1 attempt; shift-click or double-click = craft all.
            static double last_click = 0;
            bool const shift = Input::keys_held.contains('\x10');
            bool const dbl = (Game::timestamp - last_click) < 350.0;
            last_click = Game::timestamp;
            do_craft((shift || dbl) ? (uint32_t)count : 1);
        }, nullptr,
        // Muted warm gray-tan, matching the reference panel's Craft button.
        { .fill = 0xff8a7860, .line_width = 4, .round_radius = 4 }
    );

    Element *right = new VContainer({
        new CraftPreview(),
        new CraftChance(),
        craft_btn
    }, 6, 10, { .v_justify = Style::Middle });

    // h_justify Left on the row (and the text below) so the petal grid --
    // first child of the row -- hugs the panel's left edge instead of being
    // centred by the wider paragraph text.
    Element *row = new HContainer({ grid, right }, 4, 16, { .h_justify = Style::Left, .v_justify = Style::Top });

    // Header: "Craft" title on the left, red close-X on the right (stretches
    // to the panel's full width via HFlexContainer).
    Element *close_btn = new Ui::Button(24, 24, new Ui::StaticText(14, "X"),
        [](Element *, uint8_t ev) {
            if (ev != Ui::kClick) return;
            Ui::panel_open = Panel::kNone;
        }, nullptr,
        { .fill = 0xffd9534f, .line_width = 3, .round_radius = 12 }
    );
    Element *header = new Ui::HFlexContainer(
        new Ui::StaticText(22, "Craft", { .fill = 0xffffffff }),
        close_btn, 0, 10, {}
    );

    class CraftPanel final : public VContainer {
    public:
        using VContainer::VContainer;
    };

    Element *elt = new CraftPanel(std::vector<Element *>{
        header,
        row,
        new Ui::StaticParagraph(500, 12, "Click a petal to craft it: 5 become a chance at +1 rarity; every craft also loses 1-4 extra.", { .h_justify = Style::Left }),
        new Ui::StaticParagraph(500, 12, "Single click = 1 craft. Shift-click or double-click = craft the whole stack.", { .h_justify = Style::Left })
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
