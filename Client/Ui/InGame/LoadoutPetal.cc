#include <Client/Ui/InGame/Loadout.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>

#include <cmath>

using namespace Ui;

UiLoadoutPetal *Ui::UiLoadout::petal_slots[2 * MAX_SLOT_COUNT] = {nullptr};
uint8_t Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
double Ui::UiLoadout::last_key_select = 0;
uint32_t Ui::UiLoadout::num_petals_selected = 0;

uint8_t Ui::static_to_dynamic(uint8_t static_pos) {
    if (static_pos >= Game::loadout_count) 
        return std::min(MAX_SLOT_COUNT + static_pos - Game::loadout_count, 2 * MAX_SLOT_COUNT);
    else
        return static_pos;
}

void Ui::ui_store_petal(uint8_t static_pos) {
    Game::store_petal(static_pos);
    Ui::UiLoadout::petal_slots[static_pos]->curr_pos = 2 * MAX_SLOT_COUNT;
}

void Ui::clear_loadout_display() {
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        UiLoadoutPetal *s = Ui::UiLoadout::petal_slots[i];
        if (s != nullptr) {
            s->petal_id = s->last_id = PetalID::kNone;
            s->rarity = s->last_rarity = 0;
            s->no_change_ticks = 0;
        }
        Game::cached_loadout[i] = PetalID::kNone;
    }
}

void Ui::ui_swap_petals(uint8_t static_pos1, uint8_t static_pos2) {
    //found static pos, now swap
    UiLoadoutPetal *a1 = Ui::UiLoadout::petal_slots[static_pos1];
    UiLoadoutPetal *a2 = Ui::UiLoadout::petal_slots[static_pos2];
    // Skip only a truly identical swap (same petal AND same rarity). Compare the
    // slots' own frozen display state, not a live entity read that may have
    // reverted while a previous swap's round-trip is still pending.
    if (a1->petal_id == a2->petal_id && a1->rarity == a2->rarity) return;
    // Drive the swap from the OPTIMISTIC displayed values (petal_id/rarity), not
    // Game::cached_loadout -- cached is re-synced from the not-yet-updated server
    // state every frame, so a rapid second switch used to read reverted values
    // and no-op, which felt like a per-switch cooldown.
    PetalID::T const p1 = a1->petal_id, p2 = a2->petal_id;
    uint8_t const rr1 = a1->rarity, rr2 = a2->rarity;
    auto apply = [](UiLoadoutPetal *s, PetalID::T id, uint8_t rar){
        s->petal_id = id; s->rarity = rar;
        // Update the frozen render fields too so the switch shows THIS frame and
        // a follow-up switch reads the just-applied state.
        s->last_id = id; s->last_rarity = rar;
    };
    apply(a2, p1, rr1);
    apply(a1, p2, rr2);
    a1->static_pos = static_pos2;
    a2->static_pos = static_pos1;
    Ui::UiLoadout::petal_slots[static_pos1] = a2;
    Ui::UiLoadout::petal_slots[static_pos2] = a1;
    // Short freeze (was 100 frames): long enough to hold the optimistic swap
    // until the server confirms, short enough not to mask a rapid re-swap.
    a1->no_change_ticks = a2->no_change_ticks = 20;
    Game::swap_petals(static_pos1, static_pos2);
}

uint8_t Ui::dynamic_to_static(uint8_t dynamic_pos) {
    if (dynamic_pos >= MAX_SLOT_COUNT)
        dynamic_pos -= (MAX_SLOT_COUNT - Game::loadout_count);
    return dynamic_pos; 
}


uint8_t Ui::find_viable_target(float x, float y) {
    float min_dist = 10000;
    uint8_t min_index = -1;
    for (uint8_t i = 0; i < 2 * MAX_SLOT_COUNT + 1; ++i) {
        UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[i];
        float dist = std::max(fabsf(x - slot->screen_x), fabsf(y - slot->screen_y));
        if (dist >= (slot->width + 20) * Ui::scale / 2) continue;
        if (dist >= min_dist) continue;
        min_dist = dist;
        min_index = i;
        if (dist <= slot->width * Ui::scale / 2) break;
    }
    return min_index;
}

