#include <Client/Ui/InGame/Inventory.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/TextInput.hh>
#include <Client/Ui/Extern.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Game.hh>
#include <Client/Input.hh>

#include <Shared/Config.hh>

#include <string>

using namespace Ui;

// Kept so the tick loop can detect Enter-to-send and clear the box.
static TextInput *chat_input_box = nullptr;

static bool chat_visible() { return Game::alive(); }

namespace {
    // Boxless, left-aligned message log sitting above the input. Newest message
    // is on the bottom line (right above the input); older ones stack upward.
    class ChatLog final : public Element {
    public:
        ChatLog() : Element(300, 150, { .h_justify = Style::Left, .v_justify = Style::Bottom }) {}
        void on_render(Renderer &ctx) override {
            int const n = (int) Game::chat_messages.size();
            int const show = n < 8 ? n : 8;
            ctx.left_text_align();
            for (int k = 0; k < show; ++k) {
                RenderContext c(&ctx);
                ctx.translate(-width / 2, height / 2 - 6 - k * 18);
                ctx.draw_text(Game::chat_messages[n - 1 - k].c_str(), { .fill = 0xffffffff, .size = 14 });
            }
            ctx.center_text_align();   // restore default for the rest of the UI
        }
    };
}

Element *Ui::make_chat_box() {
    Element *log = new ChatLog();

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
    // The chat DOM input swallows canvas focus, so we can't rely on Ui::focused.
    // In-game, Enter with text in the chat box sends it (and consumes the Enter
    // so it isn't also treated as a spawn key).
    if (chat_input_box == nullptr || !Game::alive()) return false;
    if (!Input::keys_pressed_this_tick.contains('\r')) return false;
    if (Game::chat_input.empty()) return false;
    Game::send_chat(Game::chat_input);
    chat_input_box->clear();
    return true;
}
