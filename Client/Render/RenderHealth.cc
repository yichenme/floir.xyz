#include <Client/Render/RenderEntity.hh>

#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Render/Renderer.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Client/StaticData.hh>

void render_health(Renderer &ctx, Entity const &ent) {
    if (ent.has_component(kPetal)) return;
    bool const is_mob = ent.has_component(kMob);
    // Players fade their bar in on damage / out at full health; mobs stay
    // always visible (so their strength is readable before engaging), fading
    // only on death like everything else.
    if (!is_mob && ent.healthbar_opacity < 0.01) return;
    float const rad = ent.get_radius();
    // Mob HP bar = 75% of the model's horizontal length (diameter); w is half.
    float const w = rad * (is_mob ? 0.75f : 1.33f);
    ctx.set_global_alpha((1 - ent.deletion_animation) * (is_mob ? 1.0f : ent.healthbar_opacity));
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    if (is_mob) {
        float const bar_y = rad + 20;   // bar sits just below the model
        uint8_t const rarity = ent.get_mob_rarity();
        // Name: white + black outline, above the bar, left-aligned to its left edge.
        {
            RenderContext c(&ctx);
            ctx.left_text_align();
            ctx.translate(-w, bar_y - 11);
            ctx.draw_text(MOB_DATA[ent.get_mob_id()].name, { .fill = 0xffffffff, .size = 13 });
        }
        // Rarity: rarity colour + black outline, below the bar, right-aligned to its right edge.
        {
            RenderContext c(&ctx);
            ctx.right_text_align();
            ctx.translate(w, bar_y + 11);
            ctx.draw_text(RARITY_NAMES[rarity], { .fill = RARITY_COLORS[rarity], .size = 11 });
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
