#include <Server/Client.hh>

#include <Server/Account/Auth.hh>
#include <Server/Account/Database.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>

#include <Helpers/UTF8.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <iostream>

// Wire-safety caps for auth packet strings; AccountValidation.hh enforces the
// tighter, business-level bounds once the bytes are actually read.
constexpr uint32_t MAX_AUTH_FIELD_LENGTH = 32;
constexpr uint32_t MAX_SESSION_KEY_LENGTH = 64;

Client::Client() : game(nullptr) {}

void Client::init() {
    DEBUG_ONLY(assert(game == nullptr);)
    Server::game.add_client(this);    
}

void Client::remove() {
    if (game == nullptr) return;
    game->remove_client(this);
}

void Client::disconnect(int reason, std::string const &message) {
    if (ws == nullptr) return;
    remove();
    ws->end(reason, message);
}

uint8_t Client::alive() {
    if (game == nullptr) return false;
    Simulation *simulation = &game->simulation;
    return simulation->ent_exists(camera) 
    && simulation->ent_exists(simulation->get_ent(camera).get_player());
}

void Client::on_message(WebSocket *ws, std::string_view message, uint64_t code) {
    if (ws == nullptr) return;
    uint8_t const *data = reinterpret_cast<uint8_t const *>(message.data());
    Reader reader(data);
    Validator validator(data, data + message.size());
    Client *client = ws->getUserData();
    if (client == nullptr) {
        ws->end(CloseReason::kServer, "Server Error");
        return;
    }
    if (!client->verified) {
        if (client->check_invalid(validator.validate_uint8() && validator.validate_uint64())) return;
        if (reader.read<uint8_t>() != Serverbound::kVerify) {
            client->disconnect();
            return;
        }
        if (reader.read<uint64_t>() != VERSION_HASH) {
            client->disconnect(CloseReason::kOutdated, "Outdated Version");
            return;
        }
        client->verified = 1;
        client->init();
        return;
    }
    if (client->game == nullptr) {
        client->disconnect();
        return;
    }
    if (client->check_invalid(validator.validate_uint8())) return;
    switch (reader.read<uint8_t>()) {
        case Serverbound::kVerify:
            client->disconnect();
            return;
        case Serverbound::kClientInput: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            if (client->check_invalid(
                validator.validate_float() &&
                validator.validate_float() &&
                validator.validate_uint8()
            )) return;
            float x = reader.read<float>();
            float y = reader.read<float>();
            if (x == 0 && y == 0) player.acceleration.set(0,0);
            else {
                if (std::abs(x) > 5e3 || std::abs(y) > 5e3) break;
                Vector accel(x,y);
                float m = accel.magnitude();
                if (m > 200) accel.set_magnitude(PLAYER_ACCELERATION);
                else accel.set_magnitude(m / 200 * PLAYER_ACCELERATION);
                player.acceleration = accel;
            }
            player.input = reader.read<uint8_t>();
            break;
        }
        case Serverbound::kRegister: {
            if (client->check_invalid(
                validator.validate_string(MAX_AUTH_FIELD_LENGTH) &&
                validator.validate_string(MAX_AUTH_FIELD_LENGTH)
            )) return;
            Auth::handle_register(client, reader);
            break;
        }
        case Serverbound::kLogin: {
            if (client->check_invalid(
                validator.validate_string(MAX_AUTH_FIELD_LENGTH) &&
                validator.validate_string(MAX_AUTH_FIELD_LENGTH)
            )) return;
            Auth::handle_login(client, reader);
            break;
        }
        case Serverbound::kSessionRestore: {
            if (client->check_invalid(
                validator.validate_string(MAX_AUTH_FIELD_LENGTH) &&
                validator.validate_string(MAX_SESSION_KEY_LENGTH)
            )) return;
            Auth::handle_session_restore(client, reader);
            break;
        }
        case Serverbound::kLogout: {
            Auth::handle_logout(client);
            break;
        }
        case Serverbound::kClientSpawn: {
            if (!client->logged_in) break;
            if (client->alive()) break;
            //check string length
            std::string name;
            if (client->check_invalid(validator.validate_string(MAX_NAME_LENGTH))) return;
            reader.read<std::string>(name);
            if (client->check_invalid(UTF8Parser::is_valid_utf8(name))) return;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            InventoryOps::apply_account_loadout_to_camera(client, camera);
            // Restore the account's saved peak level so progress survives a
            // fresh connection too, not just deaths within one connection (see
            // EntityFunctions/Death.cc's write_progress for the save side).
            uint32_t saved_level = 0; uint32_t saved_xp = 0;
            if (AccountDB::read_progress(client->username, saved_level, saved_xp)) {
                // Restore the exact peak score (partial XP included). Fall back to
                // the level's base if an older account only had a level saved.
                uint32_t peak = std::max((uint32_t)saved_xp, level_to_score(saved_level));
                if (peak > camera.get_respawn_score()) {
                    camera.set_respawn_score(peak);
                    uint32_t lvl = score_to_level(peak);
                    camera.set_respawn_level(lvl < 1 ? 1 : lvl);
                }
            }
            Entity &player = alloc_player(simulation, camera.get_team());
            player_spawn(simulation, camera, player);
            player.set_name(name);
            player.set_account_name(client->username);
            break;
        }
        case Serverbound::kPetalStore: {
            if (!client->alive()) break;
            if (client->check_invalid(validator.validate_uint8())) return;
            uint8_t pos = reader.read<uint8_t>();
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            InventoryOps::store_from_loadout(client, player, pos);
            break;
        }
        case Serverbound::kEquipPetal:
        case Serverbound::kInventorySwap: {
            if (!client->alive()) break;
            if (client->check_invalid(validator.validate_uint32() && validator.validate_uint8())) return;
            uint32_t inv_index = reader.read<uint32_t>();
            uint8_t pos = reader.read<uint8_t>();
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            InventoryOps::equip_to_loadout(client, player, inv_index, pos);
            break;
        }
        case Serverbound::kPetalSwap: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.get_player());
            if (client->check_invalid(validator.validate_uint8() && validator.validate_uint8())) return;
            uint8_t pos1 = reader.read<uint8_t>();
            if (pos1 >= 2 * player.get_loadout_count()) break;
            uint8_t pos2 = reader.read<uint8_t>();
            if (pos2 >= 2 * player.get_loadout_count()) break;
            PetalID::T tmp = player.get_loadout_ids(pos1);
            uint8_t tmp_rarity = player.get_loadout_rarities(pos1);
            player.set_loadout_ids(pos1, player.get_loadout_ids(pos2));
            player.set_loadout_rarities(pos1, player.get_loadout_rarities(pos2));
            player.set_loadout_ids(pos2, tmp);
            player.set_loadout_rarities(pos2, tmp_rarity);
            break;
        }
        case Serverbound::kChat: {
            if (!client->logged_in) break;
            if (client->check_invalid(validator.validate_string(MAX_CHAT_LENGTH))) return;
            std::string message;
            reader.read<std::string>(message);
            if (client->check_invalid(UTF8Parser::is_valid_utf8(message))) return;
            if (message.empty()) break;
            // Prefer the flower's in-game name; fall back to the account name.
            std::string sender = client->username.empty() ? "Anonymous" : client->username;
            if (client->alive()) {
                Simulation *sim = &client->game->simulation;
                Entity &camera = sim->get_ent(client->camera);
                if (sim->ent_exists(camera.get_player())) {
                    std::string const nm = sim->get_ent(camera.get_player()).get_name();
                    if (!nm.empty()) sender = nm;
                }
            }
            Writer writer(Server::OUTGOING_PACKET);
            writer.write<uint8_t>(Clientbound::kChatMessage);
            writer.write<std::string>(sender);
            writer.write<std::string>(message);
            client->game->broadcast(writer.packet, writer.at - writer.packet);
            break;
        }
    }
}

void Client::on_disconnect(WebSocket *ws, int code, std::string_view message) {
    std::printf("disconnect: [%d]\n", code);
    Client *client = ws->getUserData();
    if (client == nullptr) return;
    client->remove();
}

bool Client::check_invalid(bool valid) {
    if (valid) return false;
    std::cout << "client sent an invalid packet\n";
    //optional
    disconnect();

    return true;
}