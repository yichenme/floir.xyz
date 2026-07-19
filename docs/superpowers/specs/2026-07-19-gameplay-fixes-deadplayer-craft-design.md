# Gameplay Fixes: DeadPlayer, Yggdrasil, Craft, Balance

Date: 2026-07-19  
Status: Approved for planning  
Approach: Targeted patch set across Shared / Server / Client (keep flower entity in a dead state; no separate corpse entity type)

## Goals

Ship a batch of balance and behavior fixes: spider poison nerf, dead-player corpse + death UI Close, Yggdrasil revive, Unique Queen spawn spam, ant-hole full one-shot, egg summon targeting, player HP formula, Stick sandstorm aggro, ramming knockback nerf, remove show-grid, craft UX matching florr-style five-slot flow.

## Decisions (locked)

| Topic | Decision |
|-------|----------|
| Architecture | Approach 1: dead state on existing flower entity |
| Death Close | Hide death overlay; corpse stays in-world; Yggdrasil can still revive |
| Spectate | Dead players do **not** enter spectator mode; camera stays on corpse |
| Death buttons | Keep green **Continue** (leave to title); add gray **Close** below it (same chrome, not an X) |
| Knockback | 0.5× current ramming knockback |
| Sandstorm range | Summoned Stick sandstorms chase mobs within **250** |
| Ant hole one-shot | Only when hole goes **100% HP → 0** in one hit: spawn **no** waves for that hit |
| Craft visuals | Match screenshot layout/structure; do **not** copy screenshot colors or X button |

---

## 1. DeadPlayer / death / Yggdrasil

### Dead state

On fatal damage to a real player flower (`kFlower` && !`kMob`):

- Do **not** `request_delete` the flower.
- Do **not** enter spectator.
- Enter a dead state (flag / component field, e.g. `dead` or `EntityFlags::kDead`):
  - Immobile: ignore movement input.
  - Face: dead eyes (X) + defending mouth (`FaceFlags::kDeadEyes` + defending mouth pose).
  - No combat / petal behavior until revived (or player leaves via Continue).
- Camera remains on the dead flower.
- Inventory / score persistence behavior that currently runs on flower death must still run appropriately once (on entering dead state), not on every tick.

### Death UI (`Client/Ui/TitleScreen/DeathScreen.cc`)

- Keep **Continue** (green): sets `on_game_screen = 0` (title) as today.
- Add **Close** below Continue: same button size/style, **gray** fill, label `"Close"`.
- Close sets a client flag (e.g. `death_ui_dismissed`) so the death overlay hides while still dead.
- While dismissed and still dead: game world remains visible; corpse stays.
- On a later death after respawn: show death UI again (clear dismissed flag on enter dead / on respawn).
- Hint under buttons: keep “(or press ENTER to continue)” referring to **Continue** / Enter leaving the game. **Close** only dismisses the overlay.

### Yggdrasil

- Delete the commented lethal-hit 25% revive clause (unused).
- When a **loaded** Yggdrasil petal contacts a **dead** flower:
  - Petal sticks to that flower (existing stick-on-hit or equivalent petal attach behavior).
  - Dead flower revives: clear dead state, **health = 100% max_health** (no extra immunity unless an existing revive path already requires it).
  - Yggdrasil starts reload again (existing Unique reload / rarity scaling).
- Ally/self-team dead flowers are valid revive targets (not hostile-only).
- Update petal description if it still claims powers are useless.

### Key files

- Server: `EntityFunctions/Death.cc`, `EntityFunctions/Damage.cc`, `Process/Flower.cc`, `Process/Health.cc`, `Process/Motion.cc` (or input path)
- Client: `DeathScreen.cc`, `Game.cc`, `RenderFlower.cc` / `Assets/Flower.cc`, face flag sync
- Shared: Yggdrasil petal data / description in `StaticData.cc`

---

## 2. Balance / AI / misc

### Spider poison

- In `MOB_DATA` spider attributes: `poison_damage.damage` `5.0 → 3.75` (0.75×). Time unchanged (3s).

### Player HP

Replace `hp_at_level` with:

```
200 * (243^0.01)^(min(n, 100) - 1)
```

- Cap the level used for HP at **100** (replace current 75 cap / old `200 * 3^(0.05*(level-1))` formula).

