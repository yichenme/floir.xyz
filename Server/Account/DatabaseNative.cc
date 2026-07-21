#ifndef WASM_SERVER
#include <Server/Account/Database.hh>

#include <Shared/StaticDefinitions.hh>

#include <string>
#include <vector>

namespace {
    char const *kWasmRequired = "Accounts require WASM server";
}

bool AccountDB::load() {
    return true;
}

bool AccountDB::save() {
    return true;
}

int AccountDB::register_user(std::string const &user, std::string const &pass,
                             std::string &session_key_out, std::string &err_out) {
    (void)user;
    (void)pass;
    (void)session_key_out;
    err_out = kWasmRequired;
    return 0;
}

int AccountDB::login_user(std::string const &user, std::string const &pass,
                          std::string &session_key_out, std::string &err_out) {
    (void)user;
    (void)pass;
    (void)session_key_out;
    err_out = kWasmRequired;
    return 0;
}

int AccountDB::restore_session(std::string const &user, std::string const &session_key) {
    (void)user;
    (void)session_key;
    return 0;
}

int AccountDB::read_loadout(std::string const &user, PetalItem *out) {
    (void)user;
    for (size_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        out[i] = PetalItem{};
    }
    return 1;
}

int AccountDB::write_loadout(std::string const &user, PetalItem const *in) {
    (void)user;
    (void)in;
    return 1;
}

int AccountDB::read_inventory(std::string const &user, std::vector<PetalStack> &out) {
    (void)user;
    out.clear();
    return 1;
}

int AccountDB::write_inventory(std::string const &user, std::vector<PetalStack> const &in) {
    (void)user;
    (void)in;
    return 1;
}

int AccountDB::read_progress(std::string const &user, uint8_t &level, uint32_t &xp) {
    (void)user;
    (void)level;
    (void)xp;
    return 1;
}

int AccountDB::write_progress(std::string const &user, uint8_t level, uint32_t xp) {
    (void)user;
    (void)level;
    (void)xp;
    return 1;
}

int AccountDB::read_talents(std::string const &user, uint8_t &health_rank, uint8_t &reload_rank) {
    (void)user;
    health_rank = 0;
    reload_rank = 0;
    return 1;
}

int AccountDB::write_talents(std::string const &user, uint8_t health_rank, uint8_t reload_rank) {
    (void)user;
    (void)health_rank;
    (void)reload_rank;
    return 1;
}
#endif
