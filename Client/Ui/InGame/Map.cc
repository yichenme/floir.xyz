#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>

#include <Shared/Map.hh>
#include <Shared/Tilemap.hh>

#include <algorithm>
#include <cmath>

using namespace Ui;

// Default view shows 1/ZOOM of the arena centred on the player (500% zoom);
// expanded view shows the whole arena and the widget grows to EXPAND_SCALE.
static float const MINIMAP_ZOOM = 5.0f;
static float const EXPAND_SCALE = 2.0f;

Minimap::Minimap(float w) : Element(w, w, {}), base_size(w), hovering(0) {
    expand.set(0);
}

void Minimap::on_event(uint8_t event) {
    // Click toggles the persistent expanded state; hover expands transiently
    // and reverts on release.
    if (event == kMouseDown)
        Ui::minimap_expanded = !Ui::minimap_expanded;
    if (event == kFocusLost)
        hovering = 0;
    else if (event == kMouseHover || event == kMouseDown)
        hovering = 1;
}

void Minimap::on_render(Renderer &ctx) {
    uint8_t const want_full = Ui::minimap_expanded || hovering;
    expand.set(want_full);
    expand.step(Ui::lerp_amount * 1.5);
    float const e = expand;

    // Widget grows toward EXPAND_SCALE as it opens. Keep the bottom-right
    // corner pinned by shifting the extra size up-left.
    float const size = base_size * lerp(1.0f, EXPAND_SCALE, e);
    float const grow = (size - base_size) / 2;
    ctx.translate(-grow, -grow);
    width = height = size;

    // World view rect: interpolate between a player-centred zoom window and the
    // whole arena.
    float const arena_cx = ARENA_WIDTH * 0.5f;
    float const arena_cy = ARENA_HEIGHT * 0.5f;
    float px = arena_cx, py = arena_cy;
    if (Game::simulation.ent_exists(Game::camera_id)) {
        Entity const &camera = Game::simulation.get_ent(Game::camera_id);
        px = camera.get_camera_x();
        py = camera.get_camera_y();
    }
    float const view_w = lerp(ARENA_WIDTH / MINIMAP_ZOOM, (float)ARENA_WIDTH, e);
    float const view_h = lerp(ARENA_HEIGHT / MINIMAP_ZOOM, (float)ARENA_HEIGHT, e);
    float cx = lerp(px, arena_cx, e);
    float cy = lerp(py, arena_cy, e);
    // Keep the view inside the map edges so the blank outside never shows.
    if (view_w < ARENA_WIDTH)  cx = fclamp(cx, view_w / 2, ARENA_WIDTH - view_w / 2);
    if (view_h < ARENA_HEIGHT) cy = fclamp(cy, view_h / 2, ARENA_HEIGHT - view_h / 2);
    float const view_l = cx - view_w / 2;
    float const view_t = cy - view_h / 2;

    RenderContext clip(&ctx);
    ctx.begin_path();
    ctx.round_rect(-size / 2, -size / 2, size, size, 4);
    ctx.clip();

    // White backdrop = walkable ground; blocked terrain painted black.
    ctx.set_fill(0xffffffff);
    ctx.fill_rect(-size / 2, -size / 2, size, size);
    // Map world -> widget: origin at top-left of the view rect.
    ctx.translate(-size / 2, -size / 2);
    ctx.scale(size / view_w, size / view_h);
    ctx.translate(-view_l, -view_t);

    int32_t c0 = std::max(0, (int32_t)std::floor(view_l / Tilemap::CELL_SIZE));
    int32_t c1 = std::min<int32_t>(Tilemap::GRID_W, (int32_t)std::ceil((view_l + view_w) / Tilemap::CELL_SIZE));
    int32_t r0 = std::max(0, (int32_t)std::floor(view_t / Tilemap::CELL_SIZE));
    int32_t r1 = std::min<int32_t>(Tilemap::GRID_H, (int32_t)std::ceil((view_t + view_h) / Tilemap::CELL_SIZE));
    // Overlap each run by half a cell so no white seam shows between the black
    // blocked rects.
    float const ov = Tilemap::CELL_SIZE * 0.5f;
    ctx.set_fill(0xff000000);
    for (int32_t r = r0; r < r1; ++r) {
        int32_t start = c0;
        bool in_wall = Tilemap::blocks_movement(Tilemap::TERRAIN[r * Tilemap::GRID_W + c0]);
        for (int32_t c = c0 + 1; c <= c1; ++c) {
            bool w = c < c1 && Tilemap::blocks_movement(Tilemap::TERRAIN[r * Tilemap::GRID_W + c]);
            if (w != in_wall) {
                if (in_wall)
                    ctx.fill_rect(start * Tilemap::CELL_SIZE, r * Tilemap::CELL_SIZE - ov,
                                  (c - start) * Tilemap::CELL_SIZE + ov, Tilemap::CELL_SIZE + 2 * ov);
                in_wall = w;
                start = c;
            }
        }
    }

    // Player marker: bigger when zoomed in, smaller when zoomed out, black
    // outlined. Sized in screen px then converted to the current world scale.
    if (Game::simulation.ent_exists(Game::camera_id)) {
        float const dot_screen = lerp(size * 0.07f, size * 0.03f, e);
        float const dot = dot_screen * view_w / size;
        ctx.set_fill(0xffffe763);
        ctx.set_stroke(0xff000000);
        ctx.set_line_width(dot * 0.35f);
        ctx.begin_path();
        ctx.arc(px, py, dot);
        ctx.fill();
        ctx.stroke();
    }
}

Element *Ui::make_minimap() {
    // Bottom-right, 10px inset from both edges. No panel, no border.
    Minimap *elt = new Ui::Minimap(112);
    elt->style.should_render = [](){ return Game::should_render_game_ui(); };
    elt->style.h_justify = Style::Right;
    elt->style.v_justify = Style::Bottom;
    elt->x = -10;
    elt->y = -10;
    return elt;
}
