#include <Client/Account.hh>

#include <Client/Game.hh>

#include <Shared/AccountValidation.hh>
#include <Shared/Binary.hh>

#include <emscripten.h>

namespace Account {
    std::string username_field, password_field, confirm_field;
    std::string logged_in_user, error, status;
    bool register_mode = false;

    static std::string session_key;
    // Tracks whose auth request is in flight so on_auth_response doesn't
    // depend on username_field still holding the same value once the
    // server replies (user may have kept typing in the meantime).
    static std::string pending_user;
}

static void set_local_storage(char const *key, std::string const &value) {
    EM_ASM({
        window.localStorage[UTF8ToString($0)] = UTF8ToString($1);
    }, key, value.c_str());
}

static std::string get_local_storage(char const *key) {
    char *ptr = (char *) EM_ASM_PTR({
        const v = window.localStorage[UTF8ToString($0)];
        if (!v) return 0;
        return stringToNewUTF8(v);
    }, key);
    if (ptr == nullptr) return "";
    std::string out{ptr};
    free(ptr);
    return out;
}

static void clear_local_storage(char const *key) {
    EM_ASM({
        delete window.localStorage[UTF8ToString($0)];
    }, key);
}

bool Account::logged_in() {
    return !logged_in_user.empty();
}

static void send_auth_packet(uint8_t type, std::string const &user, std::string const &pass) {
    Account::pending_user = user;
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(type);
    writer.write<std::string>(user);
    writer.write<std::string>(pass);
    Game::socket.send(writer.packet, writer.at - writer.packet);
}

void Account::request_register() {
    error = "";
    if (!account_username_valid(username_field)) {
        error = "Username must be 3-16 letters, numbers, or underscores";
        return;
    }
    if (!account_password_valid(password_field)) {
        error = "Password must be 4-32 letters, numbers, or underscores";
        return;
    }
    if (password_field != confirm_field) {
        error = "Passwords do not match";
        return;
    }
    send_auth_packet(Serverbound::kRegister, username_field, password_field);
}

void Account::request_login() {
    error = "";
    if (!account_username_valid(username_field)) {
        error = "Invalid username";
        return;
    }
    if (!account_password_valid(password_field)) {
        error = "Invalid password";
        return;
    }
    send_auth_packet(Serverbound::kLogin, username_field, password_field);
}

void Account::request_session_restore() {
    std::string user = get_local_storage("floir_user");
    std::string key = get_local_storage("floir_session_key");
    if (user.empty() || key.empty()) return;
    pending_user = user;
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kSessionRestore);
    writer.write<std::string>(user);
    writer.write<std::string>(key);
    Game::socket.send(writer.packet, writer.at - writer.packet);
}

void Account::request_logout() {
    // Tell the server to drop this connection's login, otherwise client->logged_in
    // stays set and handle_login early-returns until the socket is remade (refresh).
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kLogout);
    Game::socket.send(writer.packet, writer.at - writer.packet);
    // Tear down the game view atomically so the equipped-petal icons stop
    // rendering in the SAME frame. Without this, the loadout slots kept their
    // frozen petal ids while the flower's rarities zeroed out, drawing every
    // equipped petal as Common for a frame (the "logout Common flash").
    Game::reset();
    logged_in_user = "";
    session_key = "";
    status = "";
    error = "";
    password_field = "";
    confirm_field = "";
    clear_local_storage("floir_user");
    clear_local_storage("floir_session_key");
}

void Account::on_auth_response(uint8_t ok, std::string const &payload) {
    if (!ok) {
        error = payload;
        logged_in_user = "";
        return;
    }
    error = "";
    logged_in_user = pending_user;
    session_key = payload;
    status = "Logged in as " + logged_in_user;
    password_field = "";
    confirm_field = "";
    set_local_storage("floir_user", logged_in_user);
    set_local_storage("floir_session_key", session_key);
}
