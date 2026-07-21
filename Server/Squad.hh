#pragma once

#include <Shared/EntityDef.hh>

#include <cstdint>
#include <vector>

class Simulation;

// In-memory squad registry (session-only -- squads are formed live via
// "/squad-invite" chat commands and don't persist across reconnects). A
// camera's `squad_id` field (0 = none) is the source of truth for "which
// squad am I in"; this registry holds the member list for each id and drives
// the kSquadUpdate/kSquadNotice pushes to affected clients.
namespace Squad {
    uint32_t const MAX_SIZE = 4;

    // Records a PENDING invite from `inviter_camera` to `target_camera` and
    // pushes a kSquadInvite (Accept/Reject buttons client-side) to the
    // target. Does NOT join them -- joining only happens on accept(). Returns
    // false if the squad is full or a camera is invalid.
    bool invite(Simulation *sim, EntityID inviter_camera, EntityID target_camera);

    // The target accepts its pending invite and actually joins the inviter's
    // squad (creating one if needed, pulling the target out of any prior squad).
    // Returns false if there's no valid pending invite. Pushes kSquadUpdate +
    // kSquadNotice to affected clients.
    bool accept(Simulation *sim, EntityID target_camera);

    // The target declines its pending invite: just clears it, no join.
    // Returns false if there was no pending invite to clear.
    bool reject(EntityID target_camera);

    // Removes `camera` from its squad (dissolving it if fewer than 2 members
    // remain afterward). Pushes kSquadUpdate to affected clients, including a
    // squad_id=0 update to the leaver. Safe to call on a camera with no squad.
    void leave(Simulation *sim, EntityID camera);

    // Member camera ids of `squad_id` (empty if unknown/dissolved/0).
    std::vector<EntityID> const &members(uint32_t squad_id);
}
