#include <Client/Ui/Extern.hh>

#include <Client/Ui/Element.hh>

namespace Ui {
    namespace Panel {
        Element *settings = nullptr;
        Element *account = nullptr;
        Element *petal_gallery = nullptr;
        Element *mob_gallery = nullptr;
        Element *changelog = nullptr;
        Element *inventory = nullptr;
    }
    double dt = 0;
    double window_width = 1920;
    double window_height = 1080;
    double scale = 1;
    double lerp_amount = 0.05;
    Element *focused = nullptr;
    Element *pressed = nullptr;
    uint8_t panel_open = Panel::kNone;
    int32_t dragging_inventory_index = -1;
    float drag_start_mouse_x = 0;
    float drag_start_mouse_y = 0;
}