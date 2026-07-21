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
        uint32_t last_count = 0;
        float pop_t = 2;   // >1 = no pop playing
        uint8_t equip_pending = 0;   // just equipped a LAST copy from here; stay blank until sync
        uint32_t equip_version = 0;
        // Rapid-click lock: set on ANY equip from this slot (stack size aside);
        // blocks a second equip until the inventory update lands, without
        // blanking a multi-stack slot the way equip_pending does.
        uint8_t equip_locked = 0;
        uint32_t equip_lock_version = 0;
        InventoryStackSlot(uint32_t);

        virtual void on_render(Renderer &) override;
        virtual void on_event(uint8_t) override;
    };

    Element *make_inventory_button();
    Element *make_inventory_panel();

    // Craft: hover-triggered panel above the inventory button (5-petal ->
    // 1-above-rarity upgrade). See Craft.cc for the full mechanic.
    Element *make_craft_button();
    Element *make_craft_panel();

    // Talents: button sits directly above Craft. See Talent.cc.
    Element *make_talent_button();
    Element *make_talent_panel();

    // Chat box (input + recent-message log) shown beside the inventory icon.
    Element *make_chat_box();
    // Called each tick from Game: if the chat input is focused and Enter was
    // pressed, send the message and clear the box. Returns true if it consumed
    // the Enter (so the caller shouldn't also treat it as a spawn key).
    bool chat_try_send();
    // Send whatever is currently in the chat box (used by the mobile Send
    // button, since mobile keyboards don't reliably fire an Enter key).
    void chat_send_current();
    // Give the in-game chat input browser focus (Enter-to-open when unfocused).
    void chat_focus();
    // True if a DOM text <input> currently has browser focus (i.e. the player
    // is typing) -- used to gate Enter-to-focus so it doesn't re-fire.
    bool is_typing_dom();
}
