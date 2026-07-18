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
    static double drag_start_ts = 0;     // when this drag began
    static float pv_scale = 1.0f;        // lerped size, matched to the loadout card
    static float rel_scale = 1.0f;       // fit scale of the slot released onto
    static uint8_t rel_static = 2 * MAX_SLOT_COUNT;  // target static slot on release
    static float rel_hold = 0;           // ms spent covering while awaiting confirm

    bool const dragging = Ui::dragging_inventory_index != -1 && Game::alive() &&
                          (uint32_t)Ui::dragging_inventory_index < Game::inventory_stacks.size();
    // Match the loadout petal card's drag animation exactly: the card is drawn
    // as a 60u square, so a target on-screen width W corresponds to scale W/60.
    // Loadout slots are 70u; a free-dragged loadout card grows to slot+10 (=80u)
    // and snapping fits the slot (70u). Uses the same gentle sin(t/150)*0.1
    // wobble and the same lerp rate (0.75x) as the loadout card.
    float const drag_lerp = Ui::lerp_amount * 0.75f;
    if (dragging) {
        float tx = Input::mouse_x, ty = Input::mouse_y;
        uint8_t swap = find_viable_target(Input::mouse_x, Input::mouse_y);
        bool const snapped = swap != ((uint8_t)-1) && swap < 2 * MAX_SLOT_COUNT;
        float target_scale = (70.0f + 10.0f) / 60.0f;   // free drag: loadout slot + 10
        if (snapped) {
            UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[swap];
            tx = slot->screen_x; ty = slot->screen_y;
            target_scale = slot->width / 60.0f;          // fit the target slot
        }
        if (!was_dragging) { pv_x = tx; pv_y = ty; drag_start_ts = Game::timestamp; pv_scale = 1.0f; }
        pv_x = lerp(pv_x, tx, drag_lerp);
        pv_y = lerp(pv_y, ty, drag_lerp);
        pv_scale = lerp(pv_scale, target_scale, drag_lerp);
        RenderContext c(&ctx);
        ctx.reset_transform();
        ctx.translate(pv_x, pv_y);
        if (!snapped)
            ctx.rotate(sin(Game::timestamp / 150) * 0.1);   // gentle wobble, same as loadout
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
                rel_scale = slot->width / 60.0f;
                rel_static = dynamic_to_static(swap);
                rel_hold = 0;
            } else {
                rel_tx = Ui::inventory_icon_x; rel_ty = Ui::inventory_icon_y; rel_to_slot = 0;
            }
            release_anim = 1;
            was_dragging = 0;
        }
        if (release_anim > 0.01 && rel_type != PetalID::kNone) {
            // Onto a slot: hold the card at full size covering the slot until the
            // server's loadout update actually lands (cached_loadout matches), so
            // the old card never flashes through. Time out after ~1s in case the
            // equip was rejected. A miss just shrinks away to the icon.
            bool confirmed = true;
            if (rel_to_slot) {
                rel_hold += (float) Ui::dt;
                confirmed = (rel_static < 2 * MAX_SLOT_COUNT &&
                             Game::cached_loadout[rel_static] == rel_type) || rel_hold > 1000;
            }
            float const decay = Ui::lerp_amount * (rel_to_slot ? 1.5 : 3);
            pv_x = lerp(pv_x, rel_tx, decay);
            pv_y = lerp(pv_y, rel_ty, decay);
            if (confirmed) release_anim = lerp(release_anim, 0, decay);
            pv_scale = lerp(pv_scale, rel_to_slot ? rel_scale : 0.0f, decay);
            RenderContext c(&ctx);
            ctx.reset_transform();
            ctx.translate(pv_x, pv_y);
            ctx.scale(Ui::scale * (rel_to_slot ? rel_scale : pv_scale));
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