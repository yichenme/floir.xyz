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
        tooltip = Ui::UiLoadout::petal_tooltips[id][rarity == 255 ? PETAL_DATA[id].rarity : rarity];
    } else
        rendering_tooltip = 0;
}


static bool is_retired_petal(PetalID::T i) {
    // Retired / removed petals (kept in the enum only so saved account petal IDs
    // don't renumber). kTringer folded into kStinger.
    return i == PetalID::kHeavyLegacy || i == PetalID::kAzalea ||
           i == PetalID::kBlueIris || i == PetalID::kTriweb ||
           i == PetalID::kTricac || i == PetalID::kPoisonCactus ||
           i == PetalID::kTwin || i == PetalID::kTriplet || i == PetalID::kTringer;
}

static Element *make_scroll() {
    Element *elt = new Ui::VContainer({}, 10, 6, {});
    // One row per petal, columns are the rarities (every petal exists at every
    // rarity) -- no rarity grouping.
    for (PetalID::T i = PetalID::kBasic; i < PetalID::kNumPetals; ++i) {
        if (is_retired_petal(i)) continue;
        Element *row = new Ui::HContainer({}, 0, 6, { .v_justify = Style::Top });
        for (uint8_t r = 0; r < RarityID::kNumRarities; ++r)
            row->add_child(new GalleryPetal(i, 44, r));
        row->refactor();
        elt->add_child(row);
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