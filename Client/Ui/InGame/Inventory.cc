#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/ScrollContainer.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/StaticData.hh>

#include <Shared/StackFormat.hh>

using namespace Ui;

// Client-only render cap for the scroll grid. Inventory storage itself is
// unbounded (see Server/EntityFunctions/InventoryOps); stacks beyond this
// simply don't get a slot, same trade-off Leaderboard makes for its pool.
static uint32_t const INVENTORY_DISPLAY_CAP = 200;
static uint32_t const INVENTORY_COLUMNS = 5;
static float const INVENTORY_SLOT_SIZE = 60;

// Clicking a stack has no explicit target slot, so mirror drag-to-store:
// fill the first empty loadout slot, or bump slot 0 if the loadout is full.
static uint8_t find_equip_target() {
    for (uint8_t i = 0; i < Game::loadout_count + MAX_SLOT_COUNT; ++i)
        if (Game::cached_loadout[i] == PetalID::kNone) return i;
    return 0;
}

InventoryStackSlot::InventoryStackSlot(uint32_t idx) :
    Element(INVENTORY_SLOT_SIZE, INVENTORY_SLOT_SIZE, { .h_justify = Style::Left }),
    index(idx)
{
    style.should_render = [this](){ return index < Game::inventory_stacks.size(); };
}

void InventoryStackSlot::on_render(Renderer &ctx) {
    if (index >= Game::inventory_stacks.size()) return;
    PetalStack const &stack = Game::inventory_stacks[index];
    draw_loadout_background(ctx, stack.type);
    std::string const count_text = format_stack_count(stack.count);
    if (!count_text.empty()) {
        ctx.translate(width / 2 - 12, -height / 2 + 10);
        ctx.draw_text(count_text.c_str(), { .fill = 0xffffffff, .size = 13 });
    }
}

void InventoryStackSlot::on_event(uint8_t event) {
    if (index >= Game::inventory_stacks.size()) {
        rendering_tooltip = 0;
        return;
    }
    if (event == kClick)
        Game::equip_petal(index, find_equip_target());
    if (event != kFocusLost) {
        rendering_tooltip = 1;
        tooltip = Ui::UiLoadout::petal_tooltips[Game::inventory_stacks[index].type];
    } else
        rendering_tooltip = 0;
}

static Element *make_inventory_grid() {
    Element *grid = new Ui::VContainer({}, 10, 10, {});
    for (uint32_t i = 0; i < INVENTORY_DISPLAY_CAP;) {
        Element *row = new Ui::HContainer({}, 0, 10, { .v_justify = Style::Top });
        for (uint32_t j = 0; j < INVENTORY_COLUMNS && i < INVENTORY_DISPLAY_CAP; ++j, ++i)
            row->add_child(new InventoryStackSlot(i));
        row->refactor();
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
    elt->x = 20;
    elt->y = -20;
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
