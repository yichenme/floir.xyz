#include <Client/Game.hh>

#include <Client/Account.hh>
#include <Client/Input.hh>
#include <Client/StaticData.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/Ui.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <algorithm>

using namespace Game;

void Game::on_message(uint8_t *ptr, uint32_t len) {
    Reader reader(ptr);
    switch(reader.read<uint8_t>()) {
        case Clientbound::kClientUpdate: {
            simulation_ready = 1;
            camera_id = reader.read<EntityID>();
            EntityID curr_id = reader.read<EntityID>();
            while(!(curr_id == NULL_ENTITY)) {
                assert(simulation.ent_exists(curr_id));
                Entity &ent = simulation.get_ent(curr_id);
                simulation._delete_ent(curr_id);
                curr_id = reader.read<EntityID>();
            }
            curr_id = reader.read<EntityID>();
            while(!(curr_id == NULL_ENTITY)) {
                uint8_t create = reader.read<uint8_t>();
                if (BitMath::at(create, 0)) simulation.force_alloc_ent(curr_id);
                assert(simulation.ent_exists(curr_id));
                Entity &ent = simulation.get_ent(curr_id);
                ent.read(&reader, BitMath::at(create, 0));
                if (BitMath::at(create, 1)) ent.pending_delete = 1;
                curr_id = reader.read<EntityID>();
            }
            simulation.arena_info.read(&reader, reader.read<uint8_t>());
            break;
        }
        case Clientbound::kAuthResponse: {
            uint8_t ok = reader.read<uint8_t>();
            std::string payload;
            reader.read<std::string>(payload);
            Account::on_auth_response(ok, payload);
            break;
        }
        case Clientbound::kInventoryUpdate: {
            uint32_t n = reader.read<uint32_t>();
            ++Game::inventory_version;
            inventory_stacks.clear();
            inventory_stacks.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                PetalStack stack;
                stack.type = static_cast<PetalID::T>(reader.read<uint8_t>());
                stack.rarity = reader.read<uint8_t>();
                stack.count = reader.read<uint64_t>();
                inventory_stacks.push_back(stack);
            }
            // Display order: highest rarity first, then by petal type, so the
            // inventory panel groups by rarity. equip still uses the real index.
            inventory_display_order.resize(inventory_stacks.size());
            for (uint32_t i = 0; i < inventory_stacks.size(); ++i)
                inventory_display_order[i] = i;
            std::sort(inventory_display_order.begin(), inventory_display_order.end(),
                [](uint32_t a, uint32_t b){
                    PetalStack const &sa = inventory_stacks[a], &sb = inventory_stacks[b];
                    if (sa.rarity != sb.rarity) return sa.rarity > sb.rarity;
                    return sa.type < sb.type;
                });
            break;
        }
        case Clientbound::kChatMessage: {
            std::string sender, message;
            reader.read<std::string>(sender);
            reader.read<std::string>(message);
            Game::chat_messages.push_back({ sender + ": " + message, 0xffffffff });
            while (Game::chat_messages.size() > 8) Game::chat_messages.erase(Game::chat_messages.begin());
            break;
        }
        case Clientbound::kSystemMessage: {
            uint8_t kind = reader.read<uint8_t>();
            std::string message;
            reader.read<std::string>(message);
            // Plain coloured text, no sender label: super-green or unique-grey.
            uint32_t const color = kind == SystemMsgKind::kSysUnique
                ? RARITY_COLORS[RarityID::kUnique] : RARITY_COLORS[RarityID::kSuper];
            Game::chat_messages.push_back({ message, color });
            while (Game::chat_messages.size() > 8) Game::chat_messages.erase(Game::chat_messages.begin());
            break;
        }
        case Clientbound::kKillsUpdate: {
            uint32_t n = reader.read<uint32_t>();
            for (auto &row : Game::mob_kills) row.fill(0);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t mob = reader.read<uint8_t>();
                uint8_t rarity = reader.read<uint8_t>();
                uint64_t count = reader.read<uint64_t>();
                if (mob < MobID::kNumMobs && rarity < RarityID::kNumRarities)
                    Game::mob_kills[mob][rarity] = count;
            }
            break;
        }
        case Clientbound::kCraftResult: {
            CraftResult r;
            r.type = static_cast<PetalID::T>(reader.read<uint8_t>());
            r.out_rarity = reader.read<uint8_t>();
            r.crafted = reader.read<uint32_t>();
            r.remaining = reader.read<uint32_t>();
            r.any_success = reader.read<uint8_t>();
            r.received_at = Game::timestamp;
            Game::last_craft_result = r;
            break;
        }
        case Clientbound::kSquadUpdate: {
            uint32_t const sid = reader.read<uint32_t>();
            uint32_t const n = reader.read<uint32_t>();
            Game::squad_id = sid;
            Game::squad_members.clear();
            Game::squad_members.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                SquadMember m;
                m.camera_id = reader.read<EntityID>();
                reader.read<std::string>(m.name);
                Game::squad_members.push_back(m);
            }
            break;
        }
        case Clientbound::kSquadNotice: {
            std::string message;
            reader.read<std::string>(message);
            Game::squad_notice = message;
            Game::squad_notice_until = Game::timestamp + 4000;
            break;
        }
        default:
            break;
    }
}

