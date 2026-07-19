# Gameplay Fixes: DeadPlayer, Yggdrasil, Craft, Balance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the approved 2026-07-19 gameplay batch: spider poison nerf, dead-flower corpse + Close UI, Yggdrasil revive, Queen spawn-message silence, ant-hole full one-shot, egg/sandstorm targeting, HP formula, knockback 0.5×, remove show-grid, craft click/shift-click flow.

**Architecture:** Targeted patches across Shared (numbers + synced `dead` field), Server (death/AI/collision/craft), Client (death UI, craft, grid removal). Dead players keep their flower entity with a synced `dead` flag instead of spawning a separate corpse type. Yggdrasil revive clears that flag at 100% HP.

**Tech Stack:** C++20, Shared/Server/Client gardn-style layout, emscripten WASM builds, plain-`assert` helpers in `Scripts/test_shared_helpers.cc`.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-07-19-gameplay-fixes-deadplayer-craft-design.md`
- Do not invent duplicate balance numbers on client vs server — Shared only.
- New tick/system concerns → own modules when adding large logic; small flag checks may stay in existing death/health/AI files.
- No feature flags / `_old` shims; delete unused code (commented Yggdrasil 25% clause, show-grid).
- Craft: match screenshot **structure**, not colors / not red X.
- Knockback: **0.5×**. Sandstorm summon aggro: **250**. Ant-hole no-spawn only on **100%→0** one-hit.
- Build (from `DEPLOY.md`):
  ```sh
  source /Users/eason/emsdk/emsdk_env.sh
  cd Client/build && make
  cd ../../Server/build && make
  ```
- Helper tests:
  ```sh
  c++ -std=c++20 -I. Scripts/test_shared_helpers.cc Shared/AccountValidation.cc Shared/StackFormat.cc Shared/RarityScale.cc Shared/StaticData.cc -lm -o /tmp/test_shared_helpers && /tmp/test_shared_helpers
  ```
  (Adjust link list if the local test binary already has a Makefile target — match whatever currently builds `test_shared_helpers`.)

---

## File map

| Area | Files |
|------|--------|
| Shared balance / schema | `Shared/StaticData.cc`, `Shared/EntityDef.hh`, `Shared/StaticDefinitions.hh` (if new flag) |
| Tests | `Scripts/test_shared_helpers.cc` |
| Death / Ygg | `Server/Process/Health.cc`, `Server/EntityFunctions/Death.cc`, `Server/EntityFunctions/Damage.cc`, `Server/Process/Flower.cc`, `Server/Process/Collision.cc` or petal contact path, `Server/Client.cc`, `Server/Process/Camera.cc` |
| AI / spawn | `Server/EntityFunctions/Detection.cc`, `Server/EntityFunctions.hh`, `Server/Process/Ai.cc`, `Server/Spawn.cc` |
| Knockback | `Server/Process/Collision.cc` |
| Client death | `Client/Ui/TitleScreen/DeathScreen.cc`, `Client/Game.cc`, `Client/Game.hh`, `Client/Render/RenderFlower.cc`, `Client/Simulation.cc` |
| Grid removal | `Client/Input.hh`, `Client/Input.cc`, `Client/Storage.cc`, `Client/Ui/TitleScreen/Settings.cc`, `Client/Rendering.cc` |
| Craft | `Client/Ui/InGame/Craft.cc`, `Client/Network.cc` (only if `send_craft` amount API needs tweak) |

---

### Task 1: Spider poison 0.75× + HP formula

**Files:**
- Modify: `Shared/StaticData.cc` (spider ~L983–986, `hp_at_level` ~L1222–1234)
- Modify: `Scripts/test_shared_helpers.cc`

**Interfaces:**
- Produces: `hp_at_level(n)` = `200 * pow(pow(243.0f, 0.01f), (float)(std::min(n, 100u) - 1))` for `n >= 1`; level used for HP capped at 100.

- [ ] **Step 1: Add failing HP assertions**

In `Scripts/test_shared_helpers.cc`, include `<Shared/StaticData.hh>` (or the header that declares `hp_at_level`) and before `std::cout << "ok\n"`:

```cpp
// New HP: 200 * (243^0.01)^(min(n,100)-1)
assert(std::fabs(hp_at_level(1) - 200.0f) < 1e-3f);
float const hp100 = 200.0f * std::pow(std::pow(243.0f, 0.01f), 99.0f);
assert(std::fabs(hp_at_level(100) - hp100) < 1.0f);
assert(std::fabs(hp_at_level(200) - hp_at_level(100)) < 1e-3f);
assert(hp_at_level(50) > hp_at_level(1));
assert(hp_at_level(50) < hp_at_level(100));
```

- [ ] **Step 2: Run test — expect FAIL on old formula**

```sh
# use project’s existing compile line for test_shared_helpers
```

Expected: assertion failure on `hp_at_level(1)` or cap mismatch.

- [ ] **Step 3: Implement Shared changes**

Spider poison:

```cpp
.poison_damage = {
    .damage = 3.75f,  // was 5.0; 0.75x
    .time = 3.0
}
```

`hp_at_level`:

```cpp
float hp_at_level(uint32_t level) {
    if (level < 1) level = 1;
    if (level > 100) level = 100;
    return 200.0f * std::pow(std::pow(243.0f, 0.01f), (float)level - 1.0f);
}
```

Delete the old 75-cap / `3^(0.05*(n-1))` / linear-past-99 branches.

- [ ] **Step 4: Re-run test — expect PASS (`ok`)**

- [ ] **Step 5: Commit**

```bash
git add Shared/StaticData.cc Scripts/test_shared_helpers.cc
git commit -m "$(cat <<'EOF'
Nerf spider poison 0.75x and update player HP formula.

