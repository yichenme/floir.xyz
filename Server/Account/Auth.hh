#pragma once

#include <Shared/Binary.hh>

#include <cstdint>
#include <string>

class Client;

// Dispatch targets for Serverbound::kRegister/kLogin/kSessionRestore (see
// Server/Client.cc); own AccountDB round-trips and session bookkeeping so
// Client.cc stays a thin switch.
namespace Auth {
    void handle_register(Client *client, Reader &reader);
    void handle_login(Client *client, Reader &reader);
    void handle_session_restore(Client *client, Reader &reader);
    void handle_logout(Client *client);

    void send_auth_response(Client *client, uint8_t ok, std::string const &payload);
}
