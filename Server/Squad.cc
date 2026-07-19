#include <Server/Squad.hh>

#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>

#include <algorithm>
#include <unordered_map>

namespace {
    std::unordered_map<uint32_t, std::vector<EntityID>> g_squads;
    uint32_t g_next_id = 1;
    std::vector<EntityID> const EMPTY_MEMBERS;
    // Pending invites awaiting the target's /squad-accept: target camera id ->
    // inviter camera. Only one pending invite per target (a new one replaces it).
    std::unordered_map<uint32_t, EntityID> g_pending;

    std::string _member_name(Simulation *sim, EntityID camera) {
        if (!sim->ent_alive(camera)) return "";
        Entity &cam = sim->get_ent(camera);
        if (!sim->ent_alive(cam.get_player())) return "";
        return sim->get_ent(cam.get_player()).get_name();
    }

    // Full roster push to one member (their own client, if connected).
    void _send_roster(Simulation *sim, EntityID camera) {
        Client *c = Server::game.client_for_camera(camera);
        if (c == nullptr) return;
        if (!sim->ent_alive(camera)) return;
        uint32_t const sid = sim->get_ent(camera).get_squad_id();
        Writer writer(Server::OUTGOING_PACKET);
        writer.write<uint8_t>(Clientbound::kSquadUpdate);
        writer.write<uint32_t>(sid);
        std::vector<EntityID> const &mem = sid == 0 ? EMPTY_MEMBERS : Squad::members(sid);
        writer.write<uint32_t>((uint32_t)mem.size());
        for (EntityID const &m : mem) {
            writer.write<EntityID>(m);
            writer.write<std::string>(_member_name(sim, m));
        }
        c->send_packet(writer.packet, writer.at - writer.packet);
    }

    void _broadcast_roster(Simulation *sim, uint32_t squad_id) {
        if (squad_id == 0) return;
        // Copy: sending can't mutate the registry, but be defensive against
        // future changes making this loop unsafe over a live reference.
        std::vector<EntityID> const members = Squad::members(squad_id);
        for (EntityID const &m : members)
            if (sim->ent_alive(m)) _send_roster(sim, m);
    }

    void _send_notice(EntityID camera, std::string const &text) {
        Client *c = Server::game.client_for_camera(camera);
        if (c == nullptr) return;
        Writer writer(Server::OUTGOING_PACKET);
        writer.write<uint8_t>(Clientbound::kSquadNotice);
        writer.write<std::string>(text);
        c->send_packet(writer.packet, writer.at - writer.packet);
    }
}

std::vector<EntityID> const &Squad::members(uint32_t squad_id) {
    auto it = g_squads.find(squad_id);
    return it == g_squads.end() ? EMPTY_MEMBERS : it->second;
}

void Squad::leave(Simulation *sim, EntityID camera) {
    if (!sim->ent_alive(camera)) return;
    Entity &cam = sim->get_ent(camera);
    uint32_t const sid = cam.get_squad_id();
    if (sid == 0) return;
    cam.set_squad_id(0);
    auto it = g_squads.find(sid);
    if (it != g_squads.end()) {
        std::vector<EntityID> &mem = it->second;
        mem.erase(std::remove(mem.begin(), mem.end(), camera), mem.end());
        if (mem.size() < 2) {
            // A one-member "squad" isn't meaningful: dissolve it entirely, so
            // the last remaining member also drops back to squad_id 0.
            std::vector<EntityID> const remaining = mem;
            g_squads.erase(it);
            for (EntityID const &m : remaining) {
                if (!sim->ent_alive(m)) continue;
                sim->get_ent(m).set_squad_id(0);
                _send_roster(sim, m);
            }
        } else {
            _broadcast_roster(sim, sid);
        }
    }
    _send_notice(camera, "You left your squad.");
    _send_roster(sim, camera);
}

bool Squad::invite(Simulation *sim, EntityID inviter_camera, EntityID target_camera) {
    if (!sim->ent_alive(inviter_camera) || !sim->ent_alive(target_camera)) return false;
    if (inviter_camera == target_camera) return false;
    Entity &inviter = sim->get_ent(inviter_camera);
    Entity &target = sim->get_ent(target_camera);
    uint32_t const sid = inviter.get_squad_id();
    if (sid != 0) {
        if (target.get_squad_id() == sid) return true;   // already squadmates
        if (Squad::members(sid).size() >= MAX_SIZE) return false;
    }
    // Record the invite as PENDING and prompt the target -- they must accept
    // (type /squad-accept) before actually joining.
    g_pending[target_camera.id] = inviter_camera;
    std::string const iname = _member_name(sim, inviter_camera);
    _send_notice(target_camera, (iname.empty() ? std::string("A player") : iname)
                 + " invited you to their squad -- type /squad-accept to join");
    return true;
}

bool Squad::accept(Simulation *sim, EntityID target_camera) {
    if (!sim->ent_alive(target_camera)) return false;
    auto it = g_pending.find(target_camera.id);
    if (it == g_pending.end()) return false;
    EntityID const inviter_camera = it->second;
    g_pending.erase(it);
    if (!sim->ent_alive(inviter_camera) || inviter_camera == target_camera) return false;
    Entity &inviter = sim->get_ent(inviter_camera);
    Entity &target = sim->get_ent(target_camera);
    uint32_t sid = inviter.get_squad_id();
    if (sid != 0) {
        if (target.get_squad_id() == sid) return true;   // already squadmates
        if (Squad::members(sid).size() >= MAX_SIZE) return false;
    }
    // Pull the target out of any squad it's currently in (moving squads, not
    // stacking membership).
    if (target.get_squad_id() != 0) Squad::leave(sim, target_camera);
    if (sid == 0) {
        sid = g_next_id++;
        g_squads[sid] = { inviter_camera };
        inviter.set_squad_id(sid);
    }
    g_squads[sid].push_back(target_camera);
    target.set_squad_id(sid);

    std::string const iname = _member_name(sim, inviter_camera);
    std::string const tname = _member_name(sim, target_camera);
    _send_notice(target_camera, "You joined " + (iname.empty() ? std::string("a player's") : iname + "'s") + " squad!");
    _send_notice(inviter_camera, (tname.empty() ? std::string("A player") : tname) + " joined your squad!");
    _broadcast_roster(sim, sid);
    return true;
}
