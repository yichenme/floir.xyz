# Map Hitboxes, Rarity Scaling, 21 Zones, Info Boxes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Visible-only terrain collision polish; Common-base petal/mob stats scaled by instance rarity (Common→Unique); 21 spawn zones (Garden/Jungle/Desert × Common→Ultra); always-on petal/mob combat stats in info boxes; delete Massive Beetle, Massive Ladybug, Boulder.

**Architecture:** Shared `RarityScale` helpers multiply Common bases. Petal rarity already lives on loadout/inventory/drops; mobs gain `mob_rarity`. Spawn rolls rarity from the zone band. `MAP_DATA` becomes 21 AABBs. UI drops `DEBUG_ONLY` around combat stats. Map pipeline stays `gen_map.py` → `map-data.json` / `Tilemap.hh`.

**Tech Stack:** C++20 → WASM (Emscripten), Shared/Server/Client layering per `agents.md`, Python 3 for map gen + a small rarity math check script.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-07-17-map-rarity-info-design.md`
- Rarities: `RarityID::kCommon`…`kUnique` (9 values). Zone bands only cover Common→Ultra (`difficulty` 0…6).
- Petal HP/Damage: `base * pow(3, rarity)`. Specials not scaled.
- Mob body damage / XP: `base * pow(3, rarity)`. Armor: `base * pow(3, min(rarity, Ultra))`.
- Mob HP steps from Common: ×5,×5,×5,×5 (to Legendary), then ×10, ×45, ×30, ×5 (to Unique).
- Spawn rolls: Common–Mythic bands 75% self / 25% +1; Ultra band 25% Mythic / 75% Ultra, then Ultra→Super 1%, Super→Unique 1%.
- Drop rarity = mob instance rarity.
- Delete: `kMassiveBeetle`, `kMassiveLadybug`, `kBoulder`. Keep Rock model.
- `MAX_DIFFICULTY` becomes `6` (was `3`).
- Build: `source /Users/eason/emsdk/emsdk_env.sh` then `make` in `Client/build` and `Server/build`; `cp Client/build/floir-client.{js,wasm} Server/`.
- `agents.md`: Shared = numbers/helpers; Server = spawn/combat; Client = UI/render. New concern → new file. Delete, don’t deprecate.

## File map

| File | Role |
|------|------|
| `Shared/RarityScale.hh` (+ `.cc` if non-inline) | Multiplier helpers + spawn rarity roll |
| `Shared/StaticDefinitions.hh` | Mob attrs (`armor`, `missile_damage`); `MAX_DIFFICULTY`; MobID deletes |
| `Shared/StaticData.cc` / `.hh` | Common bases; 21 zones; remove deleted mobs |
| `Shared/EntityDef.hh` | `mob_rarity` on Mob component |
| `Shared/Map.cc` | Apply rarity on spawn; scaled XP |
| `Server/Spawn.cc` / `.hh` | Scale petal/mob on alloc; `alloc_drop`/`alloc_petal` take rarity |
| `Server/Process/Flower.cc` | Pass loadout rarity into `alloc_petal` |
| `Server/Process/Ai.cc` | Hornet missile uses scaled `missile_damage`; delete mob cases |
| `Server/EntityFunctions/Death.cc` | Drops use mob rarity |
| `Client/Ui/InGame/Tooltip.cc` | Always show petal stats + reload |
| `Client/Ui/TitleScreen/MobGallery.cc` | Always show mob stats; drop MassiveBeetle branch |
| `Client/Assets/Mob.cc` | Remove deleted mob render cases |
| `Client/CMakeLists.txt`, `Server/CMakeLists.txt` | Add `RarityScale.cc` if needed |
| `Scripts/gen_map.py`, `Scripts/check_rarity_scale.py` | Collision regen / math verify |
| `Scripts/check_zone_layout.py` | Assert 21 zones, no Tundra, difficulty 0–6 |

---

### Task 1: Shared rarity scale helpers + math check

**Files:**
- Create: `Shared/RarityScale.hh`
- Create: `Shared/RarityScale.cc`
- Create: `Scripts/check_rarity_scale.py`
- Modify: `Client/CMakeLists.txt` (add `../Shared/RarityScale.cc`)
- Modify: `Server/CMakeLists.txt` (add `../Shared/RarityScale.cc`)

**Interfaces:**
- Produces:
  - `float rarity_pow3(uint8_t rarity);` → `powf(3.f, rarity)`
  - `float petal_hp_mult(uint8_t r);` / `petal_damage_mult(uint8_t r);` → `rarity_pow3(r)`
  - `float mob_body_damage_mult(uint8_t r);` / `mob_xp_mult(uint8_t r);` → `rarity_pow3(r)`
  - `float mob_armor_mult(uint8_t r);` → `rarity_pow3(min(r, RarityID::kUltra))`
  - `float mob_hp_mult(uint8_t r);` → product of HP steps up to `r`
  - `uint8_t roll_spawn_rarity(uint8_t band_difficulty);` → band rules above (`band_difficulty` is 0…6)
- Consumes: `RarityID` from `Shared/StaticDefinitions.hh`

- [ ] **Step 1: Add the Python oracle (fail first by running before C++ exists is N/A — use as expected table)**

Create `Scripts/check_rarity_scale.py`:
```python
#!/usr/bin/env python3
"""Oracle for Shared/RarityScale multipliers. Exit 0 if tables match expected."""
HP_STEPS = [5, 5, 5, 5, 10, 45, 30, 5]  # Common→…→Unique (8 steps)

