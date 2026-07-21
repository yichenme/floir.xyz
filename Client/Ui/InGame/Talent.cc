#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Ui/Extern.hh>

#include <Client/Game.hh>
#include <Client/StaticData.hh>

#include <Shared/TalentData.hh>

#include <Helpers/Math.hh>

#include <format>
#include <string>

using namespace Ui;

// Talent panel: two rows (Petal Health, Reload), each a strip of
// TALENT_MAX_RANK circles -- one per rarity tier, Common..Unique -- colored
// by RARITY_COLORS. A circle beyond the account's current rank is grayed
// out; the very next unbought circle is clickable (buys exactly that rank);
// anything already owned is fully colored but inert. Hovering any circle
// shows a small stat-box tooltip (tier name, before/after value, TP cost),
// matching the hover convention every petal slot in the game already uses.
//
// Ranks/costs live in Shared/TalentData.hh so the panel's displayed numbers
// and the server's actual roll (Server/EntityFunctions/TalentOps.cc) can't
// drift apart. TP is 1 per 5 levels, shared across both trees -- the
// available count is pushed by the server (Clientbound::kTalentUpdate) on
// spawn and after every successful buy, not computed locally from a client-
// tracked level (the client doesn't reliably track its own live level).
namespace {
    // Panel/button color scheme, explicit hex per spec rather than the usual
    // auto-derived-from-fill Style::stroke_hsv border -- drawn the same way
    // Element::on_render draws any filled box (outer rect in the border
    // color, inset rect in the main color) so it still reads as "one style."
    uint32_t const TALENT_MAIN = 0xffdc5a5a;
    uint32_t const TALENT_OUTLINE = 0xffb24849;
    float const CIRCLE_R = 14.0f;
    float const CIRCLE_GAP = 8.0f;

    void draw_bordered_rect(Renderer &ctx, float w, float h, float round, float border_w,
                             uint32_t outline, uint32_t main) {
        ctx.set_fill(outline);
        ctx.begin_path();
        ctx.round_rect(-w / 2, -h / 2, w, h, round);
        ctx.fill();
        ctx.set_fill(main);
        ctx.begin_path();
        ctx.rect(-w / 2 + border_w, -h / 2 + border_w, w - 2 * border_w, h - 2 * border_w);
        ctx.fill();
    }

    uint32_t spent_tp() {
        return talent_cumulative_cost(TalentTree::kHealth, Game::talent_health_rank)
             + talent_cumulative_cost(TalentTree::kReload, Game::talent_reload_rank);
    }

    uint32_t available_tp() {
        uint32_t const spent = spent_tp();
        return Game::talent_points_total > spent ? Game::talent_points_total - spent : 0;
    }

    void make_rank_tooltip_body(Element *card, TalentTree::T tree, uint8_t rank) {
        std::string const label = tree == TalentTree::kHealth ? "Petal Health" : "Reload";
        std::string value;
        if (tree == TalentTree::kHealth)
            value = format_pct(talent_health_mult(rank) * 100.f);
        else
            value = "-" + format_pct((1.f - talent_reload_mult(rank)) * 100.f);
        uint32_t const cost = talent_rank_cost(tree, rank);
        card->add_child(new Ui::StaticText(16, RARITY_NAMES[rank - 1], { .fill = RARITY_COLORS[rank - 1], .h_justify = Style::Left }));
        card->add_child(new Ui::StaticText(13, label + ": " + value, { .fill = 0xffffffff, .h_justify = Style::Left }));
        card->add_child(new Ui::StaticText(12, "Cost: " + std::to_string(cost) + " TP", { .fill = 0xffcccccc, .h_justify = Style::Left }));
    }

    // One clickable/inert circle in a talent row.
    class TalentRankCircle final : public Element {
    public:
        TalentTree::T tree;
        uint8_t rank;   // 1-indexed: this circle buys/represents rank `rank`
        Element *tooltip_card;

        TalentRankCircle(TalentTree::T t, uint8_t r) :
            Element(2 * CIRCLE_R, 2 * CIRCLE_R, {}), tree(t), rank(r)
        {
            Element *card = new Ui::VContainer({}, 6, 2, { .h_justify = Style::Left });
            make_rank_tooltip_body(card, tree, rank);
            card->style.fill = 0x80000000;
            card->style.round_radius = 6;
            card->refactor();
            tooltip_card = card;
        }

