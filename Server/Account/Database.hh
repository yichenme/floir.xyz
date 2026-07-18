#pragma once

#include <Shared/PetalItem.hh>

#include <cstdint>
#include <string>
#include <vector>

// Account persistence backed by Server/database.json (see Server/Account/database.js).
// Every function loads/saves the on-disk JSON lazily, so callers don't need to
// sequence AccountDB::load() themselves; it exists for explicit warm-up/flush.
namespace AccountDB {
    bool load();
    bool save();

    // returns 1 ok, 0 fail (err_out explains why); session_key_out set on success.
    int register_user(std::string const &user, std::string const &pass,
                       std::string &session_key_out, std::string &err_out);
    int login_user(std::string const &user, std::string const &pass,
                    std::string &session_key_out, std::string &err_out);
    int restore_session(std::string const &user, std::string const &session_key);

    // out must point to a buffer of length 2 * MAX_SLOT_COUNT.
    int read_loadout(std::string const &user, PetalItem *out);
    int write_loadout(std::string const &user, PetalItem const *in);

    int read_inventory(std::string const &user, std::vector<PetalStack> &out);
    int write_inventory(std::string const &user, std::vector<PetalStack> const &in);

    int read_progress(std::string const &user, uint8_t &level, uint32_t &xp);
    int write_progress(std::string const &user, uint8_t level, uint32_t xp);

    // Per-account mob kill tally (mob id + rarity -> count). Only killed combos
    // are stored. Callers batch a save() after crediting a death.
    struct KillEntry { uint8_t mob; uint8_t rarity; uint64_t count; };
    void add_kill(std::string const &user, uint8_t mob, uint8_t rarity);
    int read_kills(std::string const &user, std::vector<KillEntry> &out);
}
