#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Ui/TextInput.hh>
#include <Client/Ui/Extern.hh>

#include <Client/Game.hh>
#include <Client/Input.hh>

#include <Shared/Config.hh>

#include <string>

using namespace Ui;

// Kept so the tick loop can detect Enter-to-send and clear the box.
static TextInput *chat_input_box = nullptr;

static bool chat_visible() { return Game::alive(); }

Element *Ui::make_chat_box() {
    // Recent-message log: 8 stacked lines, newest at the bottom.
    Element *log = new Ui::VContainer({}, 0, 2, { .h_justify = Style::Left });
    for (int line = 0; line < 8; ++line)
        log->add_child(new Ui::DynamicText(13, [line](){
            int const n = (int) Game::chat_messages.size();
            int const idx = n - 8 + line;   // bottom line = newest message
            return (idx >= 0 && idx < n) ? Game::chat_messages[idx] : std::string("");
        }, { .fill = 0xffffffff, .h_justify = Style::Left }));

    chat_input_box = new Ui::TextInput(Game::chat_input, 240, 32, MAX_CHAT_LENGTH, {
        .line_width = 3,
        .round_radius = 3,
        .h_justify = Style::Left
    }, false, "Chat  (Enter)");

    Element *box = new Ui::VContainer({
        log,
        chat_input_box
    }, 6, 6, {
        .should_render = [](){ return chat_visible(); },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    // Sit just to the right of the inventory button (x=10, width 140).
    box->x = 160;
    box->y = -10;
    return box;
}

bool Ui::chat_try_send() {
    if (chat_input_box == nullptr || Ui::focused != chat_input_box) return false;
    if (!Input::keys_pressed_this_tick.contains('\r')) return false;
    if (!Game::chat_input.empty()) {
        Game::send_chat(Game::chat_input);
        chat_input_box->clear();
    }
    return true;   // consumed the Enter
}