EOF
)"
```

---

### Task 2: Ant-hole one-shot, Queen announce, egg/sandstorm targeting, knockback

**Files:**
- Modify: `Server/EntityFunctions/Damage.cc` (anthole block ~L56–79)
- Modify: `Server/Spawn.cc` (`_announce_spawn` ~L115–123; call site)
- Modify: `Server/Process/Ai.cc` (queen spawn ~L488–503; `tick_sandstorm` ~L329–370; soldier/beetle cases)
- Modify: `Server/EntityFunctions/Detection.cc`
- Modify: `Server/EntityFunctions.hh`
- Modify: `Server/Process/Collision.cc` (`_deal_knockback` / `_cancel_movement` / `STANDING_PUSH_SCALE`)

**Interfaces:**
- Produces: `EntityID find_nearest_enemy(Simulation *, Entity const &, float radius, bool mobs_only = false);`
- Sandstorm summon detection radius constant: `250.f` (can be local in `tick_sandstorm` or Shared next to `SUMMON_RETREAT_RADIUS`).

- [ ] **Step 1: Ant-hole full one-shot skip**

In the anthole block, before computing waves:

```cpp
if (defender.has_component(kMob) && defender.get_mob_id() == MobID::kAntHole) {
    // Full-bar one-shot: no wave spawns at all for this hit.
    if (old_health >= defender.max_health && defender.health <= 0)
        ; // skip spawn loop entirely
    else {
        uint32_t const num_waves = ANTHOLE_SPAWNS.size() - 1;
        uint32_t start = ceilf((defender.max_health - old_health) / defender.max_health * num_waves);
        uint32_t end = ceilf((defender.max_health - defender.health) / defender.max_health * num_waves);
        if (defender.health <= 0) end = num_waves + 1;
        // ... existing spawn loop unchanged ...
    }
}
```

- [ ] **Step 2: Silence Unique Queen Super Soldier announce**

`_announce_spawn` currently fires for `team == NULL_ENTITY` Super+ mobs. Queen soldiers inherit wild `NULL_ENTITY` team.

Change announce to accept the spawned entity (after `on_spawn`) and skip when it has a parent **or** `kHasCulling` (queen/hole children set this in `on_spawn`):

```cpp
static void _announce_spawn(Entity &mob) {
    uint8_t const rarity = mob.get_mob_rarity();
    if (rarity < RarityID::kSuper) return;
    if (!(mob.get_team() == NULL_ENTITY)) return;
    if (!(mob.get_parent() == NULL_ENTITY)) return;
    if (BitMath::at(mob.flags, EntityFlags::kHasCulling)) return;
    // ... existing message ...
}
```

In queen AI, set parent to the **queen** inside `on_spawn` (capture `ent.id`) so parent is non-null before announce:

```cpp
EntityID const queen_id = ent.id;
Entity &spawned = alloc_mob(
    sim, MobID::kSoldierAnt, ...,
    [queen_id](Entity &mob) {
        BitMath::set(mob.flags, EntityFlags::kHasCulling);
        mob.set_parent(queen_id);
    });
