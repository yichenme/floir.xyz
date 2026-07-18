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
            uint8_t potential_swap = find_viable_target(Input::mouse_x, Input::mouse_y);
            if (potential_swap != ((uint8_t)-1) && potential_swap < 2 * MAX_SLOT_COUNT) {
                Game::equip_petal(Game::inventory_display_order[index],dynamic_to_static(potential_swap));
                equip_pending = 1; equip_version = Game::inventory_version;
            } else {
                float dist = hypotf(Input::mouse_x - Ui::drag_start_mouse_x,
                                    Input::mouse_y - Ui::drag_start_mouse_y);
                uint8_t const t = find_equip_target();
                if (dist < 10 * Ui::scale && t != 255) {
                    Game::equip_petal(Game::inventory_display_order[index], t);
                    equip_pending = 1; equip_version = Game::inventory_version;
                }
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
        Ui::drag_start_mouse_x = Input::mouse_x;
        Ui::drag_start_mouse_y = Input::mouse_y;
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
                pg->x = elt->screen_x / Ui::scale - pg->width / 2;
                pg->y = -(elt->height + 20);
                if (pg->x < 10)
                    pg->x = 10;
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
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kInventory && Game::alive();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::inventory = elt;
    return elt;
}