def pow3(r):
    return 3 ** r

def mob_hp(r):
    m = 1.0
    for i in range(r):
        m *= HP_STEPS[i]
    return m

def mob_armor(r):
    return pow3(min(r, 6))  # Ultra = 6

assert pow3(0) == 1 and pow3(2) == 9 and pow3(4) == 81
assert mob_hp(0) == 1
assert mob_hp(1) == 5
assert mob_hp(4) == 5**4  # Legendary
assert mob_hp(5) == 5**4 * 10
assert mob_hp(6) == 5**4 * 10 * 45
assert mob_armor(6) == mob_armor(7) == mob_armor(8) == 3**6
print('ok', {r: (pow3(r), mob_hp(r), mob_armor(r)) for r in range(9)})
```

- [ ] **Step 2: Run the oracle**

Run: `python3 Scripts/check_rarity_scale.py`  
Expected: prints `ok` and exits 0.

- [ ] **Step 3: Implement `Shared/RarityScale.hh` / `.cc`**

`Shared/RarityScale.hh`:
```cpp
#pragma once
#include <Shared/StaticDefinitions.hh>
#include <cstdint>
float rarity_pow3(uint8_t rarity);
float petal_hp_mult(uint8_t rarity);
float petal_damage_mult(uint8_t rarity);
float mob_body_damage_mult(uint8_t rarity);
float mob_xp_mult(uint8_t rarity);
float mob_armor_mult(uint8_t rarity);
float mob_hp_mult(uint8_t rarity);
uint8_t roll_spawn_rarity(uint8_t band_difficulty);
```

`Shared/RarityScale.cc` (include `<Helpers/Math.hh>` for `frand`, `<cmath>`):
```cpp
#include <Shared/RarityScale.hh>
#include <Helpers/Math.hh>
#include <cmath>

static float const HP_STEP[8] = {5,5,5,5,10,45,30,5};

