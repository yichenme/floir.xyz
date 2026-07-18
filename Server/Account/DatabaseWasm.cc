#ifdef WASM_SERVER
#include <Server/Account/Database.hh>

#include <Shared/AccountValidation.hh>
#include <Shared/PetalItem.hh>
#include <Shared/StaticData.hh>
#include <Shared/StaticDefinitions.hh>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include <emscripten.h>

// Bridges AccountDB onto Server/Account/database.js. The JS side owns
// Server/database.json and hashing/session primitives; this file only
// validates input (Shared/AccountValidation) and marshals the small,
// self-produced JSON shapes ({"type":N,"rarity":N[,"count":N]}) that
// database.js emits back into PetalItem/PetalStack.

EM_JS(void, ejs_ensure_ready, (), {
    if (typeof global['loadDatabase'] !== "function") {
        var path = require("path");
        require(path.join(__dirname, "..", "Account", "database.js"));
    }
    if (typeof global['__allocUtf8'] !== "function") {
        // Captured once here so later calls don't depend on whether other
        // EM_JS blocks share lexical scope with this one.
        global['__allocUtf8'] = (str) => {
            var len = lengthBytesUTF8(str) + 1;
            var ptr = _malloc(len);
            stringToUTF8(str, ptr, len);
            return ptr;
        };
    }
    global['loadDatabase']();
});

EM_JS(int, ejs_user_exists, (char const *user), {
    return global['dbUserExists'](UTF8ToString(user)) ? 1 : 0;
});

EM_JS(char *, ejs_hash_password, (char const *pass), {
    return global['__allocUtf8'](global['hashPassword'](UTF8ToString(pass)));
});

EM_JS(char *, ejs_register, (char const *user, char const *pass_hash, char const *loadout_json, char const *inventory_json), {
    var key = global['dbRegister'](UTF8ToString(user), UTF8ToString(pass_hash), UTF8ToString(loadout_json), UTF8ToString(inventory_json));
    return key === null ? 0 : global['__allocUtf8'](key);
});

EM_JS(char *, ejs_login, (char const *user, char const *pass_hash), {
    var key = global['dbLogin'](UTF8ToString(user), UTF8ToString(pass_hash));
    return key === null ? 0 : global['__allocUtf8'](key);
});

EM_JS(int, ejs_check_session, (char const *user, char const *session_key), {
    return global['dbCheckSession'](UTF8ToString(user), UTF8ToString(session_key)) ? 1 : 0;
});

EM_JS(char *, ejs_get_loadout, (char const *user), {
    var json = global['dbGetLoadout'](UTF8ToString(user));
    return json === null ? 0 : global['__allocUtf8'](json);
});

EM_JS(int, ejs_set_loadout, (char const *user, char const *json), {
    return global['dbSetLoadout'](UTF8ToString(user), UTF8ToString(json)) ? 1 : 0;
});

EM_JS(char *, ejs_get_inventory, (char const *user), {
    var json = global['dbGetInventory'](UTF8ToString(user));
    return json === null ? 0 : global['__allocUtf8'](json);
});

EM_JS(int, ejs_set_inventory, (char const *user, char const *json), {
    return global['dbSetInventory'](UTF8ToString(user), UTF8ToString(json)) ? 1 : 0;
});

EM_JS(char *, ejs_get_progress, (char const *user), {
    var json = global['dbGetProgress'](UTF8ToString(user));
    return json === null ? 0 : global['__allocUtf8'](json);
});

EM_JS(int, ejs_set_progress, (char const *user, int level, int xp), {
    return global['dbSetProgress'](UTF8ToString(user), level, xp) ? 1 : 0;
});

EM_JS(int, ejs_add_kill, (char const *user, int mob, int rarity), {
    return global['dbAddKill'](UTF8ToString(user), mob, rarity) ? 1 : 0;
});

EM_JS(char *, ejs_get_kills, (char const *user), {
    var json = global['dbGetKills'](UTF8ToString(user));
    return json === null ? 0 : global['__allocUtf8'](json);
});

