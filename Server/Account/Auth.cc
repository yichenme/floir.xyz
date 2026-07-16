#include <Server/Account/Auth.hh>

#include <Server/Account/Database.hh>
#include <Server/Client.hh>
#include <Server/Server.hh>

#include <Shared/AccountValidation.hh>

namespace Auth {

// Task 6 replaces this with the real inventory payload (AccountDB::read_inventory
// + PetalStack serialization); for now clients just get an empty list so the
// auth flow is a complete round-trip.
static void send_inventory_stub(Client *client) {
    std::vector<PetalStack> inventory;
    AccountDB::read_inventory(client->username, inventory);
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kInventoryUpdate);
    writer.write<uint32_t>(0);
    client->send_packet(writer.packet, writer.at - writer.packet);
}

static void on_authenticated(Client *client, std::string const &user, std::string const &session_key) {
    client->username = user;
    client->session_key = session_key;
    client->logged_in = 1;
    send_auth_response(client, 1, session_key);
    send_inventory_stub(client);
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

void send_auth_response(Client *client, uint8_t ok, std::string const &payload) {
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kAuthResponse);
    writer.write<uint8_t>(ok);
    writer.write<std::string>(payload);
    client->send_packet(writer.packet, writer.at - writer.packet);
}

}
