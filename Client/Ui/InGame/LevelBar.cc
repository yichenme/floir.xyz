#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Shared/StaticData.hh>

#include <format>
#include <string>

using namespace Ui;

// Player HUD bar (top-left, below the game title): live flower model on the
// left; the HP bar runs behind it (flower overlaps its left end), the name is
// drawn on the HP bar just right of the flower, and a shorter XP bar sits just
// below the HP bar, left-aligned with it.
static float const BAR_W = 220;
static float const HP_H = 22;          // 25% thicker than before
static float const XP_H = 12;
static float const GAP = 4;
static float const FACE_R = 26;
static float const XP_FRAC = 0.75f;   // xp bar = 75% of hp bar length

LevelBar::LevelBar() : Element(FACE_R + BAR_W, FACE_R * 2 + 8) {
    progress = 0;
    hp = 1;
    level = 1;
    style.animate = [&](Element *elt, Renderer &ctx) {
        // Level/XP straight from the live score so the bar always reflects it.
        double s = Game::score;
        level = score_to_level(s);
        double into = s - level_to_score(level);
        progress.set(fclamp(into / score_to_pass_level(level), 0, 1));
        if (Game::alive() && Game::simulation.ent_exists(Game::player_id))
            hp.set(Game::simulation.get_ent(Game::player_id).get_health_ratio());
        progress.step(Ui::lerp_amount);
        hp.step(Ui::lerp_amount);
    };
    style.h_justify = Style::Left;
    style.v_justify = Style::Top;
}

void LevelBar::on_render(Renderer &ctx) {
    float const left = -width / 2;
    float const face_cx = left + FACE_R;
    // Bars start at the flower centre so the flower overlaps their left end by
    // ~50% of its width; the flower is drawn on top afterwards.
    float const bar_x0 = face_cx;
    // Centre the hp+xp stack on the flower model's y axis.
    float const stack = HP_H + GAP + XP_H;
    float const hp_y = -stack / 2 + HP_H / 2;
    float const xp_y = stack / 2 - XP_H / 2;

    auto bar = [&](float x0, float y, float len, float h, uint32_t fill, float ratio) {
        RenderContext c(&ctx);
        ctx.set_stroke(0xc0222222);
        ctx.round_line_cap();
        ctx.set_line_width(h);
        ctx.begin_path();
        ctx.move_to(x0, y);
        ctx.line_to(x0 + len, y);
        ctx.stroke();
        ctx.set_stroke(fill);
        ctx.set_line_width(h * 0.72);
        ctx.begin_path();
        ctx.move_to(x0, y);
        ctx.line_to(x0 + len * ratio, y);
        ctx.stroke();
    };
    // HP bar (full) and XP bar (75% length, left-aligned, just below).
    bar(bar_x0, hp_y, BAR_W, HP_H, 0xff75dd34, (float) hp);
    bar(bar_x0, xp_y, BAR_W * XP_FRAC, XP_H, 0xfff9e496, (float) progress);

    // Level label on the XP bar.
    {
        RenderContext c(&ctx);
        ctx.translate(bar_x0 + BAR_W * XP_FRAC / 2, xp_y);
        std::string const t = "Lvl " + std::to_string(level);
        ctx.draw_text(t.c_str(), { .size = XP_H * 0.85f });
    }

    // Name drawn on the HP bar, just right of the flower model.
    {
        RenderContext c(&ctx);
        ctx.translate(face_cx + FACE_R + 8, hp_y);
        std::string const name = Game::nickname.empty() ? std::string("Unnamed") : Game::nickname;
        ctx.set_text_size(16);
        ctx.set_fill(0xffffffff);
        ctx.set_stroke(0xff222222);
        ctx.set_line_width(16 * 0.14);
        // Left-anchor by shifting right half the measured width.
        float const tw = ctx.get_text_size(name.c_str());
        ctx.translate(tw / 2, 0);
        ctx.stroke_text(name.c_str());
        ctx.fill_text(name.c_str());
    }

    // Live flower model on top (eyes/mouth mirror the in-game player).
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
