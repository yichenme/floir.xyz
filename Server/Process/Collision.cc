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

// The sole collision response: mass-ratio separation, applied to both
// entities in every colliding pair (same-team bump or enemy hit alike). The
// old "ram knockback" system (an extra impact-knockback bonus that also
// discounted itself when the entity was charging INTO what it hit, plus the
// elastic movement-cancel bounce for players ramming mobs) has been removed
// entirely -- collisions now resolve purely by this physical push.
static void _deal_push(Entity &ent, Vector knockback, float mass_ratio, float scale) {
    if (fabsf(mass_ratio) < 0.01) return;
    knockback *= scale * mass_ratio;
    ent.collision_velocity += knockback;
}

// A real player (kFlower && !kMob) killed by contact damage must become a
// corpse via the dead-state path, not vanish through request_delete -- otherwise
// pending_delete beats Health.cc to it and the death bookkeeping/corpse never
// runs. Everything else (mobs, petals, etc.) dies normally.
static void _kill_from_collision(Simulation *sim, Entity &ent) {
    if (ent.has_component(kFlower) && !ent.has_component(kMob)) {
        if (!ent.get_dead()) enter_player_dead_state(sim, ent);
    } else {
        sim->request_delete(ent.id);
    }
}

// A loaded Yggdrasil petal that touches ANY dead flower (allies or enemies,
// per spec) sticks onto the corpse and brings it back at full HP. Runs before
// _should_interact because that gate refuses petal-vs-flower contact -- the
// revive is a deliberate exception. Deleting the petal restarts the owner's
// Yggdrasil reload naturally (Flower.cc reloads the slot once its petal entity
// is gone).
static bool try_yggdrasil_revive(Simulation *sim, Entity &petal, Entity &dead) {
    if (petal.pending_delete) return false;
    if (!petal.has_component(kPetal) || petal.get_petal_id() != PetalID::kYggdrasil) return false;
    if (!dead.has_component(kFlower) || dead.has_component(kMob) || !dead.get_dead()) return false;
    Vector d(petal.get_x() - dead.get_x(), petal.get_y() - dead.get_y());
    if (d.magnitude() > petal.get_radius() + dead.get_radius()) return false;
    dead.set_dead(0);
    dead.health = dead.max_health;
    dead.set_health_ratio(1);
    dead.set_face_flags(0);
    // Brief grace so a revive amid enemies isn't instantly undone.
    dead.immunity_ticks = 1.0 * TPS;
    sim->request_delete(petal.id);
    return true;
}

void on_collide(Simulation *sim, Entity &ent1, Entity &ent2) {
    //do a distance dependent check first (it's faster)
    float min_dist = ent1.get_radius() + ent2.get_radius();
    if (fabs(ent1.get_x() - ent2.get_x()) > min_dist || fabs(ent1.get_y() - ent2.get_y()) > min_dist) return;
    if (try_yggdrasil_revive(sim, ent1, ent2) || try_yggdrasil_revive(sim, ent2, ent1)) return;
    //check if collide (distance independent)
    if (!_should_interact(ent1, ent2)) return;
    //finer distance check
    Vector separation(ent1.get_x() - ent2.get_x(), ent1.get_y() - ent2.get_y());
    float dist = min_dist - separation.magnitude();
    if (dist < 0) return;
    // A dead player corpse is physically immobile: it neither gets pushed nor
    // pushes others. Compute the dead-flower flags here so the push block below
    // (and the damage block further down) can both skip it.
    bool const dead1 = ent1.has_component(kFlower) && !ent1.has_component(kMob) && ent1.get_dead();
    bool const dead2 = ent2.has_component(kFlower) && !ent2.has_component(kMob) && ent2.get_dead();
    // kNoPush bodies (e.g. Stick's Sandstorms) overlay creatures: skip the
    // physical push below, but the damage step still runs.
    if (NO(kDrop) && NO(kWeb) && !dead1 && !dead2 && !BitMath::at((ent1.flags | ent2.flags), EntityFlags::kNoPush)) {
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

        if (!immovable1) _deal_push(ent1, separation, ratio * kb, dist);
        if (!immovable2) _deal_push(ent2, separation*-1, (1 - ratio) * kb, dist);
    }

    // A dead player corpse neither deals nor takes contact damage (so it can't be
    // farmed and doesn't chip mobs that walk over it).
    if (BOTH(kHealth) && !(ent1.get_team() == ent2.get_team()) && !BOTH(kFlower) && !dead1 && !dead2) {
        if (ent1.health > 0 && ent2.health > 0) {
            // Mjolnir deals LIGHTNING damage (ignores armor, teal damage number).
            uint8_t const dt1 = (ent1.has_component(kPetal) && ent1.get_petal_id() == PetalID::kMjolnir)
                ? (uint8_t)DamageType::kLightning : (uint8_t)DamageType::kContact;
            uint8_t const dt2 = (ent2.has_component(kPetal) && ent2.get_petal_id() == PetalID::kMjolnir)
                ? (uint8_t)DamageType::kLightning : (uint8_t)DamageType::kContact;
            inflict_damage(sim, ent1.id, ent2.id, ent1.damage, dt1);
            inflict_damage(sim, ent2.id, ent1.id, ent2.damage, dt2);
        }
        if (ent1.health == 0) _kill_from_collision(sim, ent1);
        if (ent2.health == 0) _kill_from_collision(sim, ent2);
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