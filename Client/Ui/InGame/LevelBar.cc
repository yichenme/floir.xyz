#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Shared/StaticData.hh>

#include <format>
#include <string>

using namespace Ui;

// Player HUD bar (top-left, below the game title): live flower model on the
// left; to its right the name sits above the HP bar, with the XP bar stacked
// below the HP bar.
static float const BAR_W = 220;
static float const HP_H = 16;
static float const XP_H = 14;
static float const FACE_R = 26;

LevelBar::LevelBar() : Element(FACE_R * 2 + 12 + BAR_W, FACE_R * 2 + 8) {
    progress = 0;
    hp = 1;
    style.animate = [&](Element *elt, Renderer &ctx) {
        if (Game::alive() && Game::simulation.ent_exists(Game::player_id)) {
            float xp = Game::score;
            level = score_to_level(xp);
            xp -= level_to_score(level);
            xp = fclamp(xp / score_to_pass_level(level), 0, 1);
            progress.set(xp);
            hp.set(Game::simulation.get_ent(Game::player_id).get_health_ratio());
        }
        progress.step(Ui::lerp_amount);
        hp.step(Ui::lerp_amount);
    };
    style.h_justify = Style::Left;
    style.v_justify = Style::Top;
}

void LevelBar::on_render(Renderer &ctx) {
    float const left = -width / 2;
    float const face_cx = left + FACE_R;
    float const bar_x0 = face_cx + FACE_R + 12;
    float const bar_w = width / 2 - bar_x0;

    // Live flower model (eyes/mouth mirror the in-game player).
    if (Game::alive() && Game::simulation.ent_exists(Game::player_id)) {
        Entity const &player = Game::simulation.get_ent(Game::player_id);
        RenderContext c(&ctx);
        ctx.translate(face_cx, 0);
        draw_static_flower(ctx, {
            .radius = FACE_R,
            .eye_x = player.eye_x,
            .eye_y = player.eye_y,
            .mouth = player.mouth,
            .face_flags = player.get_face_flags(),
            .equip_flags = 0,
            .color = player.get_color()
        });
    }

    // Name above the HP bar.
    {
        RenderContext c(&ctx);
        ctx.translate(bar_x0 + bar_w / 2, -HP_H / 2 - 12);
        std::string const name = Game::nickname.empty() ? std::string("Unnamed") : Game::nickname;
        ctx.draw_text(name.c_str(), { .size = 18 });
    }

    // HP bar.
    auto bar = [&](float y, float h, uint32_t fill, float ratio) {
        RenderContext c(&ctx);
        ctx.set_stroke(0xc0222222);
        ctx.round_line_cap();
        ctx.set_line_width(h);
        ctx.begin_path();
        ctx.move_to(bar_x0, y);
        ctx.line_to(bar_x0 + bar_w, y);
        ctx.stroke();
        ctx.set_stroke(fill);
        ctx.set_line_width(h * 0.75);
        ctx.begin_path();
        ctx.move_to(bar_x0, y);
        ctx.line_to(bar_x0 + bar_w * ratio, y);
        ctx.stroke();
    };
    bar(2, HP_H, 0xff75dd34, (float) hp);
    bar(2 + HP_H / 2 + XP_H / 2 + 6, XP_H, 0xfff9e496, (float) progress);

    // Level label on the XP bar.
    {
        RenderContext c(&ctx);
        ctx.translate(bar_x0 + bar_w / 2, 2 + HP_H / 2 + XP_H / 2 + 6);
        std::string const t = "Lvl " + std::to_string(level);
        ctx.draw_text(t.c_str(), { .size = XP_H * 0.8f });
    }
}

Element *Ui::make_level_bar() {
    Element *bar = new Ui::LevelBar();
    bar->style.should_render = [](){ return Game::alive() && Game::should_render_game_ui(); };
    bar->style.h_justify = Style::Left;
    bar->style.v_justify = Style::Top;
    // Below the "floir.xyz" game title in the top-left.
    bar->x = 20;
    bar->y = 54;
    bar->style.animate = [](Element *elt, Renderer &ctx) {
        ctx.translate(-(1 - (float) elt->animation) * 1.5 * elt->width, 0);
    };
    return bar;
}
