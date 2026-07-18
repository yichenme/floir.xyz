#include <Client/Render/RenderEntity.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Assets/Assets.hh>

#include <Client/Game.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Shared/StackFormat.hh>

#include <cmath>

void render_drop(Renderer &ctx, Entity const &ent) {
    float animation_value = sinf(Game::timestamp / 100);
    ctx.set_global_alpha(1 - ent.deletion_animation);
    ctx.rotate(M_PI * 2 * ent.deletion_animation);
    ctx.scale(1 - ent.deletion_animation);
    ctx.scale(1 + animation_value * 0.03);
    ctx.scale(ent.get_radius() / 30);
    ctx.set_fill(0x40000000);
    ctx.begin_path();
    ctx.round_rect(-33, -33, 66, 66, 4);
    ctx.fill();
    draw_loadout_background(ctx, ent.get_drop_id(), 1, 1, ent.get_drop_rarity());
    // Stacked drop (e.g. a Unique mob's 10x Ultra): show a count badge in the
    // top-right, same idea as an inventory stack.
    if (ent.get_drop_count() > 1) {
        ctx.translate(20, -20);
        ctx.draw_text(("x" + format_stack_count(ent.get_drop_count())).c_str(), { .fill = 0xffffffff, .size = 18 });
    }
}