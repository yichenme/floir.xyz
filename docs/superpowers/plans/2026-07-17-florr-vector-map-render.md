# florr.io-style Vector Map Rendering + Collision Tunneling Fix — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render the map as true per-frame vector SVG tiles on the client (florr.io style, crisp at any zoom, culled to visible cells) instead of blitting a server-composited raster; remove the `holes` layer; fix fast-burst terrain tunneling.

**Architecture:** `gen_map.py` emits `Server/map-data.json` (per-tile `Path2D`-ready shapes + per-layer placements + landmark objects). A new client `MapRenderer` fetches it once, caches a `Path2D` per shape, and each frame draws only the visible tiles under the camera transform. The raster pipeline (`main-map.svg`, `mapImage`/`mapCanvas`, blit) is deleted. `Motion.cc` terrain resolution becomes a proper swept sweep so bursts can't tunnel.

**Tech Stack:** Python 3 (map generator, uses PIL/numpy already), C++20 → WASM via Emscripten (`EM_ASM` for canvas/JS interop), browser Canvas2D `Path2D`, Node HTTP wrapper (`Server/Wasm.cc`).

## Global Constraints

- World scale: `CELL_SIZE = 500` world units/cell; tile drawing local space `tileSize = 256`; `GRID_W = 50`, `GRID_H = 51`; arena `25000 x 25500`.
- Tiled flip bits: `FLIP_H = 0x80000000`, `FLIP_V = 0x40000000`, `FLIP_D = 0x20000000`. Tileset `firstgid = 1`, so `gid = tile_id + 1`.
- tmj is `512` px/cell; tmj px → world = `× 500/512`. Tiled tile-objects anchor bottom-left (`y_top = y - height`).
- Player radius `BASE_FLOWER_RADIUS = 25` (mobs vary; sweep step derives from each entity's radius).
- Build: `source /Users/eason/emsdk/emsdk_env.sh` then `make` in `Client/build` and `Server/build`. Stage client into `Server/` via `cp Client/build/floir-client.js Client/build/floir-client.wasm Server/`.
- Deploy per `DEPLOY.md`: scp to remote `/tmp`, `cp` into `/var/www/floir.xyz` (server pair into `build/`), `pm2 restart floir.xyz`, verify by md5. Host `root@38.76.198.54`, `SSHPASS` = the DEPLOY.md password.
- `agents.md`: Shared = source of truth; Client = render/presentation only; new renderer → its own client file; delete, don't deprecate.
- Layer draw order (bottom→top), **holes excluded**: `bg, transitions, water, bush, cliff, dirt, castle`.

---

### Task 1: Generate `map-data.json` (tiles + placements + objects), drop `holes`

**Files:**
- Modify: `Scripts/gen_map.py`
- Create (generated): `Server/map-data.json`

**Interfaces:**
- Produces: `Server/map-data.json` with keys `cell`(int), `tileSize`(int), `tiles`({gid→[{d,fill,stroke,sw}]}), `layers`([str]), `placements`([{l,c,r,gid,flip}]), `objects`([{gid,x,y,w,h,flip}]). `l` indexes `layers`. `flip` is the raw Tiled flip-bit int. `x,y,w,h` in world units.
- Consumes: existing `_load_tileset()` (`GID_TO_SVG`, `TILE_COLL`), `_path_subpaths`, tmj decode, `LAYER_ORDER`.

- [ ] **Step 1: Remove `holes` from `LAYER_ORDER`**

In `Scripts/gen_map.py`, set:
```python
LAYER_ORDER = ['bg', 'transitions', 'water', 'bush', 'cliff', 'dirt', 'castle']
```
(This drops holes from render, collision, and the new data — one source of truth.)

- [ ] **Step 2: Add a shape-extraction helper**

Add near `_load_tileset` in `Scripts/gen_map.py`. It returns each tile's ordered opaque shapes as `Path2D`-ready path data:
```python
def _tile_shapes(svg_name):
    # Ordered opaque shapes of a tile as {d, fill, stroke, sw}, back->front.
    txt = open(os.path.join(TILES, svg_name)).read()
    def attr(el, name):
        m = re.search(name + r'="([^"]*)"', el)
        return m.group(1) if m else None
    out = []
    for el in re.findall(r'<(?:rect|path|polygon|polyline)\b[^>]*?/?>', txt):
        fill = attr(el, 'fill')
        stroke = attr(el, 'stroke')
        if (not fill or fill == 'none') and (not stroke or stroke == 'none'):
            continue
        sw = float(attr(el, 'stroke-width') or 0)
        if el.startswith('<rect'):
            x = float(attr(el, 'x') or 0); y = float(attr(el, 'y') or 0)
            w = float(attr(el, 'width') or 0); h = float(attr(el, 'height') or 0)
            d = f'M{x} {y}h{w}v{h}h{-w}Z'
        elif el.startswith('<path'):
            d = attr(el, 'd')
        else:  # polygon / polyline
            pts = attr(el, 'points') or ''
            nums = [float(v) for v in re.findall(r'-?\d*\.?\d+', pts)]
            if len(nums) < 6:
                continue
            d = 'M' + ' '.join(f'{nums[k]} {nums[k+1]}' for k in range(0, len(nums) - 1, 2))
            if el.startswith('<polygon'):
                d += 'Z'
        if not d:
            continue
        out.append({'d': d,
                    'fill': (fill if fill and fill != 'none' else None),
                    'stroke': (stroke if stroke and stroke != 'none' else None),
                    'sw': sw})
    return out
```

- [ ] **Step 3: Emit `map-data.json` inside the render function**

In `Scripts/gen_map.py`, in the function that currently writes `main-map.svg` (where `layers`, `objgroups`, `W`, `H`, `symbols` are in scope), after the placement/`out` SVG loop, add JSON emission. Collect placements while iterating the SAME `LAYER_ORDER`, and reuse `GID_TO_SVG`/`_tile_shapes` for only the gids actually used:
```python
    import json as _json
    used = set()
    placements = []
    for li, name in enumerate(LAYER_ORDER):
        lay = layers.get(name)
        if not lay:
            continue
        for r in range(H):
            for c in range(W):
                g = lay[r * W + c]
                gid = g & 0x1FFFFFFF
                if not gid:
                    continue
                used.add(gid)
                placements.append({'l': li, 'c': c, 'r': r, 'gid': gid,
                                   'flip': g & (FLIP_H | FLIP_V | FLIP_D)})
    objects = []
    W2 = 500.0 / 512.0                       # tmj px -> world
    for o in objgroups.get('img', []):
        g = o.get('gid')
        if not g:
            continue
        gid = g & 0x1FFFFFFF
        used.add(gid)
        objects.append({'gid': gid,
                        'x': o['x'] * W2, 'y': (o['y'] - o.get('height', 0)) * W2,
                        'w': o.get('width', 0) * W2, 'h': o.get('height', 0) * W2,
                        'flip': g & (FLIP_H | FLIP_V | FLIP_D)})
    tiles = {}
    for gid in sorted(used):
        svg = GID_TO_SVG.get(gid)
        if svg and os.path.exists(os.path.join(TILES, svg)):
            tiles[str(gid)] = _tile_shapes(svg)
    data = {'cell': 500, 'tileSize': 256, 'layers': LAYER_ORDER,
            'tiles': tiles, 'placements': placements, 'objects': objects}
    open(os.path.join(ROOT, 'Server/map-data.json'), 'w').write(_json.dumps(data, separators=(',', ':')))
    print(f'map-data.json: {len(tiles)} tiles, {len(placements)} placements, {len(objects)} objects')
```
Keep the existing `main-map.svg` write for now (removed in Task 5).

- [ ] **Step 4: Regenerate**

Run: `cd /Users/eason/Desktop/floir.xyz && python3 Scripts/gen_map.py`
Expected: prints `map-data.json: <N> tiles, ~4126 placements, 3 objects` and the existing `placements=`/`collision mask:` lines, no traceback.

- [ ] **Step 5: Structural check of the generated JSON**

Run:
```bash
cd /Users/eason/Desktop/floir.xyz && python3 -c "
import json
d=json.load(open('Server/map-data.json'))
assert d['cell']==500 and d['tileSize']==256
assert d['layers']==['bg','transitions','water','bush','cliff','dirt','castle'], d['layers']
assert len(d['objects'])==3, d['objects']
assert all(k in s for s in next(iter(d['tiles'].values())) for k in ('d','fill','stroke','sw'))
assert d['placements'] and all(0<=p['l']<7 for p in d['placements'])
# no anthole/holes gids leaked in: holes layer excluded
print('tiles',len(d['tiles']),'placements',len(d['placements']),'objects',len(d['objects']),'OK')
"
```
Expected: prints `... OK` with no AssertionError.

- [ ] **Step 6: Commit**

```bash
cd /Users/eason/Desktop/floir.xyz && git add Scripts/gen_map.py Server/map-data.json Shared/Tilemap.hh Server/main-map.svg docs/collision-mask.png docs/collision-editor.html
git commit -m "gen: emit map-data.json (vector tiles + placements + objects); drop holes layer"
```

---

### Task 2: Serve `map-data.json` + client fetches and caches Path2D

**Files:**
- Modify: `Server/Wasm.cc` (add `/map-data.json` route)
- Create: `Client/Render/MapRenderer.hh`, `Client/Render/MapRenderer.cc`
- Modify: `Client/Setup.cc` (call `MapRenderer::load()`; keep `mapImage` for now)
- Modify: `Client/build` CMake list if sources are enumerated (see Step 4)

**Interfaces:**
- Produces: `Ui::MapRenderer::load()` — issues the fetch + builds `Module.mapTiles` (gid→`[{path:Path2D,fill,stroke,sw}]`), `Module.mapLayers` (array[7] of `Map<r*GRID_W+c, placementIndex>`... — see code), `Module.mapObjects`, sets `Module.mapReady=true` when done.
- Consumes: nothing from earlier tasks except the JSON shape from Task 1.

- [ ] **Step 1: Add the route in `Server/Wasm.cc`**

In the URL `switch` (next to the `grass_bg.svg` case), add:
```cpp
                case "/map-data.json":
                    encodeType = "application/json";
                    file = "map-data.json";
                    break;
```

- [ ] **Step 2: Create `Client/Render/MapRenderer.hh`**

```cpp
#pragma once

namespace Ui {
    // Loads Server/map-data.json and draws the map as cached vector tiles.
    struct MapRenderer {
        static void load();
        // Draw visible tiles/objects. Cell range is [c0,c1) x [r0,r1); the ctx
        // is already under the camera (world) transform.
        static void draw(int ctx_id, int c0, int c1, int r0, int r1);
    };
}
```

- [ ] **Step 3: Create `Client/Render/MapRenderer.cc` (load only for this task)**

```cpp
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

// draw() added in Task 3.
void MapRenderer::draw(int, int, int, int, int) {}
```

- [ ] **Step 4: Register the new source in the client build**

Check how `Client` sources are collected:
Run: `grep -rn "GLOB\|floir-client\|add_executable\|\.cc" /Users/eason/Desktop/floir.xyz/Client/CMakeLists.txt | head`
- If it uses `file(GLOB_RECURSE ...)`, nothing to add.
- If sources are listed explicitly, add `Render/MapRenderer.cc` to the list.

- [ ] **Step 5: Call `MapRenderer::load()` in `Client/Setup.cc`**

Where `Module.mapImage`/`mapCanvas` are set up (do NOT delete them yet), add after that block:
```cpp
        Ui::MapRenderer::load();
```
and add `#include <Client/Render/MapRenderer.hh>` at the top of `Setup.cc`.

- [ ] **Step 6: Build the client**

Run: `cd /Users/eason/Desktop/floir.xyz && source /Users/eason/emsdk/emsdk_env.sh >/dev/null 2>&1 && cd Client/build && make 2>&1 | tail -3`
Expected: `Built target floir-client`, no errors.

- [ ] **Step 7: Verify the cache builds in-browser**

Serve locally and load:
```bash
cd /Users/eason/Desktop/floir.xyz && (python3 -m http.server 8899 >/dev/null 2>&1 &) ; sleep 1
```
In the browser at `http://localhost:8899/Server/` (or the running preview), after load run in console:
`Module.mapReady && Object.keys(Module.mapTiles).length + ' tiles, layers=' + Module.mapLayers.length`
Expected: a tile count > 40 and `layers=7`. (The map still renders via the old blit — unchanged this task.)

- [ ] **Step 8: Commit**

```bash
cd /Users/eason/Desktop/floir.xyz && git add Server/Wasm.cc Client/Render/MapRenderer.hh Client/Render/MapRenderer.cc Client/Setup.cc Client/CMakeLists.txt
git commit -m "client: fetch map-data.json and cache Path2D per tile (renderer not yet wired)"
```

---

### Task 3: Draw the map as vector; wire visible range; switch `draw_map`

**Files:**
- Modify: `Client/Render/MapRenderer.cc` (implement `draw`)
- Modify: `Client/Render/Renderer.cc` (`draw_map` delegates to vector draw)
- Modify: `Client/Rendering.cc` (compute visible cell range before the map draw, pass it)

**Interfaces:**
- Consumes: `Module.mapTiles/mapLayers/mapObjects/mapCell/mapTileSize/mapReady` from Task 2; `Renderer.id` (context id, already used by `draw_map`'s `EM_ASM`).
- Produces: `MapRenderer::draw(ctx_id, c0, c1, r0, r1)` renders visible tiles + objects.

- [ ] **Step 1: Implement `MapRenderer::draw` in `Client/Render/MapRenderer.cc`**

Replace the stub `draw`:
```cpp
void MapRenderer::draw(int ctx_id, int c0, int c1, int r0, int r1) {
    EM_ASM({
        if (!Module.mapReady) return;
        const ctx = Module.ctxs[$0];
        const cell = Module.mapCell, ts = Module.mapTileSize, s = cell / ts;
        const GW = 50, tiles = Module.mapTiles, layers = Module.mapLayers;
        const c0 = $1, c1 = $2, r0 = $3, r1 = $4;
        function paint(shapes, flip) {
            ctx.save();
            // flip within the tile's own box, matching Tiled bits
            const H = (flip & 0x80000000) !== 0, V = (flip & 0x40000000) !== 0, D = (flip & 0x20000000) !== 0;
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
        // landmark objects (world-positioned)
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
```

- [ ] **Step 2: Point `Renderer::draw_map` at the vector renderer**

In `Client/Render/Renderer.cc`, the `draw_map(float x,float y,float w,float h)` body: replace the image-blit `EM_ASM` with a call that derives the visible cell range from the passed world rect (which the camera-transformed caller supplies as the full map) — but the cull needs the *view* rect, not the full map. So change the signature to accept the cell range. Replace `draw_map` with:
```cpp
void Renderer::draw_map(int c0, int c1, int r0, int r1) {
    Ui::MapRenderer::draw(id, c0, c1, r0, r1);
}
```
Update the declaration in `Renderer.hh` to `void draw_map(int c0, int c1, int r0, int r1);` and add `#include <Client/Render/MapRenderer.hh>` to `Renderer.cc`.

- [ ] **Step 3: Compute + pass the visible range in `Client/Rendering.cc`**

`Rendering.cc` already computes `c0,c1,r0,r1` around lines 83–86 (for the grid overlay). Move that computation **above** the `draw_map` call (line ~76) and change the call to:
```cpp
        renderer.draw_map(c0, c1, r0, r1);
```
Ensure `c0/c1/r0/r1` are declared once and reused by both the map draw and the existing grid overlay. Keep the `map_w/map_h` locals only if still referenced elsewhere; otherwise remove them.

- [ ] **Step 4: Build the client**

Run: `cd /Users/eason/Desktop/floir.xyz && source /Users/eason/emsdk/emsdk_env.sh >/dev/null 2>&1 && cd Client/build && make 2>&1 | tail -3`
Expected: `Built target floir-client`, no errors.

- [ ] **Step 5: Visual verification in-browser**

Stage + serve, load the game, spawn, and screenshot the in-game view. Confirm:
- The map renders (grass/desert/jungle/water/dirt/bush/cliff, transitions, and the sewer/pyramid/factory landmarks).
- Zoom-in stays crisp (vector, no raster softening).
- No anthole/fire_anthole/termite_mound anywhere.
Run to stage: `cd /Users/eason/Desktop/floir.xyz && cp Client/build/floir-client.js Client/build/floir-client.wasm Server/`
(Compare against `~/Desktop/floir-map.png` for biome/landmark parity.)

- [ ] **Step 6: Commit**

```bash
cd /Users/eason/Desktop/floir.xyz && git add Client/Render/MapRenderer.cc Client/Render/Renderer.cc Client/Render/Renderer.hh Client/Rendering.cc Server/floir-client.js Server/floir-client.wasm
git commit -m "client: render map as culled per-frame vector tiles (florr.io style)"
```

---

### Task 4: Fix bubble / fast-burst terrain tunneling (`Motion.cc`)

**Files:**
- Modify: `Server/Process/Motion.cc` (swept terrain resolution, ~lines 53–68)

**Interfaces:**
- Consumes: existing `Tilemap::push_circle(float&,float&,float)`, `ent.get_radius()`, `prev_x/prev_y`.
- Produces: no new symbols; behavior change only.

- [ ] **Step 1: Replace the sub-stepping loop**

In `Server/Process/Motion.cc`, inside `if (terrain_collide) { ... }`, replace the existing loop:
```cpp
        float tx = ent.get_x(), ty = ent.get_y();
        float dist = std::hypot(tx - prev_x, ty - prev_y);
        int steps = std::max(1, (int)std::ceil(dist / 50.0f));
        float cx = prev_x, cy = prev_y;
        for (int s = 1; s <= steps; ++s) {
            cx = prev_x + (tx - prev_x) * s / steps;
            cy = prev_y + (ty - prev_y) * s / steps;
            Tilemap::push_circle(cx, cy, r);
        }
        ent.set_x(cx);
        ent.set_y(cy);
```
with a swept version that carries the *resolved* position forward:
```cpp
        float tx = ent.get_x(), ty = ent.get_y();
        float dist = std::hypot(tx - prev_x, ty - prev_y);
        // Step must be <= this body's radius so the swept circle overlaps any
        // wall it crosses (no thin-wall skip); floor bounds the iteration count.
        float step = std::max(4.0f, r * 0.8f);
        int steps = std::max(1, (int)std::ceil(dist / step));
        float sdx = (tx - prev_x) / steps, sdy = (ty - prev_y) / steps;
        float cx = prev_x, cy = prev_y;
        for (int s = 0; s < steps; ++s) {
            cx += sdx;
            cy += sdy;
            Tilemap::push_circle(cx, cy, r);
        }
        ent.set_x(cx);
        ent.set_y(cy);
```

- [ ] **Step 2: Build the server**

Run: `cd /Users/eason/Desktop/floir.xyz && source /Users/eason/emsdk/emsdk_env.sh >/dev/null 2>&1 && cd Server/build && make 2>&1 | tail -3`
Expected: `Built target floir-server`, no errors.

- [ ] **Step 3: In-game verification (after deploy in Task 6, or a local server run)**

Equip Bubble, face a wall (bush/cliff/castle) from close range, and defend to burst straight into it; then try bursting across a thin wall. Expected: the flower stops at the wall surface and never ends up inside or on the far side. Repeat with a strong knockback (e.g., a mob hit) toward a wall — same result.

- [ ] **Step 4: Commit**

```bash
cd /Users/eason/Desktop/floir.xyz && git add Server/Process/Motion.cc
git commit -m "fix: swept terrain resolution so bubble/knockback can't tunnel walls"
```

---

### Task 5: Delete the raster pipeline (main-map.svg, mapImage/mapCanvas, blit)

**Files:**
- Modify: `Scripts/gen_map.py` (stop writing `main-map.svg` / `docs/main-map-render.svg`)
- Modify: `Client/Setup.cc` (remove `mapImage`/`mapCanvas`)
- Modify: `Server/Wasm.cc` (remove `/main-map.svg` route)
- Delete: `Server/main-map.svg`, `docs/main-map-render.svg`

**Interfaces:**
- Consumes: nothing new. Removes dead code once Task 3 renders via vector.

- [ ] **Step 1: Stop generating the composite SVG**

In `Scripts/gen_map.py`, remove the block that builds the `out` SVG string and writes it to `OUTS` (the `main-map.svg` / `docs/main-map-render.svg` / Desktop copy). Remove the now-unused `OUTS`, `symbols`, `load_symbol`, `flip_matrix`, `BLEED`, `CELL` and render-loop code **only if** nothing else references them (grep first). Keep `map-data.json` and `write_tilemap_header`.
Run first: `grep -n "OUTS\|symbols\|flip_matrix\|main-map" Scripts/gen_map.py`

- [ ] **Step 2: Remove the pre-raster from `Client/Setup.cc`**

Delete the `Module.mapImage = new Image(); ... Module.mapImage.onload = ...; Module.mapImage.src = 'main-map.svg';` block (the `mapCanvas` pre-raster). Keep `Module.grassImage` (title backdrop) and the `MapRenderer::load()` call.

- [ ] **Step 3: Remove the `/main-map.svg` route in `Server/Wasm.cc`**

Delete the `case "/main-map.svg":` block.

- [ ] **Step 4: Delete generated artifacts + regenerate**

```bash
cd /Users/eason/Desktop/floir.xyz && git rm -q Server/main-map.svg docs/main-map-render.svg
python3 Scripts/gen_map.py
```
Expected: prints `map-data.json: ...` and `collision mask: ...`, no `main-map.svg` line, no traceback.

- [ ] **Step 5: Confirm nothing still references the removed pieces**

Run: `cd /Users/eason/Desktop/floir.xyz && grep -rn "main-map.svg\|mapImage\|mapCanvas" Client Server Scripts | grep -v build/`
Expected: no matches (build/ artifacts are regenerated next).

- [ ] **Step 6: Rebuild both, verify render still works**

```bash
cd /Users/eason/Desktop/floir.xyz && source /Users/eason/emsdk/emsdk_env.sh >/dev/null 2>&1 && cd Client/build && make 2>&1 | tail -2 && cd ../../Server/build && make 2>&1 | tail -2 && cd ../.. && cp Client/build/floir-client.js Client/build/floir-client.wasm Server/
```
Then load locally and confirm the map still renders (vector) with no console 404 for `main-map.svg`.

- [ ] **Step 7: Commit**

```bash
cd /Users/eason/Desktop/floir.xyz && git add -A
git commit -m "cleanup: delete raster map pipeline (main-map.svg, mapImage/mapCanvas, blit)"
```

---

### Task 6: Regenerate, build, deploy, verify live

**Files:** none (build + deploy)

- [ ] **Step 1: Full regenerate + build**

```bash
cd /Users/eason/Desktop/floir.xyz && python3 Scripts/gen_map.py && source /Users/eason/emsdk/emsdk_env.sh >/dev/null 2>&1 && cd Client/build && make 2>&1 | tail -2 && cd ../../Server/build && make 2>&1 | tail -2 && cd ../.. && cp Client/build/floir-client.js Client/build/floir-client.wasm Server/
```

- [ ] **Step 2: Measure frame time at player zoom**

Load locally, spawn, and in the console sample ~90 frames of `requestAnimationFrame` deltas (or read the in-game debug stats). Expected: sustained ~60fps at player zoom. If clearly heavy (>~10ms/frame from map draw), stop and note it (the bitmap-cache fallback from the spec is the remedy) before deploying.

- [ ] **Step 3: Deploy per DEPLOY.md**

Upload `Server/build/floir-server.js`, `Server/build/floir-server.wasm`, `Server/floir-client.js`, `Server/floir-client.wasm`, `Server/map-data.json` to remote `/tmp`, then `cp` server pair into `build/` and the rest into `/var/www/floir.xyz/`, `pm2 restart floir.xyz`. Verify each by md5 vs local, `curl` `/` and `/map-data.json` → 200, and `https://floir.xyz/floir-client.wasm` md5 matches local.

- [ ] **Step 4: Live verification**

Hard-refresh `https://floir.xyz`, spawn: map renders as crisp vector, no holes, landmarks present; bubble burst into a wall stops at the wall.

- [ ] **Step 5: Push**

```bash
cd /Users/eason/Desktop/floir.xyz && git push origin main
```

---

## Notes for the implementer

- The map draw runs entirely in one `EM_ASM` per frame (all tile data + `Path2D` live JS-side); the C++ side only supplies the context id and visible cell range. This keeps WASM↔JS crossings to one per frame.
- Culling is mandatory: never iterate all placements. The per-layer `Map<r*GW+c, placement>` gives O(visible cells) lookups.
- `Path2D` accepts SVG path syntax directly, so `{d}` from the generator needs no conversion.
- The flip transforms operate in the tile's own `256×256` box (applied after `translate(cell)`+`scale(cell/256)`), matching how `_flip_poly`/collision bake flips.
- Do not touch `grass_bg.svg` (title screen) or the minimap (`Map.cc` samples `Tilemap::solid_at`, unaffected).