UiLoadoutPetal::UiLoadoutPetal(uint8_t pos) : Element(60, 60),
    static_pos(pos), no_change_ticks(0), petal_id(PetalID::kNone), last_id(PetalID::kNone),
    rarity(0), last_rarity(0)
{
    reload = 1;
    health = 1;
    style.should_render = [&](){
        if (!Game::alive()) return false;
        //coincidentally also works for trashing
        if (static_pos >= 2 * Game::loadout_count) return false;
        if (curr_pos == 2 * MAX_SLOT_COUNT) return false;
        if (Game::alive()) {
            Entity const &player = Game::simulation.get_ent(Game::player_id);
            if (no_change_ticks == 0 || player.get_state_loadout_ids(static_pos)) {
                no_change_ticks = 0;
                petal_id = Game::cached_loadout[static_pos];
                rarity = player.get_loadout_rarities(static_pos);
                if (petal_id != PetalID::kNone && petal_id < PetalID::kNumPetals) {
                    last_id = petal_id;
                    last_rarity = rarity;
                }
            } else --no_change_ticks;
        }
        if (static_pos < Game::loadout_count && Game::alive()) {
            Entity const &player = Game::simulation.get_ent(Game::player_id);
            if (player.get_state_loadout_reloads(static_pos)) {
                float old = player.get_loadout_reloads(static_pos) / 255.0f;
                if (old > reload) reload.set(old);
                else reload = old;
            }
            if (player.get_state_loadout_healths(static_pos)) {
                // Opposite of reload's snap direction: taking damage animates
                // smoothly down, but healing/respawning fresh snaps instantly
                // to full (no drawn-out "regen" look for an instant refill).
                float new_health = player.get_loadout_healths(static_pos) / 255.0f;
                if (new_health < health) health.set(new_health);
                else health = new_health;
            }
        }
        else {
            reload = 0;
            health = 1;
        }
        if ((no_change_ticks == 0 && petal_id == PetalID::kNone) || last_id == PetalID::kNone) return false;
        return true;
    };
    style.animate = [&](Element *elt, Renderer &ctx) {
        float lerp_amt = Ui::lerp_amount * 0.75;
        reload.step(lerp_amt);
        // Fixed ~0.1s characteristic time regardless of framerate, distinct
        // from the slower general-purpose lerp_amt above.
        health.step(1 - powf(0.1f, Ui::dt / 100.0f));
        if (curr_pos != 2 * MAX_SLOT_COUNT) 
            curr_pos = static_to_dynamic(static_pos);
        
        float mouse_x, mouse_y;
        uint8_t released;
        if (Input::is_mobile) {
            released = 0;
            auto iter = Input::touches.find(persistent_touch_id);
            if (iter == Input::touches.end()) {
                if (persistent_touch_id != (uint32_t)-1)
                    released = 1;
                persistent_touch_id = (uint32_t)-1;
                mouse_x = elt->screen_x;
                mouse_y = elt->screen_y;
            } else {
                mouse_x = iter->second.x;
                mouse_y = iter->second.y;
            }
        }
        else {
            mouse_x = Input::mouse_x;
            mouse_y = Input::mouse_y;
            released = BitMath::at(Input::mouse_buttons_released, Input::LeftMouse);
        }
        UiLoadoutSlot *parent_slot = Ui::UiLoadout::petal_backgrounds[curr_pos];
        if (selected && Game::alive()) {
            style.layer = 1;
            // Latch "dragged" the moment the pointer leaves the press point, so a
            // click vs drag can be told at release even when the release-frame
            // position is unreliable (mobile snaps it back to the slot).
            if (!released && std::max(fabsf(mouse_x - down_x), fabsf(mouse_y - down_y)) > 12 * Ui::scale)
                moved_far = 1;
            uint8_t potential_swap = find_viable_target(mouse_x, mouse_y);
            if (potential_swap != ((uint8_t)-1) && potential_swap != static_to_dynamic(static_pos)) {
                if (released) {
                    if (potential_swap == 2 * MAX_SLOT_COUNT)
                        ui_store_petal(static_pos);
                    else {
                        //find static pos of both
                        potential_swap = dynamic_to_static(potential_swap);
                        ui_swap_petals(potential_swap, static_pos);
                    }
                } else {
                    UiLoadoutSlot *slot = Ui::UiLoadout::petal_backgrounds[potential_swap];
                    x = lerp(x, (slot->screen_x - Ui::window_width / 2) / Ui::scale, lerp_amt);
                    y = lerp(y, (slot->screen_y - Ui::window_height / 2) / Ui::scale, lerp_amt);
                    width = lerp(width, slot->width, lerp_amt);
                    height = lerp(height, slot->height, lerp_amt);
                }
            } else {
                x = lerp(x, (mouse_x - Ui::window_width / 2) / Ui::scale, lerp_amt);
                y = lerp(y, (mouse_y - Ui::window_height / 2) / Ui::scale, lerp_amt);
                width = lerp(width, parent_slot->width + 10, lerp_amt);
                height = lerp(height, parent_slot->height + 10, lerp_amt);
                ctx.rotate(sin(Game::timestamp / 150) * 0.1);
            }
            //if (released)
                //Ui::UiLoadout::petal_selected = nullptr;
        } else if (Ui::UiLoadout::selected_with_keys + Game::loadout_count == static_pos && Game::alive()) {
            if (!showed) lerp_amt = 1;
            x = lerp(x, (parent_slot->screen_x - Ui::window_width / 2) / Ui::scale, lerp_amt);
            y = lerp(y, (parent_slot->screen_y - Ui::window_height / 2) / Ui::scale, lerp_amt);
            width = lerp(width, parent_slot->width + 20, lerp_amt);
            height = lerp(height, parent_slot->height + 20, lerp_amt);
            ctx.rotate(sin(Game::timestamp / 150) * 0.1);
        } else {
            style.layer = 0;
            if (!showed) lerp_amt = 1;
            x = lerp(x, (parent_slot->screen_x - Ui::window_width / 2) / Ui::scale, lerp_amt);
            y = lerp(y, (parent_slot->screen_y - Ui::window_height / 2) / Ui::scale, lerp_amt);
            width = lerp(width, parent_slot->width, lerp_amt);
            height = lerp(height, parent_slot->height, lerp_amt);
        }
        //if (!Game::alive())
            //Ui::UiLoadout::petal_selected = nullptr;
        ctx.scale((float) animation);
        if (released && selected) {
            // A short click (never dragged) toggles this slot's main<->secondary
            // pair -- a drag onto another slot was already handled above.
            if (!moved_far) {
                uint8_t const counterpart = (static_pos < Game::loadout_count)
                    ? (uint8_t)(static_pos + Game::loadout_count)
                    : (uint8_t)(static_pos - Game::loadout_count);
                if (counterpart != static_pos && counterpart < 2 * Game::loadout_count)
                    ui_swap_petals(static_pos, counterpart);
            }
            --Ui::UiLoadout::num_petals_selected;
            selected = 0;
        }
    };
    Ui::UiLoadout::petal_slots[static_pos] = this;
}

