#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>
#include <Client/Input.hh>

#include <Helpers/Bits.hh>

#include <Shared/StaticData.hh>

using namespace Ui;

MobileJoyStick::MobileJoyStick(float w, float h, float r) : Element(w,h,{}), joystick_radius(r) {
    // Not while a panel (inventory / craft) is open: the pad must not swallow the
    // touch that's dragging a petal, and movement is frozen there anyway.
    style.should_render = [](){ return Input::is_mobile && Game::alive() && Ui::panel_open == Ui::Panel::kNone; };
    style.no_animation = 1;
}

void MobileJoyStick::on_render(Renderer &ctx) {
    auto iter = Input::touches.find(persistent_touch_id);
    if (iter != Input::touches.end()) {
        Input::Touch const &touch = iter->second;
        if (!is_pressed) {
            joystick_x = touch.x;
            joystick_y = touch.y;
        }
        is_pressed = 1;
        Input::game_inputs.x = touch.x - joystick_x;
        Input::game_inputs.y = touch.y - joystick_y;
    } else {
        is_pressed = 0;
        persistent_touch_id = (uint32_t)-1;
        Input::game_inputs.x = 0;
        Input::game_inputs.y = 0;
    }
    if (is_pressed) {
        RenderContext c(&ctx);
        ctx.reset_transform();
        ctx.translate(joystick_x, joystick_y);
        ctx.scale(Ui::scale);
        ctx.set_fill(0x40000000);
        ctx.begin_path();
        ctx.arc(0, 0, joystick_radius);
        ctx.fill();
    }
}

void MobileJoyStick::on_event(uint8_t event) {
    if (event == UiEvent::kMouseDown)
        persistent_touch_id = touch_id;
}

Element *Ui::make_mobile_attack_button() {
    Element *elt = new Ui::Button(200, 200, new Ui::StaticText(50, "A"), 
        [](Element *elt, uint8_t e) {
            if (e == Ui::kMouseDown)
                BitMath::set(Input::game_inputs.flags, InputFlags::kAttacking);
            else if (e == Ui::kClick)
                BitMath::unset(Input::game_inputs.flags, InputFlags::kAttacking);
        }, nullptr, { .fill = 0x40000000, .line_width = 0, .round_radius = 50,
            .should_render = [](){ return Input::is_mobile && Game::alive(); },
            .h_justify = Style::Right, .v_justify = Style::Bottom,
            .no_animation = 1,
            // Bottom-right, left of the minimap; shifts up while the map is
            // expanded so it doesn't sit under it, then drops back.
            .animate = [](Element *e, Renderer &ctx){
                e->y = Ui::minimap_expanded ? -290.0f : -130.0f;
                ctx.scale((float) e->animation);
            }
        }
    );
    elt->x = -240;
    elt->y = -130;
    return elt;
}

Element *Ui::make_mobile_defend_button() {
    Element *elt = new Ui::Button(150, 150, new Ui::StaticText(37.5, "B"), 
        [](Element *elt, uint8_t e) {
            if (e == Ui::kMouseDown)
                BitMath::set(Input::game_inputs.flags, InputFlags::kDefending);
            else if (e == Ui::kClick)
                BitMath::unset(Input::game_inputs.flags, InputFlags::kDefending);
        }, nullptr, { .fill = 0x40000000, .line_width = 0, .round_radius = 50,
            .should_render = [](){ return Input::is_mobile && Game::alive(); },
            .h_justify = Style::Right, .v_justify = Style::Bottom,
            .no_animation = 1,
            .animate = [](Element *e, Renderer &ctx){
                e->y = Ui::minimap_expanded ? -470.0f : -310.0f;
                ctx.scale((float) e->animation);
            }
        }
    );
    elt->x = -240;
    elt->y = -310;
    return elt;
}

Element *Ui::make_mobile_joystick() {
    Element *elt = new Ui::MobileJoyStick(800, 800, 150);
    elt->style.h_justify = Style::Left;
    return elt;
}

// Mobile equivalent of holding G on desktop: a toggle that shows/hides the
// petal-hitbox overlay (Game::show_hitboxes). Sits above the A/B buttons.
Element *Ui::make_mobile_hitbox_button() {
    Element *elt = new Ui::Button(120, 120, new Ui::StaticText(34, "G"),
        [](Element *elt, uint8_t e) {
            if (e == Ui::kClick)
                Game::show_hitboxes = !Game::show_hitboxes;
        }, [](){ return Game::show_hitboxes != 0; },
        { .fill = 0x40000000, .line_width = 0, .round_radius = 30,
            .should_render = [](){ return Input::is_mobile && Game::alive(); },
            .h_justify = Style::Right, .v_justify = Style::Bottom,
            .no_animation = 1,
            .animate = [](Element *e, Renderer &ctx){
                e->y = Ui::minimap_expanded ? -620.0f : -460.0f;
                ctx.scale((float) e->animation);
            }
        }
    );
    elt->x = -240;
    elt->y = -460;
    return elt;
}