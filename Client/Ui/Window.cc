#include <Client/Ui/Window.hh>

#include <Client/Ui/Extern.hh>

#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/Assets/Assets.hh>
#include <Client/Ui/InGame/Loadout.hh>

#include <cmath>

using namespace Ui;

Window::Window() : Container({}) {}

void Window::render(Renderer &ctx) {
    width = Ui::window_width;
    height = Ui::window_height;
    refactor();
    RenderContext context(&ctx);
    ctx.reset_transform();
    ctx.translate(width / 2, height / 2);
    Element::render(ctx);
}

void Window::on_render(Renderer &ctx) {
    RenderContext context(&ctx);
    for (uint32_t layer = 0; layer < 2; ++layer) {
        for (uint32_t i = 0; i < children.size(); ++i) {
            Element *elt = children[i];
            if (elt->style.layer != layer) continue;
            RenderContext context(&ctx);
            ctx.translate(elt->style.h_justify * width / 2, elt->style.v_justify * height / 2);
            ctx.scale(Ui::scale);
            ctx.translate(-elt->style.h_justify * elt->width / 2, -elt->style.v_justify * elt->height / 2);
            ctx.translate(elt->x, elt->y);
            elt->render(ctx);
        }
    }
    // Floating single-petal drag preview: smoothly follows the cursor, snaps
    // toward a target loadout slot, and on release travels to the slot (if
    // placed) or flies back to the inventory icon before vanishing.
    static float pv_x = 0, pv_y = 0;
    static float release_anim = 0;   // >0 while playing the release travel
    static float rel_tx = 0, rel_ty = 0;
    static PetalID::T rel_type = PetalID::kNone;
    static uint8_t was_dragging = 0;

    bool const dragging = Ui::dragging_inventory_index != -1 && Game::alive() &&
                          (uint32_t)Ui::dragging_inventory_index < Game::inventory_stacks.size();
    if (dragging) {
        float tx = Input::mouse_x, ty = Input::mouse_y;
        uint8_t swap = find_viable_target(Input::mouse_x, Input::mouse_y);
        if (swap != ((uint8_t)-1) && swap < 2 * MAX_SLOT_COUNT) {
            UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[swap];
            tx = slot->screen_x; ty = slot->screen_y;
        }
        if (!was_dragging) { pv_x = tx; pv_y = ty; }
        pv_x = lerp(pv_x, tx, Ui::lerp_amount * 2.5);
        pv_y = lerp(pv_y, ty, Ui::lerp_amount * 2.5);
        RenderContext c(&ctx);
        ctx.reset_transform();
        ctx.translate(pv_x, pv_y);
        ctx.scale(Ui::scale);
        draw_loadout_background(ctx, Game::inventory_stacks[Ui::dragging_inventory_index].type);
        rel_type = Game::inventory_stacks[Ui::dragging_inventory_index].type;
        was_dragging = 1;
    } else {
        if (was_dragging) {
            // Just released: target the snapped slot if within stick distance,
            // else fly back to the inventory icon.
            uint8_t swap = find_viable_target(Input::mouse_x, Input::mouse_y);
            if (swap != ((uint8_t)-1) && swap < 2 * MAX_SLOT_COUNT) {
                UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[swap];
                rel_tx = slot->screen_x; rel_ty = slot->screen_y;
            } else {
                rel_tx = Ui::inventory_icon_x; rel_ty = Ui::inventory_icon_y;
            }
            release_anim = 1;
            was_dragging = 0;
        }
        if (release_anim > 0.01 && rel_type != PetalID::kNone) {
            pv_x = lerp(pv_x, rel_tx, Ui::lerp_amount * 3);
            pv_y = lerp(pv_y, rel_ty, Ui::lerp_amount * 3);
            release_anim = lerp(release_anim, 0, Ui::lerp_amount * 3);
            RenderContext c(&ctx);
            ctx.reset_transform();
            ctx.translate(pv_x, pv_y);
            ctx.scale(Ui::scale * release_anim);
            draw_loadout_background(ctx, rel_type);
        }
    }
    on_render_tooltip(ctx);
}

void Window::poll_events(ScreenEvent const &event) {
    if (style.no_polling) return;
    Element::poll_events(event);
    for (Element *elt : children)
        if (elt->visible) elt->poll_events(event);
}

void Window::on_event(uint8_t event) {
    if (event == Ui::kMouseDown)
        Ui::panel_open = Ui::Panel::kNone;
}