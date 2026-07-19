#include <Shared/Simulation.hh>

#include <Server/Process.hh>
#include <Server/Client.hh>
#include <Server/EntityFunctions.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>
#include <Server/SpatialHash.hh>

#include <Shared/Map.hh>

#include <algorithm>
#include <vector>

static void calculate_leaderboard(Simulation *sim) {
    std::vector<Entity const *> players;
    sim->for_each<kCamera>([&](Simulation *sim, Entity &ent) { 
        if (sim->ent_alive(ent.get_player())) players.push_back(&sim->get_ent(ent.get_player()));
    });
    std::stable_sort(players.begin(), players.end(), [](Entity const *a, Entity const *b){
        return a->get_score() > b->get_score();
    });
    uint32_t num = players.size();
    sim->arena_info.set_player_count(num);
    num = std::min(num, LEADERBOARD_SIZE);
    for (uint32_t i = 0; i < num; ++i) {
        sim->arena_info.set_names(i, players[i]->get_name());
        sim->arena_info.set_scores(i, players[i]->get_score());
        sim->arena_info.set_colors(i, players[i]->get_color());
    }
}

void Simulation::on_tick() {
    spatial_hash.refresh(ARENA_WIDTH, ARENA_HEIGHT);
    // Roughly once a second, top the wild-mob population back up toward
    // MOB_TARGET. Without a cap this burst ran unconditionally and the
    // population drifted up toward ENTITY_CAP, making every tick progressively
    // more expensive (the "server froze for a while" reports). Counting mobs is
    // O(n) but only happens on this ~1/sec branch.
    if (frand() < 1.0f / TPS) {
        uint32_t mob_count = 0;
        for_each<kMob>([&](Simulation *, Entity &){ ++mob_count; });
        for (uint32_t i = 0; i < 10 && mob_count < MOB_TARGET; ++i) {
            Vector v;
            if (Map::find_spawn_location(this, 500, v)) {
                Map::spawn_random_mob(this, v.x, v.y);
                ++mob_count;
            }
        }
    }
    for_each_entity([](Simulation *sim, Entity &ent) {
        if (ent.has_component(kPhysics))
            sim->spatial_hash.insert(ent);
        if (BitMath::at(ent.flags, EntityFlags::kHasCulling))
            BitMath::set(ent.flags, EntityFlags::kIsCulled);
    });
    for_each<kCamera>(tick_culling_behavior);
    for_each<kFlower>(tick_player_behavior);
    for_each<kMob>(tick_ai_behavior);
    for_each<kCamera>(tick_player_ai_behavior);
    for_each<kPetal>(tick_petal_behavior);
    for_each<kHealth>(tick_health_behavior);
    spatial_hash.collide(on_collide);
    tick_curse_behavior(this);
    for_each<kPhysics>(tick_entity_motion);
    for_each<kSegmented>(tick_segment_behavior);
    for_each<kCamera>(tick_camera_behavior);
    for_each<kScore>(tick_score_behavior);
    for_each_entity(entity_clear_references);
    calculate_leaderboard(this);
}

void Simulation::post_tick() {
    arena_info.reset_protocol();
    for_each_entity([](Simulation *sim, Entity &ent) {
        //no deletions mid tick
        ent.reset_protocol();
        ++ent.lifetime;
        if (BitMath::at(ent.flags, EntityFlags::kIsDespawning)) {
            if (ent.despawn_tick == 0) sim->request_delete(ent.id);
            else --ent.despawn_tick;
        }
        if (ent.immunity_ticks > 0) --ent.immunity_ticks;
    });
    for_each_entity([](Simulation *sim, Entity &ent) {
        if (!ent.pending_delete) return;
        if (!ent.has_component(kPhysics)) 
            return sim->_delete_ent(ent.id);
        if (ent.deletion_tick >= TPS / 5) 
            return sim->_delete_ent(ent.id);
        if (ent.deletion_tick == 0)
            entity_on_death(sim, ent);
        ++ent.deletion_tick;
    });
}