        void on_render(Renderer &ctx) override {
            uint8_t const owned_rank = tree == TalentTree::kHealth ? Game::talent_health_rank : Game::talent_reload_rank;
            uint32_t const rarity = rank - 1;
            bool const owned = rank <= owned_rank;
            bool const buyable = rank == owned_rank + 1 && talent_rank_cost(tree, rank) <= available_tp();
            uint32_t const base = RARITY_COLORS[rarity];
            uint32_t fill = base;
            if (!owned && !buyable) fill = 0xff555555;   // out of reach: flat gray
            ctx.set_fill(Renderer::HSV(fill, 0.7));
            ctx.begin_path();
            ctx.arc(0, 0, CIRCLE_R);
            ctx.fill();
            ctx.set_fill(fill);
            ctx.begin_path();
            ctx.arc(0, 0, CIRCLE_R - 2.5f);
            ctx.fill();
            std::string const label = tree == TalentTree::kHealth ? "ph" : "rld";
            ctx.draw_text(label.c_str(), { .fill = 0xffffffff, .size = tree == TalentTree::kHealth ? 11.f : 9.f });
        }

        void on_event(uint8_t event) override {
            if (event != kFocusLost) {
                rendering_tooltip = 1;
                tooltip = tooltip_card;
            } else {
                rendering_tooltip = 0;
            }
            if (event != kClick) return;
            uint8_t const owned_rank = tree == TalentTree::kHealth ? Game::talent_health_rank : Game::talent_reload_rank;
            if (rank != owned_rank + 1) return;   // only the very next rank is buyable
            if (talent_rank_cost(tree, rank) > available_tp()) return;
            Game::send_talent_buy((uint8_t)tree);
        }
    };

    Element *make_talent_row(TalentTree::T tree) {
        Element *row = new HContainer({}, 0, (int)CIRCLE_GAP, { .h_justify = Style::Left });
        for (uint8_t r = 1; r <= TALENT_MAX_RANK; ++r)
            row->add_child(new TalentRankCircle(tree, r));
        return row;
    }

    class TpCounter final : public Element {
    public:
        TpCounter() : Element(70, 18, { .h_justify = Style::Right }) {}
        void on_render(Renderer &ctx) override {
            std::string const t = "TP: " + std::to_string(available_tp());
            ctx.draw_text(t.c_str(), { .fill = 0xffffffff, .size = 15 });
        }
    };
}

Element *Ui::make_talent_button() {
    class TalentIcon final : public Element {
    public:
        TalentIcon() : Element(140, 40, {
            .should_render = [](){ return Game::alive(); },
            .h_justify = Style::Left, .v_justify = Style::Bottom
        }) {}
        void on_render(Renderer &ctx) override {
            draw_bordered_rect(ctx, width, height, 3, 5, TALENT_OUTLINE, TALENT_MAIN);
            ctx.draw_text("Talents", { .fill = 0xffffffff, .size = 18 });
        }
        void on_event(uint8_t event) override {
            if (event != kClick) return;
            Ui::panel_open = Ui::panel_open == Panel::kTalent ? Panel::kNone : Panel::kTalent;
        }
    };
    Element *elt = new TalentIcon();
    elt->x = 10;
    elt->y = -110;   // directly above the Craft button (Craft.cc: x=10, y=-60)
    return elt;
}

Element *Ui::make_talent_panel() {
    Element *health_row = make_talent_row(TalentTree::kHealth);
    Element *reload_row = make_talent_row(TalentTree::kReload);

    Element *header = new HContainer({
        new Ui::StaticText(22, "Talents", { .fill = 0xffffffff, .h_justify = Style::Left }),
        new TpCounter()
    }, 0, 20, { .h_justify = Style::Left });

    class TalentPanel final : public VContainer {
    public:
        using VContainer::VContainer;
    };

    Element *elt = new TalentPanel(std::vector<Element *>{
        header,
        new Ui::StaticText(14, "Petal Health", { .fill = 0xffffffff, .h_justify = Style::Left }),
        health_row,
        new Ui::StaticText(14, "Reload", { .fill = 0xffffffff, .h_justify = Style::Left }),
        reload_row
    }, 10, 8, {
        .animate = [](Element *elt, Renderer &ctx){
            ctx.scale((float) elt->animation);
        },
        .should_render = [](){ return Ui::panel_open == Panel::kTalent && Game::alive(); },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    // Custom-drawn border (see TalentIcon) instead of the generic Style::fill
    // path, so the exact spec'd main/outline hex pair is used verbatim.
    elt->style.animate = [](Element *e, Renderer &ctx){
        ctx.scale((float) e->animation);
        draw_bordered_rect(ctx, e->width, e->height, 3, 7, TALENT_OUTLINE, TALENT_MAIN);
    };
    elt->x = 10;
    elt->y = -160;
    return elt;
}
