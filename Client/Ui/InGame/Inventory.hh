#pragma once

#include <Client/Ui/Element.hh>

#include <cstdint>

namespace Ui {
    class InventoryStackSlot final : public Element {
    public:
        uint32_t index;
        uint8_t selected;
        float drag_x;
        float drag_y;
        InventoryStackSlot(uint32_t);

        virtual void on_render(Renderer &) override;
        virtual void on_event(uint8_t) override;
    };

    Element *make_inventory_button();
    Element *make_inventory_panel();
}
