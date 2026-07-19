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

    // Adds `target_camera` to `inviter_camera`'s squad (creating one if the
    // inviter has none yet), first pulling `target_camera` out of any squad
    // it was already in. No-ops (returns false) if the inviter's squad is
    // already at MAX_SIZE, or either camera doesn't exist. Pushes
    // kSquadUpdate + kSquadNotice to the affected clients.
    bool invite(Simulation *sim, EntityID inviter_camera, EntityID target_camera);

    // Removes `camera` from its squad (dissolving it if fewer than 2 members
    // remain afterward). Pushes kSquadUpdate to affected clients, including a
    // squad_id=0 update to the leaver. Safe to call on a camera with no squad.
    void leave(Simulation *sim, EntityID camera);

    // Member camera ids of `squad_id` (empty if unknown/dissolved/0).
    std::vector<EntityID> const &members(uint32_t squad_id);
}
