#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Game.hh>

#include <Shared/Map.hh>
#include <Shared/Tilemap.hh>

using namespace Ui;

Minimap::Minimap(float w) : Element(w, w*ARENA_HEIGHT/ARENA_WIDTH, {}) {}

void Minimap::on_render(Renderer &ctx) {
    ctx.set_line_width(7);
    ctx.set_stroke(0xff444444);
    ctx.stroke_rect(-width/2,-height/2,width,height);
    ctx.translate(-width/2,-height/2);
    ctx.scale(width/ARENA_WIDTH);
    // Tilemap fill – same run-length merge as the world renderer, no culling
    // needed because the minimap always shows the whole arena.
    for (uint32_t r = 0; r < Tilemap::GRID_H; ++r) {
        uint32_t start = 0;
        uint8_t cur = Tilemap::TERRAIN[r * Tilemap::GRID_W];
        for (uint32_t c = 1; c <= Tilemap::GRID_W; ++c) {
            uint8_t t = c < Tilemap::GRID_W ? Tilemap::TERRAIN[r * Tilemap::GRID_W + c] : 255;
            if (t != cur) {
                if (cur != Tilemap::TerrainID::kVoid) {
                    ctx.set_fill(Tilemap::COLORS[cur]);
                    ctx.fill_rect(start * Tilemap::CELL_SIZE, r * Tilemap::CELL_SIZE,
                                  (c - start) * Tilemap::CELL_SIZE, Tilemap::CELL_SIZE);
                }
                cur = t;
                start = c;
            }
        }
    }
    // Biome name labels
    for (ZoneDefinition const &def : MAP_DATA) {
        ctx.translate((def.left+def.right)/2,(def.top+def.bottom)/2);
        ctx.draw_text(def.name, { .size = 800 });
        ctx.translate(-(def.left+def.right)/2,-(def.top+def.bottom)/2);
    }
    if (!Game::simulation.ent_exists(Game::camera_id)) return;
    Entity const &camera = Game::simulation.get_ent(Game::camera_id);
    ctx.set_fill(0xffffe763);
    ctx.set_stroke(Renderer::HSV(0xffffe763, 0.8));
    ctx.set_line_width(ARENA_WIDTH / 120);
    ctx.begin_path();
    ctx.arc(camera.get_camera_x(), camera.get_camera_y(), ARENA_WIDTH / 40);
    ctx.fill();
    ctx.stroke();
}

Element *Ui::make_minimap() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(20, "Minimap"),
        new Ui::Minimap(300)
    }, 20, 10, { 
        .should_render = [](){ return Game::should_render_game_ui(); },
        .h_justify = Style::Right,
        .v_justify = Style::Bottom
    });
    elt->y = -50;
    return elt;
}