#include <Client/Render/MapRenderer.hh>

#include <emscripten.h>

using namespace Ui;

void MapRenderer::load() {
    EM_ASM({
        Module.mapReady = false;
        fetch('map-data.json').then(function(r){ return r.json(); }).then(function(d){
            const tiles = {};
            for (const gid in d.tiles) {
                tiles[gid] = d.tiles[gid].map(function(s){
                    return { path: new Path2D(s.d), fill: s.fill, stroke: s.stroke, sw: s.sw };
                });
            }
            const NL = d.layers.length, GW = 50;
            const layerGrid = [];
            for (let i = 0; i < NL; i++) layerGrid.push(new Map());
            for (const p of d.placements) layerGrid[p.l].set(p.r * GW + p.c, p);
            Module.mapTiles = tiles;
            Module.mapLayers = layerGrid;
            Module.mapObjects = d.objects;
            Module.mapLayerCount = NL;
            Module.mapCell = d.cell;
            Module.mapTileSize = d.tileSize;
            Module.mapReady = true;
        });
    });
}

void MapRenderer::draw(int ctx_id, int c0, int c1, int r0, int r1) {
    // NOTE: EM_ASM is a macro -- top-level commas (incl. multi-`const`) split the
    // argument list, so every declaration below is on its own statement.
    EM_ASM({
        if (!Module.mapReady) return;
        const ctx = Module.ctxs[$0];
        const cell = Module.mapCell;
        const ts = Module.mapTileSize;
        const s = cell / ts;
        const GW = 50;
        const tiles = Module.mapTiles;
        const layers = Module.mapLayers;
        const c0 = $1;
        const c1 = $2;
        const r0 = $3;
        const r1 = $4;
        function paint(shapes, flip) {
            ctx.save();
            // flip within the tile's own 256 box, matching the Tiled bits
            const H = (flip & 0x80000000) !== 0;
            const V = (flip & 0x40000000) !== 0;
            const D = (flip & 0x20000000) !== 0;
            if (D) ctx.transform(0, 1, 1, 0, 0, 0);
            if (H) ctx.transform(-1, 0, 0, 1, ts, 0);
            if (V) ctx.transform(1, 0, 0, -1, 0, ts);
            for (const sh of shapes) {
                if (sh.fill) { ctx.fillStyle = sh.fill; ctx.fill(sh.path); }
                if (sh.stroke) { ctx.strokeStyle = sh.stroke; ctx.lineWidth = sh.sw; ctx.lineJoin = 'round'; ctx.stroke(sh.path); }
            }
            ctx.restore();
        }
        for (let l = 0; l < layers.length; l++) {
            const grid = layers[l];
            for (let r = r0; r < r1; r++) {
                for (let c = c0; c < c1; c++) {
                    const p = grid.get(r * GW + c);
                    if (!p) continue;
                    const shapes = tiles[p.gid];
                    if (!shapes) continue;
                    ctx.save();
                    ctx.translate(c * cell, r * cell);
                    ctx.scale(s, s);
                    paint(shapes, p.flip);
                    ctx.restore();
                }
            }
        }
        // landmark objects (world-positioned), culled to the visible rect
        for (const o of Module.mapObjects) {
            const shapes = tiles[o.gid];
            if (!shapes) continue;
            if (o.x + o.w < c0 * cell || o.x > c1 * cell || o.y + o.h < r0 * cell || o.y > r1 * cell) continue;
            ctx.save();
            ctx.translate(o.x, o.y);
            ctx.scale(o.w / ts, o.h / ts);
            paint(shapes, o.flip);
            ctx.restore();
        }
    }, ctx_id, c0, c1, r0, r1);
}