float rarity_pow3(uint8_t rarity) {
    return std::pow(3.f, (float)rarity);
}
float petal_hp_mult(uint8_t r) { return rarity_pow3(r); }
float petal_damage_mult(uint8_t r) { return rarity_pow3(r); }
float mob_body_damage_mult(uint8_t r) { return rarity_pow3(r); }
float mob_xp_mult(uint8_t r) { return rarity_pow3(r); }
float mob_armor_mult(uint8_t r) {
    return rarity_pow3(r > RarityID::kUltra ? RarityID::kUltra : r);
}
float mob_hp_mult(uint8_t r) {
    float m = 1.f;
    for (uint8_t i = 0; i < r && i < 8; ++i) m *= HP_STEP[i];
    return m;
}
uint8_t roll_spawn_rarity(uint8_t band) {
    // band = Common..Ultra (0..6)
    if (band >= RarityID::kUltra) {
        uint8_t r = (frand() < 0.25f) ? RarityID::kMythic : RarityID::kUltra;
        if (r == RarityID::kUltra && frand() < 0.01f) {
            r = RarityID::kSuper;
            if (frand() < 0.01f) r = RarityID::kUnique;
        }
        return r;
    }
    if (frand() < 0.75f) return band;
    return (uint8_t)(band + 1);
}
```

- [ ] **Step 4: Wire CMake**

Add `../Shared/RarityScale.cc` next to `../Shared/StaticData.cc` in both `Client/CMakeLists.txt` and `Server/CMakeLists.txt`.

- [ ] **Step 5: Commit**

```bash
git add Shared/RarityScale.hh Shared/RarityScale.cc Scripts/check_rarity_scale.py Client/CMakeLists.txt Server/CMakeLists.txt
git commit -m "feat: Shared rarity scale helpers for petals and mobs"
```

---

### Task 2: Delete Massive Beetle, Massive Ladybug, Boulder

**Files:**
- Modify: `Shared/StaticDefinitions.hh` (MobID enum)
- Modify: `Shared/StaticData.cc` (remove three `MobData` entries; keep array order matching enum)
- Modify: `Shared/StaticData.hh` (spawn tables — temporary; Task 4 replaces zones)
- Modify: `Server/Process/Ai.cc`
- Modify: `Client/Assets/Mob.cc`
- Modify: `Client/Ui/TitleScreen/MobGallery.cc`

**Interfaces:**
- Consumes: remaining `MobID` values contiguous through `kNumMobs`
- Produces: codebase compiles with no references to deleted IDs

- [ ] **Step 1: Remove enum entries**

In `Shared/StaticDefinitions.hh` `namespace MobID`, delete `kMassiveLadybug`, `kMassiveBeetle`, `kBoulder` lines. Keep `kRock`. Ensure enum stays contiguous (no holes).

- [ ] **Step 2: Remove matching `MOB_DATA` structs**

In `Shared/StaticData.cc`, delete the three mob definitions that belonged to those IDs. Confirm `MOB_DATA.size()` still equals `MobID::kNumMobs` by build.

- [ ] **Step 3: Strip references**

- `Shared/StaticData.hh`: replace `{ MobID::kBoulder, W }` with added weight on `{ MobID::kRock, … }`; remove Massive* spawn lines; remove Tundra later in Task 4 (for now just compile).
- `Server/Process/Ai.cc`: remove `case MobID::kMassiveLadybug`, `kMassiveBeetle`, `kBoulder` (fold MassiveBeetle into Beetle case if it shared AI, else delete).
- `Client/Assets/Mob.cc`: remove Massive* from ladybug/beetle cases; remove `kBoulder` from rock case (keep `kRock` only).
- `Client/Ui/TitleScreen/MobGallery.cc`: change `kBeetle || kMassiveBeetle` → `kBeetle` only.

- [ ] **Step 4: Build**

Run:
```bash
source /Users/eason/emsdk/emsdk_env.sh
cd /Users/eason/Desktop/floir.xyz/Client/build && make -j4
cd /Users/eason/Desktop/floir.xyz/Server/build && make -j4
```
Expected: both succeed with no missing-enumerator / incomplete-switch errors (fix `-Wswitch` if enabled).

- [ ] **Step 5: Commit**

```bash
git add Shared/StaticDefinitions.hh Shared/StaticData.cc Shared/StaticData.hh \
  Server/Process/Ai.cc Client/Assets/Mob.cc Client/Ui/TitleScreen/MobGallery.cc
