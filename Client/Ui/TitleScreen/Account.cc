#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Choose.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Ui/TextInput.hh>

#include <Client/Account.hh>
#include <Client/Game.hh>

using namespace Ui;

static Element *make_logged_in_view() {
    return new Ui::VContainer({
        new Ui::StaticText(18, "Account"),
        new Ui::DynamicText(13, [](){
            return "Logged in as: " + Account::logged_in_user;
        }, { .fill = 0xffffffff }),
        new Ui::Button(140, 36,
            new Ui::StaticText(15, "Log out"),
            [](Element *, uint8_t e){ if (e == Ui::kClick) Account::request_logout(); },
            nullptr,
            { .fill = 0xffdb5a5a, .line_width = 5, .round_radius = 3 }
        )
    }, 15, 10, { .h_justify = Style::Left, .no_animation = 1 });
}

static Element *make_logged_out_view() {
    return new Ui::VContainer({
        new Ui::StaticText(18, "Account"),
        new Ui::HContainer({
            new Ui::Button(90, 32,
                new Ui::StaticText(13, "Login"),
                [](Element *, uint8_t e){ if (e == Ui::kClick) {
                    Account::register_mode = false;
                    Account::error = "";
                } },
                [](){ return !Account::register_mode; },
                { .fill = 0xff5a9fdb, .line_width = 4, .round_radius = 3 }
            ),
            new Ui::Button(90, 32,
                new Ui::StaticText(13, "Register"),
                [](Element *, uint8_t e){ if (e == Ui::kClick) {
                    Account::register_mode = true;
                    Account::error = "";
                } },
                [](){ return Account::register_mode; },
                { .fill = 0xff5a9fdb, .line_width = 4, .round_radius = 3 }
            )
        }, 0, 10),
        // Reminder/placeholder text inside the fields at half size (font_scale 0.5).
        new Ui::TextInput(Account::username_field, 220, 36, 16, {
            .line_width = 4,
            .round_radius = 4
        }, false, "Username", 0.5f),
        new Ui::TextInput(Account::password_field, 220, 36, 32, {
            .line_width = 4,
            .round_radius = 4
        }, true, "Password", 0.5f),
        new Ui::Choose(
            new Ui::Element(0, 0, { .no_animation = 1 }),
            new Ui::TextInput(Account::confirm_field, 220, 36, 32, {
                .line_width = 4,
                .round_radius = 4,
                .no_animation = 1
            }, true, "Confirm password", 0.5f),
            [](){ return Account::register_mode; },
            { .no_animation = 1 }
        ),
        new Ui::Button(140, 36,
            new Ui::StaticText(15, "Submit"),
            [](Element *, uint8_t e){ if (e == Ui::kClick) {
                if (Account::register_mode) Account::request_register();
                else Account::request_login();
            } },
            nullptr,
            { .fill = 0xff1dd129, .line_width = 5, .round_radius = 3 }
        ),
        new Ui::DynamicText(12, [](){ return Account::error; }, { .fill = 0xffff5555 })
    }, 12, 10, { .h_justify = Style::Left, .no_animation = 1 });
}

Element *Ui::make_account_panel() {
    Element *elt = new Ui::Choose(
        make_logged_out_view(),
        make_logged_in_view(),
        [](){ return Account::logged_in(); },
        {
            .fill = 0xff5a9fdb,
            .line_width = 7,
            .round_radius = 3,
            // Slide up/down on open/close (Choose::refactor guards the first-open
            // collapse so it doesn't glitch).
            .animate = [](Element *elt, Renderer &ctx){
                ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
            },
            .should_render = [](){
                return Ui::panel_open == Panel::kAccount && Game::should_render_title_ui();
            },
            .h_justify = Style::Left,
            .v_justify = Style::Bottom
        }
    );
    Ui::Panel::account = elt;
    return elt;
}