### Unique Queen Super Soldier announce

- `_announce_spawn` must not fire for Super Soldier Ants spawned by Unique Queen (parented / summon path).
- Preferred: skip announce when mob has a parent or is marked summon / temporary queen child — so Unique Queen waves stop chat spam without silencing wild Super+ spawns.

### Ant hole one-shot

In `Damage.cc` anthole wave logic:

- If `old_health >= max_health` (full bar) **and** `health <= 0` after the hit → **do not spawn any waves** for that hit (skip the loop; do not set `end = num_waves + 1`).
- Otherwise keep existing threshold-based wave spawning for gradual damage / non-full one-shots.

### Egg summons target mobs only

- Ant Egg / Beetle Egg summons (soldier ant / beetle with `is_summon`) find nearest **enemy mob** only — not enemy flowers.
- Implement via summon-specific detection helper or a `mobs_only` parameter on `find_nearest_enemy`, used by summon AI paths only.

### Stick sandstorm

- If `is_summon` and `MobID::kSandstorm`: chase nearest **mob** within detection radius **250**.
- No target: keep current wander + parent accel blend.
- Wild (non-summon) sandstorms: unchanged (no chase).

### Ramming knockback

- Apply **0.5×** to current ramming knockback path (`_deal_knockback` / `_cancel_movement` effective scale, or equivalent halving of `STANDING_PUSH_SCALE` and matching factors so net effect is half).

### Remove show grid

- Delete settings toggle “Show grid”.
- Delete `Input::show_grid`, storage persistence bit, and grid drawing in `Client/Rendering.cc`.

### Key files

- Shared: `StaticData.cc` (spider, HP, sandstorm aggro if stored there)
- Server: `Spawn.cc`, `Damage.cc`, `Detection.cc`, `Ai.cc`, `Collision.cc`
- Client: `Settings.cc`, `Input.*`, `Storage.cc`, `Rendering.cc`

---

## 3. Craft UI + flow

### Layout (structure from screenshots; keep existing colors/style)

- Title **Craft**
- Five craft slots + **Craft** button with success % under button
- Instruction text:
  - Combine 5 of the same petal to craft an upgrade
  - Failure will destroy 1–4 petals
- Inventory / recipe grid below

### Interactions

Two equivalent ways to craft (slots always reflect the attempt):

| Input | Behavior |
|-------|----------|
| Click craftable inventory petal | Fill **5** craft slots **and** immediately attempt one craft (`amount = 5`) |
| Shift-click inventory petal | Fill slots for the stack **and** attempt all available (`amount = owned count`; server loops while ≥5) |
| Click **Craft** (with a selection already in slots) | Attempt craft for the current selection: single-5 if 5 placed, or all if shift-filled |

Server `try_craft` already loops `while remaining >= 5` with success (−5, +1 higher rarity) or fail (lose 1–4). Client must send the correct `amount` for shift-all vs single-5.

### Out of scope for craft

- Changing craft success chance formula
- Copying screenshot colors or red X close control

### Key files

- Client: `Client/Ui/InGame/Craft.cc`, `Network.cc` (`send_craft`)
- Server: `EntityFunctions/CraftOps.cc` (verify amount handling only if needed)

---

## Testing checklist

- [ ] Spider poison DoT is 3.75/s for 3s
- [ ] HP at levels 1, 50, 100 matches new formula; level >100 uses level-100 HP
- [ ] Die → corpse with X eyes + defend mouth, cannot move, no spectate
- [ ] Close hides death UI; Continue still leaves; Yggdrasil can revive closed corpse at 100% HP and reloads
- [ ] Unique Queen does not spam Super Soldier spawn messages
- [ ] Ant hole deleted from full HP in one hit spawns zero ants
- [ ] Ant/Beetle egg summons attack mobs, not players
- [ ] Stick sandstorms chase mobs in 250; wild sandstorms do not
- [ ] Ramming knockback feels ~half
- [ ] No Show grid setting; no grid drawn
- [ ] Craft: click places 5, shift places all, Craft / click / shift-click attempt behaviors match table

## Non-goals

- Redesigning death screen art beyond Close button
- New spectator camera modes
- Changing craft RNG formula
- Broader AI retargeting beyond egg summons and Stick sandstorms
