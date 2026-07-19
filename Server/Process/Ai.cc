#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/RarityScale.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>

static void _focus_lose_clause(Entity &ent, Vector const &v) {
    if (v.magnitude() > 1.5 * ent.detection_radius) ent.target = NULL_ENTITY;
}

static void default_tick_idle(Simulation *sim, Entity &ent) {
    if (ent.ai_tick >= 1 * TPS) {
        ent.ai_tick = 0;
        ent.set_angle(frand() * 2 * M_PI);
        ent.ai_state = AIState::kIdleMoving;
    }
}

static void default_tick_idle_moving(Simulation *sim, Entity &ent) {
    if (ent.ai_tick > 2.5 * TPS) {
        ent.ai_tick = 0;
        ent.ai_state = AIState::kIdle;
        return;
    }
    if (ent.ai_tick < 0.5 * TPS) return;
    float r = (ent.ai_tick - 0.5 * TPS) / (2 * TPS);
    ent.acceleration
        .unit_normal(ent.get_angle())
        .set_magnitude(2 * PLAYER_ACCELERATION * (r - r * r));
}

static void default_tick_returning(Simulation *sim, Entity &ent, float speed = 1.0) {
    if (!sim->ent_alive(ent.get_parent())) {
        ent.ai_tick = 0;
        ent.ai_state = AIState::kIdle;
        return;
    }
    Entity &parent = sim->get_ent(ent.get_parent());
    Vector delta(parent.get_x() - ent.get_x(), parent.get_y() - ent.get_y());
    if (delta.magnitude() > 300) {
        ent.ai_tick = 0;
    } else if (ent.ai_tick > 2 * TPS || delta.magnitude() < 100) {
        ent.ai_tick = 0;
        ent.ai_state = AIState::kIdle;
        return;
    } 
    delta.set_magnitude(PLAYER_ACCELERATION * speed);
    ent.acceleration = delta;
    ent.set_angle(delta.angle());
}


static void tick_default_passive(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            default_tick_idle(sim, ent);
            break;
        }
        case AIState::kIdleMoving: {
            default_tick_idle_moving(sim, ent);
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent);
            break;
        }
        default:
            ent.ai_state = AIState::kIdle;
            break;
    }
}

static void tick_default_neutral(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
        v.set_magnitude(PLAYER_ACCELERATION * 0.975);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.target = NULL_ENTITY;
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        tick_default_passive(sim, ent);
    }
}

static void tick_default_aggro(Simulation *sim, Entity &ent, float speed) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
        _focus_lose_clause(ent, v);
        v.set_magnitude(PLAYER_ACCELERATION * speed);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        //if (ent.ai_state != AIState::kReturning) 
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.get_radius());
        tick_default_passive(sim, ent);
    }
}

static void tick_bee_passive(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            if (ent.ai_tick >= 5 * TPS) {
                ent.ai_tick = 0;
                ent.set_angle(frand() * 2 * M_PI);
                ent.ai_state = AIState::kIdle;
            }
            ent.set_angle(ent.get_angle() + 1.5 * sinf(((float) ent.lifetime) / (TPS / 2)) / TPS);
            Vector v(cosf(ent.get_angle()), sinf(ent.get_angle()));
            v *= 1.5;
            if (ent.lifetime % (TPS * 3 / 2) < TPS / 2)
                v *= 0.5;
            ent.acceleration = v;
            break;
        }
        case AIState::kIdleMoving: {
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent);
            break;
        }
        default:
            ent.ai_state = AIState::kIdle;
            break;
    }
}

