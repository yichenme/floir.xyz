#include <Client/Render/RenderEntity.hh>

#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Render/Renderer.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Client/StaticData.hh>

#include <algorithm>

void render_health(Renderer &ctx, Entity const &ent) {
    if (ent.has_component(kPetal)) return;
    bool const is_mob = ent.has_component(kMob);
    // Pollen is a Bumble Bee projectile, not a real mob -- no health bar.
    if (is_mob && ent.get_mob_id() == MobID::kPollen) return;
    // A dead player is an immobile corpse, not a combatant -- no health bar.
    if (!is_mob && ent.has_component(kFlower) && ent.get_dead()) return;
    // Players fade their bar in on damage / out at full health; mobs stay
    // always visible (so their strength is readable before engaging), fading
    // only on death like everything else.
    if (!is_mob && ent.healthbar_opacity < 0.01) return;
    float const rad = ent.get_radius();
    ctx.set_global_alpha((1 - ent.deletion_animation) * (is_mob ? 1.0f : ent.healthbar_opacity));
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    float w;   // half the bar length
    if (is_mob) {
        uint8_t const rarity = ent.get_mob_rarity();
        char const *name = MOB_DATA[ent.get_mob_id()].name;
        char const *rname = RARITY_NAMES[rarity];
        // Labels are drawn at full size; the bar starts at 105% of the model
        // width and EXTENDS if a label is longer, so text never overruns it (a
        // label may span well over half the bar).
        float const name_w = Renderer::get_ascii_text_size(name) * 13.0f;
        float const rar_w  = Renderer::get_ascii_text_size(rname) * 11.0f;
        w = std::max({ 1.05f * rad, name_w * 0.5f, rar_w * 0.5f });
        float const bar_y = rad + 20;   // bar sits just below the model
        // Name: white, above the bar, hard left-aligned to the bar's left edge.
        {
            RenderContext c(&ctx);
            ctx.left_text_align();
            ctx.translate(-w, bar_y - 11);
            ctx.draw_text(name, { .fill = 0xffffffff, .size = 13 });
        }
        // Rarity: rarity colour, below the bar, hard right-aligned to the right edge.
        {
            RenderContext c(&ctx);
            ctx.right_text_align();
            ctx.translate(w, bar_y + 11);
            ctx.draw_text(rname, { .fill = RARITY_COLORS[rarity], .size = 11 });
        }
        ctx.center_text_align();   // restore for later draws
        ctx.translate(-w, bar_y);
    } else {
        w = rad * 1.33f;
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