entity_set_despawn_tick(spawned, 10 * TPS);
// remove the old spawned.set_parent(ent.get_parent());
```

Update all `_announce_spawn(...)` call sites in `Spawn.cc` to pass the entity.

- [ ] **Step 3: `find_nearest_enemy` mobs-only + summon use**

```cpp
EntityID find_nearest_enemy(Simulation *simulation, Entity const &entity, float radius, bool mobs_only) {
    // same as today, but:
    if (mobs_only) {
        if (!ent.has_component(kMob)) return;
    } else {
        if (!ent.has_component(kMob) && !ent.has_component(kFlower)) return;
    }
    // also skip dead flowers if/when Flower.dead exists (Task 3): if (ent.has_component(kFlower) && ent.get_dead()) return;
}
```

Update declaration in `EntityFunctions.hh` with default `mobs_only = false`.

In `tick_default_aggro` (or only soldier/beetle cases): when `ent.get_is_summon()`, call `find_nearest_enemy(..., true)`.

- [ ] **Step 4: Stick sandstorm chase mobs in 250**

At start of `tick_sandstorm`, if `ent.get_is_summon()`:

```cpp
float const SANDSTORM_AGGRO = 250.f;
if (!sim->ent_alive(ent.target))
    ent.target = find_nearest_enemy(sim, ent, SANDSTORM_AGGRO + ent.get_radius(), true);
if (sim->ent_alive(ent.target)) {
    Entity &target = sim->get_ent(ent.target);
    Vector v(target.get_x() - ent.get_x(), target.get_y() - ent.get_y());
    v.set_magnitude(PLAYER_ACCELERATION * 0.95f);
    ent.acceleration = v;
    // still allow parent blend below
} else {
    // existing wander switch
}
```

Wild (non-summon) path unchanged.

- [ ] **Step 5: Knockback 0.5×**

Single change only (do not also halve other multipliers):

```cpp
static float const STANDING_PUSH_SCALE = 0.025f; // was 0.05 → 0.5x net knockback
```

Leave `_deal_knockback`’s `PLAYER_ACCELERATION * 2 * _push_factor(...)` and `_cancel_movement` formulas unchanged so the nerf is exactly half, not quarter.

- [ ] **Step 6: Build Server**

```sh
source /Users/eason/emsdk/emsdk_env.sh && cd Server/build && make
```

Expected: success.

- [ ] **Step 7: Commit**

```bash
git add Server/EntityFunctions/Damage.cc Server/Spawn.cc Server/Process/Ai.cc \
  Server/EntityFunctions/Detection.cc Server/EntityFunctions.hh Server/Process/Collision.cc
git commit -m "$(cat <<'EOF'
Fix anthole one-shot, queen announce spam, summon targeting, knockback.