git commit -m "chore: delete Massive Beetle, Massive Ladybug, and Boulder"
```

---

### Task 3: Mob fields — armor, missile_damage, mob_rarity; scale alloc

**Files:**
- Modify: `Shared/StaticDefinitions.hh` (`MobAttributes`, `MobData` if armor on data vs attrs)
- Modify: `Shared/StaticData.cc` (Hornet `missile_damage = 10`; armor defaults 0)
- Modify: `Shared/EntityDef.hh` (`FIELDS_Mob` add `mob_rarity`)
- Modify: `Server/Spawn.hh` / `Server/Spawn.cc`
- Modify: `Server/Process/Flower.cc`
- Modify: `Server/Process/Ai.cc` (hornet missile)
- Modify: `Server/EntityFunctions/Death.cc`
- Modify: `Shared/Map.cc` (score_reward; rarity set in Task 4 — here accept rarity arg path)

**Interfaces:**
- Produces:
  - `MobAttributes::armor` (float, default 0), `MobAttributes::missile_damage` (float, default 0)
  - `Entity::get_mob_rarity()` / `set_mob_rarity(uint8_t)` via `FIELDS_Mob`
  - `Entity &alloc_petal(Simulation *, PetalID::T, Entity const &, uint8_t rarity);`
  - `Entity &alloc_drop(Simulation *, PetalID::T, uint8_t rarity);`
  - `__alloc_mob` applies `mob_hp_mult` / body / armor when `mob_rarity` already set on entity **or** takes rarity before stats (prefer: set rarity then scale inside `__alloc_mob` from a parameter)
- Preferred signature change:
  - `alloc_mob(..., uint8_t rarity = RarityID::kCommon, ...)` **or** set rarity in `on_spawn` **before** health assignment — cleaner: pass rarity into `__alloc_mob` and scale there.

- [ ] **Step 1: Extend definitions**

In `MobAttributes`:
```cpp
float armor = 0;
float missile_damage = 0;
```

In `FIELDS_Mob`:
```cpp
SINGLE(Mob, mob_id, MobID::T) \
SINGLE(Mob, mob_rarity, uint8_t)
```

Hornet in `StaticData.cc`:
```cpp
.attributes = {
    .missile_damage = 10,
    // keep existing aggro etc.
}
```

- [ ] **Step 2: Change spawn APIs**

`Spawn.hh`:
```cpp
Entity &alloc_drop(Simulation *, PetalID::T, uint8_t rarity);
Entity &alloc_petal(Simulation *, PetalID::T, Entity const &, uint8_t rarity);
Entity &alloc_mob(
    Simulation *, MobID::T, float, float,
    EntityID const, uint8_t rarity = 0,
    std::function<void(Entity &)> = nullptr
);
```

In `__alloc_mob`, after reading `data`:
```cpp
mob.set_mob_rarity(rarity);
float hp_m = mob_hp_mult(rarity);
float dmg_m = mob_body_damage_mult(rarity);
float arm_m = mob_armor_mult(rarity);
mob.health = mob.max_health = data.health.get_single(seed) * hp_m;
mob.damage = data.damage * dmg_m;
mob.armor = data.attributes.armor * arm_m;
mob.score_reward = (uint32_t)(data.xp * mob_xp_mult(rarity) + 0.5f);
```

`alloc_petal`:
```cpp
petal.health = petal.max_health = petal_data.health * petal_hp_mult(rarity);
petal.damage = petal_data.damage * petal_damage_mult(rarity);
```

`alloc_drop`:
```cpp
drop.set_drop_rarity(rarity);
entity_set_despawn_tick(drop, 10 * (2 + rarity) * TPS);
```

- [ ] **Step 3: Call sites**

- `Flower.cc`: `alloc_petal(sim, slot_petal_id, player, player.get_loadout_rarities(i));`
- Other `alloc_petal` call sites: pass `0` or parent mob rarity as appropriate (`Petal.cc` splits, etc. — grep and fix every call).
- `Death.cc` `_alloc_drops`: change to `alloc_drop(sim, id, rarity)` and pass `ent.get_mob_rarity()` from `entity_on_death`.
- `Ai.cc` hornet:
```cpp
Entity &missile = alloc_petal(sim, PetalID::kMissile, ent, ent.get_mob_rarity());
float md = MOB_DATA[ent.get_mob_id()].attributes.missile_damage
         * mob_body_damage_mult(ent.get_mob_rarity());
