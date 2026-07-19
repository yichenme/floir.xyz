#include <Client/Particle.hh>

#include <Client/Game.hh>

#include <Client/Assets/Assets.hh>

#include <Helpers/Math.hh>

#include <cmath>
#include <vector>

using namespace Particle;

static std::vector<TitleParticleEntity> title_particles;
static std::vector<GameParticleEntity> game_particles;
static std::vector<DamageNumber> damage_numbers;

void Particle::tick_title(Renderer &ctx, double dt) {
    RenderContext c(&ctx);
    ctx.reset_transform();
    size_t len = title_particles.size();
    for (size_t i = len; i > 0; --i) {
        TitleParticleEntity &part = title_particles[i - 1];
        if (part.x > ctx.width + std::max(10.0f, PETAL_DATA[part.id].radius) * part.radius) {
            part = title_particles[title_particles.size() - 1];
            title_particles.pop_back();
            continue;
        }
        RenderContext c(&ctx);
        part.x += part.x_velocity * (dt / 1000) * Ui::scale;
        part.angle += dt / 1000 * Ui::scale;
        ctx.translate(part.x, part.y + 12.5 * sin(Game::timestamp / 500 + part.sin_offset));
        ctx.scale(Ui::scale * part.radius);
        if (PETAL_DATA[part.id].attributes.rotation_style == PetalAttributes::kPassiveRot)
            ctx.rotate(part.angle);
        if (part.id == PetalID::kPeas || part.id == PetalID::kGrapes)
            draw_static_petal(part.id, ctx);
        else
            draw_static_petal_single(part.id, ctx);
    }
    std::vector<PetalID::T> ids = {PetalID::kBasic};
    float freq_sum = 1;
    for (PetalID::T pot = PetalID::kBasic + 1; pot < PetalID::kNumPetals; ++pot)
        if (Game::seen_petals[pot]) { ids.push_back(pot); freq_sum += pow(0.5, PETAL_DATA[pot].rarity); }

    for (size_t i = 0; i < 4; ++i) {
        if (frand() > 0.02) continue;
        TitleParticleEntity npart;
        npart.x = -100;
        
        float freq_score = freq_sum * frand();
        for (PetalID::T id : ids) {
            freq_score -= pow(0.5, PETAL_DATA[id].rarity);
            if (freq_score > 0) continue;
            npart.id = id;   
            npart.y = frand() * ctx.height;
            npart.angle = frand() * 2 * M_PI;
            npart.x_velocity = frand() * 100 + 100;
            npart.sin_offset = frand() * M_PI;
            npart.radius = frand() + 0.5;
            title_particles.push_back(std::move(npart));
            break;
        }
    }
}


void Particle::tick_game(Renderer &ctx, double dt) {
    size_t len = game_particles.size();
    for (size_t i = len; i > 0; --i) {
        GameParticleEntity &part = game_particles[i - 1];
        if (part.opacity < 0.1) {
            part = game_particles[game_particles.size() - 1];
            game_particles.pop_back();
            continue;
        }
        RenderContext c(&ctx);
        part.x += part.x_velocity * dt / 1000;
        part.y += part.y_velocity * dt / 1000;
        part.opacity = fclamp(part.opacity - dt / 1000, 0, 1);
        ctx.set_global_alpha(part.opacity);
        ctx.set_fill(part.color);
        ctx.begin_path();
        ctx.arc(part.x,part.y,part.radius);
        ctx.fill();
    }
    // Floating damage numbers (world space): rise slowly and fade. A number
    // only starts fading once ~1s has passed since its last hit, so ongoing
    // damage keeps it alive and climbing in place.
    for (size_t i = damage_numbers.size(); i > 0; --i) {
        DamageNumber &d = damage_numbers[i - 1];
        double const age = Game::timestamp - d.last_hit;
        if (age > 1000.0) d.opacity = fclamp(d.opacity - dt / 500.0f, 0, 1);
        if (d.opacity < 0.05f) {
            d = damage_numbers[damage_numbers.size() - 1];
            damage_numbers.pop_back();
            continue;
        }
        d.y -= 18.0f * dt / 1000.0f;   // gentle rise
        RenderContext c(&ctx);
        ctx.set_global_alpha(d.opacity);
        ctx.translate(d.x, d.y);
        ctx.center_text_align();
        ctx.set_text_size(22);
        ctx.set_fill(d.color);
        ctx.set_stroke(0xff222222);
        ctx.set_line_width(22 * 0.14f);
        // Raw integer, no k/m/b abbreviation.
        std::string const text = std::to_string((long long) (d.value + 0.5));
        ctx.stroke_text(text.c_str());
        ctx.fill_text(text.c_str());
    }
}

void Particle::add_damage_number(float x, float y, double amount, uint32_t color, uint32_t owner_id) {
    if (amount < 1) return;
    // Stack onto a recent number for the same target (within ~1s) instead of
    // spawning a separate one -- reads as a single climbing number.
    for (DamageNumber &d : damage_numbers) {
        if (d.owner_id == owner_id && Game::timestamp - d.last_hit < 1000.0) {
            d.value += amount;
            d.last_hit = Game::timestamp;
            d.opacity = 1.0f;
            d.color = color;          // latest hit's colour (lightning wins if last)
            d.x = x; d.y = y;         // follow the target
            return;
        }
    }
    DamageNumber d;
    d.x = x;
    d.y = y;
    d.opacity = 1.0f;
    d.color = color;
    d.owner_id = owner_id;
    d.value = amount;
    d.last_hit = Game::timestamp;
    damage_numbers.push_back(std::move(d));
}

void Particle::add_game_particle(float x, float y, uint32_t color) {
    GameParticleEntity part;
    part.x = x;
    part.y = y;
    part.radius = 4;
    part.opacity = 1;
    part.color = color;
    Vector rand = Vector::rand(50);
    part.x_velocity = rand.x;
    part.y_velocity = rand.y;
    game_particles.push_back(std::move(part));
}