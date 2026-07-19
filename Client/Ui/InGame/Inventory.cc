#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/ScrollContainer.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/StaticData.hh>
#include <Client/Input.hh>
#include <Client/Ui/InGame/Loadout.hh>

#include <Shared/StackFormat.hh>
#include <Helpers/Bits.hh>
#include <cmath>

using namespace Ui;

// Client-only render cap for the scroll grid. Inventory storage itself is
// unbounded (see Server/EntityFunctions/InventoryOps); stacks beyond this
// simply don't get a slot, same trade-off Leaderboard makes for its pool.
static uint32_t const INVENTORY_DISPLAY_CAP = 200;
static uint32_t const INVENTORY_COLUMNS = 5;
static float const INVENTORY_SLOT_SIZE = 60;

// Clicking a stack has no explicit target slot, so mirror drag-to-store:
// fill the first empty loadout slot, or bump slot 0 if the loadout is full.
uint8_t Ui::find_equip_target() {
    for (uint8_t i = 0; i < 2 * Game::loadout_count; ++i)
        if (Game::cached_loadout[i] == PetalID::kNone) return i;
    return 255;   // loadout full -> no target (click does nothing)
}

InventoryStackSlot::InventoryStackSlot(uint32_t idx) :
    Element(INVENTORY_SLOT_SIZE, INVENTORY_SLOT_SIZE, { .h_justify = Style::Left }),
    index(idx), selected(0), drag_x(0), drag_y(0)
{
    style.should_render = [this](){ return index < Game::inventory_display_order.size(); };
    // The slot stays put in the grid; the single dragged petal is drawn by the
    // Window as a floating preview. Release is polled here (no kMouseUp event).
    style.animate = [this](Element *elt, Renderer &ctx){
        ctx.scale((float) elt->animation);
        if (!selected) return;
        if (BitMath::at(Input::mouse_buttons_released, Input::LeftMouse)) {
            // Rapid-click guard: once we've fired an equip from this slot, ignore
            // further equips until the server-confirmed inventory update lands
            // (inventory_version bumps). Without this, a second click before the
            // round-trip re-targeted the SAME empty slot and double-decremented
            // the stack -- the petal appeared to "mutate" in the slot.
            if (equip_locked && Game::inventory_version == equip_lock_version) {
                selected = 0;
                Ui::dragging_inventory_index = -1;
                return;
            }
            // Equip one from this stack. The rapid-click lock latches on ANY
            // equip; equip_pending (last-copy blank) stays separate so a
            // multi-stack slot still shows count-1 rather than going blank.
            auto equip_one = [this](uint32_t real, uint8_t static_target){
                Game::equip_petal(real, static_target);
                if (Game::inventory_stacks[real].count > 1)
                    --Game::inventory_stacks[real].count;
                else { equip_pending = 1; equip_version = Game::inventory_version; }
                equip_locked = 1;
                equip_lock_version = Game::inventory_version;
            };
            uint32_t const real = Game::inventory_display_order[index];
            uint8_t potential_swap = find_viable_target(Input::mouse_x, Input::mouse_y);
            if (potential_swap != ((uint8_t)-1) && potential_swap < 2 * MAX_SLOT_COUNT) {
                equip_one(real, dynamic_to_static(potential_swap));
            } else {
                float dist = hypotf(Input::mouse_x - Ui::drag_start_mouse_x,
                                    Input::mouse_y - Ui::drag_start_mouse_y);
                uint8_t const t = find_equip_target();
                if (dist < 10 * Ui::scale && t != 255)
                    equip_one(real, t);
            }
            selected = 0;
            Ui::dragging_inventory_index = -1;
        }
    };
}

void InventoryStackSlot::on_render(Renderer &ctx) {
    // Just equipped from this slot: keep it blank until the server-confirmed
    // inventory update lands (version bumps), so the card doesn't flash back.
    if (equip_pending) {
        if (Game::inventory_version != equip_version) equip_pending = 0;
        else return;
    }
    if (index >= Game::inventory_display_order.size()) return;
    PetalStack const &stack = Game::inventory_stacks[Game::inventory_display_order[index]];
    // Dragging the last (or only) one out: leave the slot empty so a single
    // petal visibly leaves the inventory instead of lingering as a ghost.
    if (selected && stack.count <= 1) return;
    draw_loadout_background(ctx, stack.type, 1, 1, stack.rarity);
    // While dragging one out, show the remaining count (999 -> 998).
    uint32_t shown = stack.count;
    if (selected && shown > 0) --shown;
    std::string const count_text = format_stack_count(shown);
    if (!count_text.empty()) {
        ctx.translate(width / 2 - 12, -height / 2 + 10);
        ctx.draw_text(count_text.c_str(), { .fill = 0xffffffff, .size = 13 });
    }
}

