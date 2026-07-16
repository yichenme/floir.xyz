#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Game.hh>

#include <Shared/Map.hh>
#include <Shared/Tilemap.hh>

using namespace Ui;

Minimap::Minimap(float w) : Element(w, w*ARENA_HEIGHT/ARENA_WIDTH, {}) {}

// Black = walls / water / thick foliage (impassable-ish);
// white = anything the flower can walk across freely.
static bool is_wall_terrain(uint8_t t) {
    switch (t) {
        case Tilemap::TerrainID::kWater:
        case Tilemap::TerrainID::kJungle:
        case Tilemap::TerrainID::kCliff:
        case Tilemap::TerrainID::kJungleWall:
        case Tilemap::TerrainID::kCliffWall:
            return true;
        default:
            return false;
    }
}

void Minimap::on_render(Renderer &ctx) {
    ctx.set_line_width(7);
    ctx.set_stroke(0xff444444);
    ctx.stroke_rect(-width/2,-height/2,width,height);
    // White backdrop = standable ground; walls painted on top in black.
    ctx.set_fill(0xffffffff);
    ctx.fill_rect(-width/2,-height/2,width,height);
    ctx.translate(-width/2,-height/2);
    ctx.scale(width/ARENA_WIDTH);
    ctx.set_fill(0xff000000);
    for (uint32_t r = 0; r < Tilemap::GRID_H; ++r) {
        uint32_t start = 0;
        bool in_wall = is_wall_terrain(Tilemap::TERRAIN[r * Tilemap::GRID_W]);
        for (uint32_t c = 1; c <= Tilemap::GRID_W; ++c) {
            bool w = c < Tilemap::GRID_W && is_wall_terrain(Tilemap::TERRAIN[r * Tilemap::GRID_W + c]);
            if (w != in_wall) {
                if (in_wall)
                    ctx.fill_rect(start * Tilemap::CELL_SIZE, r * Tilemap::CELL_SIZE,
                                  (c - start) * Tilemap::CELL_SIZE, Tilemap::CELL_SIZE);
                in_wall = w;
                start = c;
            }
        }
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
    // Bottom-right, 10px inset from both edges to match the Settings button.
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(20, "Minimap"),
        new Ui::Minimap(112)
    }, 10, 10, {
        .should_render = [](){ return Game::should_render_game_ui(); },
        .h_justify = Style::Right,
        .v_justify = Style::Bottom
    });
    return elt;
}