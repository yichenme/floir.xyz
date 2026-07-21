#include <Client/Game.hh>

#include <Client/Debug.hh>
#include <Client/Input.hh>
#include <Client/Particle.hh>
#include <Client/Setup.hh>
#include <Client/Storage.hh>
#include <Client/Ui/Extern.hh>

#include <Shared/Config.hh>

#include <cmath>

static double g_last_time = 0;
static uint8_t g_player_was_dead_corpse = 0;
float const MAX_TRANSITION_CIRCLE = 2500;

static int _c = setup_canvas();
static int _i = setup_inputs();

namespace Game {
    Simulation simulation;
    Renderer renderer;
    Renderer game_ui_renderer;
    Socket socket;
    Ui::Window title_ui_window;
    Ui::Window game_ui_window;
    Ui::Window other_ui_window;
    EntityID camera_id;
    EntityID player_id;
    std::string nickname;
    std::string disconnect_message;
    std::string chat_input;
    std::vector<ChatMessage> chat_messages;
    std::array<uint8_t, PetalID::kNumPetals> seen_petals;
    std::array<uint8_t, MobID::kNumMobs> seen_mobs;
    std::vector<PetalStack> inventory_stacks;
    std::vector<uint32_t> inventory_display_order;
    uint32_t inventory_version = 0;
    std::array<std::array<uint64_t, RarityID::kNumRarities>, MobID::kNumMobs> mob_kills = {};
    std::array<PetalID::T, 2 * MAX_SLOT_COUNT> cached_loadout = {PetalID::kNone};

    double timestamp = 0;

    double score = 0;
    uint32_t mobs_killed = 0;
    uint32_t petals_collected = 0;
    float overlevel_timer = 0;
    float slot_indicator_opacity = 0;
    float transition_circle = 0;
    float view_cam_x = 0;
    float view_cam_y = 0;
    uint8_t show_hitboxes = 0;

    uint32_t respawn_level = 1;

    uint32_t squad_id = 0;
    std::vector<SquadMember> squad_members;
    std::string squad_notice;
    double squad_notice_until = 0;
    std::string squad_invite_from;
    double squad_invite_until = 0;
    CraftResult last_craft_result;

    uint8_t talent_health_rank = 0;
    uint8_t talent_reload_rank = 0;
    uint32_t talent_points_total = 0;

    uint8_t loadout_count = 5;
    uint8_t simulation_ready = 0;
    uint8_t on_game_screen = 0;
    uint8_t death_ui_dismissed = 0;
    uint8_t leaving = 0;
    uint8_t show_debug = 1;
}

using namespace Game;

