#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/Tilemap.hh>

void tick_segment_behavior(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.seg_head)) {
        Entity &par = sim->get_ent(ent.seg_head);
        Vector diff(ent.get_x() - par.get_x(), ent.get_y() - par.get_y());
        diff.set_magnitude(ent.get_radius() + par.get_radius() + 0.01);
        float sx = par.get_x() + diff.x;
        float sy = par.get_y() + diff.y;
        // Segments follow the head by direct placement, which bypasses the
        // motion-step terrain resolution -- so push each one out of walls/water
        // here too, or the body clips into terrain when it rounds a corner.
        Tilemap::push_circle(sx, sy, ent.get_radius());
        ent.set_x(sx);
        ent.set_y(sy);
        ent.set_angle(diff.angle() + M_PI);
        if (sim->ent_alive(par.target))
            ent.target = par.target;
    }
}