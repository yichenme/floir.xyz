#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>
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
    Ui::minimap_expand = e;   // let the debug stats slide clear of the minimap

    // Widget grows toward EXPAND_SCALE as it opens. The Window anchors this
    // element bottom-right, so growth naturally extends up-left (corner stays).
    float const size = base_size * lerp(1.0f, EXPAND_SCALE, e);
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

    // Paint blocked collision black by sampling the collision mask, so the
    // minimap matches exactly what blocks movement (including hand edits).
    float const step = Tilemap::CELL_SIZE / 5;   // 100 world units
    int32_t sc0 = std::max(0, (int32_t)std::floor(view_l / step));
    int32_t sc1 = (int32_t)std::ceil((view_l + view_w) / step);
    int32_t sr0 = std::max(0, (int32_t)std::floor(view_t / step));
    int32_t sr1 = (int32_t)std::ceil((view_t + view_h) / step);
    float const ov = step * 0.5f;
    ctx.set_fill(0xff000000);
    for (int32_t r = sr0; r < sr1; ++r) {
        int32_t start = -1;
        for (int32_t c = sc0; c <= sc1; ++c) {
            bool w = c < sc1 && Tilemap::solid_at(c * step + step / 2, r * step + step / 2);
            if (w && start < 0) start = c;
            else if (!w && start >= 0) {
                ctx.fill_rect(start * step, r * step - ov,
                              (c - start) * step + ov, step + 2 * ov);
                start = -1;
            }
        }
    }

    // Player marker: zoomed-in dot 50% of the old size, zoomed-out 10% of it,
    // black outlined. Sized in screen px then converted to world scale.
    if (Game::simulation.ent_exists(Game::camera_id)) {
        float const dot_screen = lerp(size * 0.035f, size * 0.007f, e);
        float const dot = dot_screen * view_w / size;
        ctx.set_fill(0xffffe763);
        ctx.set_stroke(0xff000000);
        ctx.set_line_width(dot * 0.18f);
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