void Game::init() {
    Input::is_mobile = check_mobile();
    Storage::retrieve();
    reset();
    title_ui_window.add_child(
        [](){ 
            Ui::Element *elt = new Ui::StaticText(60, "floir.xyz");
            elt->x = 0;
            elt->y = -270;
            return elt;
        }()
    );
    title_ui_window.add_child(
        Ui::make_title_input_box()
    );
    title_ui_window.add_child(
        Ui::make_title_info_box()
    );
    title_ui_window.add_child(
        Ui::make_panel_buttons()
    );
    title_ui_window.add_child(
        Ui::make_settings_panel()
    );
    title_ui_window.add_child(
        Ui::make_account_panel()
    );
    title_ui_window.add_child(
        Ui::make_petal_gallery()
    );
    title_ui_window.add_child(
        Ui::make_mob_gallery()
    );
    title_ui_window.add_child(
        Ui::make_changelog()
    );
    game_ui_window.add_child(
        Ui::make_death_main_screen()
    );
    game_ui_window.add_child(
        Ui::make_level_bar()
    );
    game_ui_window.add_child(
        Ui::make_squad_bars()
    );
    game_ui_window.add_child(
        Ui::make_minimap()
    );
    game_ui_window.add_child(
        Ui::make_loadout_backgrounds()
    );
    // Mobile controls first so later-added UI (the inventory button/panel) wins
    // touch focus over the full-screen joystick pad -- otherwise a tap on the
    // inventory button is swallowed by the joystick and it never opens.
    game_ui_window.add_child(
        Ui::make_mobile_joystick()
    );
    game_ui_window.add_child(
        Ui::make_mobile_attack_button()
    );
    game_ui_window.add_child(
        Ui::make_mobile_defend_button()
    );
    game_ui_window.add_child(
        Ui::make_mobile_hitbox_button()
    );
    game_ui_window.add_child(
        Ui::make_chat_box()
    );
    // Buttons sit at the bottom corners (never overlap the loadout), so their
    // order relative to the loadout doesn't matter.
    game_ui_window.add_child(
        Ui::make_craft_button()
    );
    game_ui_window.add_child(
        Ui::make_talent_button()
    );
    game_ui_window.add_child(
        Ui::make_inventory_button()
    );
    for (uint8_t i = 0; i < MAX_SLOT_COUNT * 2; ++i) game_ui_window.add_child(new Ui::UiLoadoutPetal(i));
    // Panels added AFTER the loadout petals so an open inventory/craft panel
    // draws ON TOP of the loadout (overlays it). Craft before Inventory so
    // Inventory wins if they ever briefly overlap during a switch.
    game_ui_window.add_child(
        Ui::make_craft_panel()
    );
    game_ui_window.add_child(
        Ui::make_talent_panel()
    );
    game_ui_window.add_child(
        Ui::make_inventory_panel()
    );
    game_ui_window.add_child(
        Ui::make_leaderboard()
    );
    game_ui_window.add_child(
        Ui::make_boss_bars()
    );
    game_ui_window.add_child(
        Ui::make_stat_screen()
    );
    game_ui_window.add_child(
        new Ui::HContainer({
            new Ui::StaticText(20, "floir.xyz")
        }, 20, 0, { .h_justify = Ui::Style::Left, .v_justify = Ui::Style::Top })
    );
    Ui::make_petal_tooltips();
    other_ui_window.add_child(
        Ui::make_debug_stats()
    );
    other_ui_window.add_child(
        [](){ 
            Ui::Element *elt = new Ui::HContainer({
                new Ui::DynamicText(16, [](){ return Game::disconnect_message; })
            }, 5, 5, { 
                .fill = 0x40000000,
                .round_radius = 5,
                .should_render = [](){
                    return !Game::socket.ready && Game::disconnect_message != "";
                },
                .v_justify = Ui::Style::Top
            });
            elt->y = 50;
            return elt;
        }()
    );
    other_ui_window.add_child(
        [](){
            // Squad join/leave notice, same visual style as the disconnect
            // banner above but its own timed visibility (not tied to socket
            // state).
            Ui::Element *elt = new Ui::HContainer({
                new Ui::DynamicText(16, [](){ return Game::squad_notice; })
            }, 5, 5, {
                .fill = 0x40000000,
                .round_radius = 5,
                .should_render = [](){
                    return Game::timestamp < Game::squad_notice_until;
                },
                .v_justify = Ui::Style::Top
            });
            elt->y = 90;
            return elt;
        }()
    );
    // Squad invite lives in game_ui_window, NOT other_ui_window: the whole
    // point of other_ui_window.style.no_polling below is that it's a
    // click-through overlay (debug stats, disconnect/squad-notice banners) --
    // Window::poll_events short-circuits on no_polling before it ever reaches
    // children, so Accept/Reject buttons placed there would never receive a
    // click on ANY platform. game_ui_window is already correctly polled (it's
    // where the Craft/Inventory buttons live) and renders to a same-sized
    // renderer (game_ui_renderer.set_dimensions mirrors the main renderer),
    // so the same x/y placement works unchanged.
    game_ui_window.add_child(
        [](){
            // Squad invite: same disconnect-banner look (fill/round_radius),
            // but with a message plus Accept (DeathScreen's green Continue
            // style) / Reject (DeathScreen's gray Close style) buttons
            // underneath, and its own 5s auto-dismiss window.
            Ui::Element *accept_btn = new Ui::Button(120, 36, new Ui::StaticText(18, "Accept"),
                [](Ui::Element *, uint8_t ev) {
                    if (ev != Ui::kClick) return;
                    Game::send_squad_accept();
                    Game::squad_invite_until = 0;
                }, nullptr,
                { .fill = 0xff1dd129, .line_width = 5, .round_radius = 3 }
            );
            Ui::Element *reject_btn = new Ui::Button(120, 36, new Ui::StaticText(18, "Reject"),
                [](Ui::Element *, uint8_t ev) {
                    if (ev != Ui::kClick) return;
                    Game::send_squad_reject();
                    Game::squad_invite_until = 0;
                }, nullptr,
                { .fill = 0xff888888, .line_width = 5, .round_radius = 3 }
            );
            Ui::Element *elt = new Ui::VContainer({
                new Ui::DynamicText(16, [](){ return Game::squad_invite_from + " is inviting you to join their squad!"; }),
                new Ui::HContainer({ accept_btn, reject_btn }, 0, 10, {})
            }, 8, 8, {
                .fill = 0x40000000,
                .round_radius = 5,
                .should_render = [](){
                    return Game::timestamp < Game::squad_invite_until && !Game::squad_invite_from.empty();
                },
                .v_justify = Ui::Style::Top
            });
            elt->y = 130;
            return elt;
        }()
    );
    other_ui_window.style.no_polling = 1;
    socket.connect(WS_URL);
}