EOF
)"
```

---

### Task 3: Dead flower state (server)

**Files:**
- Modify: `Shared/EntityDef.hh` (`FIELDS_Flower` — add `dead`)
- Modify: `Server/Process/Health.cc`
- Modify: `Server/EntityFunctions/Death.cc` (extract persist; skip double work on corpse delete)
- Modify: `Server/Process/Flower.cc` (no movement/petals when dead; face flags)
- Modify: `Server/Client.cc` (`Client::alive`, `kLeave`, `kClientSpawn`)
- Modify: `Server/Process/Collision.cc` (optional: dead flowers deal/take no contact damage)

**Interfaces:**
- Produces: synced `Flower.dead` (`uint8_t`) via `get_dead()` / `set_dead()`.
- Produces: `void enter_player_dead_state(Simulation *, Entity &flower)` — persist once, clear petals/acceleration, set face, `set_dead(1)`, keep entity.

- [ ] **Step 1: Add synced field**

In `Shared/EntityDef.hh` `FIELDS_Flower`:

```cpp
SINGLE(Flower, dead, uint8_t) \
```

(Regenerate / rebuild so Entity getters exist — this project’s macros expand in place; just rebuild.)

- [ ] **Step 2: Enter dead instead of delete**

In `tick_health_behavior`:

```cpp
if (ent.health <= 0) {
    if (ent.has_component(kFlower) && !ent.has_component(kMob) && !ent.get_dead()) {
        enter_player_dead_state(sim, ent);
    } else if (!(ent.has_component(kFlower) && ent.get_dead())) {
        sim->request_delete(ent.id);
    }
}
```

Implement `enter_player_dead_state` in `Death.cc` (or new `Server/EntityFunctions/DeadPlayer.cc` if Death.cc is already large — prefer new file if Death.cc > ~400 lines after edit):

- Call the existing flower persistence block once (inventory/score/account) — refactor out of `entity_on_death`’s flower branch into a shared helper, and from `entity_on_death` call it only when `!ent.get_dead()` **or** when deleting a non-yet-persisted flower. Simplest: set a server-only `uint8_t death_persisted` on the entity if available, else `if (ent.get_dead())` skip flower persist on delete.
- `ent.set_dead(1)`
- `ent.health = 0` (keep ratio 0)
- Despawn active petal entities / clear loadout spawn state so corpse has no orbiting petals
- Zero input acceleration; set face flags dead eyes + defending
- Do **not** `request_delete`

When a dead flower is later deleted (`kLeave` / cleanup), `entity_on_death` flower branch must **not** double-write progress — gate with `if (ent.get_dead()) { /* only petal tracker cleanup if needed */ return; }` or equivalent.

- [ ] **Step 3: Immobile + face while dead**

In `tick_player_behavior` / Flower process early-out:

```cpp
if (player.get_dead()) {
    player.input = 0;
    player.acceleration.set(0, 0);
    player.set_face_flags((1 << FaceFlags::kDeadEyes) | (1 << FaceFlags::kDefending));
    // client mouth uses kDefending → mouth lerp 8 (unsmile)
    return;
}
```

- [ ] **Step 4: Server alive / spawn / leave**

```cpp
uint8_t Client::alive() {
    ...
    EntityID pid = simulation->get_ent(camera).get_player();
    if (!simulation->ent_exists(pid)) return false;
    Entity &p = simulation->get_ent(pid);
    if (p.has_component(kFlower) && p.get_dead()) return false;
    return true;
}
```

`kLeave`: still `request_delete` camera player if exists (including dead corpse).

`kClientSpawn`: if camera has a dead player entity, `request_delete` it first, then spawn fresh (or rely on `!alive()` after Task 3 + delete-on-Continue from client).

- [ ] **Step 5: Dead flowers are not valid combat targets for contact** (recommended)

In collision damage section / `find_nearest_enemy`: skip `ent.get_dead()` flowers so corpses are not farmed and eggs do not target them.

- [ ] **Step 6: Build Server**

- [ ] **Step 7: Commit**

```bash
git add Shared/EntityDef.hh Server/Process/Health.cc Server/EntityFunctions/Death.cc \
  Server/Process/Flower.cc Server/Client.cc Server/Process/Collision.cc \
  Server/EntityFunctions/Detection.cc
# plus DeadPlayer.cc if created
git commit -m "$(cat <<'EOF'
Keep dead players as immobile corpses instead of deleting them.