void Game::send_inputs() {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kClientInput);
    if (Input::freeze_input) {
        writer.write<float>(0);
        writer.write<float>(0);
        writer.write<uint8_t>(0);
    } else {
        writer.write<float>(Input::game_inputs.x);
        writer.write<float>(Input::game_inputs.y);
        writer.write<uint8_t>(Input::game_inputs.flags);
    }
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::spawn_in() {
    if (!Account::logged_in()) {
        Ui::panel_open = Ui::Panel::kAccount;
        Account::error = "Register or log in to play";
        // Position the panel over the Account button, same as clicking it would
        // (spawn_in bypasses the button, which is what set the position before).
        Ui::Element *pg = Ui::Panel::account;
        Ui::Element *btn = Ui::account_button;
        if (pg && btn) {
            pg->x = btn->screen_x / Ui::scale - pg->get_target_width() / 2;
            pg->y = -(btn->height + 20);
            if (pg->x < 10)
                pg->x = 10;
        }
        return;
    }
    // Coming back from Leave while the flower is still alive: just resume the
    // game view (the reverse-spawn animation plays forward again).
    Game::leaving = 0;
    if (Game::alive()) {
        Game::on_game_screen = 1;
        return;
    }
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (Game::on_game_screen == 0) {
        writer.write<uint8_t>(Serverbound::kClientSpawn);
        std::string name = Game::nickname;
        writer.write<std::string>(name);
        socket.send(writer.packet, writer.at - writer.packet);
    } else Game::on_game_screen = 0;
}

void Game::store_petal(uint8_t pos) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kPetalStore);
    writer.write<uint8_t>(pos);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::equip_petal(uint32_t inv_index, uint8_t pos) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kEquipPetal);
    writer.write<uint32_t>(inv_index);
    writer.write<uint8_t>(pos);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::send_chat(std::string const &msg) {
    if (msg.empty()) return;
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kChat);
    writer.write<std::string>(msg);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::send_craft(PetalID::T type, uint8_t rarity, uint32_t amount) {
    if (amount < 5) return;
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kCraft);
    writer.write<uint8_t>(type);
    writer.write<uint8_t>(rarity);
    writer.write<uint32_t>(amount);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::swap_petals(uint8_t pos1, uint8_t pos2) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kPetalSwap);
    writer.write<uint8_t>(pos1);
    writer.write<uint8_t>(pos2);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::swap_all_petals() {
    if (!Game::alive()) return;
    for (uint32_t i = 0; i < Game::loadout_count; ++i)
        Ui::ui_swap_petals(i, i + Game::loadout_count);
}