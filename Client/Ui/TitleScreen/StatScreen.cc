#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/InGame/Loadout.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>

#include <format>
#include <string>

using namespace Ui;

TitlePetalSlot::TitlePetalSlot(uint8_t p) : Element(50,50,{}), pos(p) {
    style.v_justify = Style::Bottom;
    style.should_render = [=](){
        if (!Game::simulation.ent_exists(Game::camera_id))
            return false;
        Entity const &camera = Game::simulation.get_ent(Game::camera_id);
        if (p >= loadout_slots_at_level(camera.get_respawn_level()) + MAX_SLOT_COUNT) return false;
        return camera.get_inventory(p) > PetalID::kBasic;
    };
}

void TitlePetalSlot::on_render(Renderer &ctx) {
    if (!Game::simulation.ent_exists(Game::camera_id))
        return;
    Entity const &camera = Game::simulation.get_ent(Game::camera_id);
    uint8_t id = camera.get_inventory(pos);
    if (id == PetalID::kNone) return;
    ctx.scale(width / 60);
    draw_loadout_background(ctx, id, 1, 1, camera.get_inventory_rarity(pos));
}

void TitlePetalSlot::on_event(uint8_t event) {
    if (!Game::simulation.ent_exists(Game::camera_id))
        return;
    Entity const &camera = Game::simulation.get_ent(Game::camera_id);
    uint8_t id = camera.get_inventory(pos);
    if (event != kFocusLost && id != PetalID::kNone) {
        rendering_tooltip = 1;
        tooltip = Ui::UiLoadout::petal_tooltips[id][camera.get_inventory_rarity(pos)];
    } else {
        rendering_tooltip = 0;
    }
}

Element *Ui::make_stat_screen() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Stats", { .fill = 0xffffffff, .h_justify = Style::Left }),
        new Ui::DynamicText(16, [](){
            return std::format("Level: {}", score_to_level(Game::score));
        }, { .fill = 0xffffffff, .h_justify = Style::Left }),
        new Ui::DynamicText(16, [](){
            return std::format("Mobs killed: {}", format_score(Game::mobs_killed));
        }, { .fill = 0xffffffff, .h_justify = Style::Left }),
        new Ui::DynamicText(16, [](){
            return std::format("Petals collected: {}", format_score(Game::petals_collected));
        }, { .fill = 0xffffffff, .h_justify = Style::Left })
    }, 20, 7);
    elt->style.animate = [](Element *elt, Renderer &ctx) {
        ctx.translate(0, (1 - elt->animation) * elt->height);
    };
    elt->style.should_render = [](){
        return !Game::alive() && Game::should_render_game_ui();
    };
    elt->animation.set(0);
    elt->style.h_justify = Style::Left;
    elt->style.v_justify = Style::Bottom;
    // Chat is now usable while dead (a corpse can chat before respawning), so
    // this panel and the chat box (Chat.cc's make_chat_box, ~200px tall +
    // its own -10 offset -> top edge sits 210px above the bottom) can now be
    // visible at the same time. Both are bottom-left anchored, so stack this
    // one above the chat box instead of letting them overlap.
    elt->y = -220;
    return elt;
}