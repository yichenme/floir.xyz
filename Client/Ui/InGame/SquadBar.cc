#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>

#include <Shared/Entity.hh>

#include <Helpers/Math.hh>

#include <string>

using namespace Ui;

namespace {
    // Compact per-squadmate rows under the main LevelBar: a mini flower model on
    // the left with the member's HP bar running behind it and their name to the
    // right -- the same layout as the player's own LevelBar, scaled down. The
    // bar's left edge is aligned with the player HP bar's left edge (both start
    // 26px in from x=20, i.e. screen x 46). Max 3 rows (squad cap 4 -> 3 others).
    // Flower icon is the SAME size as the player's own bar (FACE_R 26); only the
    // bar is shorter -- 75% of the player's 220-wide HP bar. Rows are spaced for
    // the full-size flower.
    float const BAR_W = 165, BAR_H = 20, GAP = 12, FACE_R = 26, FACE_OFF = 26;

    class SquadBars final : public Element {
    public:
        SquadBars() : Element(FACE_OFF + BAR_W, 3 * (BAR_H + GAP),
                              { .h_justify = Style::Left, .v_justify = Style::Top }) {
            style.no_animation = 1;
            style.should_render = [](){
                return Game::alive() && Game::should_render_game_ui() && Game::squad_id != 0;
            };
        }

        void on_render(Renderer &ctx) override {
            // Bar left edge, matched to LevelBar's: -width/2 + FACE_OFF lands at
            // the same screen x as the player HP bar (both anchored at x=20).
            float const bar_x0 = -width / 2 + FACE_OFF;
            float y = 0;
            uint32_t shown = 0;
            for (Game::SquadMember const &m : Game::squad_members) {
                if (m.camera_id == Game::camera_id) continue;   // self already has the full bar
                if (shown >= 3) break;
                float hp = 0;
                bool has_flower = false;
                Entity const *flower = nullptr;
                if (Game::simulation.ent_exists(m.camera_id)) {
                    Entity const &cam = Game::simulation.get_ent(m.camera_id);
                    if (Game::simulation.ent_exists(cam.get_player())) {
                        flower = &Game::simulation.get_ent(cam.get_player());
                        hp = flower->get_health_ratio();
                        has_flower = true;
                    }
                }
                RenderContext c(&ctx);
                ctx.translate(0, y);
                // HP bar (background + green fill), starting at bar_x0.
                ctx.round_line_cap();
                ctx.set_stroke(0xc0222222);
                ctx.set_line_width(BAR_H);
                ctx.begin_path();
                ctx.move_to(bar_x0, 0);
                ctx.line_to(bar_x0 + BAR_W, 0);
                ctx.stroke();
                if (has_flower) {
                    ctx.set_stroke(0xff75dd34);
                    ctx.set_line_width(BAR_H * 0.72f);
                    ctx.begin_path();
                    ctx.move_to(bar_x0, 0);
                    ctx.line_to(bar_x0 + BAR_W * fclamp(hp, 0, 1), 0);
                    ctx.stroke();
                }
                // Name to the right of the flower model, left-anchored.
                {
                    RenderContext c2(&ctx);
                    ctx.translate(bar_x0 + FACE_R + 6, 0);
                    std::string const label = m.name.empty() ? "..." : m.name;
                    float const ts = BAR_H * 0.72f;
                    ctx.set_text_size(ts);
                    ctx.set_fill(0xffffffff);
                    ctx.set_stroke(0xff222222);
                    ctx.set_line_width(ts * 0.14f);
                    float const tw = ctx.get_text_size(label.c_str());
                    ctx.translate(tw / 2, 0);
                    ctx.stroke_text(label.c_str());
                    ctx.fill_text(label.c_str());
                }
                // Mini flower model on top, overlapping the bar's left end.
                {
                    RenderContext c2(&ctx);
                    ctx.translate(bar_x0, 0);
                    if (has_flower) {
                        draw_static_flower(ctx, {
                            .radius = FACE_R,
                            .eye_x = flower->eye_x,
                            .eye_y = flower->eye_y,
                            .mouth = flower->mouth,
                            .face_flags = flower->get_face_flags(),
                            // Show equipped face gear (antennae / third eye / cutter).
                            .equip_flags = flower->get_equip_flags(),
                            .color = flower->get_color()
                        });
                    } else {
                        // No synced flower (dead / far): a plain grey disc stand-in.
                        ctx.set_fill(0xff888888);
                        ctx.set_stroke(0xff5a5a5a);
                        ctx.set_line_width(3);
                        ctx.begin_path();
                        ctx.arc(0, 0, FACE_R);
                        ctx.fill();
                        ctx.stroke();
                    }
                }
                y += BAR_H + GAP;
                ++shown;
            }
        }
    };
}

Element *Ui::make_squad_bars() {
    Element *elt = new SquadBars();
    elt->style.h_justify = Style::Left;
    elt->style.v_justify = Style::Top;
    // x=20 matches LevelBar, so the bar-left alignment math above holds; y sits
    // just below the player LevelBar.
    elt->x = 20;
    elt->y = 130;
    return elt;
}
