# Craft Remake + Antennae Vision + Magnet Icon + Squad Shared XP — Implementation Plan

> **For agentic workers:** This codebase (C++/Emscripten → WASM, custom canvas renderer) has **no unit-test harness**. Verification for every task is: `make -j4` builds clean → `node run-server.js` boots clean → browser smoke-test. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Fully remake the crafting page (new layout + economics), fix Antennae per-rarity vision, fix the corrupted Magnet icon, and make squads share XP.

**Architecture:** Server owns all crafting economics (`CraftOps.cc`) and XP (`Death.cc`); flat per-rarity success chance lives in `Shared/RarityScale.cc` so client display and server roll can't diverge. Client `Craft.cc` is a canvas-drawn panel (no DOM). Vision and magnet are pure server/client render tweaks respectively.

**Tech Stack:** C++20, Emscripten, custom `Renderer` (canvas 2D wrapper), PM2/Node WASM host.

## Global Constraints

- Deploy target is **38.76.196.218 only** (root / xsJi08TR), `/var/www/floir.xyz`; `.43` is no longer synced.
- Client artifacts: `Client/build/floir-client.{js,wasm}` → also `cp` into `Server/`. Server: `Server/build/floir-server.{js,wasm}` → `/var/www/floir.xyz/build/`.
- Every deploy: md5-verify on server, `pm2 restart floir.xyz`, curl 200 check.
- Retired PetalIDs never render-crash (default case degrades to grey placeholder — already in place).

---

## Craft economics (locked with user)

- One **attempt** requires ≥5 of one (type, rarity). It consumes 5, rolls the flat chance, and on success produces **1** petal of rarity+1. Regardless of success/failure, an **extra random 1–4** petals are then destroyed (capped at what remains).
- Flat success chance = `0.64 * 0.5^rarity`: Common→Uncommon 64%, 32%, 16%, 8%, 4%, 2%, Ultra→Super 1%. Super+ uncraftable. **The pity/attempt system added earlier is reverted.**
- Click semantics: single click = **one** attempt; double-click OR shift+click = **craft the whole stack** (repeat attempts until <5 remain).
- Protocol: `send_craft(type, rarity, amount)` where `amount` is reinterpreted as **number of attempts** (1 = single; the owned count = "all", server caps by availability). `kCraftResult` shape unchanged: `(type, out_rarity, crafted, remaining, any_success)`.
- The `PetalStack.craft_attempt` field + its DB/protocol plumbing is left in place but unused (ripping it out is pure protocol churn with mismatch risk; it just stays 0).

## Layout (locked with user)

Rectangular panel. **Left half:** inventory grid — one row per petal *type*, 9 columns = rarities Common…Unique. **Right half:** craft UI — selected petal, success %, Craft button, result text. Click a left cell to select; the right side crafts it.

---

## File Structure

- `Shared/RarityScale.cc` / `.hh` — revert `craft_success_chance` to flat single-arg.
- `Server/EntityFunctions/CraftOps.cc` — new attempts-based loop (5 + 1–4 always lost, +1 on success).
- `Server/Process/Flower.cc` — Antennae per-rarity vision lookup table + FOV floor.
- `Client/Assets/Petal.cc` — Magnet `kMagnet` case: correct scale + centering.
- `Client/Ui/InGame/Craft.cc` — full rewrite: rectangular left-grid / right-UI, single/double/shift semantics.
- `Server/EntityFunctions/Death.cc` — `_add_score` distributes XP to squad members.

---

### Task 1: Revert flat craft chance
**Files:** Modify `Shared/RarityScale.hh`, `Shared/RarityScale.cc`, `Server/EntityFunctions/CraftOps.cc` (caller), `Client/Ui/InGame/Craft.cc` (caller).
- `float craft_success_chance(uint8_t rarity)` → `return rarity >= kSuper ? 0 : 0.64f * powf(0.5f, rarity);`
- Update both callers to the single-arg form.
- **Verify:** builds clean.

### Task 2: New craft loop (CraftOps.cc)
**Files:** Modify `Server/EntityFunctions/CraftOps.cc`.
- Interpret `amount` as attempt count. Loop `while (attempts_left > 0 && count >= 5)`: `count -= 5`; roll `frand() < craft_success_chance(rarity)` → `crafted++`; `extra = min(count, 1 + rand%4)`; `count -= extra`; `attempts_left--`.
- Add `crafted` petals of rarity+1; write inventory; resync; send `kCraftResult`; Super system message unchanged.
- **Verify:** builds; boot; craft via browser reduces stack by 5+extra per click.

### Task 3: Antennae per-rarity vision (Flower.cc)
**Files:** Modify `Server/Process/Flower.cc`.
- Lookup `bonus[9] = {0.10,0.20,0.35,0.50,0.75,1.00,1.75,2.50,6.00}`; `vision_factor = 1/(1+bonus[rarity])`.
- FOV floor = `1.f/7.f` (the unique-tier minimum, 1/(1+6)).
- **Verify:** builds; numeric check all 9 distinct.

### Task 4: Magnet icon fix (Petal.cc)
**Files:** Modify `Client/Assets/Petal.cc` `kMagnet`.
- Content bbox center ≈ (-6.645, -0.76), max radius+stroke ≈ 42. Replace `scale(r/60); translate(0,14)` with `scale(r/38); translate(6.645, 0.76)` (center content in slot).
- **Verify:** builds; browser petal gallery shows a proper horseshoe magnet, centered, filling the slot.

### Task 5: Squad shared XP (Death.cc)
**Files:** Modify `Server/EntityFunctions/Death.cc` `_add_score`.
- If killer's camera `squad_id != 0`, award full `score_reward` to every living member flower (+bank respawn_level per member camera); killer alone gets the `mobs_killed++`. No squad → current single-award behaviour.
- **Verify:** builds; boot; two squadded accounts both gain XP from one's kill.

### Task 6: Craft.cc UI rewrite
**Files:** Rewrite `Client/Ui/InGame/Craft.cc`.
- Rectangular `make_craft_panel`: left = scroll grid, rows per owned type, 9 rarity columns; right = selected icon + `format_pct(chance)` + Craft button + result.
- Cell click selects (type,rarity). Craft button / cell: single click → `send_craft(type,rarity,1)`; double-click or shift+click → `send_craft(type,rarity,ownedCount)`.
- Keep click-to-open `CraftIcon`, panel z-order, slide anim.
- **Verify:** builds; browser: open craft, select a ≥5 stack, single-craft consumes 5+extra, shift-craft drains stack.

### Task 7: Build, verify, deploy
- Build client + server; `cp` client into `Server/`; boot-check; browser smoke test (craft flow, magnet icon).
- Commit, push, deploy client+server to `.218`, restart, curl 200.

## Self-Review
- Craft chance flat ✓ (Task 1). New economics ✓ (Task 2). Antennae ✓ (Task 3). Magnet ✓ (Task 4). Squad XP ✓ (Task 5). Layout ✓ (Task 6). Deploy ✓ (Task 7).
- No type drift: `craft_success_chance(uint8_t)` single-arg used identically in CraftOps + Craft.cc.
