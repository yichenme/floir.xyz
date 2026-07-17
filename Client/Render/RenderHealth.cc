#include <Client/Render/RenderEntity.hh>

#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Render/Renderer.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Client/StaticData.hh>

#include <algorithm>

// Largest font size at which `text` fits within `max_w` pixels, capped at
// `base` — so labels shrink to at most half the HP bar and never overflow it.
static float fit_text_size(char const *text, float base, float max_w) {
    float const units = Renderer::get_ascii_text_size(text);
    if (units <= 0) return base;
    return std::min(base, max_w / units);
}

void render_health(Renderer &ctx, Entity const &ent) {
    if (ent.has_component(kPetal)) return;
    bool const is_mob = ent.has_component(kMob);
    // Players fade their bar in on damage / out at full health; mobs stay
    // always visible (so their strength is readable before engaging), fading
    // only on death like everything else.
    if (!is_mob && ent.healthbar_opacity < 0.01) return;
    float const rad = ent.get_radius();
    // Mob HP bar = the model's full horizontal length (diameter); w is half.
    float const w = rad * (is_mob ? 1.0f : 1.33f);
    ctx.set_global_alpha((1 - ent.deletion_animation) * (is_mob ? 1.0f : ent.healthbar_opacity));
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    if (is_mob) {
        float const bar_y = rad + 20;   // bar sits just below the model
        uint8_t const rarity = ent.get_mob_rarity();
        // Name + rarity both cap their width at half the bar (= w), so they
        // scale down with small mobs and never overrun the bar on long names.
        // Name: white + black outline, above the bar, left-aligned to its left edge.
        {
            RenderContext c(&ctx);
            char const *name = MOB_DATA[ent.get_mob_id()].name;
            ctx.left_text_align();
            ctx.translate(-w, bar_y - 11);
            ctx.draw_text(name, { .fill = 0xffffffff, .size = fit_text_size(name, 13, w) });
        }
        // Rarity: rarity colour + black outline, below the bar, right-aligned to its right edge.
        {
            RenderContext c(&ctx);
            char const *rname = RARITY_NAMES[rarity];
            ctx.right_text_align();
            ctx.translate(w, bar_y + 11);
            ctx.draw_text(rname, { .fill = RARITY_COLORS[rarity], .size = fit_text_size(rname, 11, w) });
        }
        ctx.center_text_align();   // restore for later draws
        ctx.translate(-w, bar_y);
    } else {
        ctx.translate(-w, rad + 15);
    }
    ctx.round_line_cap();
    ctx.set_stroke(0xff222222);
    ctx.set_line_width(9);
    ctx.begin_path();
    ctx.move_to(0, 0);
    ctx.line_to(2 * w, 0);
    ctx.stroke();
    if (ent.healthbar_lag > ent.get_health_ratio()) {
        ctx.set_stroke(0xffed2f31);
        ctx.set_line_width(7);
        ctx.begin_path();
        ctx.move_to(2 * w * ent.get_health_ratio(), 0);
        ctx.line_to(2 * w * ent.healthbar_lag, 0);
        ctx.stroke();
    }
    ctx.set_stroke(0xff75dd34);
    ctx.set_line_width(7);
    ctx.begin_path();
    ctx.move_to(0, 0);
    ctx.line_to(2 * w * ent.get_health_ratio(), 0);
    ctx.stroke();
}
