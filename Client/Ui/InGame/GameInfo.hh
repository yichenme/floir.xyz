#pragma once

#include <Client/Ui/Element.hh>

namespace Ui {
    class LevelBar final : public Element {
    public:
        LerpFloat progress;
        LerpFloat hp;
        uint32_t level;
        LevelBar();
        virtual void on_render(Renderer &) override;
    };

    class LeaderboardSlot final : public Element {
    public:
        uint8_t pos;
        LerpFloat ratio;
        LeaderboardSlot(uint8_t);

        virtual void on_render(Renderer &) override;
    };

    class Minimap final : public Element {
    public:
        float base_size;
        LerpFloat expand;   // 0 = zoomed follow-cam, 1 = full map
        uint8_t hovering;
        Minimap(float);
        virtual void on_render(Renderer &) override;
        virtual void on_event(uint8_t) override;
    };


    class MobileJoyStick final : public Element {
        float joystick_x;
        float joystick_y;
        float joystick_radius;
        uint32_t persistent_touch_id;
        uint8_t is_pressed;
    public:
        MobileJoyStick(float, float, float);
        virtual void on_render(Renderer &) override;
        virtual void on_event(uint8_t) override;
    };

    Element *make_leaderboard();
    Element *make_level_bar();
    Element *make_minimap();
    Element *make_mobile_attack_button();
    Element *make_mobile_defend_button();
    Element *make_mobile_joystick();
    Element *make_mobile_hitbox_button();
}