void Game::reset() {
    simulation_ready = 0;
    on_game_screen = 0;
    death_ui_dismissed = 0;
    g_player_was_dead_corpse = 0;
    leaving = 0;
    score = 0;
    overlevel_timer = 0;
    slot_indicator_opacity = 0;
    transition_circle = 0;
    respawn_level = 1;
    loadout_count = 5;
    camera_id = player_id = NULL_ENTITY;
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
        cached_loadout[i] = PetalID::kNone;
    simulation.reset();
}

uint8_t Game::alive() {
    if (!(socket.ready && simulation_ready && simulation.ent_exists(camera_id))) return 0;
    EntityID pid = simulation.get_ent(camera_id).get_player();
    if (!simulation.ent_alive(pid)) return 0;
    Entity const &player = simulation.get_ent(pid);
    if (player.has_component(kFlower) && player.get_dead()) return 0;
    return 1;
}

uint8_t Game::player_is_dead_corpse() {
    if (!(simulation_ready && simulation.ent_exists(camera_id))) return 0;
    EntityID pid = simulation.get_ent(camera_id).get_player();
    if (!simulation.ent_alive(pid)) return 0;
    Entity const &player = simulation.get_ent(pid);
    return player.has_component(kFlower) && player.get_dead();
}

uint8_t Game::in_game() {
    return simulation_ready && on_game_screen
    && simulation.ent_exists(camera_id);
}

uint8_t Game::should_render_title_ui() {
    return transition_circle < MAX_TRANSITION_CIRCLE;
}

uint8_t Game::should_render_game_ui() {
    return transition_circle > 0 && simulation_ready && simulation.ent_exists(camera_id);
}

void Game::poll_ui_event(Ui::ScreenEvent const &event) {
    Ui::focused = nullptr;
    if (Game::should_render_title_ui())
        title_ui_window.poll_events(event);
    if (Game::should_render_game_ui())
        game_ui_window.poll_events(event);
    other_ui_window.poll_events(event);
    if (Ui::focused != nullptr)
        Ui::focused->focused = 1;
}