missile.damage = md;
missile.health = missile.max_health = md; // or keep HP separate if desired; use md for both for now matching old 10/10
```
- Update every `alloc_mob(...)` call to pass rarity (default Common for anthole children / digger until Task 4 wires zone roll).

- [ ] **Step 4: Build**

Same `make` commands as Task 2. Expected: success.

- [ ] **Step 5: Commit**

```bash
git add Shared/StaticDefinitions.hh Shared/StaticData.cc Shared/EntityDef.hh \
  Server/Spawn.hh Server/Spawn.cc Server/Process/Flower.cc Server/Process/Ai.cc \
  Server/EntityFunctions/Death.cc Shared/Map.cc
git commit -m "feat: apply instance rarity to petal and mob combat stats"
```

---

### Task 4: 21 zones + spawn rarity roll

**Files:**
- Modify: `Shared/StaticDefinitions.hh` (`MAX_DIFFICULTY = 6`)
- Modify: `Shared/StaticData.hh` (replace `MAP_DATA`)
- Modify: `Shared/Map.cc` (`spawn_random_mob` rolls rarity, passes to `alloc_mob`)
- Create: `Scripts/check_zone_layout.py`

**Interfaces:**
- Produces: `MAP_DATA` length 21; `difficulty` ∈ 0…6; names `"Garden · Common"` … `"Desert · Ultra"`
- Consumes: `roll_spawn_rarity`, `alloc_mob` rarity arg

**v1 biome AABBs (world units) and strip axes:**

| Biome | Rect `(left,top)-(right,bottom)` | Strip axis (Common → Ultra) |
|-------|----------------------------------|-----------------------------|
| Garden | `(0, 2500)-(12500, 14000)` | +x (west spawn → east) |
| Jungle | `(12500, 5000)-(25000, 25500)` | +y (north edge → south) |
| Desert | `(0, 14000)-(14000, 25500)` | +y (north edge → south) |

`get_zone_from_pos` keeps last-match order: emit Garden bands first, then Jungle, then Desert so overlays win.

Each biome’s 7 bands share the **same spawn weight table** (biome mobs only; Rock absorbs old Boulder weight).

Example Garden Common entry shape:
```cpp
{
    .left = 0, .top = 2500, .right = 12500/7.f, .bottom = 14000,
    .density = 1, .drop_multiplier = 0.3,
    .spawns = {
        { MobID::kRock, 510000 },
        { MobID::kLadybug, 100000 },
        { MobID::kBee, 100000 },
        { MobID::kBabyAnt, 25000 },
        { MobID::kCentipede, 10000 },
        { MobID::kSquare, 1 }
    },
    .difficulty = 0, .color = 0xff58c05c, .name = "Garden · Common"
},
```
Compute integer edges without gaps: for strip `i` in `0..6`,  
`left_i = L + i * (R-L) / 7`, `right_i = L + (i+1) * (R-L) / 7` (last strip uses `R` exactly).

Jungle / Desert analogous on Y. Colors: Garden green, Jungle green-dark, Desert sand; optionally darken per band.

- [ ] **Step 1: Write `Scripts/check_zone_layout.py`**

Parse is hard for C++ headers — instead assert after edit by grepping, or embed expected constants in the script duplicated from the plan:
```python
#!/usr/bin/env python3
import pathlib, re
text = pathlib.Path('Shared/StaticData.hh').read_text()
assert 'Tundra' not in text
assert text.count('.difficulty') == 21 or text.count('difficulty =') >= 21
for bad in ('kBoulder', 'kMassiveBeetle', 'kMassiveLadybug'):
    assert bad not in text
