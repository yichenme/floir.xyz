#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Extern.hh>
#include <Client/Game.hh>
#include <Client/StaticData.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

using namespace Ui;

namespace {
    // First timestamp (ms) each super/unique mob entered vision, so bars can be
    // ordered by appearance. Pruned each frame to only mobs currently in view.
    std::unordered_map<uint32_t, double> g_first_seen;
    // Decaying "recent health" per boss, so the bar shows a red trail draining
    // behind the green fill (same feel as the in-world mob HP bar).
    std::unordered_map<uint32_t, float> g_lag;

    struct BossEntry { uint8_t rarity; float ratio; char const *name; double seen; uint32_t id; };

    // Top-centre boss bars for Super and Unique mobs in view. Unique(s) on top,
    // then by the order they entered vision; at most 3, drawn in absolute screen
    // space (like the mobile joystick) a little below the top edge.
    class BossBars final : public Element {
    public:
        // Sized to cover the bar stack so viewport culling never skips on_render
        // (the actual draw is in absolute screen space via reset_transform).
        BossBars() : Element(400, 300, { .h_justify = Style::Center, .v_justify = Style::Top }) {
            style.no_animation = 1;
            style.should_render = [](){ return Game::alive() && Game::should_render_game_ui(); };
        }

        void on_render(Renderer &ctx) override {
            if (!Game::simulation.ent_exists(Game::camera_id)) return;
            float const fov = Game::simulation.get_ent(Game::camera_id).get_fov();
            float const vs = Ui::scale * fov;
            float const halfw = (float) Ui::window_width / 2, halfh = (float) Ui::window_height / 2;

            std::vector<BossEntry> bosses;
            std::unordered_map<uint32_t, double> seen_now;
            Game::simulation.for_each<kMob>([&](Simulation *sim, Entity const &ent){
                uint8_t const rar = ent.get_mob_rarity();
                if (rar < RarityID::kSuper) return;
                // Petal-summoned creatures (Ant Egg, Beetle Egg, Stick, Square,
                // Queen Ant's ants, ...) never get a boss bar even at Super/Unique.
                if (ent.get_is_summon()) return;
                // On-screen test in screen pixels (the boss bar only shows while
                // the mob is actually in the player's vision).
                float const sx = (ent.get_x() - Game::view_cam_x) * vs;
                float const sy = (ent.get_y() - Game::view_cam_y) * vs;
                float const m = ent.get_radius() * vs;
                if (std::fabs(sx) > halfw + m || std::fabs(sy) > halfh + m) return;
                double const seen = g_first_seen.count(ent.id.id) ? g_first_seen[ent.id.id] : Game::timestamp;
                seen_now[ent.id.id] = seen;
                bosses.push_back({ rar, ent.get_health_ratio(), MOB_DATA[ent.get_mob_id()].name, seen, ent.id.id });
            });
            g_first_seen.swap(seen_now);   // forget mobs that left vision / died
            if (bosses.empty()) return;

            // Unique(s) first, then earliest-seen on top.
            std::sort(bosses.begin(), bosses.end(), [](BossEntry const &a, BossEntry const &b){
                bool const au = a.rarity >= RarityID::kUnique, bu = b.rarity >= RarityID::kUnique;
                if (au != bu) return au;
                return a.seen < b.seen;
            });
            if (bosses.size() > 3) bosses.resize(3);

            float const bar_w = 340, bar_h = 26, name_h = 20, rarity_h = 15;
            float const unit = name_h + 6 + bar_h + rarity_h + 10;   // one bar block

            RenderContext c(&ctx);
            ctx.reset_transform();
            // Top centre, a little below the edge (not touching it).
            ctx.translate(halfw, (float) Ui::window_height * 0.06f);
            ctx.scale(Ui::scale);
            ctx.center_text_align();

            float y = 0;
            for (BossEntry const &b : bosses) {
                // Name above the bar.
                {
                    RenderContext c2(&ctx);
                    ctx.translate(0, y + name_h / 2);
                    ctx.set_text_size(name_h);
                    ctx.set_stroke(0xff222222);
                    ctx.set_line_width(name_h * 0.14f);
                    ctx.set_fill(0xffffffff);
                    ctx.stroke_text(b.name);
                    ctx.fill_text(b.name);
                }
                // Actual HP bar.
                {
                    RenderContext c2(&ctx);
                    ctx.translate(0, y + name_h + 6 + bar_h / 2);
                    ctx.round_line_cap();
                    ctx.set_stroke(0xc0222222);
                    ctx.set_line_width(bar_h);
                    ctx.begin_path();
                    ctx.move_to(-bar_w / 2, 0);
                    ctx.line_to(bar_w / 2, 0);
                    ctx.stroke();
                    // Red "lag" trail: a decaying copy of health that eases down to
                    // the real ratio, so a hit leaves a draining red streak.
                    float const cur = fclamp(b.ratio, 0, 1);
                    float lag = g_lag.count(b.id) ? g_lag[b.id] : cur;
                    if (lag < cur) lag = cur;                 // healing snaps up
                    else lag += (cur - lag) * 0.08f;          // damage drains slowly
                    g_lag[b.id] = lag;
                    ctx.set_stroke(0xffed2f31);
                    ctx.set_line_width(bar_h * 0.72f);
                    ctx.begin_path();
                    ctx.move_to(-bar_w / 2, 0);
                    ctx.line_to(-bar_w / 2 + bar_w * lag, 0);
                    ctx.stroke();
                    ctx.set_stroke(0xff75dd34);
                    ctx.begin_path();
                    ctx.move_to(-bar_w / 2, 0);
                    ctx.line_to(-bar_w / 2 + bar_w * cur, 0);
                    ctx.stroke();
                }
                // Rarity below the bar.
                {
                    RenderContext c2(&ctx);
                    ctx.translate(0, y + name_h + 6 + bar_h + rarity_h / 2 + 2);
                    ctx.set_text_size(rarity_h);
                    ctx.set_stroke(0xff222222);
                    ctx.set_line_width(rarity_h * 0.14f);
                    ctx.set_fill(RARITY_COLORS[b.rarity]);
                    ctx.stroke_text(RARITY_NAMES[b.rarity]);
                    ctx.fill_text(RARITY_NAMES[b.rarity]);
                }
                y += unit;
            }
        }
    };
}

Element *Ui::make_boss_bars() {
    Element *elt = new BossBars();
    elt->style.h_justify = Style::Center;
    elt->style.v_justify = Style::Top;
    return elt;
}