EOF
)"
```

---

### Task 4: Death UI Close + client dead rendering

**Files:**
- Modify: `Client/Ui/TitleScreen/DeathScreen.cc`
- Modify: `Client/Game.hh` / `Client/Game.cc`
- Modify: `Client/Render/RenderFlower.cc`
- Modify: `Client/Simulation.cc` (mouth: dead eyes should use defend mouth — already if `kDefending` synced)
- Modify: `Client/Network.cc` if Continue should call `leave_game`

**Interfaces:**
- Produces: `Game::death_ui_dismissed` (`uint8_t`), cleared when becoming alive again / on new death.

- [ ] **Step 1: `Game::alive` respects `dead`**

```cpp
uint8_t Game::alive() {
    if (!(socket.ready && simulation_ready && simulation.ent_exists(camera_id))) return 0;
    EntityID pid = simulation.get_ent(camera_id).get_player();
    if (!simulation.ent_alive(pid)) return 0;
    Entity const &p = simulation.get_ent(pid);
    if (p.has_component(kFlower) && p.get_dead()) return 0;
    return 1;
}
```

Add helper if useful:

```cpp
uint8_t Game::player_is_dead_corpse() {
    ...
    return p.get_dead();
}
```

- [ ] **Step 2: Death screen buttons**

```cpp
Ui::Element *continue_button = new Ui::Button(
    145, 40, new Ui::StaticText(28, "Continue"),
    [](Element *elt, uint8_t e){
        if (e == Ui::kClick && Game::on_game_screen)
            Game::leave_game(); // deletes corpse server-side + title
    },
    [](){ return !Game::in_game(); },
    {.fill = 0xff1dd129, .line_width = 5, .round_radius = 3 }
);
Ui::Element *close_button = new Ui::Button(
    145, 40, new Ui::StaticText(28, "Close"),
    [](Element *elt, uint8_t e){
        if (e == Ui::kClick) Game::death_ui_dismissed = 1;
    },
    [](){ return !Game::in_game(); },
    {.fill = 0xff888888, .line_width = 5, .round_radius = 3 }
);
```

`should_render` for the death container:

```cpp
.should_render = [](){
    return Game::player_is_dead_corpse()
        && !Game::death_ui_dismissed
        && Game::should_render_game_ui();
}
```

Clear `death_ui_dismissed = 0` when `get_dead()` transitions 0→1 (detect in `Game::tick` / render path). Clear when alive again.

Dim overlay in `Game.cc` (`!Game::alive()` fill): only when `!death_ui_dismissed` **or** when not a corpse (title transitions). Spec: world visible after Close → skip dim when `death_ui_dismissed && player_is_dead_corpse()`.

- [ ] **Step 3: In-world dead face**

In `RenderFlower.cc`, if `ent.get_dead()`, force `kDeadEyes` (and rely on server `kDefending` for mouth). Remove dependence on `deletion_animation` alone for the corpse case.

In `Client/Simulation.cc` mouth lerp: if `kDeadEyes`, force defend mouth (`mouth → 8`) even without separate defending bit:

```cpp
if (BitMath::at(face_flags, FaceFlags::kDeadEyes))
    mouth = lerp(mouth, 8, amt);
else if ...
```

- [ ] **Step 4: Enter while dead**

Today Enter calls `spawn_in` when `!alive()`. With corpse + dismissed UI, Enter would respawn — keep that (matches old “press ENTER to continue” leave/respawn intent) **or** only allow Enter to leave when death UI visible. Spec: hint refers to Continue/Enter leaving. Wire Enter when `player_is_dead_corpse()` to `leave_game()` (same as Continue), not silent respawn on top of corpse — then title → user clicks play to `spawn_in`.

```cpp
else if (Game::player_is_dead_corpse())
    Game::leave_game();
else if (!Game::alive())
    Game::spawn_in();
```

- [ ] **Step 5: Build Client**

- [ ] **Step 6: Commit**

```bash
git add Client/Ui/TitleScreen/DeathScreen.cc Client/Game.cc Client/Game.hh \
  Client/Render/RenderFlower.cc Client/Simulation.cc Client/Network.cc
