# Map hitboxes, rarity scaling, zones, and info boxes

Date: 2026-07-17

## Summary

Finish the map collision intent (block tiles collide only on their visible
solid), introduce florr-style **instance rarity** for petals and mobs
(Common→Unique) with explicit multipliers, replace the 4-zone map with
**3 biomes × 7 difficulty bands** (21 zones), wire spawn/drop rarity, always
show petal/mob combat stats in info boxes, and delete Massive Beetle, Massive
Ladybug, and Boulder.

## Goals

- Terrain collision for water / bush / cliff / dirt / castle follows the
  **visible** tile solid (tileset objectgroups → union mask). No on-screen
  terrain hitbox outline. No silly gaps or irrational overlaps.
- Same petal ID and same mob ID exist at every rarity Common→Unique.
- `PETAL_DATA` / `MOB_DATA` store **Common base** stats; instance rarity
  multiplies at spawn / equip time.
- 21 zone definitions: Garden, Jungle, Desert × Common→Ultra bands, with the
  agreed spawn rarity rolls.
- Drop rarity matches the killed mob’s instance rarity (temporary; retune later).
- Petal and mob info boxes always show the listed combat stats (not
  `DEBUG_ONLY`).
- Remove Massive Beetle, Massive Ladybug, Boulder; keep Rock’s existing model.

## Non-goals

- Scaling petal specials (vision, attack range, etc.) — deferred.
- Lightning damage type / armor ignore for lightning — reserved until lightning
  exists (poison already ignores contact armor via poison path).
- Merging `UniqueBasic` into Basic@Unique.
- Redrawing the Rock mob art.
- Rewriting Tiled biome art layout (only zone AABBs / difficulty bands change).
- Full rebalance of every Common base number that was authored for a native
  higher rarity (may need a later pass).

## Architecture

### A. Shared rarity helpers

New Shared module (e.g. `Shared/RarityScale.hh` + thin `.cc` if needed):

| Stat | Formula |
|------|---------|
| Petal HP, petal Damage | `base * 3^rarity` |
| Mob body damage | `base * 3^rarity` |
| Mob XP | `base * 3^rarity` |
| Mob armor | `base * 3^min(rarity, Ultra)`; Ultra = Super = Unique armor |
| Mob HP | Product of per-step factors from Common up to `rarity`: ×5 each step Common→Uncommon→Rare→Epic→Legendary; then ×10 Legendary→Mythic; ×45 Mythic→Ultra; ×30 Ultra→Super; ×5 Super→Unique |

Contact damage received: `max(0, damage − armor)`. Poison does not use contact
armor (existing `DamageType::kPoison` path).

`PETAL_DATA[].health` / `.damage` and `MOB_DATA` HP / damage / xp / armor are
Common bases. Gallery sort may still use a type’s “native” rarity field as a
display hint, but combat must not treat that field as the instance rarity.

### B. Instance rarity on entities

- **Petals**: already have loadout / inventory / drop rarity fields. `alloc_petal`
  and petal respawn paths must read the slot/stack rarity and apply petal
  multipliers to HP and damage.
- **Mobs**: add a networked (or at least server-authoritative) `mob_rarity`
  field set at spawn. `score_reward` = scaled XP. Drops set
  `drop_rarity = mob_rarity`.
- Hornet (and similar) missiles: Common-base `missile_damage` on
  `MobAttributes`; when fired, scale with the same ×3 body-damage curve as the
  parent mob’s rarity.

### C. Zones — 3 biomes × 7 bands

Replace current `MAP_DATA` (Grasslands / Tundra / Jungle / Desert) with:

- **Garden** (ex-Grasslands; includes NW spawn) — no Tundra.
- **Jungle**
- **Desert**

Each biome is split into **7 AABB strips** along a “deeper into biome” axis
(v1 placeholder geometry, retunable):