assert 'MAX_DIFFICULTY' not in text  # lives in StaticDefinitions
print('zone layout grep checks ok')
```
Also assert `MAX_DIFFICULTY = 6` in `StaticDefinitions.hh`.

- [ ] **Step 2: Set `MAX_DIFFICULTY = 6`**

In `Shared/StaticDefinitions.hh`.

- [ ] **Step 3: Replace `MAP_DATA`**

Rewrite the `inline std::array const MAP_DATA = …` block in `Shared/StaticData.hh` with 21 zones as specified. Remove Tundra entirely.

- [ ] **Step 4: Wire spawn rarity in `Map::spawn_random_mob`**

```cpp
uint8_t rarity = roll_spawn_rarity((uint8_t)zone.difficulty);
Entity &ent = alloc_mob(sim, s.id, x, y, NULL_ENTITY, rarity, [&](Entity &mob){
    mob.zone = zone_id;
    mob.immunity_ticks = TPS;
    BitMath::set(mob.flags, EntityFlags::kSpawnedFromZone);
    BitMath::set(mob.flags, EntityFlags::kHasCulling);
    sim->zone_mob_counts[zone_id]++;
    // score_reward already set in __alloc_mob from rarity
});
```
Remove the old `mob.score_reward = MOB_DATA[...].xp` overwrite.

Anthole / digger / other `alloc_mob` sites: pass `roll_spawn_rarity` from parent zone if available, else parent `get_mob_rarity()`, else Common.

- [ ] **Step 5: Run checks + build**

```bash
python3 Scripts/check_zone_layout.py
# also fix script to read MAX_DIFFICULTY from StaticDefinitions.hh
source /Users/eason/emsdk/emsdk_env.sh
cd Client/build && make -j4 && cd ../../Server/build && make -j4
```
Expected: script ok; builds succeed.

- [ ] **Step 6: Commit**

```bash
git add Shared/StaticDefinitions.hh Shared/StaticData.hh Shared/Map.cc Scripts/check_zone_layout.py
git commit -m "feat: 21 biome difficulty zones with spawn rarity rolls"
```

---

### Task 5: Info boxes always show combat stats

**Files:**
- Modify: `Client/Ui/InGame/Tooltip.cc`
- Modify: `Client/Ui/TitleScreen/MobGallery.cc`

**Interfaces:**
- Consumes: `PETAL_DATA` / `MOB_DATA` Common bases; `missile_damage`, `armor`
- Produces: release builds show petal HP/Damage/specials/Reload; mob HP/Body damage/Missile/Armor/XP

- [ ] **Step 1: Petal tooltip**

In `make_petal_tooltip`:
- Always use the `HFlexContainer` title row that shows reload (remove `#ifdef DEBUG` around it).
- Replace `DEBUG_ONLY(make_petal_stat_container(id))` with unconditional `make_petal_stat_container(id)`.

Ensure container always includes Health, Damage (when > 0), and that reload is visible. Existing special lines stay.

- [ ] **Step 2: Mob gallery stats**

Replace `DEBUG_ONLY(make_mob_stat_container(id),)` with unconditional call.

Update `make_mob_stat_container`:
```cpp
stats.push_back(/* Health */);
stats.push_back(/* Body Damage: label "Body Damage:" using mob_data.damage */);
if (attrs.missile_damage > 0) {
    stats.push_back(HContainer({
        StaticText(12, "Missile Damage:", {.fill=0xffff7777}),
        StaticText(12, format_number(attrs.missile_damage))
    }, 0, 5, {.h_justify=Style::Left}));
}
if (attrs.armor > 0) {
    stats.push_back(/* Armor */);
}
stats.push_back(/* XP */);
// keep poison line if present
```

