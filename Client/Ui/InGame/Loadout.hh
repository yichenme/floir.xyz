#pragma once

#include <Client/Ui/Choose.hh>
#include <Client/Ui/Element.hh>

#include <Client/StaticData.hh>

namespace Ui {
    class UiLoadoutSlot : public Element {
    public:
        uint8_t position;
        UiLoadoutSlot(uint8_t);

        virtual void on_render(Renderer &) override;
    };

    class UiStoreSlot final : public UiLoadoutSlot {
    public:
        UiStoreSlot();
        LerpFloat store_text_opacity;
        virtual void on_render(Renderer &) override;
    };

    class UiLoadoutPetal final : public Element {
    public:
        LerpFloat reload;
        LerpFloat health;
        uint32_t persistent_touch_id;
        uint8_t static_pos;
        uint8_t curr_pos;
        uint8_t no_change_ticks;
        uint8_t selected;
        // Pointer position when this petal was pressed, and whether it has since
        // moved far, to tell a short click (swap main/secondary) from a drag.
        // Tracked across frames because the release frame's pointer position is
        // unreliable on mobile (it snaps back to the slot).
        float down_x = 0, down_y = 0;
        uint8_t moved_far = 0;
        PetalID::T petal_id;
        PetalID::T last_id;
        // Rarity is frozen alongside petal_id/last_id (same no_change_ticks /
        // get_state_loadout_ids gate) instead of being re-read live from the
        // entity every frame. Reading it live let a rapid second swap -- fired
        // before the first swap's server round-trip landed -- pair the FROZEN
        // (optimistic) type with a STALE live rarity from a different position,
        // showing a type/rarity combination that never actually existed.
        uint8_t rarity;
        uint8_t last_rarity;
        UiLoadoutPetal(uint8_t);

        virtual void on_render(Renderer &) override;
        virtual void on_render_skip(Renderer &) override;
        virtual void on_event(uint8_t) override;
    };

    namespace UiLoadout {
        extern UiLoadoutPetal *petal_slots[2 * MAX_SLOT_COUNT];
        extern UiLoadoutSlot *petal_backgrounds[2 * MAX_SLOT_COUNT + 1];
        extern Element *petal_tooltips[PetalID::kNumPetals][RarityID::kNumRarities];
        extern uint8_t selected_with_keys;
        extern double last_key_select;
        extern uint32_t num_petals_selected;
    };

    Element *make_loadout_backgrounds();
    void make_petal_tooltips();
    uint8_t find_viable_target(float, float);
    uint8_t find_equip_target();   // first blank loadout slot (static pos), 255 if full
    uint8_t dynamic_to_static(uint8_t);
    uint8_t static_to_dynamic(uint8_t);

    void ui_store_petal(uint8_t);
    void ui_swap_petals(uint8_t, uint8_t);
}