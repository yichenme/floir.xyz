#include <Client/Render/RenderEntity.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Game.hh>

#include <Shared/Entity.hh>

void render_name(Renderer &ctx, Entity const &ent) {
    if (!ent.get_nametag_visible()) return;
    if (ent.id == Game::player_id) return;
    ctx.translate(0, -ent.get_radius() - 18);
    ctx.set_global_alpha(1 - ent.deletion_animation);
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    // Tag squadmates -- a flower whose owning camera shares our squad id (only
    // meaningful once that camera is actually synced to us, which the server
    // guarantees for squadmates regardless of distance).
    std::string label = ent.get_name();
    if (Game::squad_id != 0 && Game::simulation.ent_exists(ent.get_parent())) {
        Entity const &owner_cam = Game::simulation.get_ent(ent.get_parent());
        if (owner_cam.has_component(kCamera) && owner_cam.get_squad_id() == Game::squad_id)
            label += " [squad]";
    }
    ctx.draw_text(label.c_str(), { .size = 18 });
    if (!ent.get_account_name().empty()) {
        ctx.translate(0, -20);
        std::string tag = "@" + ent.get_account_name();
        ctx.draw_text(tag.c_str(), { .size = 14 });
    }
}