void InventoryStackSlot::on_event(uint8_t event) {
    if (index >= Game::inventory_display_order.size()) {
        rendering_tooltip = 0;
        return;
    }
    uint32_t const real = Game::inventory_display_order[index];
    if (event == kMouseDown && !selected) {
        selected = 1;
        Ui::dragging_inventory_index = real;
        // On mobile the drag is driven by a touch, not the mouse: latch the
        // owning touch id (the tick loop bridges its position into mouse_x/y)
        // and seed the drag-start point from that touch directly, since the
        // mouse fields aren't populated yet on this first frame.
        float sx = Input::mouse_x, sy = Input::mouse_y;
        if (Input::is_mobile) {
            Ui::drag_touch_id = touch_id;
            auto it = Input::touches.find(touch_id);
            if (it != Input::touches.end()) { sx = it->second.x; sy = it->second.y; }
        }
        Ui::drag_start_mouse_x = sx;
        Ui::drag_start_mouse_y = sy;
        // Remember this slot's screen position so an un-placed release can fly
        // the petal back to it.
        Ui::inventory_drag_origin_x = screen_x;
        Ui::inventory_drag_origin_y = screen_y;
    }
    if (event != kFocusLost && !selected) {
        rendering_tooltip = 1;
        tooltip = Ui::UiLoadout::petal_tooltips[Game::inventory_stacks[real].type][Game::inventory_stacks[real].rarity];
    } else
        rendering_tooltip = 0;
}

static Element *make_inventory_grid() {
    Element *grid = new Ui::VContainer({}, 10, 10, {});
    for (uint32_t i = 0; i < INVENTORY_DISPLAY_CAP;) {
        uint32_t const start = i;   // index of this row's first slot
        Element *row = new Ui::HContainer({}, 0, 10, { .v_justify = Style::Top });
        for (uint32_t j = 0; j < INVENTORY_COLUMNS && i < INVENTORY_DISPLAY_CAP; ++j, ++i)
            row->add_child(new InventoryStackSlot(i));
        row->refactor();
        row->width = INVENTORY_COLUMNS * INVENTORY_SLOT_SIZE + (INVENTORY_COLUMNS - 1) * 10;
        // Hide (and drop from layout) rows with no petals, so the panel height
        // tracks the number of filled rows instead of leaving blank space.
        row->style.should_render = [start](){ return start < Game::inventory_display_order.size(); };
        grid->add_child(row);
    }
    return new Ui::ScrollContainer(grid, 300);
}

Element *Ui::make_inventory_button() {
    Element *elt = new Ui::Button(140, 40,
        new Ui::StaticText(18, "Inventory"),
        [](Element *elt, uint8_t e){ if (e == Ui::kClick) {
            if (Ui::panel_open != Panel::kInventory) {
                Ui::panel_open = Panel::kInventory;
                Element *pg = Ui::Panel::inventory;
                // To the right of the button (not stacked above it), bottom-
                // aligned with it.
                pg->x = elt->screen_x / Ui::scale + elt->width / 2 + 10;
                pg->y = elt->y;
                if (pg->x < 10)
                    pg->x = 10;
                if (pg->x + pg->width > Ui::window_width / Ui::scale - 10)
                    pg->x = Ui::window_width / Ui::scale - 10 - pg->width;
            }
            else Ui::panel_open = Panel::kNone;
        } },
        [](){ return Ui::panel_open == Panel::kInventory; },
        {
            .fill = 0xff5a9fdb,
            .line_width = 5,
            .round_radius = 3,
            .should_render = [](){ return Game::alive(); },
            .h_justify = Style::Left,
            .v_justify = Style::Bottom
        }
    );
    // Match the Settings button's 10px inset from the bottom-left corner.
    elt->x = 10;
    elt->y = -10;
    // Publish the icon's screen position so the drag preview can fly back to it.
    elt->style.animate = [](Element *e, Renderer &ctx){
        Ui::inventory_icon_x = e->screen_x;
        Ui::inventory_icon_y = e->screen_y;
        ctx.scale((float) e->animation);
    };
    return elt;
}

Element *Ui::make_inventory_panel() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(22, "Inventory"),
        make_inventory_grid()
    }, 15, 10, {
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        // Hard show/hide (no slide-out lerp): craft & inventory share the same
        // corner, so an animated close would briefly overlap the other panel and
        // read as "both open at once".
        .should_render = [](){
            return Ui::panel_open == Panel::kInventory && Game::alive();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom,
        .no_animation = 1
    });
    Ui::Panel::inventory = elt;
    return elt;
}