git commit -m "$(cat <<'EOF'
Add death Close button and render dead flowers with X eyes.

EOF
)"
```

---

### Task 5: Yggdrasil revive on dead flowers

**Files:**
- Modify: `Shared/StaticData.cc` (Yggdrasil description ~L657–668)
- Modify: `Server/EntityFunctions/Damage.cc` (delete commented 25% clause + unused `_yggdrasil_revival_clause` **or** replace with contact revive helper)
- Modify: `Server/Process/Collision.cc` and/or `Server/Process/Petal.cc` / Flower petal contact — wherever petal-vs-flower overlap is handled
- Modify: `Server/Process/Flower.cc` (reload after revive consume)

**Interfaces:**
- Produces: `bool try_yggdrasil_revive(Simulation *, Entity &petal, Entity &dead_flower)` — on success: stick petal (despawn petal entity / mark slot reloading), `dead_flower.set_dead(0)`, `dead_flower.health = dead_flower.max_health`, clear death face, start Yggdrasil reload on owner.

- [ ] **Step 1: Contact revive**

When a petal with `petal_id == kYggdrasil` overlaps a flower with `get_dead()`:

```cpp
// team: allow any dead flower (including allies), per spec
dead.set_dead(0);
dead.health = dead.max_health;
dead.set_face_flags(0);
// Stick: delete petal entity; set owner's loadout slot already_spawned=0 and reload timer to full Unique reload (Flower.cc already divides Ygg reload by rarity_pow3)
sim->request_delete(petal.id);
// Ensure owner slot reloads: mirror how other petals start reload after despawn
```

Remove the old commented 25%-on-lethal block and `_yggdrasil_revival_clause` that consumed loadout on death.

- [ ] **Step 2: Description**

```cpp
.description = "Revives a dead flower on contact",
```

- [ ] **Step 3: Build Server + Client**

- [ ] **Step 4: Commit**

```bash
git add Shared/StaticData.cc Server/EntityFunctions/Damage.cc \
  Server/Process/Collision.cc Server/Process/Flower.cc # + any new helper file
git commit -m "$(cat <<'EOF'
Make Yggdrasil revive dead flowers at full health on contact.

EOF
)"
```

---

### Task 6: Remove show grid

**Files:**
- Modify: `Client/Input.hh`, `Client/Input.cc`
- Modify: `Client/Ui/TitleScreen/Settings.cc`
- Modify: `Client/Storage.cc`
- Modify: `Client/Rendering.cc` (delete grid draw block ~L87–117)

- [ ] **Step 1: Delete toggle + flag + draw + storage bit**

- Remove `show_grid` from Input hh/cc and Settings row.
- In `Storage.cc`, remove `X(6, Input::show_grid)` and the opts bit 3 read/write for grid. **Keep bit positions stable for other settings** — if bit 3 was grid, stop reading/writing it (leave unused) so other bits do not shift, **or** renumber carefully and accept one-time settings reset. Prefer leave bit 3 unused (always 0) to avoid scrambling other toggles.
- Delete the `if (Input::show_grid) { ... }` block in `Rendering.cc`.

- [ ] **Step 2: Build Client**

- [ ] **Step 3: Commit**

```bash
git add Client/Input.hh Client/Input.cc Client/Ui/TitleScreen/Settings.cc \
  Client/Storage.cc Client/Rendering.cc
git commit -m "$(cat <<'EOF'
Remove show-grid setting and grid rendering.

