# florr.io-style vector map rendering + collision tunneling fix

Date: 2026-07-17

## Summary

Replace the gardn-style map pipeline (server composites one big `main-map.svg`,
client blits it as a pre-rasterized image) with florr.io-style rendering: the
client draws the map from tile data + per-tile SVG drawings as **true vector,
crisp at any zoom**, culled to the visible cells. Remove the `holes` layer
entirely. Fix a terrain-collision bug where a fast burst (bubble petal, big
knockback) tunnels through walls.

Collision generation (from the Tiled tileset objectgroups) is already correct and
is **not** re-architected here — only the *visual* pipeline changes, plus the
Motion sweep fix.

## Goals

- Client renders each visible tile as cached `Path2D` + flat fills under the
  camera transform — vector-crisp at every zoom (no raster softening).
- Client renders the map **from data** (tile drawings + placements), not from a
  server-baked composite image. Clean Shared/Client split per `agents.md`.
- No `holes` layer in render or collision.
- A fast velocity burst can no longer push a body through/into a wall.

## Non-goals

- Changing tile art or biome layout (uses the existing `tiles/*.svg` + `main.tmj`).
- Changing collision *shapes* (already generated from `tileset.tsj` objectgroups).
- Server-side rendering or any server change beyond the Motion sweep fix.
- Minimap rendering (already samples `Tilemap::solid_at`; unchanged).

## Architecture

### A. Data generation — `Scripts/gen_map.py`

`gen_map.py` stops writing `main-map.svg` / `docs/main-map-render.svg` and instead
writes one client render asset: **`Server/map-data.json`** (served by the node
wrapper like `main-map.svg` was; add `/map-data.json` to `Server/Wasm.cc`).

Structure:

```
{
  "cell": 500,                       // world units per cell (CELL_SIZE)
  "tileSize": 256,                   // tile drawing local coordinate space
  "tiles": {
    "<gid>": [                       // ordered shapes, back to front
      { "d": "<svg path data>", "fill": "#rrggbb", "stroke": "#rrggbb"|null, "sw": <num> },
      ...
    ]
  },
  "layers": ["bg","transitions","water","bush","cliff","dirt","castle"],
  "placements": [                    // one per non-empty tile cell (holes excluded)
    { "l": <layerIndex>, "c": <col>, "r": <row>, "gid": <gid>, "flip": <bits> }
  ],
  "objects": [                       // landmark img sprites
    { "gid": <gid>, "x": <world>, "y": <world>, "w": <world>, "h": <world>, "flip": <bits> }
  ]
}
```

Details:

- **Shape extraction**: reuse the existing SVG path parser. Convert each tile's
  `<path>`/`<rect>`/`<polygon>` with an opaque `fill` into `{d, fill, stroke, sw}`.
  Rects and polygons are emitted as equivalent path `d` strings so the client can
  feed everything to `new Path2D(d)`. Keep shape order (paint order) intact.
- **Only tiles actually placed** in the map need their drawing emitted (keeps the
  asset small); include the landmark sprite gids too.
- **Placements**: iterate the 7 tile layers (drop `holes`), emit non-zero cells
  with gid + Tiled flip bits.
- **Objects**: from the `img` object layer, converting Tiled's bottom-left tile-
  object anchor to a top-left world rect (`y_top = y - height`), scaled tmj→world.
- Collision output (`Shared/Tilemap.hh`) and the editor sync stay as they are,
  minus the `holes` layer contribution.

### B. Client map renderer — new file `Client/Render/MapRenderer.cc` + `.hh`

- **Load**: fetch `map-data.json` once (mirrors how `main-map.svg` was loaded in
  `Setup.cc`). Build a JS-side cache: for each gid, an array of
  `{ path: Path2D, fill, stroke, sw }`. Store on `Module.mapTiles`, placements on
  `Module.mapPlacements` (bucketed by layer for ordered draw), objects on
  `Module.mapObjects`. Path2D objects are created once here, never per frame.
- **Draw** (`draw_map`, replacing the blit): the C++ world render already computes
  the visible cell range `(c0..c1, r0..r1)` and has the camera transform on the
  context. `draw_map` becomes an `EM_ASM` that, for each layer in order and each
  visible placement in that layer, does:
  `ctx.save(); ctx.translate(c*cell, r*cell); apply flip; ctx.scale(cell/tileSize, cell/tileSize);`
  then for each cached shape `ctx.fillStyle=fill; ctx.fill(path); if(stroke){ctx.strokeStyle=stroke; ctx.lineWidth=sw; ctx.stroke(path);} ctx.restore();`
  Landmark objects draw the same way at their world rect. Visible-range culling is
  required (do **not** iterate all placements every frame).
