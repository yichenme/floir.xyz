#pragma once

#include <cstdint>
#include <string>

// Client-side account state + auth packet round-trips (kRegister/kLogin/
// kSessionRestore -> kAuthResponse). Session key persists to localStorage
// (floir_user / floir_session_key) via Storage.cc's StorageProtocol.
namespace Account {
    extern std::string username_field, password_field, confirm_field;
    extern std::string logged_in_user, error, status;
    extern bool register_mode;

    bool logged_in();
    void request_register();
    void request_login();
    void request_session_restore();
    void request_logout();
    void on_auth_response(uint8_t ok, std::string const &payload);
}
