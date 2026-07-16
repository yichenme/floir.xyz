# floir.xyz — Handoff Summary

## What this project is
**floir.xyz** is a florr.io-style multiplayer flower PvP game forked from
`trigonal-bacon/gardn`. C++20 → WASM (Emscripten) for both client and server,
served by a Node wrapper. See `CHANGES_SINCE_GARDN.md` for the full diff of
gameplay/feature changes.

## Ground rules (from `agents.md`)
- Strict Client / Server / Shared layering.
- New features go in **new files**; **delete, don't deprecate**.
- Use skills.

## Build
```
source /Users/eason/emsdk/emsdk_env.sh
cd Client/build && make      # -> Client/build/floir-client.{js,wasm}
cd Server/build && make      # -DWASM_SERVER=1 -> Server/build/floir-server.{js,wasm}
```
Map/collision are generated: `python3 Scripts/gen_map.py` reads `main.tmj`,
writes `Server/main-map.svg`, `docs/main-map-render.svg`, and
`Shared/Tilemap.hh` (collision data).

## Collision system (key subsystem)
- **Exact polygon collision, ~1 world-unit precision.** `Shared/Tilemap.hh` is
  generated with `POLY_VERTS` (flat floats, cell-local 0..500), `POLY_START`
  (**vertex** index — C reads `POLY_VERTS[2*(s+i)]`), `POLY_LEN`, `CELL_POLY`
  (per-cell 1-based poly index, 0 = walkable).
- Helpers: `solid_at`, `solid_circle(x,y,rad)`, `push_circle(&x,&y,rad)`.
- `Server/Process/Motion.cc` sub-steps movement (≤50-unit hops from the pre-move
  position) then calls `push_circle` — prevents knockback/bubble tunnelling.
- Void cells (outside the map) get a FULL square poly so the outside blocks.
- Blocking layers: `castle, dirt, cliff, bush, water` (BLOCK_LAYERS in gen_map).

### Two collision bugs fixed most recently (commit 39e20c1)
1. `_path_points` concatenated multiple SVG subpaths (`M...M...`) into one
   self-intersecting ring → thin diagonal collision artifact. Fixed by parsing
   each subpath separately (`_path_subpaths`) and taking the largest by area.
2. `POLY_START` was emitted in **float** units but C indexes in **vertex**
   units (`2*(s+i)`) → every polygon after the first read corrupted vertices.
   Fixed generator to emit `len(verts)//2`.
   Verify collision with the ASCII dump technique (point-in-poly at cell centres)
   before trusting a regenerated `Tilemap.hh`.

## Spawning
- `Shared/Map.cc`: `spawn_random_mob` / `find_spawn_location` use radius-aware
  `Tilemap::solid_circle` and re-roll — mobs never spawn in blocked terrain.
- `Server/Spawn.cc`: players spawn/respawn near NW (SPAWN_CX=2750, CY=7750,
  jitter 1500), rejecting blocked positions. Respawn keeps level (no petal loss).

## Deploy (production) — READ THE GOTCHAS
- Host: `38.76.198.54` (root). nginx 443 → localhost:3000. PM2 process
  `floir.xyz` runs `node /var/www/floir.xyz/floir-server.js` (cwd
  `/var/www/floir.xyz`).
- **The pm2 entry `floir-server.js` MUST be the WRAPPER** (identical to
  `Server/run-server.js`): it `require`s `./Account/database.js` (sets
  `global.loadDatabase`/etc.) THEN `./build/floir-server.js`. The raw
  emscripten output run directly crashes — no DB globals, never opens the
  socket, nginx returns 502. Local `Server/floir-server.js` is now a copy of the
  wrapper so the upload is correct.
- **The emscripten `build/floir-server.js` and `build/floir-server.wasm` are a
  matched pair from the same `emcc` run.** A stale js + fresh wasm (or vice
  versa) throws `TypeError: kb[a] is not a function`. Always upload BOTH; verify
  with `md5sum` on both sides after upload.
- Deployable bundle = local `Server/`. Before deploying: `python3
  Scripts/gen_map.py`, rebuild `Client/build` + `Server/build`, then copy fresh
  `Client/build/floir-client.{js,wasm}` into `Server/`.
- Upload set: `floir-server.js Account build floir-client.js floir-client.wasm
  index.html main-map.svg` → `/var/www/floir.xyz/`. **Do NOT upload
  `database.json`** — prod holds live account data; the set excludes it.
- **Remote has NO `rsync`** — use `scp` (`-C` for compression). The link is slow
  (~2MB can exceed a 3-min scp timeout); split large files, run in background,
  and **verify by `md5sum`, not timestamps** (scp/rsync can skip on size+mtime).
- After upload: `pm2 restart floir.xyz --update-env`, then verify:
  `curl localhost:3000` → 200, `curl -k https://floir.xyz` → 200, and
  `ss -ltnp | grep 3000` shows node listening. Check `pm2 logs` for the
  `Server running at http://localhost:3000` line (ignore older stale error lines).
- Client derives WS from `location.host`; server on port 3000.
- Last verified deploy: served `floir-client.wasm`/`main-map.svg` md5 == local,
  6 accounts intact, `https://floir.xyz → 200`.

### ⚠️ Account-schema caution
Production historically ran a **different** account schema (deck[16]/stash[30])
than the local loadout/inventory/PetalStack schema. A prior session reconciled
this (location-based WS, port 3000, migrated database). Before any change that
touches account serialization, verify local vs prod schema still match — a
mismatch corrupts accounts.

## Still open / interpret-with-user
- **"Adjust all hitboxes"** was addressed via exact-polygon terrain collision +
  the G-key petal-hitbox overlay. Mob/petal collision *radii* (in the data
  tables) were **not** individually retuned — confirm with the user if that's
  what they meant.

## Memory / repo
- Single `main` branch on `https://github.com/yichenme/floir.xyz.git`.
- Work happens in the `/Users/eason/Desktop/floir.xyz` checkout (the
  `.claude/worktrees/...` path resolves to the same repo).