- Placements are indexed by cell for O(visible) lookup (e.g., per-layer
  `Map<cellIndex, placement>` or a per-layer typed grid) so culling is cheap.

### C. Remove the raster pipeline (delete, don't deprecate — `agents.md`)

- Delete `main-map.svg` generation and its outputs from `gen_map.py`.
- Delete `Module.mapImage` / `Module.mapCanvas` creation in `Setup.cc` and the
  image-blit body of `Renderer::draw_map`.
- Remove `/main-map.svg` from `Server/Wasm.cc` once nothing references it.
- Keep `grass_bg.svg` (title-screen backdrop) untouched.

### D. Bubble / fast-burst tunneling fix — `Server/Process/Motion.cc`

Current terrain sub-stepping recomputes `cx,cy` on the straight line prev→target
every step and discards the previous `push_circle` correction, so only the final
position resolves; a fast burst lands past a wall and pops out the far side.

Fix: **swept resolution** — carry the resolved position forward.

```
float cx = prev_x, cy = prev_y;
float dist = hypot(tx - prev_x, ty - prev_y);
float step = max(4.0f, r * 0.8f);               // <= this entity's radius; floor bounds step count
int   steps = max(1, ceil(dist / step));
float sdx = (tx - prev_x) / steps, sdy = (ty - prev_y) / steps;
for (int s = 0; s < steps; ++s) {
    cx += sdx; cy += sdy;                        // advance from RESOLVED position
    Tilemap::push_circle(cx, cy, r);             // re-resolve each step
}
ent.set_x(cx); ent.set_y(cy);
```

The step is derived from **this entity's** radius `r` (not a constant) so the
swept circle always overlaps any wall it passes — no thin-wall skip — while the
`max(4, …)` floor bounds the iteration count for very small entities. This fixes
bubble burst and any large knockback, and is server-authoritative (client
prediction runs the same Shared Motion code).

## Component boundaries (`agents.md`)

- **Shared**: `Tilemap.hh` (collision) — both sides. Unchanged shape logic.
- **Client render data**: `map-data.json` (tile drawings + placements + objects) —
  client-only presentation data, generated.
- **Client**: new `MapRenderer` module owns the Path2D cache + per-frame vector
  draw. `Rendering.cc` keeps only the visible-range calc + calls `draw_map`.
- **Server**: only `Motion.cc` changes (the sweep). `Petal.cc` bubble logic is
  correct and unchanged — the bug is in collision resolution, not the burst.

## Testing / verification

- **Render**: build client, load the game, spawn; confirm the map renders (all
  biomes, transitions, landmarks) and stays crisp when zoomed in (no softening).
  Confirm no anthole/fire_anthole/termite_mound anywhere.
- **Perf**: measure frame time at player zoom (target: no regression that drops
  below ~60fps on desktop). If heavy, apply the bitmap-cache fallback.
- **Collision unchanged**: `solid_at` results identical to current (minus holes);
  spot-check the hitbox overlay export still matches walls.
- **Bubble fix**: in-game, fire a bubble burst straight at a wall from close range
  and from across a thin wall — the flower stops at the wall, never ends up inside
  or on the far side. Repeat with a strong knockback source.

## Files

- **Modified**: `Scripts/gen_map.py` (emit `map-data.json`, drop `holes` +
  `main-map.svg`), `Server/Wasm.cc` (serve `/map-data.json`, drop `/main-map.svg`),
  `Client/Setup.cc` (drop mapImage/mapCanvas, load map-data), `Client/Render/Renderer.cc`
  (draw_map → vector), `Client/Rendering.cc` (wire visible range to renderer),
  `Server/Process/Motion.cc` (swept collision).
- **New**: `Client/Render/MapRenderer.*` (Path2D cache + vector draw),
  `Server/map-data.json` (generated).
- **Deleted**: `Server/main-map.svg`, `docs/main-map-render.svg` generation; the
  raster-blit path.

## Rollout

Regenerate → build client + server → deploy (`map-data.json` + client/server
binaries) → verify render + bubble fix live. Standard deploy per `DEPLOY.md`.
