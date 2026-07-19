#pragma once

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

#include <cstdint>
#include <functional>
#include <vector>

class Simulation;
class Entity;

static const uint32_t GRID_SIZE = 100 * 2;
// Largest radius any entity can reach (a Unique wild mob: base radius ~60 x
// mob_size_mult(8) = 1.4^8 ~= 15 -> ~900). The Uniform hash query pads its cell
// scan by this so a big mob whose centre cell is off-view but whose body
// overlaps the view is still visited (and then precisely AABB-culled).
static const uint32_t MAX_ENTITY_QUERY_RADIUS = 1000;
static const uint32_t MAX_GRID_X = div_round_up(ARENA_WIDTH, GRID_SIZE);
static const uint32_t MAX_GRID_Y = div_round_up(ARENA_HEIGHT, GRID_SIZE);

class SpatialHash {
    Simulation *simulation;
    std::vector<EntityID> cells[MAX_GRID_X][MAX_GRID_Y];
    uint32_t width;
    uint32_t height;
public:
    SpatialHash(Simulation *);
    void refresh(uint32_t, uint32_t);
    void insert(Entity const &);
    void collide(std::function<void(Simulation *, Entity &, Entity &)>);
    void query(float, float, float, float, std::function<void(Simulation *, Entity &)>);
};