EOF
)"
```

---

### Task 7: Craft UI interactions + layout polish

**Files:**
- Modify: `Client/Ui/InGame/Craft.cc`
- Verify: `Client/Network.cc` `send_craft(type, rarity, amount)` already sends amount
- Verify: `Server/EntityFunctions/CraftOps.cc` loops `while remaining >= 5`

**Interfaces:**
- Uses: `Input::keys_held.contains('\x10')` for Shift (same as defend key in `Game.cc`).
- Produces: recipe click → select + send craft; Craft button uses current selection amount.

- [ ] **Step 1: Layout structure to match screenshots**

Reorder panel children roughly:

1. Title `"Craft"`
2. `HContainer` / overlay: `CraftControls` (5 slots) + vertical stack of Craft button + success% text under button
3. Instruction paragraphs (two lines from spec)
4. Recipe grid — change `CRAFT_COLUMNS` from `9` to `8` to match screenshot density

Keep existing colors (`0xff5a9fdb` panel, etc.). No red X.

- [ ] **Step 2: Click / shift-click behavior**

In `RecipeCell::on_event`:

```cpp
void on_event(uint8_t event) override {
    if (event != kClick) return;
    uint64_t const count = owned_count(type, rarity);
    if (!craftable(rarity, count)) return;
    if (g_anim != kAnimIdle) return;
    g_sel_type = type;
    g_sel_rarity = rarity;
    bool const shift = Input::keys_held.contains('\x10');
    uint64_t const amount = shift ? count : 5;
    Game::send_craft(type, rarity, (uint32_t)amount);
    g_anim = kAnimRolling;
    g_anim_started = Game::timestamp;
    g_roll_request_at = Game::timestamp;
}
```

Craft button:

```cpp
uint64_t const count = owned_count(g_sel_type, g_sel_rarity);
uint64_t const amount = g_craft_all ? count : 5; // track whether selection was shift-filled
Game::send_craft(g_sel_type, g_sel_rarity, (uint32_t)amount);
```

Track `g_craft_all` set true on shift-click select-without-immediate-send **if** you also support place-then-Craft. Spec: click both fills slots **and** attempts immediately; Craft button re-attempts current selection (use `g_craft_all` remembered from last shift, default false → 5).

Idle slot fill: when `g_sel_type` set, draw all 5 icons (already does). For shift-all, still show 5 slot icons (visual), server consumes many groups.

- [ ] **Step 3: Instruction copy**

```cpp
new Ui::StaticParagraph(300, 13, "Combine 5 of the same petal to craft an upgrade", {}),
new Ui::StaticParagraph(300, 13, "Failure will destroy 1-4 petals", {}),
```

- [ ] **Step 4: Build Client + smoke Server craft path**

- [ ] **Step 5: Commit**

```bash
git add Client/Ui/InGame/Craft.cc
git commit -m "$(cat <<'EOF'
Fix craft panel layout and shift-click craft-all attempts.

EOF
)"
```

---

### Task 8: Integration verify

**Files:** none (manual / build)

- [ ] **Step 1: Full rebuild Client + Server**

- [ ] **Step 2: Run helper tests**

Expected: `ok`

- [ ] **Step 3: Manual checklist from spec**

- Spider poison 3.75/s  
- HP formula levels 1 / 50 / 100  
- Death corpse, Close, Continue/leave, Yggdrasil 100% revive + reload  
- Unique Queen no Super Soldier spawn spam  
- Ant hole 100%→0 one-shot: zero wave spawns  
- Egg summons ignore players  
- Stick sandstorm aggro 250 to mobs only  
- Knockback ~half  
- No Show grid  
- Craft click / shift-click / Craft button  

- [ ] **Step 4: Final commit only if fixups needed**

---

## Spec coverage (self-review)

| Spec item | Task |
|-----------|------|
| Spider 0.75× poison | 1 |
| HP formula + cap 100 | 1 |
| Dead state / no spectate / immobile / X+defend mouth | 3, 4 |
| Close gray button + Continue | 4 |
| Yggdrasil stick + 100% revive + reload | 5 |
| Queen Super Soldier no announce | 2 |
| Ant hole 100%→0 no spawns | 2 |
| Egg summons → mobs only | 2 |
| Sandstorm summon range 250 → mobs | 2 |
| Knockback 0.5× | 2 |
| Remove show grid | 6 |
| Craft layout + click/shift/Craft | 7 |

No TBD placeholders. `Flower.dead` naming is consistent across Tasks 3–5.