void UiLoadoutPetal::on_render(Renderer &ctx) {
    if (last_id == PetalID::kNone) return;
    ctx.scale(width / 60);
    // last_rarity is frozen alongside last_id (see the should_render refresh in
    // the constructor) instead of read live, so a rapid second swap fired
    // before the first's server confirmation can't pair this slot's frozen
    // (optimistic) petal type with a stale live rarity from a different swap.
    if (static_pos < Game::loadout_count && PETAL_DATA[last_id].count != 0)
        draw_loadout_background(ctx, last_id, (float) reload, (float) health, last_rarity);
    else
        draw_loadout_background(ctx, last_id, 1, 1, last_rarity);
}

void UiLoadoutPetal::on_render_skip(Renderer &ctx) {
    x = y = 0;
    if (Game::alive()) showed = 1;
    else showed = 0;
    curr_pos = 0;
    petal_id = last_id = PetalID::kNone;
    //if (Ui::UiLoadout::petal_selected == this) 
        //Ui::UiLoadout::petal_selected = nullptr;
}

void UiLoadoutPetal::on_event(uint8_t event) {
    if (Game::alive() && event == kMouseDown) {
        //Ui::UiLoadout::petal_selected = this;
        if (!selected)
            ++Ui::UiLoadout::num_petals_selected;
        selected = 1;
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        if (Input::touches.contains(touch_id))
            persistent_touch_id = touch_id;
        // Remember where the press started so release can tell a click from a drag.
        down_x = Input::mouse_x;
        down_y = Input::mouse_y;
        moved_far = 0;
        if (Input::is_mobile) {
            auto it = Input::touches.find(touch_id);
            if (it != Input::touches.end()) { down_x = it->second.x; down_y = it->second.y; }
        }
    }
    if (event != kFocusLost && (!selected || Input::is_mobile) && last_id != PetalID::kNone) {
        rendering_tooltip = 1;
        // last_rarity, not a live re-read -- matches whatever this slot is
        // actually showing right now (see on_render).
        tooltip = Ui::UiLoadout::petal_tooltips[last_id][last_rarity];
    } else
        rendering_tooltip = 0;
}