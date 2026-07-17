// Standalone regression test for the push_circle corner-resolution algorithm.
// This mirrors (and must be kept in sync with) the push_circle template emitted
// by Scripts/gen_map.py's write_tilemap_header into Shared/Tilemap.hh -- it
// re-implements the algorithm locally rather than including the real generated
// header, since that header's _usolid() depends on the actual map's static
// collision arrays and can't be pointed at a synthetic test grid.
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>
#include <utility>

static float const U = 10.0f;   // COLL_UNIT

std::set<std::pair<int,int>> g_solid;

bool usolid(int ux, int uy) {
    return g_solid.count({ux, uy}) != 0;
}

void push_circle(float &x, float &y, float rad) {
    for (int pass = 0; pass < 6; ++pass) {
        int cx0 = (int)std::floor((x - rad) / U) - 1, cx1 = (int)std::floor((x + rad) / U) + 1;
        int cy0 = (int)std::floor((y - rad) / U) - 1, cy1 = (int)std::floor((y + rad) / U) + 1;
        float best = 0.f, bx = x, by = y; bool found = false;
        for (int uy = cy0; uy <= cy1; ++uy) for (int ux = cx0; ux <= cx1; ++ux) {
            if (!usolid(ux, uy)) continue;
            float ax0 = ux * U, ay0 = uy * U, ax1 = ax0 + U, ay1 = ay0 + U;
            auto face = [&](float nx, float ny, float p0x, float p0y, float p1x, float p1y) {
                float ex = p1x - p0x, ey = p1y - p0y, l2 = ex * ex + ey * ey;
                float t = l2 > 0 ? ((x - p0x) * ex + (y - p0y) * ey) / l2 : 0.f;
                if (t < 0) t = 0; else if (t > 1) t = 1;
                float qx = p0x + t * ex, qy = p0y + t * ey;
                float vx = x - qx, vy = y - qy, d = std::sqrt(vx * vx + vy * vy), dn = vx * nx + vy * ny;
                float dx, dy, pen;
                if (d > 1e-4f) { dx = vx / d; dy = vy / d; pen = rad - d; }
                else { dx = nx; dy = ny; pen = rad; }
                if (pen <= 0.f) return;
                if (pen > best) { best = pen; bx = qx + dx * rad; by = qy + dy * rad; found = true; }
            };
            if (!usolid(ux - 1, uy)) face(-1, 0, ax0, ay0, ax0, ay1);
            if (!usolid(ux + 1, uy)) face(1, 0, ax1, ay0, ax1, ay1);
            if (!usolid(ux, uy - 1)) face(0, -1, ax0, ay0, ax1, ay0);
            if (!usolid(ux, uy + 1)) face(0, 1, ax0, ay1, ax1, ay1);
        }
        if (!found) break;
        x = bx; y = by;
    }
}

int main() {
    float const rad = 25.0f;

    // A square block occupies collision units (ux>=5, uy>=5), i.e. world
    // x>=50, y>=50. An entity slides in +x at constant y=20 -- its bottom edge
    // (y=20+rad=45) never reaches the block (y>=50), so it must never be
    // deflected. This reproduces a real false-positive: sliding tangentially
    // past a convex (outer) corner without ever touching it.
    g_solid.clear();
    for (int ux = 5; ux < 50; ++ux)
        for (int uy = 5; uy < 50; ++uy)
            g_solid.insert({ux, uy});

    float x = -80.f, y = 20.f;
    for (int tick = 0; tick < 25; ++tick) {
        float tx = x + 8.f, ty = y;   // velocity (8,0)/tick, matches Motion.cc's dt=1 sub-step
        float dist = std::hypot(tx - x, ty - y);
        float step = std::max(4.0f, rad * 0.8f);
        int steps = std::max(1, (int)std::ceil(dist / step));
        float sdx = (tx - x) / steps, sdy = (ty - y) / steps;
        float cx = x, cy = y;
        for (int s = 0; s < steps; ++s) {
            cx += sdx; cy += sdy;
            push_circle(cx, cy, rad);
        }
        x = cx; y = cy;
        assert(std::fabs(y - 20.0f) < 1e-3f && "entity was deflected despite never touching the block");
    }

    std::cout << "ok\n";
    return 0;
}
