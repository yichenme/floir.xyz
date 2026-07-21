#pragma once

#include <Client/Render/Renderer.hh>
#include <Client/Socket.hh>
#include <Client/Ui/Ui.hh>
#include <Client/StaticData.hh>

#include <Shared/PetalItem.hh>
#include <Shared/Simulation.hh>

#include <array>
#include <vector>

namespace Game {
    extern Simulation simulation;
    extern Renderer renderer;
    extern Renderer game_ui_renderer;
    extern Socket socket;
    extern Ui::Window title_ui_window;
    extern Ui::Window game_ui_window;
    extern Ui::Window other_ui_window;
    extern EntityID camera_id;
    extern EntityID player_id;
    extern std::string nickname;
    extern std::string disconnect_message;
    // Chat: the text being typed, and the recent message log (newest last). A
    // system line carries a colour and no sender label (color != white).
    extern std::string chat_input;
    struct ChatMessage { std::string text; uint32_t color; };
    extern std::vector<ChatMessage> chat_messages;
    void send_chat(std::string const &);
    extern std::array<uint8_t, PetalID::kNumPetals> seen_petals;
    extern std::array<uint8_t, MobID::kNumMobs> seen_mobs;
    // Populated from kInventoryUpdate; consumed by the Task 8 inventory UI.
    extern std::vector<PetalStack> inventory_stacks;
    // Indices into inventory_stacks, sorted by rarity (highest first) so the
    // inventory panel can group by rarity while equip still uses the real index.
    extern std::vector<uint32_t> inventory_display_order;
    // Bumped on every inventory sync; lets a slot stay blank after an equip
    // until the server-confirmed removal actually lands (no card flash).
    extern uint32_t inventory_version;
    // Per-(mob, rarity) kill tally from kKillsUpdate; drives the Mob Gallery
    // (only killed mob/rarity combos are shown, with their counts).
    extern std::array<std::array<uint64_t, RarityID::kNumRarities>, MobID::kNumMobs> mob_kills;
    
    extern double timestamp;
    
    extern double score;
    extern uint32_t mobs_killed;
    extern uint32_t petals_collected;
    extern float overlevel_timer;
    extern float slot_indicator_opacity;
    extern float transition_circle;
    // Camera position actually used for the view (player pos clamped to the map
    // edges so the blank outside never shows). Shared by render + mouse aim.
    extern float view_cam_x;
    extern float view_cam_y;
    extern uint8_t show_hitboxes;   // G toggles petal-hitbox overlay

    extern uint32_t respawn_level;

    // Squad: 0 = not in one. squad_members excludes nobody (includes self);
    // UI code filters self out where that matters (e.g. SquadBar).
    extern uint32_t squad_id;
    struct SquadMember { EntityID camera_id; std::string name; };
    extern std::vector<SquadMember> squad_members;
    // Standalone banner text (join/leave notices), shown the same way as the
    // disconnect-with-code banner -- not routed through the chat log.
    extern std::string squad_notice;
    extern double squad_notice_until;
    // Pending invite awaiting Accept/Reject (empty = none showing). Cleared
    // immediately on either response, or after squad_invite_until elapses
    // (auto-dismiss, same 5s-banner pattern as the disconnect/notice banners).
    extern std::string squad_invite_from;
    extern double squad_invite_until;
    void send_squad_accept();
    void send_squad_reject();
    // Latest craft outcome, for the Craft panel's roll/reveal animation.
    struct CraftResult {
        PetalID::T type = PetalID::kNone;
        uint8_t out_rarity = 0;
        uint32_t crafted = 0;
        uint32_t remaining = 0;
        uint8_t any_success = 0;
        double received_at = 0;
    };
    extern CraftResult last_craft_result;
    void send_craft(PetalID::T, uint8_t rarity, uint32_t amount);

    // Talent ranks (0-9, Common..Unique) and unspent-TP snapshot, pushed by
    // the server on spawn and after every successful buy (Shared/TalentData.hh
    // has the actual rank->percent/cost tables).
    extern uint8_t talent_health_rank;
    extern uint8_t talent_reload_rank;
    extern uint32_t talent_points_total;
    void send_talent_buy(uint8_t tree, uint8_t target_rank);

    extern std::array<PetalID::T, 2 * MAX_SLOT_COUNT> cached_loadout;

    extern uint8_t loadout_count;
    extern uint8_t simulation_ready;
    extern uint8_t on_game_screen;
    extern uint8_t death_ui_dismissed;
    // Set when the player clicks Leave: suppresses the on_game_screen=1 reset so
    // the spawn transition plays in reverse back to the title screen.
    extern uint8_t leaving;
    extern uint8_t show_debug;
    
    void init();
    void reset();
    uint8_t alive();
    uint8_t player_is_dead_corpse();
    uint8_t in_game();
    uint8_t should_render_title_ui();
    uint8_t should_render_game_ui();
    void tick(double);
    void render_game();
    void render_title_screen();
    void send_inputs();
    void spawn_in();
    void leave_game();   // despawn the flower and return to the title screen
    void store_petal(uint8_t);
    void equip_petal(uint32_t, uint8_t);
    void swap_petals(uint8_t, uint8_t);
    void swap_all_petals();
    void on_message(uint8_t *, uint32_t);
    void poll_ui_event(Ui::ScreenEvent const &);
};