static void tick_hornet_aggro(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
        _focus_lose_clause(ent, v);
        float dist = v.magnitude();
        if (dist > 300) {
            v.set_magnitude(PLAYER_ACCELERATION * 0.975);
            ent.acceleration = v;
        } else {
            ent.acceleration.set(0,0);
        }
        ent.set_angle(v.angle());
        if (ent.ai_tick >= 1.5 * TPS && dist < 800) {
            ent.ai_tick = 0;

            Entity &missile = alloc_petal(sim, PetalID::kMissile, ent, ent.get_mob_rarity());
            // Scale the missile with the hornet's own rarity size, so a bigger
            // hornet fires a proportionally bigger missile.
            missile.set_radius(missile.get_radius() * mob_size_mult(ent.get_mob_rarity()));
            float missile_damage = MOB_DATA[ent.get_mob_id()].attributes.missile_damage
                * mob_body_damage_mult(ent.get_mob_rarity());
            missile.damage = missile_damage;
            missile.health = missile.max_health = missile_damage;
            missile.friction = DEFAULT_FRICTION;

            entity_set_despawn_tick(missile, 3 * TPS);
            missile.set_angle(ent.get_angle());
            missile.velocity.unit_normal(ent.get_angle()).set_magnitude(15 * PLAYER_ACCELERATION);
            Vector kb;
            kb.unit_normal(ent.get_angle() - M_PI).set_magnitude(2.5 * PLAYER_ACCELERATION);
            ent.velocity += kb;            
        }
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
            ent.target = NULL_ENTITY;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius);
        tick_bee_passive(sim, ent);;
    }
}

static void tick_bumblebee(Simulation *sim, Entity &ent) {
    // Never aggros: drop any target the generic retaliate-when-hit logic set, so
    // it just keeps wandering and the arena-wall reflection below always applies.
    ent.target = NULL_ENTITY;
    // Fast, curvy drift: weave the heading with a sine and thrust forward.
    ent.set_angle(ent.get_angle() + 2.2f * sinf((float) ent.lifetime / (TPS * 0.6f)) / TPS);
    // Every ~3s pick a fresh random heading too ("rotate randomly").
    if (ent.ai_tick >= (uint32_t)(3 * TPS)) {
        ent.ai_tick = 0;
        ent.set_angle(frand() * 2 * M_PI);
    }
    Vector v;
    v.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION * 0.9f);
    ent.acceleration = v;
    // Land a Pollen particle on the ground every 0.5s.
    if (ent.lifetime % (uint32_t)(TPS / 2) == 0) {
        Entity &pollen = alloc_mob(sim, MobID::kPollen, ent.get_x(), ent.get_y(),
            ent.get_team(), ent.get_mob_rarity());
        entity_set_despawn_tick(pollen, 5 * TPS);
    }
}

static void tick_dandelion(Simulation *sim, Entity &ent) {
    // Rooted (stationary). When its body is struck, fire a ring of destroyable
    // seed missiles outward -- rate-limited so a flurry of hits is one volley.
    // Each missile has its own small round hitbox + HP, so it can be shot down in
    // flight before it reaches a flower.
    // Damage lands AFTER this AI pass each tick (collision runs later in on_tick),
    // so a hit in tick T is only visible at T+1 -- test "damaged in the last
    // couple of ticks", not "== lifetime" (which never matches here). mob_damage
    // stays empty until something has actually hit it.
    bool const struck = !ent.mob_damage.empty() && (ent.lifetime - ent.last_damage_tick) <= 2;
    if (struck && ent.ai_tick >= (uint32_t)(0.6f * TPS)) {
        ent.ai_tick = 0;
        uint8_t const rarity = ent.get_mob_rarity();
        float const dmg = MOB_DATA[ent.get_mob_id()].attributes.missile_damage
            * mob_body_damage_mult(rarity);
        int const N = 10;
        for (int i = 0; i < N; ++i) {
            float const a = 2 * M_PI * i / N + ent.get_angle();
            Entity &m = alloc_petal(sim, PetalID::kMissile, ent, rarity);
            m.set_radius(m.get_radius() * mob_size_mult(rarity));
            m.damage = dmg;
            m.health = m.max_health = 2.5f * mob_hp_mult(rarity);
            m.set_health_ratio(1);
            m.friction = DEFAULT_FRICTION;
            m.set_angle(a);
            entity_set_despawn_tick(m, 3 * TPS);
            m.velocity.unit_normal(a).set_magnitude(12 * PLAYER_ACCELERATION);
        }
    }
}

