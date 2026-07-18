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
    static uint8_t rel_rarity = 255;
    static uint8_t was_dragging = 0;
    static uint8_t rel_to_slot = 0;      // released onto a loadout slot?
    static double drag_start_ts = 0;     // when this drag began (for the shake)
    static float pv_scale = 1.0f;        // lerped size (grow to 1.25, fit to 1.0)

    bool const dragging = Ui::dragging_inventory_index != -1 && Game::alive() &&
                          (uint32_t)Ui::dragging_inventory_index < Game::inventory_stacks.size();
    if (dragging) {
        float tx = Input::mouse_x, ty = Input::mouse_y;
        uint8_t swap = find_viable_target(Input::mouse_x, Input::mouse_y);
        bool const snapped = swap != ((uint8_t)-1) && swap < 2 * MAX_SLOT_COUNT;
        if (snapped) {
            UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[swap];
            tx = slot->screen_x; ty = slot->screen_y;
        }
        if (!was_dragging) { pv_x = tx; pv_y = ty; drag_start_ts = Game::timestamp; pv_scale = 1.0f; }
        pv_x = lerp(pv_x, tx, Ui::lerp_amount * 2.5);
        pv_y = lerp(pv_y, ty, Ui::lerp_amount * 2.5);
        // Smoothly lerp the size like the loadout petal card: grow toward 125%
        // while free-dragging, shrink to fit (1.0) when stuck to a slot.
        pv_scale = lerp(pv_scale, snapped ? 1.0f : 1.25f, Ui::lerp_amount * 2.5);
        RenderContext c(&ctx);
        ctx.reset_transform();
        ctx.translate(pv_x, pv_y);
        if (!snapped) {
            // Free drag: shake fast (a small right tilt then rocking ~10 degrees).
            float const t = (float)(Game::timestamp - drag_start_ts);
            ctx.rotate(sinf(t * 0.045f) * (10.0f * (float)M_PI / 180.0f));
        }
        ctx.scale(Ui::scale * pv_scale);
        draw_loadout_background(ctx, Game::inventory_stacks[Ui::dragging_inventory_index].type, 1, 1, Game::inventory_stacks[Ui::dragging_inventory_index].rarity);
        rel_type = Game::inventory_stacks[Ui::dragging_inventory_index].type;
        rel_rarity = Game::inventory_stacks[Ui::dragging_inventory_index].rarity;
        was_dragging = 1;
    } else {
        if (was_dragging) {
            // Just released: target the snapped slot if within stick distance,
            // else fly back to the inventory icon.
            uint8_t swap = find_viable_target(Input::mouse_x, Input::mouse_y);
            if (swap != ((uint8_t)-1) && swap < 2 * MAX_SLOT_COUNT) {
                UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[swap];
                rel_tx = slot->screen_x; rel_ty = slot->screen_y; rel_to_slot = 1;
            } else {
                rel_tx = Ui::inventory_icon_x; rel_ty = Ui::inventory_icon_y; rel_to_slot = 0;
            }
            release_anim = 1;
            was_dragging = 0;
        }
        if (release_anim > 0.01 && rel_type != PetalID::kNone) {
            // Onto a slot: linger a little longer at full size so the petal
            // keeps covering the slot until the server's loadout update lands
            // (both are the same petal, so overlap is seamless). A miss shrinks
            // away back to the inventory icon as before.
            float const decay = Ui::lerp_amount * (rel_to_slot ? 1.5 : 3);
            pv_x = lerp(pv_x, rel_tx, decay);
            pv_y = lerp(pv_y, rel_ty, decay);
            release_anim = lerp(release_anim, 0, decay);
            // Onto a slot: settle to fit size (1.0), covering it. Miss: shrink away.
            pv_scale = lerp(pv_scale, rel_to_slot ? 1.0f : 0.0f, decay);
            RenderContext c(&ctx);
            ctx.reset_transform();
            ctx.translate(pv_x, pv_y);
            ctx.scale(Ui::scale * (rel_to_slot ? 1.0f : pv_scale));
            draw_loadout_background(ctx, rel_type, 1, 1, rel_rarity);
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