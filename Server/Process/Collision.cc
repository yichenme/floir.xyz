#include <Server/EntityFunctions.hh>

#include <Server/Client.hh>
#include <Server/EntityFunctions/InventoryOps.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>

#include <cmath>
#include <iostream>

static bool _should_interact(Entity const &ent1, Entity const &ent2) {
    //if (ent1.has_component(kFlower) || ent2.has_component(kFlower)) return false;
    //if (ent1.has_component(kPetal) || ent2.has_component(kPetal)) return false;
    if (ent1.pending_delete || ent2.pending_delete) return false;
    // Digger is a kFlower (so it renders/behaves like one) but its hitbox
    // should only apply to other creatures -- players walk right over it.
    // Checked before the flower-flower rule below, which would otherwise
    // always force a Digger/player collision (both have kFlower).
    bool const digger1 = ent1.has_component(kMob) && ent1.get_mob_id() == MobID::kDigger;
    bool const digger2 = ent2.has_component(kMob) && ent2.get_mob_id() == MobID::kDigger;
    bool const real_player1 = ent1.has_component(kFlower) && !ent1.has_component(kMob);
    bool const real_player2 = ent2.has_component(kFlower) && !ent2.has_component(kMob);
    if ((digger1 && real_player2) || (digger2 && real_player1)) return false;
    // Flowers physically push each other (a hitbox), but never deal PvP damage
    // (skipped in on_collide's damage step below).
    if (ent1.has_component(kFlower) && ent2.has_component(kFlower)) return true;
    if (!(ent1.get_team() == ent2.get_team())) {
        // No PvP: two player-owned entities (both non-NULL teams -- flowers,
        // their petals, their summons) never interact. Wild mobs use NULL_ENTITY,
        // so player-vs-mob still collides and damages normally.
        if (ent1.get_team() != NULL_ENTITY && ent2.get_team() != NULL_ENTITY) return false;
        return true;
    }
    if (BitMath::at((ent1.flags | ent2.flags), EntityFlags::kNoFriendlyCollision)) return false;
    //if (ent1.has_component(kPetal) || ent2.has_component(kPetal)) return false;
    if (ent1.has_component(kMob) && ent2.has_component(kMob)) return true;
    return false;
}

static void _pickup_drop(Simulation *sim, Entity &player, Entity &drop) {
    if (!sim->ent_alive(player.get_parent())) return;
    if (drop.immunity_ticks > 0) return;
    // Individual loot: an owned drop can only be collected by its owner (the
    // camera it was assigned to on the mob's death). owner 0 = shared/legacy.
    uint32_t const owner = drop.get_drop_owner();
    if (owner != 0 && owner != player.get_parent().id) return;
    Client *client = Server::game.client_for_camera(player.get_parent());
    InventoryOps::pickup_drop(sim, client, player, drop);
}

#define NO(component) (!ent1.has_component(component) && !ent2.has_component(component))
#define BOTH(component) (ent1.has_component(component) && ent2.has_component(component))
#define EITHER(component) (ent1.has_component(component) || ent2.has_component(component))

static void _deal_push(Entity &ent, Vector knockback, float mass_ratio, float scale) {
    if (fabsf(mass_ratio) < 0.01) return;
    knockback *= scale * mass_ratio;
    ent.collision_velocity += knockback;
}

// Fraction of the base pushback an entity actually feels, based on whether
// IT is ramming into the thing it hit or just standing there. `toward` is
// the unit direction from this entity to whatever it collided with -- if the
// entity's own velocity is heading that way (it's the one charging in), the
// impact shouldn't bounce it back at all; a stationary (or retreating)
// entity still gets knocked, but only a token amount.
static float const STANDING_PUSH_SCALE = 0.05f;
static float _push_factor(Entity const &ent, Vector const &toward) {
    float const closing = ent.velocity.x * toward.x + ent.velocity.y * toward.y;
    float const ramming_speed = PLAYER_ACCELERATION * 30;
    float const ramming_amount = fclamp(closing / ramming_speed, 0, 1);
    return STANDING_PUSH_SCALE * (1.0f - ramming_amount);
}

static void _deal_knockback(Entity &ent, Vector knockback, float mass_ratio, Vector const &toward) {
    if (fabsf(mass_ratio) < 0.01) return;
    float scale = PLAYER_ACCELERATION * 2 * _push_factor(ent, toward);
    knockback *= scale * mass_ratio;
    ent.collision_velocity += knockback;
    ent.velocity += knockback * 2;
}

static void _cancel_movement(Entity &ent, Vector dir, Vector add, Vector const &toward) {
    Vector push = dir;
    push.normalize();
    float dot = fclamp(push.x * add.x + push.y * add.y, PLAYER_ACCELERATION * 0.5, PLAYER_ACCELERATION * 25);
    float const factor = _push_factor(ent, toward);
    ent.velocity += push * (PLAYER_ACCELERATION + dot * 2) * factor;
    ent.collision_velocity += push * (0.5 * PLAYER_ACCELERATION) * factor;
}