static void tick_centipede_passive(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            ent.set_angle(ent.get_angle() + 0.25 / TPS);
            if (frand() < 1 / (5.0 * TPS)) ent.ai_state = AIState::kIdleMoving;
            break;
        }
        case AIState::kIdleMoving: {
            ent.set_angle(ent.get_angle() - 0.25 / TPS);
            if (frand() < 1 / (5.0 * TPS)) ent.ai_state = AIState::kIdle;
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent);
            break;
        }
    }
    ent.acceleration.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION / 10);
}

static void tick_centipede_neutral(Simulation *sim, Entity &ent, float speed) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
        v.set_magnitude(PLAYER_ACCELERATION * speed);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        switch(ent.ai_state) {
            case AIState::kIdle: {
                ent.set_angle(ent.get_angle() + 0.25 / TPS);
                if (frand() < 1 / (5.0 * TPS)) ent.ai_state = AIState::kIdleMoving;
                ent.acceleration.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION * speed);
                break;
            }
            case AIState::kIdleMoving: {
                ent.set_angle(ent.get_angle() - 0.25 / TPS);
                if (frand() < 1 / (5.0 * TPS)) ent.ai_state = AIState::kIdle;
                ent.acceleration.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION * speed);
                break;
            }
            case AIState::kReturning: {
                default_tick_returning(sim, ent);
                break;
            }
        }
    }
}

static void tick_centipede_aggro(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
        _focus_lose_clause(ent, v);
        v.set_magnitude(PLAYER_ACCELERATION * 0.95);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.get_radius());
        switch(ent.ai_state) {
            case AIState::kIdle: {
                ent.set_angle(ent.get_angle() + 0.25 / TPS);
                if (frand() < 1 / (5.0 * TPS)) ent.ai_state = AIState::kIdleMoving;
                ent.acceleration.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION / 10);
                break;
            }
            case AIState::kIdleMoving: {
                ent.set_angle(ent.get_angle() - 0.25 / TPS);
                if (frand() < 1 / (5.0 * TPS)) ent.ai_state = AIState::kIdle;
                ent.acceleration.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION / 10);
                break;
            }
            case AIState::kReturning: {
                default_tick_returning(sim, ent);
                break;
            }
        }
    }
}

static void tick_sandstorm(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            if (frand() > 1.0f / TPS) {
                ent.ai_tick = 0;
                ent.heading_angle = frand() * 2 * M_PI;
                ent.ai_state = AIState::kIdleMoving;
            }
            Vector rand = Vector::rand(PLAYER_ACCELERATION * 0.5);
            ent.acceleration.set(rand.x, rand.y);
            break;
        }
        case AIState::kIdleMoving: {
            if (ent.ai_tick >= 2.5 * TPS) {
                ent.ai_tick = 0;
                ent.ai_state = AIState::kIdle;
            }
            if (frand() > 2.5f / TPS)
                ent.heading_angle += frand() * M_PI - M_PI / 2;
            Vector head;
            head.unit_normal(ent.heading_angle);
            head.set_magnitude(PLAYER_ACCELERATION);
            Vector rand;
            rand.unit_normal(ent.heading_angle + frand() * M_PI - M_PI / 2);
            rand.set_magnitude(PLAYER_ACCELERATION * 0.5);
            head += rand;
            ent.acceleration.set(head.x, head.y);
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent, 1.5);
            break;
        }
        default:
            ent.ai_state = AIState::kIdle;
            break;
    }
    if (sim->ent_alive(ent.get_parent())) {
        Entity &parent = sim->get_ent(ent.get_parent());
        ent.acceleration = (ent.acceleration + parent.acceleration) * 0.75;
    }
}

