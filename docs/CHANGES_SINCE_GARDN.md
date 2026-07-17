# floir.xyz — Changes Since Original gardn

This document summarizes how **floir.xyz** diverges from its upstream base,
[trigonal-bacon/gardn](https://github.com/trigonal-bacon/gardn). Grouped by
subsystem. Layering follows `agents.md` (Client / Server / Shared).

## Branding
- All "gardn" text/branding replaced with **floir.xyz** (title, server name,
  displayed frontend, credits "made by yichennn").
- Removed the GitHub icon and the "How to Play" section.

## Map / World
- **Custom Tiled map import.** `main.tmj` (50×51 cells, 512 px tiles) is
  composited into `Server/main-map.svg` and `Shared/Tilemap.hh` by
  `Scripts/gen_map.py`. Upstream had a procedurally-tinted arena; floir uses a
  hand-authored biome map (garden / jungle / desert grounds, water, cliff,
  dirt, bush, castle, transition tiles).
- **All teleporters/portals removed.**
- Arena is 25000×25500 world units; camera view is clamped flush to the map
  edges (upstream let the blank outside show).

## Collision (largest change)
- Upstream: coarse cell/zone collision.
- floir: **exact polygon collision at ~1 world-unit precision.** Each blocking
  tile's opaque-fill outline is extracted as a polygon (`POLY_VERTS/START/LEN`,
  `CELL_POLY`) and resolved with point/circle-in-polygon (`solid_at`,
  `solid_circle`, `push_circle`). Collision follows the visible asset outline,
  not the grid.
- **Void cells (outside the map) block movement** — removes the "external white
  lines" where players could stand outside the arena.
- **Sub-stepped resolver** (`Server/Process/Motion.cc`): movement is sub-stepped
  from the pre-move position so knockback / bubble / large hops can't tunnel
  through thin walls.
- Blocking layers: castle, dirt, cliff, bush, water.

## Spawning
- **Mobs never spawn in water/cliff/dirt/bush/castle** — `spawn_random_mob` and
  `find_spawn_location` use radius-aware `Tilemap::solid_circle` and re-roll.
- **Players never spawn/respawn in blocked terrain** — `Server/Spawn.cc` jitters
  around a safe NW spawn and rejects blocked positions.
- **Respawn keeps the player's level** (no petal loss / level reset on death).

## Progression & Rarities
- **Score renamed to XP** throughout; leaderboard ranks by **level**.
- **9 rarity tiers**: Common, Uncommon, Rare, Epic, Legendary, Mythic, Ultra,
  Super, Unique (IDs 0–8). Super = `#2AFFA3`, Unique = `#555555`,
  Ultra = pink `#DE1F65`.

## UI / Client
- **Settings**: High Quality toggle (render scale + 30 fps cap) and Show Grid
  toggle (grid + `cc,rr` coordinate labels).
- **Minimap**: no panel/border, default size, 500% player-centred zoom that
  expands to the whole arena on click/hover; collision-accurate black/white
  sampled from `solid_at`; player dot sizing per zoom.
- **In-game HUD**: player level bar (flower model + HP + XP + name); in-game
  leave (X) button beside the minimap.
- **Inventory**: solid panel background; drag-one-from-stack; fixed
  duplicate-on-drag; stack cap 998; smooth stick/pop animations
  (90%→110%); fly-to-icon drag preview.
- **Death screen** remade: shows Level, Mobs killed, Petals collected
  (with k/M abbreviations).
- **G key**: petal-hitbox overlay (rarity-coloured circles + names).
- **M key / click / hover**: minimap expand.
- Account panel: no startup motion (preloaded, no first-open glitch); text
  sizes reduced.
- Movement helper anchored to the player's real on-screen position at map edges.

## Assets
- `grass2_b_0.svg` replaced with `desert_b_0.svg` everywhere.
- Transition-tile GID mapping updated (23 = dirt, 48 = desert transition, etc.).

## Data / Deploy
- Score component gained `mobs_killed`, `petals_collected`.
- Client derives the WebSocket URL from `location.host`; server runs on port 3000
  behind nginx (443 → localhost:3000), managed by PM2 as process `floir.xyz`.
