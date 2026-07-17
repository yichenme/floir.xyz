# Mob rarity size, mob health bars, tile shadows, hitbox corner-snap fix

Date: 2026-07-17

## Summary

Four independent changes:

1. Mob visual/collision size scales with rarity (~3x from Common to Unique).
2. Mobs get a health bar (name + rarity + bar) below their model, reusing the
   player health bar's exact logic.
3. Blocking terrain tiles (castle/dirt/cliff/bush/water) get a synthesized
   drop-shadow where they border open ground.
4. Fix a real geometry bug in `Tilemap::push_circle` that causes a
   discontinuous snap (perceived as jitter/vibration) when an entity's circle
   passes near a convex terrain corner, including false positives where the
   circle never actually touches the corner.

These are unrelated subsystems (rarity/spawn, client rendering x2, collision
math) sharing only the general area of "mob/map feel," so each is implemented
and can be verified independently.

## 1. Mob size scaling by rarity

**Formula:** `mob_size_mult(r) = pow(3.0f, (float)r / 8.0f)`, added next to the
existing multiplier helpers in `Shared/RarityScale.hh`/`.cc`. With
`RarityID` running `kCommon=0 .. kUnique=8` (9 tiers), this gives exactly
`1.0x` at Common and `3.0x` at Unique, compounding smoothly (~1.147x per
tier) in between. This deliberately does NOT reuse `rarity_pow3` (3^r) as-is
since that would yield up to 6561x at Unique — fine for invisible stats like
damage/XP, not for a radius that drives both rendering and collision.

**Where applied:** `Server/Spawn.cc`, in the mob-spawn path, after
`mob.set_mob_rarity(rarity)` is set (around the existing
`hp_m`/`dmg_m`/`arm_m` multiplier block). Multiply the mob's already-set
radius: `mob.set_radius(mob.get_radius() * mob_size_mult(rarity))`.

**Why this is enough:** `Physics.radius` (`Shared/EntityDef.hh:39`) is the
*only* radius value in the engine — used identically for collision
(`Server/Process/Collision.cc`, `Server/Process/Motion.cc` terrain push) and
for rendering (`Client/Render/RenderMob.cc` passes `ent.get_radius()`
straight through). One multiply changes both hitbox and visual size
together, consistently.

## 2. Mob health bar (name + rarity, below model)

**Current state:** `Client/Simulation.cc:37-53` already maintains
`healthbar_opacity` / `healthbar_lag` for any entity with `kHealth`,
mob or not — the fade-in-on-damage / fade-out-at-full-health / red lag-drain
behavior is entity-generic already. `Client/Render/RenderHealth.cc:11`
explicitly skips mobs (`if (ent.has_component(kMob)) return;`) — that is the
only reason mobs don't already show a bar.

**Change:** Remove the mob skip in `render_health()`. The bar itself
(background stroke, red lag segment, green fill, position formula
`translate(-w, w + 15)` where `w = radius * 1.33`) stays byte-for-byte
identical between players and mobs — that's the "same logic as player's"
requirement satisfied directly by not forking the code path.

**Name + rarity line:** Immediately after the mob skip is removed, add a
mob-only branch (`if (ent.has_component(kMob)) { ... }`) that draws one line
of text just above the bar (so it still reads as "below the model" overall,
between the model and the bar):
- Text: `MOB_DATA[ent.get_mob_id()].name` (existing field,
  `Shared/StaticDefinitions.hh:248`).
- Color: `RARITY_COLORS[ent.get_mob_rarity()]` (existing table,
  `Client/StaticData.cc:3-13`), passed as `TextArgs.fill`.
- Positioned via an extra `ctx.translate(0, -14)` (or similar small offset)
  before the bar's own translate, mirroring `RenderName.cc`'s
  `draw_text(name, { .size = ... })` call style.
- Only drawn for mobs; players keep their existing separate nametag
  (`RenderName.cc`) untouched.

## 3. Block-tile shadows

**Constraint:** tiles are deliberately flat-fill only (no gradients/filters,
per the comment in `Scripts/gen_map.py`) for render performance. Shadows must
be cheap: no per-frame neighbor lookups, no canvas gradients.