EM_JS(int, ejs_save_database, (), {
    try {
        global['saveDatabase']();
        return 1;
    } catch (e) {
        console.error(e);
        return 0;
    }
});

namespace {
    std::string take_owned(char *ptr) {
        if (!ptr) return {};
        std::string s(ptr);
        free(ptr);
        return s;
    }

    long extract_field(std::string const &obj, char const *key) {
        std::string const pattern = std::string("\"") + key + "\":";
        size_t const pos = obj.find(pattern);
        if (pos == std::string::npos) return 0;
        return std::strtol(obj.c_str() + pos + pattern.size(), nullptr, 10);
    }

    // Parses the flat `[{"type":N,"rarity":N[,"count":N]}, ...]` shape emitted
    // by database.js. Not a general JSON parser: relies on the JS side never
    // producing nested braces/strings for this data.
    std::vector<std::array<long, 3>> parse_petal_json(std::string const &json) {
        std::vector<std::array<long, 3>> out;
        size_t i = 0;
        while (i < json.size()) {
            size_t const obj_start = json.find('{', i);
            if (obj_start == std::string::npos) break;
            size_t const obj_end = json.find('}', obj_start);
            if (obj_end == std::string::npos) break;
            std::string const obj = json.substr(obj_start, obj_end - obj_start + 1);
            out.push_back({extract_field(obj, "type"), extract_field(obj, "rarity"), extract_field(obj, "count")});
            i = obj_end + 1;
        }
        return out;
    }

    std::string petal_items_to_json(PetalItem const *items, size_t count) {
        std::string json = "[";
        for (size_t i = 0; i < count; ++i) {
            if (i) json += ',';
            json += "{\"type\":" + std::to_string(items[i].type) + ",\"rarity\":" + std::to_string(items[i].rarity) + "}";
        }
        json += ']';
        return json;
    }

    std::string petal_stacks_to_json(std::vector<PetalStack> const &stacks) {
        std::string json = "[";
        for (size_t i = 0; i < stacks.size(); ++i) {
            if (i) json += ',';
            json += "{\"type\":" + std::to_string(stacks[i].type) + ",\"rarity\":" + std::to_string(stacks[i].rarity) +
                     ",\"count\":" + std::to_string(stacks[i].count) + "}";
        }
        json += ']';
        return json;
    }

    // New accounts start with Basic petals filling the active loadout slots at
    // level 1 (matching Server/Game.cc's fresh-flower default) and no backup
    // slots or inventory.
    std::string default_loadout_json() {
        std::array<PetalItem, 2 * MAX_SLOT_COUNT> loadout{};
        uint32_t const active = loadout_slots_at_level(1);
        for (uint32_t i = 0; i < active && i < 2 * MAX_SLOT_COUNT; ++i) {
            loadout[i].type = PetalID::kBasic;
            loadout[i].rarity = PETAL_DATA[PetalID::kBasic].rarity;
        }
        return petal_items_to_json(loadout.data(), loadout.size());
    }
}

bool AccountDB::load() {
    ejs_ensure_ready();
    return true;
}

bool AccountDB::save() {
    ejs_ensure_ready();
    return ejs_save_database();
}

int AccountDB::register_user(std::string const &user, std::string const &pass, std::string &session_key_out, std::string &err_out) {
    ejs_ensure_ready();
    if (!account_username_valid(user)) {
        err_out = "invalid username";
        return 0;
    }
    if (!account_password_valid(pass)) {
        err_out = "invalid password";
        return 0;
    }
    if (ejs_user_exists(user.c_str())) {
        err_out = "username taken";
        return 0;
    }
    std::string const pass_hash = take_owned(ejs_hash_password(pass.c_str()));
    std::string const loadout_json = default_loadout_json();
    char *key_ptr = ejs_register(user.c_str(), pass_hash.c_str(), loadout_json.c_str(), "[]");
    if (!key_ptr) {
        err_out = "registration failed";
        return 0;
    }
    session_key_out = take_owned(key_ptr);
    return 1;
}