- [ ] **Step 3: Build client**

`cd Client/build && make -j4`  
Expected: success.

- [ ] **Step 4: Commit**

```bash
git add Client/Ui/InGame/Tooltip.cc Client/Ui/TitleScreen/MobGallery.cc
git commit -m "feat: always show petal and mob combat stats in info boxes"
```

---

### Task 6: Map collision spot-check + regenerate if needed

**Files:**
- Modify (only if gaps found): `tiles/tileset.tsj` objectgroups for water/bush/cliff/dirt/castle tiles
- Regenerate: `Shared/Tilemap.hh`, `Server/map-data.json` via `Scripts/gen_map.py`

**Interfaces:**
- Consumes: existing `BLOCK_LAYERS`, `TILE_COLL`
- Produces: collision mask matching visible solids; no silly gaps / irrational overlaps

- [ ] **Step 1: Regenerate**

```bash
python3 Scripts/gen_map.py
```
Expected: prints tile/placement counts; rewrites `Tilemap.hh` and `map-data.json`.

- [ ] **Step 2: ASCII / sampling spot-check**

Add a one-off in Python or reuse prior technique: sample `solid_at` equivalents along known water/cliff edges from exported mask. Minimum: document 3 coordinates that must be solid and 3 walkable adjacent cells; verify with a tiny Python reader of `CELL_MASK`/`SUBMASK` **or** in-game G-key + minimap after local server run.

If silly gaps found between adjacent block tiles, expand/fix the tileset objectgroup polygons (not full-cell squares), then regenerate.

- [ ] **Step 3: Build + stage client**

```bash
source /Users/eason/emsdk/emsdk_env.sh
cd Client/build && make -j4
cd ../../Server/build && make -j4
cp ../../Client/build/floir-client.js ../../Client/build/floir-client.wasm .
```

- [ ] **Step 4: Commit**

```bash
git add Shared/Tilemap.hh Server/map-data.json tiles/tileset.tsj
git commit -m "fix: polish block-tile collision to visible outlines"
```
(Skip commit if generator output is bitwise-identical and no tileset edits.)

---

### Task 7: End-to-end verification

**Files:** none (manual / script)

- [ ] **Step 1: Rarity math**

`python3 Scripts/check_rarity_scale.py` → ok  
`python3 Scripts/check_zone_layout.py` → ok

- [ ] **Step 2: Local smoke**

Run server (`node Server/run-server.js` or project’s usual entry). In browser:
1. Spawn in Garden west — mostly Common/Uncommon mobs.
2. Equip Basic at rarity 0 vs manually set higher rarity (or drop) — petal HP/damage scale ×3ⁿ.
3. Kill a mob — drop rarity matches mob rarity name color if shown.
4. Open Petal + Mob galleries — stats visible without DEBUG build.
5. Walk water/cliff/bush/dirt/castle edges — blocked on visible solid only.
6. Confirm Massive Beetle / Massive Ladybug / Boulder absent from Mob gallery.

- [ ] **Step 3: Final commit if stray fixes**

Only if verification required code fixes; message should state the fix.

---

## Spec coverage checklist

| Spec requirement | Task |
|------------------|------|
| Visible-only block collision + no silly gaps | 6 |
| Petal ×3 HP/Damage by instance rarity | 1, 3 |
| Mob HP/body/armor/XP multipliers | 1, 3 |
| Armor subtract on contact; poison ignores | already in `Damage.cc`; armor scaled in 3 |
| Same petal/mob ID all rarities | 3, 4 |
| 21 zones, 3 biomes, spawn rolls | 4 |
| Drop rarity = mob rarity | 3 |
| Info boxes stats | 5 |
| Delete Massive Beetle/Ladybug/Boulder; keep Rock | 2 |
| Missile damage on mob info + combat | 3, 5 |
| No UniqueBasic merge; no Rock redraw; specials deferred | non-goals (no task) |