| `difficulty` | Band rarity |
|--------------|-------------|
| 0 | Common |
| 1 | Uncommon |
| 2 | Rare |
| 3 | Epic |
| 4 | Legendary |
| 5 | Mythic |
| 6 | Ultra |

Zone names like `"Garden · Rare"`. Each zone keeps a biome-specific mob spawn
table (weights updated after deletions).

**Spawn rarity roll** (after mob type is chosen):

| Band | Roll |
|------|------|
| Common…Mythic | 75% this rarity, 25% one tier up |
| Ultra | 25% Mythic, 75% Ultra; if result is Ultra, 1% → Super; if Super, 1% → Unique |

`get_zone_from_pos` / density / camera difficulty gating continue to use
`MAP_DATA` + `zone.difficulty` (now 0–6).

### D. Map render & collision

Keep the existing pipeline:

- `main.tmj` + `tiles/tileset.tsj` → `Scripts/gen_map.py` → `Server/map-data.json`
  + `Shared/Tilemap.hh`
- Client `MapRenderer`: culled vector tiles, Tiled flips
- Layers: `bg, transitions, water, bush, cliff, dirt, castle` (+ landmarks)
- Block collision from tileset **objectgroups** on water/bush/cliff/dirt/castle
  only — visible solid, not full grid cells
- Polish: regenerate and spot-check silly gaps / irrational overlaps; fix
  tileset shapes when the objectgroup disagrees with the visible fill

No terrain hitbox debug overlay as part of this feature.

### E. Info boxes

Remove `DEBUG_ONLY` gating for combat stats:

**Petal tooltip**: Damage, HP, existing special lines that already render,
Reload (reload already partially DEBUG-gated in the title row — show in
release).

**Mob gallery card**: HP, Body damage, Missile damage (if > 0), Armor, XP.
Gallery shows **Common base** values; rarity name can remain a type hint until
a rarity picker exists.

### F. Deletions & data cleanup

Delete completely (IDs, `StaticData`, spawns, AI cases, assets branches,
gallery):

- `MobID::kMassiveBeetle`
- `MobID::kMassiveLadybug`
- `MobID::kBoulder`

Keep `MobID::kRock` and its current render. Fold Boulder spawn weight into Rock
where a biome previously used Boulder.

Add mob `armor` (and `missile_damage` where needed) as Common bases; default 0.

Keep `PetalID::kUniqueBasic` as a separate ID for this pass.

## Data flow

```
Zone AABB → zone_id → pick mob type from biome table
                   → roll instance rarity from band rules
                   → alloc_mob(base stats × rarity mults)
Mob death → drop petal IDs from table
         → drop_rarity = mob_rarity
Player equip / petal spawn → alloc_petal(base × loadout rarity)
UI tooltip / gallery → read bases (+ instance rarity when available)
```

## Error handling / edge cases

- Armor cannot reduce damage below 0 (existing early-out when `amt <= 0`).
- Unique / Super only appear via Ultra-band chain rolls, not as zone bands.
- Out-of-bounds / void cells remain solid (existing Tilemap behavior).
- Account inventory already stores per-stack rarity; ensure equip/spawn paths
  never overwrite instance rarity with `PETAL_DATA[id].rarity`.

## Testing

- Spot-check collision: walk along water/cliff/bush/dirt/castle edges; no
  hairline tunnels; no standing inside visible solids.
- Spawn in each band of each biome: rarity histogram roughly matches 75/25
  (and Ultra chain).
- Equip Common vs Epic Basic: HP/damage differ by 3^n; specials unchanged.
- Kill Uncommon mob: drops are Uncommon.
- Gallery / petal tooltip show stats in non-DEBUG builds.
- Massive Beetle / Massive Ladybug / Boulder absent from gallery and spawns.
- Rock still renders and spawns.

## Approach chosen

Shared multiplier helpers + instance rarity on entities + 21 zone defs
(Approach 1 from brainstorm). Rejected: baking 9 copies per type into static
tables; deferring zones while shipping multipliers only.
