#include <Client/Render/RenderEntity.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Assets/Assets.hh>

#include <Client/Particle.hh>

#include <Client/Ui/Ui.hh>
#include <Client/Input.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

#include <cmath>

void render_petal(Renderer &ctx, Entity const &ent) {
    ctx.scale(ent.get_radius() / PETAL_DATA[ent.get_petal_id()].radius);
    if (ent.get_split_projectile()) draw_static_petal(ent.get_petal_id(), ctx);
    else {
        // Apply the petal's icon_angle in-world too, so a petal whose icon is
        // drawn at an offset (e.g. Stinger's inward-pointing tip) looks the same
        // in the game as on its loadout card instead of detached.
        float const ia = PETAL_DATA[ent.get_petal_id()].attributes.icon_angle;
        if (ia != 0) ctx.rotate(ia);
        draw_static_petal_single(ent.get_petal_id(), ctx);
    }
    // Rarity particles (toggle-able): Super = white, Unique = black. Square has
    // none. Uses the petal's actual rarity, not its base.
    uint8_t const rar = ent.get_petal_rarity();
    if (Input::show_particles && ent.get_petal_id() != PetalID::kSquare
        && frand() < fclamp(0.25 * Ui::dt / 16.67, 0, 1)) {
        if (rar == RarityID::kUnique) Particle::add_game_particle(ent.get_x(), ent.get_y(), 0x80000000);
        else if (rar == RarityID::kSuper) Particle::add_game_particle(ent.get_x(), ent.get_y(), 0x80ffffff);
    }
}