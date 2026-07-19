#pragma once

#include <Client/Render/Renderer.hh>

#include <Shared/StaticData.hh>

#include <string>

namespace Particle {
    class TitleParticleEntity {
    public:
        float x;
        float y;
        float x_velocity;
        float angle;
        float sin_offset;
        float radius;
        PetalID::T id;
    };

    class GameParticleEntity {
    public:
        float x;
        float y;
        float x_velocity;
        float y_velocity;
        float radius;
        float opacity;
        uint32_t color;
    };

    // Floating damage number: rises and fades at a world position, showing the
    // total damage a creature took recently. White for normal damage, teal for
    // lightning. Consecutive hits on the same target within ~1s accumulate into
    // the SAME number (it climbs) instead of spawning a new one.
    class DamageNumber {
    public:
        float x;
        float y;
        float opacity;
        uint32_t color;
        uint32_t owner_id;   // entity the number belongs to (for stacking)
        double value;        // accumulated damage
        double last_hit;     // timestamp of the most recent hit
    };

    void tick_title(Renderer &, double);
    void tick_game(Renderer &, double);
    void add_game_particle(float, float, uint32_t color);
    void add_damage_number(float x, float y, double amount, uint32_t color, uint32_t owner_id);
}