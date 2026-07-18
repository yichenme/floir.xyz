#include <Client/Ui/InGame/Loadout.hh>

#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Shared/Entity.hh>

using namespace Ui;

UiLoadoutSlot *Ui::UiLoadout::petal_backgrounds[2 * MAX_SLOT_COUNT + 1] = {nullptr};

UiLoadoutSlot::UiLoadoutSlot(uint8_t pos) : Element(70, 70, { .fill = 0xffeeeeee, .round_radius = 0 }) {
    if (pos >= MAX_SLOT_COUNT) width = height = 50;
    style.line_width = width / 12;
    style.round_radius = width / 20;
    style.should_render = [=](){
        // The Store slot (last background) always shows so petals can be stored.
        if (pos == 2 * MAX_SLOT_COUNT) return true;
        // Main slots [0, loadout_count); secondary row [MAX_SLOT_COUNT, +count)
        // -- the number of secondary slots equals the number of main slots.
        if (pos < Game::loadout_count) return true;
        if (pos >= MAX_SLOT_COUNT && pos < MAX_SLOT_COUNT + Game::loadout_count) return true;
        return false;
    };
    position = pos;
    Ui::UiLoadout::petal_backgrounds[pos] = this;
}

void UiLoadoutSlot::on_render(Renderer &ctx) {
    Element::on_render(ctx);
    if (Game::slot_indicator_opacity < 0.01) return;
    if (position < Game::loadout_count) {
        ctx.set_global_alpha(Game::slot_indicator_opacity);
        ctx.translate(0, -height / 2 - 15);
        char str[4] = {'[', SLOT_KEYBINDS[position], ']', '\0'};
        ctx.draw_text(&str[0], { .size = 16 });
    } else if (position == 2 * MAX_SLOT_COUNT) {
        ctx.set_global_alpha(Game::slot_indicator_opacity);
        ctx.translate(width / 2 + 16, 0);
        ctx.draw_text("[T]", { .size = 16 });
        ctx.translate(-width / 2 - 16, 0);
    }
}

UiStoreSlot::UiStoreSlot() : UiLoadoutSlot(2 * MAX_SLOT_COUNT) {
    style.fill = 0xff5a9fdb;
    store_text_opacity.set(0);
    style.animate = [&](Element *, Renderer &) {
        store_text_opacity.set(Ui::UiLoadout::num_petals_selected != 0 || Ui::UiLoadout::selected_with_keys != MAX_SLOT_COUNT);
        store_text_opacity.step(Ui::lerp_amount);
    };
}

void UiStoreSlot::on_render(Renderer &ctx) {
    UiLoadoutSlot::on_render(ctx);
    if ((float) store_text_opacity > 0.01) {
        ctx.set_global_alpha((float) store_text_opacity);
        ctx.draw_text("Store", { .size = height / 4});
    }
}

Element *Ui::make_loadout_backgrounds() {
    Element *base = new Ui::VContainer(
        {
            (new Ui::HContainer(
                Ui::make_range(0, MAX_SLOT_COUNT, [](uint32_t i){ return (Element *) (new Ui::UiLoadoutSlot(i)); })
            , 5, 20, { .layer = 1 }
            )),
            (new Ui::HContainer(
                Ui::make_range(MAX_SLOT_COUNT, 2*MAX_SLOT_COUNT+1, 
                    [](uint32_t i){ 
                        if (i == 2*MAX_SLOT_COUNT) return (Element *) new Ui::UiStoreSlot();
                        else return (Element *) (new Ui::UiLoadoutSlot(i)); 
                    })
                , 10, 15, { .layer = 1 }
            ))
        }, 0, 10, { .should_render = [](){ return Game::alive(); } }
    );
    base->style.v_justify = Style::Bottom;
    base->style.animate = [](Element *elt, Renderer &ctx) {
        ctx.translate(0, (1 - elt->animation) * elt->height * 2);
    };
    return base;
}