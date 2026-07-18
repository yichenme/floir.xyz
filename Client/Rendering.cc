#include <Client/Game.hh>

#include <Client/Input.hh>
#include <Client/Particle.hh>

#include <Client/Ui/Extern.hh>
#include <Client/Render/RenderEntity.hh>

#include <Helpers/Vector.hh>

#include <Shared/Map.hh>
#include <Shared/StaticData.hh>
#include <Shared/Tilemap.hh>
#include <Client/StaticData.hh>

#include <cmath>
#include <cstdio>

void _apply_damage_filter(Renderer &ctx, Entity const &ent) {
    ctx.set_global_alpha(1 - ent.deletion_animation);
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    if (ent.damage_flash > 0.66)
        ctx.add_color_filter(0xffffffff, ent.damage_flash);
    else if (ent.damage_flash > 0.1)
        ctx.add_color_filter(0xffff1200, ent.damage_flash * 1.5);
}

static float screen_shake_radius = 0;

void Game::render_game() {
    RenderContext context(&renderer);
    DEBUG_ONLY(assert(simulation.ent_exists(camera_id));)
    Entity const &camera = simulation.get_ent(camera_id);
    // Clamp the view to the map edges: the camera follows the player until the
    // viewport would show the blank outside the arena, then it stops so the edge
    // sits flush with the screen edge (player drifts off-centre near borders).
    {
        float const view_scale = Ui::scale * camera.get_fov();
        float const half_w = (renderer.width  / 2) / view_scale;
        float const half_h = (renderer.height / 2) / view_scale;
        float const map_w = Tilemap::GRID_W * Tilemap::CELL_SIZE;
        float const map_h = Tilemap::GRID_H * Tilemap::CELL_SIZE;
        float cx = camera.get_camera_x();
        float cy = camera.get_camera_y();
        Game::view_cam_x = (map_w > 2 * half_w) ? fclamp(cx, half_w, map_w - half_w) : map_w / 2;
        Game::view_cam_y = (map_h > 2 * half_h) ? fclamp(cy, half_h, map_h - half_h) : map_h / 2;
    }
    renderer.translate(renderer.width / 2, renderer.height / 2);
    renderer.scale(Ui::scale * camera.get_fov());
    renderer.translate(-Game::view_cam_x, -Game::view_cam_y);
    if (alive()) {
        Entity const &player = simulation.get_ent(player_id);
        if (Game::timestamp - player.last_damaged_time < 150) {
            screen_shake_radius = lerp(screen_shake_radius, 5, Ui::lerp_amount);
        } else {
            screen_shake_radius = lerp(screen_shake_radius, 0, 2 * Ui::lerp_amount);
        }
        if (screen_shake_radius > 0.01) {
            Vector rand = Vector::rand(std::sqrt(frand()) * screen_shake_radius);
            renderer.translate(rand.x, rand.y);
        }
    }
    uint32_t alpha = (uint32_t)(camera.get_fov() * 255 * 0.2) << 24;
    {
        RenderContext context(&renderer);
        renderer.reset_transform();
        renderer.set_fill(0xff987d72);
        renderer.fill_rect(0,0,renderer.width,renderer.height);
        renderer.set_fill(alpha);
        renderer.fill_rect(0,0,renderer.width,renderer.height);
    }
    {
        RenderContext context(&renderer);
        // Vector map: compute the visible cell range, then draw only those tiles
        // (culled, cached Path2D under the camera transform -- crisp at any zoom).
        float scale = 1 / (2 * camera.get_fov() * Ui::scale);
        float leftX   = Game::view_cam_x - renderer.width  * scale;
        float rightX  = Game::view_cam_x + renderer.width  * scale;
        float topY    = Game::view_cam_y - renderer.height * scale;
        float bottomY = Game::view_cam_y + renderer.height * scale;
        int32_t c0 = std::max(0, (int32_t) std::floor(leftX   / Tilemap::CELL_SIZE));
        int32_t c1 = std::min<int32_t>(Tilemap::GRID_W, (int32_t) std::ceil (rightX  / Tilemap::CELL_SIZE));
        int32_t r0 = std::max(0, (int32_t) std::floor(topY    / Tilemap::CELL_SIZE));
        int32_t r1 = std::min<int32_t>(Tilemap::GRID_H, (int32_t) std::ceil (bottomY / Tilemap::CELL_SIZE));
        renderer.draw_map(c0, c1, r0, r1);

        if (Input::show_grid) {
            // Fine grid (normal outline).
            renderer.set_stroke(alpha);
            renderer.set_line_width(0.5);
            float gx = ceilf(leftX / 50) * 50;
            float gy = ceilf(topY  / 50) * 50;
            renderer.begin_path();
            for (; gx < rightX; gx += 50) { renderer.move_to(gx, topY); renderer.line_to(gx, bottomY); }
            for (; gy < bottomY; gy += 50) { renderer.move_to(leftX, gy); renderer.line_to(rightX, gy); }
            renderer.stroke();
            // Coordinate grid: cell boundaries at 2x the normal outline, labelled cc,rr.
            renderer.set_line_width(1.0);
            renderer.begin_path();
            for (int32_t c = c0; c <= c1; ++c) {
                renderer.move_to(c * Tilemap::CELL_SIZE, topY);
                renderer.line_to(c * Tilemap::CELL_SIZE, bottomY);
            }
            for (int32_t r = r0; r <= r1; ++r) {
                renderer.move_to(leftX, r * Tilemap::CELL_SIZE);
                renderer.line_to(rightX, r * Tilemap::CELL_SIZE);
            }
            renderer.stroke();
            for (int32_t r = r0; r < r1; ++r)
            for (int32_t c = c0; c < c1; ++c) {
                char coord[12];
                std::snprintf(coord, sizeof(coord), "%02d,%02d", c, r);
                RenderContext lctx(&renderer);
                renderer.translate((c + 0.5f) * Tilemap::CELL_SIZE, (r + 0.5f) * Tilemap::CELL_SIZE);
                renderer.draw_text(coord, { .fill = 0xffffffff, .stroke = 0xff000000, .size = 60, .stroke_scale = 0.12f });
            }
        }
    }

    if (alive() && Input::movement_helper && !Input::keyboard_movement && !Input::is_mobile) {
        Entity const &player = simulation.get_ent(player_id);
        // Player drifts off screen-centre when the camera is clamped at an edge;
        // anchor the helper to the player's real on-screen position.
        float view_scale = Ui::scale * camera.get_fov();
        float px_off = (player.get_x() - Game::view_cam_x) * view_scale;
        float py_off = (player.get_y() - Game::view_cam_y) * view_scale;
        float norm_mouse_x = (Input::mouse_x - renderer.width / 2 - px_off) / Ui::scale;
        float norm_mouse_y = (Input::mouse_y - renderer.height / 2 - py_off) / Ui::scale;
        Vector delta{norm_mouse_x, norm_mouse_y};
        float dist = delta.magnitude();
        if (dist >= player.get_radius() + 80) {
            RenderContext context(&renderer);
            renderer.reset_transform();
            renderer.translate(renderer.width/2 + px_off, renderer.height/2 + py_off);
            renderer.scale(Ui::scale);
            uint8_t alpha = (uint8_t) (fclamp((dist - player.get_radius() - 80) / 80, 0, 1) * 255 * 0.15);
            delta.set_magnitude(player.get_radius() + 40);
            renderer.set_line_width(18);
            renderer.round_line_cap();
            renderer.round_line_join();
            renderer.set_stroke(alpha << 24);
            renderer.begin_path();
            renderer.move_to(delta.x,delta.y);
            renderer.line_to(norm_mouse_x,norm_mouse_y);
            renderer.translate(norm_mouse_x,norm_mouse_y);
            renderer.rotate(delta.angle());
            renderer.rotate(2.5);
            renderer.move_to(0,0);
            renderer.line_to(40,0);
            renderer.rotate(-5);
            renderer.move_to(0,0);
            renderer.line_to(40,0);
            renderer.stroke();
        }
    }

    Particle::tick_game(renderer, Ui::dt);

    simulation.for_each<kWeb>([](Simulation *sim, Entity const &ent){
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        renderer.rotate(ent.get_angle());
        renderer.scale(ent.animation);
        render_web(renderer, ent);
    });
    simulation.for_each<kDrop>([](Simulation *sim, Entity const &ent){
        // Individual loot: only show drops that are mine (or shared, owner 0).
        if (ent.get_drop_owner() != 0 && ent.get_drop_owner() != Game::camera_id.id) return;
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        renderer.rotate(ent.get_angle() + (ent.animation - 1) * 3 * M_PI);
        renderer.scale(ent.animation);
        render_drop(renderer, ent);
    });
    simulation.for_each<kHealth>([](Simulation *sim, Entity const &ent){
        // Mob HP bars are drawn later, on top of the mob model (see below).
        if (ent.has_component(kMob)) return;
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        render_health(renderer, ent);
    });
    simulation.for_each<kPetal>([](Simulation *sim, Entity const &ent){
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        renderer.rotate(ent.get_angle());
        _apply_damage_filter(renderer, ent);
        render_petal(renderer, ent);
    });
    simulation.for_each<kMob>([](Simulation *sim, Entity const &ent){
        if (ent.get_mob_id() != MobID::kAntHole) return;
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        if (!ent.has_component(kFlower))
            renderer.rotate(ent.get_angle());
        _apply_damage_filter(renderer, ent);
        render_mob(renderer, ent);
    });
    simulation.for_each<kMob>([](Simulation *sim, Entity const &ent){
        if (ent.get_mob_id() == MobID::kAntHole) return;
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        if (!ent.has_component(kFlower))
            renderer.rotate(ent.get_angle());
        _apply_damage_filter(renderer, ent);
        render_mob(renderer, ent);
    });
    // Mob HP bars last (after the models) so they layer above the mob, not under.
    simulation.for_each<kMob>([](Simulation *sim, Entity const &ent){
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        render_health(renderer, ent);
    });
    simulation.for_each<kFlower>([](Simulation *sim, Entity const &ent){
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        _apply_damage_filter(renderer, ent);
        render_flower(renderer, ent);
    });
    simulation.for_each<kName>([](Simulation *sim, Entity const &ent){
        RenderContext context(&renderer);
        renderer.translate(ent.get_x(), ent.get_y());
        render_name(renderer, ent);
    });

    // G: overlay every petal's hitbox as a filled circle in its rarity colour at
    // 25% opacity, drawn larger than the whole petal so it clears the outline.
    // The rarity label is kept but rendered at 0% opacity (present, invisible).
    if (Game::show_hitboxes) {
        simulation.for_each<kPetal>([](Simulation *sim, Entity const &ent){
            uint8_t rarity = ent.get_petal_rarity();   // the petal's actual rarity, not its base
            uint32_t rcol = RARITY_COLORS[rarity];
            RenderContext context(&renderer);
            renderer.translate(ent.get_x(), ent.get_y());
            float r = ent.get_radius() * 1.4f;             // bigger than the petal art
            renderer.set_fill((rcol & 0x00ffffff) | 0x40000000);   // 25% opacity
            renderer.begin_path();
            renderer.arc(0, 0, r);
            renderer.fill();
            renderer.translate(0, r + 10);
            renderer.draw_text(RARITY_NAMES[rarity],
                { .fill = 0x00000000, .stroke = 0x00000000, .size = 13, .stroke_scale = 0.2f });
        });
    }
}

void Game::render_title_screen() {
    RenderContext context(&renderer);
    renderer.reset_transform();
    // Endlessly scrolling grass field: tile the grass image and drift the offset
    // diagonally, wrapping every tile so the loop is seamless.
    float const tile = std::max(160.0f, renderer.height / 4.0f);
    float off = std::fmod((float)(Game::timestamp * 0.02), tile);
    if (off < 0) off += tile;
    renderer.draw_grass_bg(off, off, tile, renderer.width, renderer.height);
}