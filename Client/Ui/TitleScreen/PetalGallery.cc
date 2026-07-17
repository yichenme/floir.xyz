#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/InGame/GameInfo.hh>
#include <Client/Ui/ScrollContainer.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>
#include <Client/StaticData.hh>
#include <Client/Assets/Assets.hh>

#include <algorithm>
#include <cstring>

using namespace Ui;

GalleryPetal::GalleryPetal(PetalID::T id, float w, uint8_t rarity) :
    Element(w,w,{ .fill = 0x40000000, .round_radius = w/20 , .h_justify = Style::Left }), id(id), rarity(rarity) {}

void GalleryPetal::on_render(Renderer &ctx) {
    ctx.scale(width / 60);
    draw_loadout_background(ctx, id, 1, 1, rarity);
}

void GalleryPetal::on_event(uint8_t event) {
    if (event != kFocusLost && id != PetalID::kNone) {
        rendering_tooltip = 1;
        tooltip = Ui::UiLoadout::petal_tooltips[id];
    } else
        rendering_tooltip = 0;
}


static Element *make_scroll() {
    Element *elt = new Ui::VContainer({}, 10, 8, {});
    // Grouped by rarity, highest rarity at the top (matches the reference layout).
    // Every petal is shown at every rarity.
    for (int r = RarityID::kNumRarities - 1; r >= 0; --r) {
        elt->add_child(new Ui::StaticText(15, RARITY_NAMES[r], { .fill = RARITY_COLORS[r] }));
        for (PetalID::T i = PetalID::kBasic; i < PetalID::kNumPetals;) {
            Element *row = new Ui::HContainer({}, 0, 8, { .v_justify = Style::Top });
            for (uint8_t j = 0; j < 5 && i < PetalID::kNumPetals; ++j, ++i) {
                row->add_child(new Ui::VContainer({
                    new GalleryPetal(i, 55, (uint8_t) r),
                    new Ui::StaticText(11, PETAL_DATA[i].name)
                }, 0, 3, { .v_justify = Style::Top }));
            }
            row->refactor();
            elt->add_child(row);
        }
    }
    return new Ui::ScrollContainer(elt, 340);
}

Element *Ui::make_petal_gallery() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Petal Gallery"),
        make_scroll()
    }, 15, 10, { 
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kPetals && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::petal_gallery = elt;
    return elt;
}