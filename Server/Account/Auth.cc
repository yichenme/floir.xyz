#include <Server/Account/Auth.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>

#include <Shared/AccountValidation.hh>

namespace Auth {

static void on_authenticated(Client *client, std::string const &user, std::string const &session_key) {
    // Wipe any leftover game session before binding the new account, so a
    // stale flower/loadout/progress from a previous login on this socket can
    // never carry into -- or be written back over -- the new account.
    if (client->game != nullptr) client->game->reset_session(client);
    client->username = user;
    client->session_key = session_key;
    client->logged_in = 1;
    send_auth_response(client, 1, session_key);
    InventoryOps::sync_inventory_update(client);
    InventoryOps::sync_kills_update(client);
}

void handle_register(Client *client, Reader &reader) {
    std::string user, pass;
    reader.read<std::string>(user);
    reader.read<std::string>(pass);
    if (client->logged_in) return;
    if (!account_username_valid(user)) {
        send_auth_response(client, 0, "Invalid username");
        return;
    }
    if (!account_password_valid(pass)) {
        send_auth_response(client, 0, "Invalid password");
        return;
    }
    std::string session_key, err;
    if (!AccountDB::register_user(user, pass, session_key, err)) {
        send_auth_response(client, 0, err);
        return;
    }
    on_authenticated(client, user, session_key);
}

void handle_login(Client *client, Reader &reader) {
    std::string user, pass;
    reader.read<std::string>(user);
    reader.read<std::string>(pass);
    if (client->logged_in) return;
    std::string session_key, err;
    if (!AccountDB::login_user(user, pass, session_key, err)) {
        send_auth_response(client, 0, err);
        return;
    }
    on_authenticated(client, user, session_key);
}

void handle_session_restore(Client *client, Reader &reader) {
    std::string user, session_key;
    reader.read<std::string>(user);
    reader.read<std::string>(session_key);
    if (client->logged_in) return;
    if (!AccountDB::restore_session(user, session_key)) {
        send_auth_response(client, 0, "Session expired");
        return;
    }
    on_authenticated(client, user, session_key);
}

void handle_logout(Client *client) {
    // Tear down the live game session FIRST: kill any flower and reset the
    // camera loadout/progress. Without this the flower (holding this account's
    // equipped petals) survived logout and, after logging into another account,
    // leaked those petals into the new account -- a cross-account item transfer
    // / duplication exploit, and forced the new account's saved level down.
    if (client->game != nullptr) client->game->reset_session(client);
    // Clear this connection's login so the account can log in again on the same
    // socket without a page refresh. No response packet: the client already
    // cleared its own state before sending this.
    client->logged_in = 0;
    client->username = "";
    client->session_key = "";
}

void send_auth_response(Client *client, uint8_t ok, std::string const &payload) {
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kAuthResponse);
    writer.write<uint8_t>(ok);
    writer.write<std::string>(payload);
    client->send_packet(writer.packet, writer.at - writer.packet);
}

}