**Approach — bake at generation time:** In `Scripts/gen_map.py`, after the
per-cell `terrain` array is computed (already used for `BLOCK_LAYERS`
detection), do a second pass: for every cell whose terrain is blocking
(`castle`, `dirt`, `cliff`, `bush`, `water`) that is 4-directionally adjacent
to a non-blocking cell, emit a small stack of flat-fill rects along that
shared edge, extending into the open cell:
- 3 bands, each `~15-20` world units deep, stepping alpha down (e.g. `#000`
  at `0.25` / `0.15` / `0.08`), so it reads as a soft fake drop-shadow/AO
  without a real gradient.
- These are computed once, in world coordinates, and written to a new
  top-level `"shadows"` array in `map-data.json` (siblings of `"objects"`):
  `{x, y, w, h, fill}` per band, already in world space (no per-tile Path2D
  scaling needed client-side, unlike the tile-shape cache which is
  gid-shared and can't vary per-placement's neighbor openness).

**Client draw order:** `Client/Render/MapRenderer.cc`'s `draw()` renders
`"shadows"` (culled to the visible rect, same pattern as `"objects"`) right
after the `bg` layer and before `transitions`, so shadows sit under any
transition/edge art and under the blocking tile's own layer draw.

## 4. Hitbox corner-snap fix (not a bounce feature)

**Root cause (confirmed via standalone reproduction — see conversation):** in
`Tilemap::push_circle`'s inner `face()` lambda (`Shared/Tilemap.hh`, emitted
by `Scripts/gen_map.py`'s `write_tilemap_header`), when the closest point on
a wall-face segment clamps to an endpoint (a corner vertex shared by two
solid units) and the circle center falls on the back side of that face's
flat normal (`dn < 0`), the code takes an `else` branch using
`pen = rad - dn`. Because `dn` can be very negative, `pen` can become an
arbitrarily large bogus "penetration," winning the deepest-correction
comparison and producing a large discontinuous position snap — confirmed via
a Python 1:1 port of the algorithm, where an entity sliding tangentially past
a corner (never geometrically within `rad` of it) was still yanked ~38 units
sideways.

**Fix:** in that `face()` lambda, change the condition from
`dn >= 0.f && d > 1e-4f` to just `d > 1e-4f` for the radial
(`dx=vx/d, dy=vy/d, pen=rad-d`) branch, keeping the flat-normal fallback only
for the true degenerate case `d <= 1e-4f` (circle center exactly coincident
with the closest point), where `pen = rad` (since `d≈0`).

**Verification done:** re-ran the same standalone harness across flat-wall
approach, sliding along a flat wall, sliding along a rasterized 45° staircase
edge, walking straight into a convex corner, and sliding tangentially past a
convex corner — the fix removes the false-positive snap and leaves every
other case's resolved position smooth/monotonic, including exact-distance
correctness at a real corner contact (stops exactly `rad` units from the
vertex).

**Where it lives:** `Shared/Tilemap.hh` is auto-generated
(`Scripts/gen_map.py`'s `write_tilemap_header`, which emits `push_circle` as
a Python string template). The fix must go in the Python template, then
`Shared/Tilemap.hh` regenerated by re-running `python3 Scripts/gen_map.py`
(also regenerates `Server/map-data.json`, harmless/idempotent since no map
art changed) and both Client+Server WASM rebuilt.

## Out of scope

- No changes to `Server/Process/Motion.cc`'s terrain-collide sub-stepping
  loop itself — the fix is entirely inside `push_circle`.
- No actual velocity reflection/bounce physics — user confirmed the "bounce"
  wording described jitter, not a desired new mechanic.
- No per-rarity mob stat changes beyond size (HP/damage/armor/XP multipliers
  are unchanged, already handled by existing `RarityScale.cc` helpers).
- Performance findings from the earlier survey (EM_ASM overhead, per-tick
  `std::set` allocation, tick-log spam) are tracked separately and not part
  of this change.