int AccountDB::login_user(std::string const &user, std::string const &pass, std::string &session_key_out, std::string &err_out) {
    ejs_ensure_ready();
    if (!account_username_valid(user) || !account_password_valid(pass)) {
        err_out = "invalid username or password";
        return 0;
    }
    std::string const pass_hash = take_owned(ejs_hash_password(pass.c_str()));
    char *key_ptr = ejs_login(user.c_str(), pass_hash.c_str());
    if (!key_ptr) {
        err_out = "invalid username or password";
        return 0;
    }
    session_key_out = take_owned(key_ptr);
    return 1;
}

int AccountDB::restore_session(std::string const &user, std::string const &session_key) {
    ejs_ensure_ready();
    if (!account_username_valid(user)) return 0;
    return ejs_check_session(user.c_str(), session_key.c_str());
}

int AccountDB::read_loadout(std::string const &user, PetalItem *out) {
    ejs_ensure_ready();
    char *json_ptr = ejs_get_loadout(user.c_str());
    if (!json_ptr) return 0;
    std::vector<std::array<long, 3>> const parsed = parse_petal_json(take_owned(json_ptr));
    for (size_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
        if (i < parsed.size()) {
            out[i].type = static_cast<PetalID::T>(parsed[i][0]);
            out[i].rarity = static_cast<uint8_t>(parsed[i][1]);
        } else {
            out[i] = PetalItem{};
        }
    }
    return 1;
}

int AccountDB::write_loadout(std::string const &user, PetalItem const *in) {
    ejs_ensure_ready();
    std::string const json = petal_items_to_json(in, 2 * MAX_SLOT_COUNT);
    return ejs_set_loadout(user.c_str(), json.c_str());
}

int AccountDB::read_inventory(std::string const &user, std::vector<PetalStack> &out) {
    ejs_ensure_ready();
    char *json_ptr = ejs_get_inventory(user.c_str());
    if (!json_ptr) return 0;
    std::vector<std::array<long, 3>> const parsed = parse_petal_json(take_owned(json_ptr));
    out.clear();
    out.reserve(parsed.size());
    for (auto const &item : parsed) {
        PetalStack stack;
        stack.type = static_cast<PetalID::T>(item[0]);
        stack.rarity = static_cast<uint8_t>(item[1]);
        stack.count = static_cast<uint64_t>(item[2]);
        out.push_back(stack);
    }
    return 1;
}

int AccountDB::write_inventory(std::string const &user, std::vector<PetalStack> const &in) {
    ejs_ensure_ready();
    std::string const json = petal_stacks_to_json(in);
    return ejs_set_inventory(user.c_str(), json.c_str());
}

int AccountDB::read_progress(std::string const &user, uint8_t &level, uint32_t &xp) {
    ejs_ensure_ready();
    char *json_ptr = ejs_get_progress(user.c_str());
    if (!json_ptr) return 0;
    std::string const json = take_owned(json_ptr);
    level = static_cast<uint8_t>(extract_field(json, "level"));
    xp = static_cast<uint32_t>(extract_field(json, "xp"));
    return 1;
}

int AccountDB::write_progress(std::string const &user, uint8_t level, uint32_t xp) {
    ejs_ensure_ready();
    return ejs_set_progress(user.c_str(), level, xp);
}

void AccountDB::add_kill(std::string const &user, uint8_t mob, uint8_t rarity) {
    ejs_ensure_ready();
    ejs_add_kill(user.c_str(), mob, rarity);
}

int AccountDB::read_kills(std::string const &user, std::vector<KillEntry> &out) {
    ejs_ensure_ready();
    char *json_ptr = ejs_get_kills(user.c_str());
    if (!json_ptr) return 0;
    // Reuses the inventory triple parser: "type" carries the mob id here.
    std::vector<std::array<long, 3>> const parsed = parse_petal_json(take_owned(json_ptr));
    out.clear();
    out.reserve(parsed.size());
    for (auto const &item : parsed)
        out.push_back(KillEntry{
            static_cast<uint8_t>(item[0]),
            static_cast<uint8_t>(item[1]),
            static_cast<uint64_t>(item[2])
        });
    return 1;
}
#endif