void on_collide(Simulation *sim, Entity &ent1, Entity &ent2) {
    //do a distance dependent check first (it's faster)
    float min_dist = ent1.get_radius() + ent2.get_radius();
    if (fabs(ent1.get_x() - ent2.get_x()) > min_dist || fabs(ent1.get_y() - ent2.get_y()) > min_dist) return;
    //check if collide (distance independent)
    if (!_should_interact(ent1, ent2)) return;
    //finer distance check
    Vector separation(ent1.get_x() - ent2.get_x(), ent1.get_y() - ent2.get_y());
    float dist = min_dist - separation.magnitude();
    if (dist < 0) return;
    // kNoPush bodies (e.g. Stick's Sandstorms) overlay creatures: skip the
    // physical push below, but the damage step still runs.
    if (NO(kDrop) && NO(kWeb) && !BitMath::at((ent1.flags | ent2.flags), EntityFlags::kNoPush)) {
        if (separation.x == 0 && separation.y == 0)
            separation.unit_normal(frand() * 2 * M_PI);
        else
            separation.normalize();
        float ratio = ent2.mass / (ent1.mass + ent2.mass);

        // Bug 15: Heavy's mass-based shove must NOT move Super+ mobs -- treat
        // the Super+ mob as immovable for a Heavy-vs-it pair (Heavy still deals
        // damage; only the push is neutralized).
        bool const heavy1 = ent1.has_component(kPetal) && ent1.get_petal_id() == PetalID::kHeavy;
        bool const heavy2 = ent2.has_component(kPetal) && ent2.get_petal_id() == PetalID::kHeavy;
        bool const super1 = ent1.has_component(kMob) && ent1.get_mob_rarity() >= RarityID::kSuper;
        bool const super2 = ent2.has_component(kMob) && ent2.get_mob_rarity() >= RarityID::kSuper;
        bool const immovable1 = (heavy2 && super1);
        bool const immovable2 = (heavy1 && super2);

        // Bug 14: a Light petal imparts far less knockback to what it hits.
        bool const light_involved =
            (ent1.has_component(kPetal) && ent1.get_petal_id() == PetalID::kLight) ||
            (ent2.has_component(kPetal) && ent2.get_petal_id() == PetalID::kLight);
        float const kb = light_involved ? 0.25f : 1.0f;

        if (!(ent1.get_team() == ent2.get_team())) {
            // toward1/toward2: unit direction from each entity to the OTHER
            // one, so _push_factor can tell a charging rammer (velocity
            // pointing this way) from a stationary victim.
            Vector const toward1 = separation * -1;
            Vector const toward2 = separation;
            // Bug 13: only REAL players (kFlower && !kMob) get the elastic
            // movement-cancel bounce; Digger (a kFlower mob) falls through to
            // the mass-based knockback so it stops behaving like a trampoline.
            if (ent1.has_component(kFlower) && !ent1.has_component(kMob) && !ent2.has_component(kPetal))
                _cancel_movement(ent1, separation, ent2.velocity - ent1.velocity, toward1);
            else if (!immovable1)
                _deal_knockback(ent1, separation, ratio * kb, toward1);
            if (ent2.has_component(kFlower) && !ent2.has_component(kMob) && !ent1.has_component(kPetal))
                _cancel_movement(ent2, separation*-1, ent1.velocity - ent2.velocity, toward2);
            else if (!immovable2)
                _deal_knockback(ent2, separation*-1, (1 - ratio) * kb, toward2);
        }
        if (!immovable1) _deal_push(ent1, separation, ratio, dist);
        if (!immovable2) _deal_push(ent2, separation*-1, 1 - ratio, dist);
    }

    if (BOTH(kHealth) && !(ent1.get_team() == ent2.get_team()) && !BOTH(kFlower)) {
        if (ent1.health > 0 && ent2.health > 0) {
            // Mjolnir deals LIGHTNING damage (ignores armor, teal damage number).
            uint8_t const dt1 = (ent1.has_component(kPetal) && ent1.get_petal_id() == PetalID::kMjolnir)
                ? (uint8_t)DamageType::kLightning : (uint8_t)DamageType::kContact;
            uint8_t const dt2 = (ent2.has_component(kPetal) && ent2.get_petal_id() == PetalID::kMjolnir)
                ? (uint8_t)DamageType::kLightning : (uint8_t)DamageType::kContact;
            inflict_damage(sim, ent1.id, ent2.id, ent1.damage, dt1);
            inflict_damage(sim, ent2.id, ent1.id, ent2.damage, dt2);
        }
        if (ent1.health == 0) sim->request_delete(ent1.id);
        if (ent2.health == 0) sim->request_delete(ent2.id);
    }

    if (ent1.has_component(kDrop) && ent2.has_component(kFlower)) 
        _pickup_drop(sim, ent2, ent1);
    if (ent2.has_component(kDrop) && ent1.has_component(kFlower))
        _pickup_drop(sim, ent1, ent2);

    if (ent1.has_component(kWeb) && !ent2.has_component(kPetal) && !ent2.has_component(kDrop))
        ent2.speed_ratio = 0.5;
    if (ent2.has_component(kWeb) && !ent1.has_component(kPetal) && !ent1.has_component(kDrop))
        ent1.speed_ratio = 0.5;
}