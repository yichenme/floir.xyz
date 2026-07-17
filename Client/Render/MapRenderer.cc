#include <Client/Render/MapRenderer.hh>

#include <emscripten.h>

using namespace Ui;

void MapRenderer::load() {
    // Bracket-quote every JSON key: Closure renames d.tiles → d.ua etc., but
    // map-data.json still uses the original string names.
    EM_ASM({
        Module.mapReady = false;
        fetch('map-data.json').then(function(r){ return r.json(); }).then(function(d){
            const tiles = {};
            const srcTiles = d["tiles"];
            for (const gid in srcTiles) {
                tiles[gid] = srcTiles[gid].map(function(s){
                    return { path: new Path2D(s["d"]), fill: s["fill"], stroke: s["stroke"], sw: s["sw"], op: s["op"] };
                });
            }
            const layers = d["layers"];
            const NL = layers.length;
            const GW = 50;
            const layerGrid = [];
            for (let i = 0; i < NL; i++) layerGrid.push(new Map());
            const placements = d["placements"];
            for (let i = 0; i < placements.length; i++) {
                const p = placements[i];
                layerGrid[p["l"]].set(p["r"] * GW + p["c"], p);
            }
            Module.mapTiles = tiles;
            Module.mapLayers = layerGrid;
            Module.mapLayerNames = layers;
            Module.mapObjects = d["objects"];
            Module.mapLayerCount = NL;
            Module.mapCell = d["cell"];
            Module.mapTileSize = d["tileSize"];
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
        const layerNames = Module.mapLayerNames;
        const c0 = $1;
        const c1 = $2;
        const r0 = $3;
        const r1 = $4;
        const SHADOW_ALPHA = 0.1;
        function tileTransform(targetCtx, flip) {
            // flip within the tile's own 256 box, matching the Tiled bits.
            // ctx.transform() is cumulative: calling D, then H, then V applies
            // them to a drawn point in REVERSE order (V first, D last). To match
            // Scripts/gen_map.py's _flip_pt (D applied first, then H, then V --
            // also the collision mask's ground truth), the calls must go in the
            // opposite order: V, then H, then D last.
            const H = (flip & 0x80000000) !== 0;
            const V = (flip & 0x40000000) !== 0;
            const D = (flip & 0x20000000) !== 0;
            if (V) targetCtx.transform(1, 0, 0, -1, 0, ts);
            if (H) targetCtx.transform(-1, 0, 0, 1, ts, 0);
            if (D) targetCtx.transform(0, 1, 1, 0, 0, 0);
        }
        function paint(shapes, flip) {
            // Shadow shapes (op<1) are handled separately (see the per-layer
            // offscreen pass below) so they don't get drawn twice.
            ctx.save();
            tileTransform(ctx, flip);
            for (const sh of shapes) {
                if (sh.op !== undefined && sh.op < 1) continue;
                if (sh.fill) { ctx.fillStyle = sh.fill; ctx.fill(sh.path); }
                if (sh.stroke) { ctx.strokeStyle = sh.stroke; ctx.lineWidth = sh.sw; ctx.lineJoin = 'round'; ctx.stroke(sh.path); }
            }
            ctx.restore();
        }
        function paintShadowsOnly(targetCtx, shapes, flip) {
            // Draws only the low-opacity shadow shapes, always at full alpha --
            // the shared alpha is applied once when the whole accumulated
            // buffer is composited, so overlapping shadows of the SAME terrain
            // layer union instead of stacking darker.
            targetCtx.save();
            tileTransform(targetCtx, flip);
            for (const sh of shapes) {
                if (sh.op === undefined || sh.op >= 1) continue;
                if (sh.fill) { targetCtx.fillStyle = '#000'; targetCtx.fill(sh.path); }
                if (sh.stroke) { targetCtx.strokeStyle = '#000'; targetCtx.lineWidth = sh.sw; targetCtx.lineJoin = 'round'; targetCtx.stroke(sh.path); }
            }
            targetCtx.restore();
        }
        if (!Module.mapShadowCanvas) {
            Module.mapShadowCanvas = document.createElement('canvas');
            Module.mapShadowCtx = Module.mapShadowCanvas.getContext('2d');
        }
        const shadowCanvas = Module.mapShadowCanvas;
        const shadowCtx = Module.mapShadowCtx;
        if (shadowCanvas.width !== ctx.canvas.width || shadowCanvas.height !== ctx.canvas.height) {
            shadowCanvas.width = ctx.canvas.width;
            shadowCanvas.height = ctx.canvas.height;
        }
        for (let l = 0; l < layers.length; l++) {
            const grid = layers[l];
            const name = layerNames[l];
            // bg/transitions never carry shadow shapes -- skip the offscreen
            // pass entirely for them.
            const hasShadows = name !== 'bg' && name !== 'transitions';
            if (hasShadows) {
                shadowCtx.setTransform(1, 0, 0, 1, 0, 0);
                shadowCtx.clearRect(0, 0, shadowCanvas.width, shadowCanvas.height);
                shadowCtx.setTransform(ctx.getTransform());
                for (let r = r0; r < r1; r++) {
                    for (let c = c0; c < c1; c++) {
                        const p = grid.get(r * GW + c);
                        if (!p) continue;
                        const shapes = tiles[p["gid"]];
                        if (!shapes) continue;
                        const pad = 2;
                        shadowCtx.save();
                        shadowCtx.translate(c * cell - pad, r * cell - pad);
                        shadowCtx.scale((cell + 2 * pad) / ts, (cell + 2 * pad) / ts);
                        paintShadowsOnly(shadowCtx, shapes, p["flip"]);
                        shadowCtx.restore();
                    }
                }
                ctx.save();
                ctx.setTransform(1, 0, 0, 1, 0, 0);
                ctx.globalAlpha = SHADOW_ALPHA;
                ctx.drawImage(shadowCanvas, 0, 0);
                ctx.restore();
            }
            for (let r = r0; r < r1; r++) {
                for (let c = c0; c < c1; c++) {
                    const p = grid.get(r * GW + c);
                    if (!p) continue;
                    const shapes = tiles[p["gid"]];
                    if (!shapes) continue;
                    ctx.save();
                    // Slight bleed so adjacent tiles (castle walls, etc.) don't
                    // show hairline chops between cells under camera zoom.
                    const pad = 2;
                    ctx.translate(c * cell - pad, r * cell - pad);
                    ctx.scale((cell + 2 * pad) / ts, (cell + 2 * pad) / ts);
                    paint(shapes, p["flip"]);
                    ctx.restore();
                }
            }
        }
        // landmark objects (world-positioned), culled to the visible rect
        const objects = Module.mapObjects;
        for (let i = 0; i < objects.length; i++) {
            const o = objects[i];
            const shapes = tiles[o["gid"]];
            if (!shapes) continue;
            if (o["x"] + o["w"] < c0 * cell || o["x"] > c1 * cell || o["y"] + o["h"] < r0 * cell || o["y"] > r1 * cell) continue;
            ctx.save();
            ctx.translate(o["x"], o["y"]);
            ctx.scale(o["w"] / ts, o["h"] / ts);
            paint(shapes, o["flip"]);
            ctx.restore();
        }
    }, ctx_id, c0, c1, r0, r1);
}
