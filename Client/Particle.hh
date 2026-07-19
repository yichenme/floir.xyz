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
    // amount a creature just took. White for normal damage, teal for lightning.
    class DamageNumber {
    public:
        float x;
        float y;
        float vx;
        float opacity;
        uint32_t color;
        std::string text;
    };

    void tick_title(Renderer &, double);
    void tick_game(Renderer &, double);
    void add_game_particle(float, float, uint32_t color);
    void add_damage_number(float x, float y, double amount, uint32_t color);
}