static void tick_digger(Simulation *sim, Entity &ent) {
    ent.input = 0;
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
        _focus_lose_clause(ent, v);
        v.set_magnitude(PLAYER_ACCELERATION * 0.95);
        if (ent.health / ent.max_health > 0.1) {
            BitMath::set(ent.input, InputFlags::kAttacking);
        } else {
            BitMath::set(ent.input, InputFlags::kDefending);
            v *= -1;
        }
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
            ent.target = NULL_ENTITY;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.get_radius());
        switch(ent.ai_state) {
            case AIState::kIdle: {
                ent.set_angle(frand() * M_PI * 2);
                ent.ai_state = AIState::kIdleMoving;
                ent.ai_tick = 0;
                break;
            }
            case AIState::kIdleMoving: {
                if (ent.ai_tick > 5 * TPS)
                    ent.ai_state = AIState::kIdle;
                ent.acceleration.unit_normal(ent.get_angle()).set_magnitude(PLAYER_ACCELERATION);
                break;
            }
            case AIState::kReturning: {
                default_tick_returning(sim, ent);
                break;
            }
        }
    }
}

void tick_ai_behavior(Simulation *sim, Entity &ent) {
    if (ent.pending_delete) return;
    if (sim->ent_alive(ent.seg_head)) return;
    // Loot damage tally decays: if a mob hasn't been damaged for 120s, drop all
    // accumulated per-player damage so stale contributors don't claim its loot.
    if (!ent.mob_damage.empty() && ent.lifetime - ent.last_damage_tick > 120 * TPS)
        ent.mob_damage.clear();
    ent.acceleration.set(0,0);
    if (!sim->ent_alive(ent.target) && sim->ent_alive(ent.last_damaged_by))
        ent.target = ent.last_damaged_by;
    if (!(ent.get_parent() == NULL_ENTITY)) {
        if (!sim->ent_alive(ent.get_parent())) {
            if (BitMath::at(ent.flags, EntityFlags::kDieOnParentDeath))
                sim->request_delete(ent.id);
            ent.set_parent(NULL_ENTITY);
        } else {
            Entity const &parent = sim->get_ent(ent.get_parent());
            Vector delta(parent.get_x() - ent.get_x(), parent.get_y() - ent.get_y());
            if (delta.magnitude() > SUMMON_RETREAT_RADIUS) {
                ent.target = NULL_ENTITY;
                ent.ai_state = AIState::kReturning;
            }
            if (sim->ent_alive(ent.target)) {
                Entity const &target = sim->get_ent(ent.target);
                delta = Vector(parent.get_x() - target.get_x(), parent.get_y() - target.get_y());
                if (delta.magnitude() > SUMMON_RETREAT_RADIUS)
                    ent.target = NULL_ENTITY;
            }
        }
    }
    if (BitMath::at(ent.flags, EntityFlags::kIsCulled)) {
        ent.target = NULL_ENTITY;
        ent.ai_tick = 0;
        return;
    }
    switch(ent.get_mob_id()) {
        case MobID::kBabyAnt:            
        case MobID::kLadybug:
            tick_default_passive(sim, ent);
            break;
        case MobID::kBee:
            tick_bee_passive(sim, ent);
            break;
        case MobID::kCentipede:
            tick_centipede_passive(sim, ent);
            break;
        case MobID::kEvilCentipede:
            tick_centipede_aggro(sim, ent);
            break;
        case MobID::kDesertCentipede:
            tick_centipede_neutral(sim, ent, 1.33);
            break;
        case MobID::kWorkerAnt:
        case MobID::kDarkLadybug:
        case MobID::kShinyLadybug:
            tick_default_neutral(sim, ent);
            break;
        case MobID::kSoldierAnt:
        case MobID::kBeetle:
            tick_default_aggro(sim, ent, 0.95);
            break;
        case MobID::kScorpion:
            tick_default_aggro(sim, ent, 1.20);
            break;
        case MobID::kSpider:
            // Web grows with the spider: same 1.4x/tier size scaling the body
            // gets, so a big (high-rarity) spider lays a proportionally big web.
            if (ent.lifetime % (TPS) == 0)
                alloc_web(sim, 25 * mob_size_mult(ent.get_mob_rarity()), ent);
            tick_default_aggro(sim, ent, 1.20);
            break;
        case MobID::kQueenAnt:
            if (ent.lifetime % (2 * TPS) == 0) {
                Vector behind;
                behind.unit_normal(ent.get_angle() + M_PI);
                behind *= ent.get_radius();
                // Soldiers spawn exactly one rarity tier below the queen (floored
                // at Common), instead of matching her rarity.
                uint8_t const qr = ent.get_mob_rarity();
                uint8_t const soldier_rarity = qr > RarityID::kCommon ? (uint8_t)(qr - 1) : (uint8_t)RarityID::kCommon;
                Entity &spawned = alloc_mob(
                    sim, MobID::kSoldierAnt, ent.get_x() + behind.x, ent.get_y() + behind.y,
                    ent.get_team(), soldier_rarity, [](Entity &mob) {
                    BitMath::set(mob.flags, EntityFlags::kHasCulling);
                });
                entity_set_despawn_tick(spawned, 10 * TPS);
                spawned.set_parent(ent.get_parent());
            }
            tick_default_aggro(sim, ent, 0.95);
            break;
        case MobID::kHornet:
            tick_hornet_aggro(sim, ent);
            break;
        case MobID::kBumbleBee:
            tick_bumblebee(sim, ent);
            break;
        case MobID::kDandelion:
            tick_dandelion(sim, ent);
            break;
        case MobID::kRock:
        case MobID::kCactus:
        case MobID::kSquare:
        case MobID::kPollen:   // inert: just sits and stings on contact
            break;
        case MobID::kSandstorm:
            tick_sandstorm(sim, ent);
            break;
        case MobID::kDigger:
            tick_digger(sim, ent);
            break;
        case MobID::kMoon: {
            // A heavy moon rock that gently roams around its owner. It picks a
            // new slow wander heading every ~2.5s and ambles back toward the
            // player when it drifts too far; its high mass keeps it from being
            // shoved around (except by the player's own Bubble, see Petal.cc).
            if (sim->ent_alive(ent.get_parent())) {
                Entity const &parent = sim->get_ent(ent.get_parent());
                Vector delta(parent.get_x() - ent.get_x(), parent.get_y() - ent.get_y());
                float const dist = delta.magnitude();
                if (ent.lifetime % (uint32_t)(2.5 * TPS) == 0)
                    ent.heading_angle = frand() * 2 * M_PI;
                if (dist > 240) {
                    delta.set_magnitude(PLAYER_ACCELERATION * 0.6f);
                    ent.acceleration = delta;
                } else {
                    Vector wander;
                    wander.unit_normal(ent.heading_angle).set_magnitude(PLAYER_ACCELERATION * 0.4f);
                    ent.acceleration = wander;
                }
                ent.set_angle(ent.get_angle() + 0.02f);   // slow tumble
            }
            break;
        }
        default:
            break;
    }
    //wall avoidance
    if (!sim->ent_alive(ent.target)) {
        if (ent.get_x() - ent.get_radius() <= 0 && angle_within(ent.get_angle(), M_PI, M_PI / 2))
            ent.set_angle(M_PI - ent.get_angle());
        if (ent.get_x() + ent.get_radius() >= ARENA_WIDTH && angle_within(ent.get_angle(), 0, M_PI / 2))
            ent.set_angle(M_PI - ent.get_angle());
        if (ent.get_y() - ent.get_radius() <= 0 && angle_within(ent.get_angle(), 3 * M_PI / 2, M_PI / 2))
            ent.set_angle(0 - ent.get_angle());
        if (ent.get_y() + ent.get_radius() >= ARENA_HEIGHT && angle_within(ent.get_angle(), M_PI / 2, M_PI / 2))
            ent.set_angle(0 - ent.get_angle());
    }
    ++ent.ai_tick;
}