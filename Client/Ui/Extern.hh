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
    // The bottom-row "Account" button, so code that opens the account panel
    // without going through the button (e.g. spawning without an account) can
    // anchor the panel to it the same way the button does.
    extern Element *account_button;
    extern uint8_t panel_open;
    extern uint8_t minimap_expanded;
    // Smoothed 0..1 minimap open amount, published by the Minimap each frame so
    // the debug stats can slide left/right to stay clear of it.
    extern float minimap_expand;
    extern int32_t dragging_inventory_index;
    extern float drag_start_mouse_x;
    extern float drag_start_mouse_y;
    extern float inventory_icon_x;
    extern float inventory_icon_y;
    extern float inventory_drag_origin_x;   // screen pos of the slot a drag began from
    extern float inventory_drag_origin_y;
};