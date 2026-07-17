#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>

using namespace Ui;

static Element *make_divider() {
    return new Element(400, 6, { .fill = 0x20000000, .line_width = 3, .round_radius = 3, .v_justify = Style::Top });
}

static Element *make_date(std::string const date) {
    return new Ui::StaticText(20, date, { .fill = 0xffffffff, .h_justify = Style::Left, .v_justify = Style::Top });
}

static Element *make_paragraph(std::string const contents) {
    return new Ui::HContainer({
        new Ui::StaticText(14, "-", { .fill = 0xffffffff, .v_justify = Style::Top }),
        new Ui::StaticParagraph(360, 14, contents)
    }, 0, 10, { .h_justify = Style::Left, .v_justify = Style::Top });
}

static Element *make_entries(std::initializer_list<std::string const> contents) {
    Element *elt = new Ui::VContainer({}, 5, 3, { .h_justify = Style::Left, .v_justify = Style::Top });
    for (std::string const str : contents)
        elt->add_child(make_paragraph(str));
    return elt;
}

static Element *make_changelog_contents() {
    Element *elt = new Ui::VContainer({
        make_date("July 18th 2026"),
        make_entries({
            "Fixed the lag: the server now holds a full 20 ticks per second even on a busy map",
            "Item drops now roll their rarity from the mob's rarity, and mobs grow larger with rarity",
            "Ants and spiders keep their proportions at every rarity (mouth and legs scale with the body)",
            "Mob health bars now span the mob's full width; the name and rarity labels scale to fit",
            "Petal tooltips show the item's real rarity and its rarity-scaled Health and Damage",
            "Merged the Heavy and Heaviest petals into a single Heavy",
            "Petals are no longer disabled in low-level zones, and you never lose petals on death",
            "Players can no longer damage each other, and cleaned up the petal gallery"
        }),
        make_divider(),
        make_date("July 16th 2026"),
        make_entries({
            "Brand new hand-crafted biome map with exact terrain collision",
            "Walls, water, cliffs, dirt, bushes and castle now block you precisely",
            "Added Ultra, Super and Unique rarities (9 tiers total)",
            "Score is now XP; the leaderboard ranks by level",
            "New minimap with zoom, a player HUD bar and a redesigned death screen",
            "Settings: High Quality and Show Grid toggles",
            "Inventory: drag a single petal, smoother stacking animations",
            "Fixed logging out then back in, and lots of other bugfixes"
        }),
        make_divider(),
        make_date("October 20th 2025"),
        make_entries({
            "Added 1 new petal",
            "Peas now split into 4 orthogonal directions",
            "Even more bugfixes"
        }),
        make_divider(),
        make_date("August 30th 2025"),
        make_entries({
            "Added mobile touch controls",
            "Neutral mobs will now aggro the leader if above level 60",
            "More bugfixes"
        }),
        make_divider(),
        make_date("August 10th 2025"),
        make_entries({
            "Added TDM",
            "Made some faster mobs slower",
            "Various bugfixes"
        }),
        make_divider(),
        make_date("July 31st 2025"),
        make_entries({
            "Added 1 new petal",
            "Changed initial petal spawn delay from 2.5s to 1s"
        }),
        make_divider(),
        make_date("June 15th 2025"),
        make_entries({
            "Finished base game",
            "Added 3 new petals",
            "Drop rates are now autobalanced",
            "Spawning a petal for the first time now takes 2.5 extra seconds"
        }),
        make_divider(),
        make_date("June 11th 2025"),
        make_entries({
            "Added 1 new mob",
            "Balanced some mob stats",
            "Revamped petal descriptions"
        }),
        make_divider(),
        make_date("June 7th 2025"),
        make_entries({
            "Finished changelog",
            "Tweaked some petal stats",
            "Added settings and mob gallery"
        }),
        make_divider(),
        make_date("June 6th 2025"),
        make_entries({
            "Added Changelog",
            "Added 2 new petals",
            "Added 1 new mob",
        }),
        make_divider(),
        make_date("March 23rd 2025"),
        make_entries({
            "Game is now playable",
            "Added spawner petals"
        }),
        make_divider(),
        new Ui::StaticText(14, "Older changelog entries not available")
    }, 0, 10, {});
    return new Ui::ScrollContainer(elt, 300);
}

Element *Ui::make_changelog() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Changelog"),
        make_changelog_contents()
    }, 15, 10, { 
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kChangelog && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::changelog = elt;
    return elt;
}