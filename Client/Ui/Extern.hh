#pragma once

#include <cstdint>

namespace Ui {
    class Element;
    namespace Panel {
        enum {
            kNone,
            kSettings,
            kAccount,
            kPetals,
            kMobs,
            kChangelog,
            kInventory
        };
        extern Element *settings;
        extern Element *account;
        extern Element *petal_gallery;
        extern Element *mob_gallery;
        extern Element *changelog;
        extern Element *inventory;
    }
    extern double dt;
    extern double window_width;
    extern double window_height;
    extern double scale;
    extern double lerp_amount;
    extern Element *focused;
    extern Element *pressed;
    extern uint8_t panel_open;
    extern uint8_t minimap_expanded;
    extern int32_t dragging_inventory_index;
    extern float drag_start_mouse_x;
    extern float drag_start_mouse_y;
};