void Game::tick(double time) {
    double tick_start = Debug::get_timestamp();
    Game::timestamp = time;
    Ui::dt = time - g_last_time;
    Ui::lerp_amount = 1 - pow(1 - 0.2, Ui::dt * 60 / 1000);
    g_last_time = time;
    simulation.tick();

    uint8_t const dead_corpse = player_is_dead_corpse();
    if (dead_corpse != g_player_was_dead_corpse)
        death_ui_dismissed = 0;
    g_player_was_dead_corpse = dead_corpse;
    
    renderer.reset();
    game_ui_renderer.set_dimensions(renderer.width, renderer.height);
    game_ui_renderer.reset();

    Ui::window_width = renderer.width;
    Ui::window_height = renderer.height;
    Ui::focused = nullptr;
    double a = Ui::window_width / 1920;
    double b = Ui::window_height / 1080;
    Ui::scale = std::max(a, b);
    if (alive() && !leaving) {
        on_game_screen = 1;
        player_id = simulation.get_ent(camera_id).get_player();
        Entity const &player = simulation.get_ent(player_id);
        Game::loadout_count = player.get_loadout_count();
        for (uint32_t i = 0; i < 2 * Game::loadout_count; ++i) {
            cached_loadout[i] = player.get_loadout_ids(i);
            Game::seen_petals[cached_loadout[i]] = 1;
        }
        score = player.get_score();
        mobs_killed = player.get_mobs_killed();
        petals_collected = player.get_petals_collected();
        overlevel_timer = player.get_overlevel_timer();
    } else {
        player_id = NULL_ENTITY;
        overlevel_timer = 0;
    }

    //event poll
    if (Input::is_mobile) {
        for (auto &x : Input::touches) {
            Input::Touch const &touch = x.second;
            if (touch.saturated) continue;
            Game::poll_ui_event({ .id = touch.id, .x = touch.x, .y = touch.y, .press = 1 });
        }
        // Bridge the inventory-drag touch into the mouse fields the shared drag
        // logic reads (Window preview, InventoryStackSlot release, drop-target).
        // While the finger is down, track its position; when it lifts, fire a
        // single left-release at the last position so the petal is equipped.
        BitMath::unset(Input::mouse_buttons_released, Input::LeftMouse);
        if (Ui::dragging_inventory_index != -1 && Ui::drag_touch_id != (uint32_t)-1) {
            auto it = Input::touches.find(Ui::drag_touch_id);
            if (it != Input::touches.end()) {
                Input::mouse_x = it->second.x;
                Input::mouse_y = it->second.y;
            } else {
                BitMath::set(Input::mouse_buttons_released, Input::LeftMouse);
                Ui::drag_touch_id = (uint32_t)-1;
            }
        }
    }
    else {
        Game::poll_ui_event({ .id = 0, .x = Input::mouse_x, .y = Input::mouse_y, .press = 0 });
    }

    if (in_game())
        transition_circle = fclamp(transition_circle * powf(1.05, Ui::dt * 60 / 1000) + Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);
    else 
        transition_circle = fclamp(transition_circle / powf(1.05, Ui::dt * 60 / 1000) - Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);

    if (should_render_title_ui()) {
        render_title_screen();
        Particle::tick_title(renderer, Ui::dt);
        title_ui_window.render(renderer);
    } else
        title_ui_window.on_render_skip(renderer);

    if (should_render_game_ui()) {
        RenderContext c(&renderer);
        if (should_render_title_ui()) {
            renderer.set_stroke(0xff222222);
            renderer.set_line_width(Ui::scale * 10);
            renderer.begin_path();
            renderer.arc(renderer.width / 2, renderer.height / 2, transition_circle);
            renderer.stroke();
            renderer.clip();
        }
        render_game();
        if (!Game::alive() && (!Game::death_ui_dismissed || !Game::player_is_dead_corpse())) {
            RenderContext c(&renderer);
            renderer.reset_transform();
            renderer.set_fill(0x20000000);
            renderer.fill_rect(0,0,renderer.width,renderer.height);
        }
        game_ui_window.render(game_ui_renderer);
        // Composite the in-game UI fully opaque (was 0.85) so panels -- the
        // inventory in particular -- don't show the game world through them.
        renderer.set_global_alpha(1.0);
        renderer.translate(renderer.width/2,renderer.height/2);
        renderer.draw_image(game_ui_renderer);
        //process keybind petal switches: R swaps every main/secondary pair at
        // once; each number key SLOT_KEYBINDS[i] directly swaps just that one
        // main/secondary pair (no more Q/E navigate-then-swap).
        if (Input::keys_pressed_this_tick.contains('R'))
            Game::swap_all_petals();
        else {
            for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                if (Input::keys_pressed_this_tick.contains(SLOT_KEYBINDS[i])) {
                    Ui::ui_swap_petals(i, i + Game::loadout_count);
                    break;
                }
            }
        }
    } else {
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        game_ui_window.on_render_skip(game_ui_renderer);
    }
        
    if (Input::keys_held_this_tick.contains('M'))
        Ui::minimap_expanded = !Ui::minimap_expanded;
    // Hold G to show hitboxes; release to hide. On mobile the on-screen G button
    // toggles Game::show_hitboxes directly, so don't clobber it here.
    if (!Input::is_mobile)
        Game::show_hitboxes = Input::keys_held.contains('G');

    if (Game::timestamp - Ui::UiLoadout::last_key_select > 5000)
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
    slot_indicator_opacity = lerp(slot_indicator_opacity, Ui::UiLoadout::selected_with_keys != MAX_SLOT_COUNT, Ui::lerp_amount);

    other_ui_window.render(renderer);

    //no rendering past this point
    if (!Input::is_mobile) {
        if (Input::keyboard_movement) {
            Input::game_inputs.x = 300 * (Input::keys_held.contains('D') - Input::keys_held.contains('A') + Input::keys_held.contains(39) - Input::keys_held.contains(37));
            Input::game_inputs.y = 300 * (Input::keys_held.contains('S') - Input::keys_held.contains('W') + Input::keys_held.contains(40) - Input::keys_held.contains(38));
        } else {
           // Aim relative to the player's on-screen position, which drifts from
           // centre when the camera is clamped at a map edge.
           float px_screen = renderer.width / 2;
           float py_screen = renderer.height / 2;
           if (alive() && simulation.ent_exists(camera_id)) {
               Entity const &player = simulation.get_ent(player_id);
               float view_scale = Ui::scale * simulation.get_ent(camera_id).get_fov();
               px_screen += (player.get_x() - Game::view_cam_x) * view_scale;
               py_screen += (player.get_y() - Game::view_cam_y) * view_scale;
           }
           Input::game_inputs.x = (Input::mouse_x - px_screen) / Ui::scale;
           Input::game_inputs.y = (Input::mouse_y - py_screen) / Ui::scale;
        }
        uint8_t attack = Input::keys_held.contains(' ') || BitMath::at(Input::mouse_buttons_state, Input::LeftMouse);
        uint8_t defend = Input::keys_held.contains('\x10') || BitMath::at(Input::mouse_buttons_state, Input::RightMouse);
        // Invert toggles: the flower attacks/defends by default and holding the
        // key/button CANCELS it (the opposite of the normal hold-to-act).
        if (Input::invert_attack) attack = !attack;
        if (Input::invert_defend) defend = !defend;
        Input::game_inputs.flags = (attack << InputFlags::kAttacking) | (defend << InputFlags::kDefending);
    }

    // Close any in-game panel on death so a stale panel_open doesn't keep
    // movement frozen after respawn (the panel itself is alive-gated).
    if (!alive() && (Ui::panel_open == Ui::Panel::kInventory || Ui::panel_open == Ui::Panel::kCraft))
        Ui::panel_open = Ui::Panel::kNone;
    // Inventory being open freezes movement: the player holds still and
    // touches drive the panel instead of the joystick. Craft is exempt --
    // unlike inventory's drag-and-drop, crafting doesn't need the player to
    // stand still, so movement stays live while it's open.
    if (Ui::panel_open != Ui::Panel::kNone && Ui::panel_open != Ui::Panel::kCraft) {
        Input::game_inputs.x = 0;
        Input::game_inputs.y = 0;
    }

    if (socket.ready && alive()) send_inputs();

    if (Input::keys_held_this_tick.contains(';'))
        show_debug = !show_debug;
    // Enter handling, in priority order: send chat if the box has text ->
    // otherwise, in-game & not already typing, focus the chat box (Enter-to-open)
    // -> otherwise, on the title screen, spawn. A dead corpse must NOT
    // auto-continue on Enter -- the DeathScreen's explicit Close button is the
    // only way to leave, so a player doesn't accidentally dismiss it.
    if (!Ui::chat_try_send() && Input::keys_held_this_tick.contains('\r')) {
        if ((Game::alive() || Game::player_is_dead_corpse()) && !Ui::is_typing_dom())
            Ui::chat_focus();
        else if (!Game::alive() && !Game::player_is_dead_corpse())
            Game::spawn_in();
    }

    //clearing operations
    simulation.post_tick();
    Storage::set();
    Input::reset();
    Debug::frame_times.push_back(Ui::dt);
    Debug::tick_times.push_back(Debug::get_timestamp() - tick_start);
}