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
    float w = ent.get_radius() * 1.33;
    ctx.set_global_alpha((1 - ent.deletion_animation) * (is_mob ? 1.0f : ent.healthbar_opacity));
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    if (is_mob) {
        // Name + rarity line sits just above the bar, still below the model.
        ctx.translate(0, w + 15 - 16);
        ctx.draw_text(MOB_DATA[ent.get_mob_id()].name,
                       { .fill = RARITY_COLORS[ent.get_mob_rarity()], .size = 14 });
        ctx.translate(-w, 16);
    } else {
        ctx.translate(-w, w + 15);
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
