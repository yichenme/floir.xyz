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
    if (Ui::dragging_inventory_index != -1 && Game::alive() && (uint32_t)Ui::dragging_inventory_index < Game::inventory_stacks.size()) {
        RenderContext c(&ctx);
        ctx.reset_transform();
        float draw_x = Input::mouse_x;
        float draw_y = Input::mouse_y;
        uint8_t potential_swap = find_viable_target(Input::mouse_x, Input::mouse_y);
        if (potential_swap != ((uint8_t)-1) && potential_swap < 2 * MAX_SLOT_COUNT) {
            UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[potential_swap];
            draw_x = slot->screen_x;
            draw_y = slot->screen_y;
        }
        ctx.translate(draw_x, draw_y);
        ctx.scale(Ui::scale);
        PetalID::T type = Game::inventory_stacks[Ui::dragging_inventory_index].type;
        draw_loadout_background(ctx, type);
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