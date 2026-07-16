# Accounts + Inventory + Store Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add account auth, stacked infinite inventory, blue Store (replacing Delete), no death loss, and `@account` nameplates on a fresh gardn remake destined for `yichenme/floir.xyz`.

**Architecture:** Shared owns protocol, `(type, rarity)` item/stack types, and validation/format helpers. Server owns authority: WASM/Node `database.json`, auth, inventory mutations, pickup, persistence. Client owns title Account UI, in-game inventory UI, Store chrome, and nameplate presentation. Main tick/Client packet switch stay thin and dispatch into focused modules.

**Tech Stack:** C++20 (gardn ECS), Emscripten client + WASM server, Node.js `ws`/`fs`/`crypto` for `database.json`, existing Canvas UI engine under `Client/Ui`.

## Global Constraints

- Follow **Client / Server / Shared** layout: numbers/protocol/schema in Shared; simulation/auth/inventory authority on Server; render/input/UI on Client. Never duplicate balance or stack rules on both sides.
- **New concern → new file.** Do not append auth, inventory ops, or inventory UI into unrelated god-files (`Server/Client.cc`, `Client/Game.cc`, `LoadoutPetal.cc`) beyond thin wiring.
- If a touched file is already large and the change adds a new concern, extract a module instead of growing it.
- No copy-paste blocks >~10 lines — extract Shared helpers.
- Delete unused delete/trash paths (`kPetalDelete`, `deleted_petals`); do not leave `_old` shims.
- Account fields: `session_key`, `loadout`, `inventory` (never `token`/`deck`/`stash`).
- Stack key: numeric `type` + numeric `rarity`; infinite inventory; count badge rules from the spec.
- Spawn requires login; no guest petal progress; no death petal loss.
- Supported host path for accounts: **WASM/Node server**.
- Commits only when the user asks during execution (skip commit steps unless explicitly requested).

## File Structure (locked)

| Path | Responsibility |
|------|----------------|
| `Shared/PetalItem.hh` | `PetalItem {type,rarity}`, `PetalStack {type,rarity,count}` |
| `Shared/StackFormat.hh` + `.cc` | `format_stack_count(uint64_t)` → display string |
| `Shared/AccountValidation.hh` + `.cc` | charset/length checks for username/password |
| `Shared/Binary.hh` | protocol enums + Writer/Reader (extend) |
| `Shared/EntityDef.hh` | instance rarity fields; `account_name`; remove `deleted_petals` |
| `Server/Account/Database.hh` + `DatabaseWasm.cc` | load/save/hash/session/loadout/inventory bridge |
| `Server/Account/Auth.hh` + `Auth.cc` | register/login/session restore handlers |
| `Server/EntityFunctions/InventoryOps.hh` + `.cc` | store/equip/add_stack/pickup helpers |
| `Server/Client.cc` | thin packet dispatch only |
| `Server/EntityFunctions/Death.cc` | persist, no loss |
| `Server/Spawn.cc` / `Process/Collision.cc` | account loadout spawn; full→inventory pickup |
| `Client/Account.hh` + `Account.cc` | client auth state + outbound auth packets |
| `Client/Ui/TitleScreen/Account.cc` | Account panel UI |
| `Client/Ui/InGame/Inventory.hh` + `Inventory.cc` | button + panel + stack badges |
| `Client/Ui/InGame/LoadoutSlot.cc` | Delete→Store chrome |
| `Client/Render/RenderName.cc` | nickname + `@account` |
| `Client/Network.cc` | handle `kAuthResponse` / `kInventoryUpdate` |
| `Scripts/test_shared_helpers.cc` | unit tests for validation + stack format |
| `docs/superpowers/specs/2026-07-16-floir-inventory-accounts-design.md` | source of truth |

---

### Task 1: Shared petal item types + stack format + account validation

**Files:**
- Create: `Shared/PetalItem.hh`
- Create: `Shared/StackFormat.hh`
- Create: `Shared/StackFormat.cc`
- Create: `Shared/AccountValidation.hh`
- Create: `Shared/AccountValidation.cc`
- Create: `Scripts/test_shared_helpers.cc`
- Modify: `Client/CMakeLists.txt` (add `../Shared/StackFormat.cc`, `../Shared/AccountValidation.cc`)
- Modify: `Server/CMakeLists.txt` (same)

**Interfaces:**
- Produces:
  - `struct PetalItem { PetalID::T type; uint8_t rarity; };`
  - `struct PetalStack { PetalID::T type; uint8_t rarity; uint64_t count; };`
  - `std::string format_stack_count(uint64_t count);` // empty if count<=1; else "x99"/"x1k"/"x1.5k"/"x1m"...
  - `bool account_username_valid(std::string_view);`
  - `bool account_password_valid(std::string_view);`

- [ ] **Step 1: Write failing helper tests**

Create `Scripts/test_shared_helpers.cc`:

```cpp
#include <Shared/AccountValidation.hh>
#include <Shared/StackFormat.hh>
#include <cassert>
#include <iostream>
#include <string>

int main() {
    assert(account_username_valid("alice"));
    assert(account_username_valid("A_1"));
    assert(!account_username_valid("ab"));          // too short
    assert(!account_username_valid("has-dash"));
    assert(!account_username_valid("has space"));
    assert(account_password_valid("abcd"));
    assert(!account_password_valid("abc"));         // too short
    assert(!account_password_valid("bad!pass"));

    assert(format_stack_count(1) == "");
    assert(format_stack_count(99) == "x99");
    assert(format_stack_count(1000) == "x1k");
    assert(format_stack_count(1500) == "x1.5k");
    assert(format_stack_count(1000000) == "x1m");
    assert(format_stack_count(1200000) == "x1.2m");
    std::cout << "ok\n";
    return 0;
}
```

- [ ] **Step 2: Compile test (expect fail — missing symbols)**

```bash
c++ -std=c++20 -I. Scripts/test_shared_helpers.cc -o /tmp/test_shared_helpers
```

Expected: compile error (headers/sources missing).

- [ ] **Step 3: Implement Shared helpers**

`Shared/PetalItem.hh`:

```cpp
#pragma once
#include <Shared/StaticDefinitions.hh>
#include <cstdint>
struct PetalItem {
    PetalID::T type = PetalID::kNone;
    uint8_t rarity = 0;
};
struct PetalStack {
    PetalID::T type = PetalID::kNone;
    uint8_t rarity = 0;
    uint64_t count = 0;
};
```

`Shared/AccountValidation.cc` — accept only `[A-Za-z0-9_]`; username length 3–16; password 4–32.

`Shared/StackFormat.cc` — if `count <= 1` return `""`; if `count < 1000` return `"x" + std::to_string(count)`; if `count < 1000000` format compact `k` with at most one decimal when needed; else compact `m`. Trim trailing `.0`.

- [ ] **Step 4: Add sources to both CMakeLists and re-run test**

```bash
c++ -std=c++20 -I. Scripts/test_shared_helpers.cc Shared/StackFormat.cc Shared/AccountValidation.cc -o /tmp/test_shared_helpers && /tmp/test_shared_helpers
```

Expected: prints `ok`, exit 0.

- [ ] **Step 5: Commit only if user requested**

---

### Task 2: Protocol updates in Shared

**Files:**
- Modify: `Shared/Binary.hh`

**Interfaces:**
- Produces clientbound: `kAuthResponse`, `kInventoryUpdate`
- Produces serverbound: `kRegister`, `kLogin`, `kSessionRestore`, `kPetalStore`, `kEquipPetal`, `kInventorySwap`
- Removes: `kPetalDelete`

- [ ] **Step 1: Replace packet enums**

In `Shared/Binary.hh`:

```cpp
enum Clientbound {
    kClientUpdate,
    kAuthResponse,
    kInventoryUpdate
};

enum Serverbound {
    kVerify,
    kClientInput,
    kClientSpawn,
    kPetalSwap,
    kPetalStore,
    kEquipPetal,
    kInventorySwap,
    kRegister,
    kLogin,
    kSessionRestore
};
```

Packet payloads (document in comments above enums):

- `kRegister` / `kLogin`: `string username`, `string password`
- `kSessionRestore`: `string username`, `string session_key`
- `kAuthResponse`: `uint8 success`, `string session_key_or_error`
- `kInventoryUpdate`: `uint32 n`, then `n` × (`uint8 type`, `uint8 rarity`, `uint64 count`)
- `kPetalStore`: `uint8 loadout_static_pos`
- `kEquipPetal`: `uint32 inventory_index`, `uint8 loadout_static_pos`
- `kInventorySwap`: `uint32 inventory_index`, `uint8 loadout_static_pos` (swap one equipped ↔ one from stack)

Keep existing `kPetalSwap` as loadout↔loadout.

- [ ] **Step 2: Grep and fix compile breaks for `kPetalDelete`**

```bash
rg -n 'kPetalDelete' -g '*.{cc,hh}'
```

Replace call sites with `kPetalStore` in later tasks; temporarily comment/guard only if needed to compile mid-plan — prefer fixing in Task 6/8 same session.

- [ ] **Step 3: Commit only if user requested**

---

### Task 3: ECS schema — instance rarity + account name; delete trash field

**Files:**
- Modify: `Shared/EntityDef.hh`
- Modify: `Shared/Entity.hh` (remove `circ_arr_t` / `deleted_petals` dependency if unused)
- Modify: any generated accessors consumers that break

**Interfaces:**
- Produces replicated fields:
  - `MULTIPLE(Camera, inventory_rarity, uint8_t, 2 * MAX_SLOT_COUNT)`
  - `MULTIPLE(Flower, loadout_rarities, uint8_t, 2 * MAX_SLOT_COUNT)`
  - `SINGLE(Drop, drop_rarity, uint8_t)`
  - `SINGLE(Name, account_name, std::string)`
- Removes: `SINGLE(deleted_petals, circ_arr_t, ={})`

- [ ] **Step 1: Edit `FIELDS_Camera` / `FIELDS_Flower` / `FIELDS_Drop` / `FIELDS_Name`**

```cpp
#define FIELDS_Camera \
SINGLE(Camera, player, EntityID) \
SINGLE(Camera, respawn_level, uint8_t) \
MULTIPLE(Camera, inventory, PetalID::T, 2 * MAX_SLOT_COUNT) \
MULTIPLE(Camera, inventory_rarity, uint8_t, 2 * MAX_SLOT_COUNT) \
SINGLE(Camera, killed_by, std::string) \
SINGLE(Camera, camera_x, Float) \
SINGLE(Camera, camera_y, Float) \
SINGLE(Camera, fov, Float)

#define FIELDS_Flower \
SINGLE(Flower, overlevel_timer, float) \
SINGLE(Flower, loadout_count, uint8_t) \
SINGLE(Flower, face_flags, uint8_t) \
SINGLE(Flower, equip_flags, uint8_t) \
MULTIPLE(Flower, loadout_ids, PetalID::T, 2 * MAX_SLOT_COUNT) \
MULTIPLE(Flower, loadout_rarities, uint8_t, 2 * MAX_SLOT_COUNT) \
MULTIPLE(Flower, loadout_reloads, uint8_t, MAX_SLOT_COUNT)

#define FIELDS_Drop \
SINGLE(Drop, drop_id, PetalID::T) \
SINGLE(Drop, drop_rarity, uint8_t)

#define FIELDS_Name \
SINGLE(Name, name, std::string) \
SINGLE(Name, account_name, std::string) \
SINGLE(Name, nametag_visible, uint8_t)
```

Under `PER_EXTRA_FIELD` (serverside): **delete** `deleted_petals` line.

- [ ] **Step 2: Build server+client to surface missing set/get updates**

```bash
cmake -S Server -B Server/build -DWASM_SERVER=1 && cmake --build Server/build -j4
cmake -S Client -B Client/build && cmake --build Client/build -j4
```

Expected: errors only at old delete/trash or missing rarity writes — fix compile blockers by initializing rarities to `PETAL_DATA[id].rarity` (or 0 for empty) wherever loadout/drops are set. Prefer small local fixes; larger inventory logic waits for Task 6.

- [ ] **Step 3: Commit only if user requested**

---

### Task 4: Server account database (WASM/Node)

**Files:**
- Create: `Server/Account/Database.hh`
- Create: `Server/Account/DatabaseWasm.cc`
- Create: `Server/database.json` (empty `{}` starter; gitignore secrets if needed — file itself OK empty)
- Modify: `Server/CMakeLists.txt` — add `Account/DatabaseWasm.cc` when `WASM_SERVER`
- Modify: Node entry / generated `gardn-server.js` side as needed so `global.loadDatabase` / `global.saveDatabase` exist (inspect current WASM bootstrap; add helpers in a new `Server/Account/database.js` required from the server JS)

**Interfaces:**
- Produces C++ API:

```cpp
namespace AccountDB {
  bool load();
  bool save();
  // returns 1 ok, 0 fail; writes session_key out via EM or out-params
  int register_user(std::string const &user, std::string const &pass, std::string &session_key_out, std::string &err_out);
  int login_user(std::string const &user, std::string const &pass, std::string &session_key_out, std::string &err_out);
  int restore_session(std::string const &user, std::string const &session_key);
  int read_loadout(std::string const &user, PetalItem *out /*len 2*MAX_SLOT_COUNT*/);
  int write_loadout(std::string const &user, PetalItem const *in /*len 2*MAX_SLOT_COUNT*/);
  int read_inventory(std::string const &user, std::vector<PetalStack> &out);
  int write_inventory(std::string const &user, std::vector<PetalStack> const &in);
  int read_progress(std::string const &user, uint8_t &level, uint32_t &xp);
  int write_progress(std::string const &user, uint8_t level, uint32_t xp);
}
```

JSON shape per account matches the spec (`password_hash`, `session_key`, `level`, `xp`, `loadout[{type,rarity}]`, `inventory[{type,rarity,count}]`).

- [ ] **Step 1: Implement `database.js` helpers**

```js
const fs = require('fs');
const crypto = require('crypto');
const path = require('path');
const DB_PATH = path.join(__dirname, 'database.json');
global.loadDatabase = () => {
  if (!global.db) {
    try { global.db = JSON.parse(fs.readFileSync(DB_PATH, 'utf8')); }
    catch { global.db = {}; }
  }
};
global.saveDatabase = () => {
  fs.writeFileSync(DB_PATH, JSON.stringify(global.db, null, 2));
};
global.hashPassword = (p) => crypto.createHash('sha256').update(p).digest('hex');
global.makeSessionKey = () => crypto.randomBytes(16).toString('hex');
```

Wire `require('./Account/database.js')` (or equivalent path) from the WASM server JS entry.

- [ ] **Step 2: Implement `DatabaseWasm.cc` via `EM_ASM` / `EM_ASM_INT` calling those globals**

Validate username/password with `account_*_valid` before JS. On register: reject taken names; create default loadout of Basics; empty inventory; return session_key.

- [ ] **Step 3: Smoke-test in Node REPL after server build**

```bash
node -e "require('./Server/Account/database.js'); global.loadDatabase(); console.log(global.db);"
```

Expected: `{}` or existing object, no throw.

- [ ] **Step 4: Commit only if user requested**

---

### Task 5: Server auth handlers + Client session fields

**Files:**
- Create: `Server/Account/Auth.hh`
- Create: `Server/Account/Auth.cc`
- Modify: `Server/Client.hh` — add `std::string username; std::string session_key; uint8_t logged_in;`
- Modify: `Server/Client.cc` — dispatch `kRegister`/`kLogin`/`kSessionRestore` to `Auth::*`; gate `kClientSpawn` on `logged_in`
- Modify: `Server/CMakeLists.txt` — add `Account/Auth.cc`

**Interfaces:**
- Consumes: `AccountDB::*`, `account_*_valid`
- Produces:
  - `void Auth::handle_register(Client *, Reader &);`
  - `void Auth::handle_login(Client *, Reader &);`
  - `void Auth::handle_session_restore(Client *, Reader &);`
  - `void Auth::send_auth_response(Client *, uint8_t ok, std::string const &payload);`
  - After success: `client->logged_in = 1`, set username/session_key, call inventory sync (stub OK until Task 6)

- [ ] **Step 1: Implement Auth module (keep Client.cc thin)**

`Server/Client.cc` cases:

```cpp
case Serverbound::kRegister:
    Auth::handle_register(client, reader); break;
case Serverbound::kLogin:
    Auth::handle_login(client, reader); break;
case Serverbound::kSessionRestore:
    Auth::handle_session_restore(client, reader); break;
case Serverbound::kClientSpawn: {
    if (!client->logged_in) break;
    // existing spawn...
}
```

- [ ] **Step 2: Build WASM server**

```bash
cmake --build Server/build -j4
```

Expected: success.

- [ ] **Step 3: Manual packet smoke (optional):** run server, send register from a tiny node ws script or wait for Task 7 UI.

- [ ] **Step 4: Commit only if user requested**

---

### Task 6: Server inventory ops, pickup, death persistence, spawn from account

**Files:**
- Create: `Server/EntityFunctions/InventoryOps.hh`
- Create: `Server/EntityFunctions/InventoryOps.cc`
- Modify: `Server/EntityFunctions.hh` — declare inventory ops if that header aggregates entity functions
- Modify: `Server/Client.cc` — dispatch store/equip/swap; call persist helpers
- Modify: `Server/Process/Collision.cc` — `_pickup_drop` uses InventoryOps
- Modify: `Server/EntityFunctions/Death.cc` — remove deleted_petals loss; persist account loadout/inventory; keep camera loadout intact for respawn
- Modify: `Server/Spawn.cc` — `player_spawn` reads account loadout+rarities into flower; set `account_name`
- Modify: `Server/Spawn.cc` `alloc_drop` — set `drop_rarity` from `PETAL_DATA[id].rarity` (instance authoritative; v1 loot = base rarity)
- Modify: `Server/CMakeLists.txt` — add InventoryOps.cc
- Delete all remaining `deleted_petals` / XP-on-delete logic

**Interfaces:**
- Produces:

```cpp
namespace InventoryOps {
  void add_stack(std::vector<PetalStack> &inv, PetalID::T type, uint8_t rarity, uint64_t n = 1);
  bool take_one(std::vector<PetalStack> &inv, uint32_t index, PetalItem &out);
  void store_from_loadout(Client *client, Entity &player, uint8_t static_pos);
  void equip_to_loadout(Client *client, Entity &player, uint32_t inv_index, uint8_t static_pos);
  void pickup_drop(Simulation *sim, Client *client, Entity &player, Entity &drop);
  void sync_inventory_update(Client *client);
  void persist_account_petals(Client *client, Entity &player);
  void apply_account_loadout_to_camera(Client *client, Entity &camera);
}
```

Client must be reachable from collision/death: either pass `Client*` via flower parent camera lookup map, or persist using camera→client map already in `GameInstance`. Inspect `Server/Game.hh` / client list and use the existing association (do not invent a second registry).

Pickup algorithm:

```cpp
// 1) first empty loadout slot among primary+secondary → set type+rarity, delete drop
// 2) else InventoryOps::add_stack on account inventory + sync + delete drop
```

Death algorithm:

```cpp
// copy flower loadout ids/rarities → account loadout via AccountDB::write_loadout
// inventory already on account; ensure latest write
// DO NOT clear petals randomly; DO NOT use deleted_petals
```

- [ ] **Step 1: Implement InventoryOps + wire packets**

Replace `kPetalDelete` case with:

```cpp
case Serverbound::kPetalStore: {
    // validate pos; InventoryOps::store_from_loadout(...)
    break;
}
```

- [ ] **Step 2: Update Collision `_pickup_drop` to call `InventoryOps::pickup_drop`**

Need `Client*` — find via game instance cameras. If collision lacks Client today, change signature to accept a resolver callback from the process that has game context, **or** store `username` on camera and persist via username key only. Prefer resolving `Client*` once in a small helper in `Server/Account/` or `GameInstance` (`Client *client_for_camera(EntityID)`).

- [ ] **Step 3: Rewrite Death petal section**

Remove potential-drop reshuffle from loadout. Persist and leave account loadout as-is for next spawn.

- [ ] **Step 4: Build server**

```bash
cmake --build Server/build -j4
```

Expected: success.

- [ ] **Step 5: Commit only if user requested**

---

### Task 7: Client account module + title Account UI + spawn gate

**Files:**
- Create: `Client/Account.hh`
- Create: `Client/Account.cc`
- Create: `Client/Ui/TitleScreen/Account.cc`
- Modify: `Client/Ui/TitleScreen/TitleScreen.hh` — declare `make_account_panel()`
- Modify: `Client/Ui/Extern.hh` / `Extern.cc` — `Panel::kAccount`, `Panel::account`
- Modify: `Client/Ui/TitleScreen/MainScreen.cc` — insert Account button between Settings and Petals only (no form logic here)
- Modify: `Client/Game.cc` — add account panel child; do not inline auth
- Modify: `Client/Network.cc` — handle `kAuthResponse`; call `Account::on_auth_response`
- Modify: `Client/Ui/TextInput.hh` / `.cc` — add password masking flag (`bool password=false`) rendering `*`
- Modify: `Client/CMakeLists.txt` — add `Account.cc`, `Ui/TitleScreen/Account.cc`
- Modify: `Client/Network.cc` `Game::spawn_in` — refuse unless `Account::logged_in()`

**Interfaces:**
- Produces:

```cpp
namespace Account {
  extern std::string username_field, password_field, confirm_field;
  extern std::string logged_in_user, error, status;
  extern bool register_mode;
  bool logged_in();
  void request_register();
  void request_login();
  void request_session_restore(); // reads localStorage
  void request_logout();
  void on_auth_response(uint8_t ok, std::string const &payload);
}
```

- [ ] **Step 1: Implement `Client/Account.cc` packet sends using Shared validation first**

- [ ] **Step 2: Implement `make_account_panel()` mirroring Settings panel styling (`0xff5a9fdb` chrome)**

Logged-out: Login/Register toggle, fields, submit.  
Logged-in: `Logged in as: …`, Log out.

- [ ] **Step 3: Insert Account button in `make_panel_buttons` between Settings and Petals**

Same button style as Settings (`0xff5a9fdb`, line 5, radius 3).

- [ ] **Step 4: Gate spawn**

```cpp
void Game::spawn_in() {
    if (!Account::logged_in()) {
        Ui::panel_open = Ui::Panel::kAccount;
        Account::error = "Register or log in to play";
        return;
    }
    // existing verify/spawn write...
}
```

- [ ] **Step 5: On socket open, `Account::request_session_restore()` if localStorage has keys `floir_user` / `floir_session_key`

- [ ] **Step 6: Build client**

```bash
cmake --build Client/build -j4
```

Expected: success.

- [ ] **Step 7: Commit only if user requested**

---

### Task 8: Client Store chrome + inventory UI

**Files:**
- Create: `Client/Ui/InGame/Inventory.hh`
- Create: `Client/Ui/InGame/Inventory.cc`
- Modify: `Client/Ui/InGame/Loadout.hh` — rename `UiDeleteSlot` → `UiStoreSlot` (or keep class file-local name but label Store)
- Modify: `Client/Ui/InGame/LoadoutSlot.cc` — blue fill `0xff5a9fdb`, text `Store`
- Modify: `Client/Ui/InGame/LoadoutPetal.cc` — drop-on-store calls `Game::store_petal` / `kPetalStore` (not delete)
- Modify: `Client/Game.hh` / `Network.cc` — `store_petal`, `equip_petal`; handle `kInventoryUpdate` into `Account`/`Game` inventory vector
- Modify: `Client/Game.cc` — `add_child(make_inventory_button())`, `add_child(make_inventory_panel())`
- Modify: `Client/Ui/Extern.hh` — `Panel::kInventory`, inventory screen anchors
- Modify: `Client/CMakeLists.txt` — add Inventory.cc

**Interfaces:**
- Client inventory cache: `std::vector<PetalStack> Game::inventory_stacks;`
- UI: bottom-left button; panel opens with `Panel::kInventory`; scrollable grid of stacks; badge via `format_stack_count`
- Drag from inventory → loadout sends `kEquipPetal` / `kInventorySwap`

- [ ] **Step 1: Change Store slot chrome**

```cpp
UiStoreSlot::UiStoreSlot() : UiLoadoutSlot(2 * MAX_SLOT_COUNT) {
    style.fill = 0xff5a9fdb;
    // animate label opacity when selection active
}
void UiStoreSlot::on_render(Renderer &ctx) {
    UiLoadoutSlot::on_render(ctx);
    if ((float) store_text_opacity > 0.01) {
        ctx.set_global_alpha((float) store_text_opacity);
        ctx.draw_text("Store", { .size = height / 4 });
    }
}
```

- [ ] **Step 2: Implement Inventory button + panel (new file only)**

- Button: `h_justify=Left`, `v_justify=Bottom`, alive-only, blue fill.
- Panel: scroll container of stack slots; count text top-right using `format_stack_count`.
- Do not dump this into `LoadoutSlot.cc`.

- [ ] **Step 3: Wire network**

```cpp
void Game::store_petal(uint8_t pos) {
    Writer w(...);
    w.write<uint8_t>(Serverbound::kPetalStore);
    w.write<uint8_t>(pos);
    socket.send(...);
}
```

On `kInventoryUpdate`, replace `Game::inventory_stacks`.

- [ ] **Step 4: Build client + quick UI check in browser**

Expected: Store label blue; inventory button bottom-left only in-game.

- [ ] **Step 5: Commit only if user requested**

---

### Task 9: Nameplate `@account` + spawn sets account_name

**Files:**
- Modify: `Client/Render/RenderName.cc`
- Modify: `Server/Spawn.cc` / spawn path — `player.set_account_name(client->username)`
- Modify: own-player HUD if local name is drawn elsewhere (grep `get_name()` / `draw_text` for self)

**Interfaces:**
- Consumes replicated `account_name`
- Renders nickname then `@` + account_name beneath

- [ ] **Step 1: Update `render_name`**

```cpp
void render_name(Renderer &ctx, Entity const &ent) {
    if (!ent.get_nametag_visible()) return;
    if (ent.id == Game::player_id) return;
    ctx.translate(0, -ent.get_radius() - 18);
    ctx.set_global_alpha(1 - ent.deletion_animation);
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    ctx.draw_text(ent.get_name().c_str(), { .size = 18 });
    if (!ent.get_account_name().empty()) {
        ctx.translate(0, -20);
        std::string tag = "@" + ent.get_account_name();
        ctx.draw_text(tag.c_str(), { .size = 14 });
    }
}
```

Also render for local player if a self nameplate exists; if not, add a small self-name overlay only if the game already shows one — do not invent extra HUD.

- [ ] **Step 2: Build + visual check with two clients**

Expected: other players show nickname with `@user` under it.

- [ ] **Step 3: Commit only if user requested**

---

### Task 10: End-to-end verification + git remote replace

**Files:**
- Modify: `README.md` / `INSTALLATION.md` — document WASM server + accounts requirement
- Modify: git remotes

- [ ] **Step 1: Full build**

```bash
cmake -S Server -B Server/build -DWASM_SERVER=1 && cmake --build Server/build -j4
cmake -S Client -B Client/build && cmake --build Client/build -j4
# copy client artifacts into Server/build or Client/public per INSTALLATION.md
node Server/build/gardn-server.js   # or actual output name
```

- [ ] **Step 2: Manual test checklist**

1. Cannot spawn logged out; Account panel prompts.
2. Register `alice` / `abcd` works; bad charset rejected.
3. Login works; refresh restores via session_key.
4. In-game: inventory button bottom-left; Store is blue.
5. Store petal → stack appears; `x2` on second; hide on single.
6. Full loadout pickup → inventory stack increases.
7. Death → petals remain; respawn same loadout/inventory.
8. Nameplate shows `@alice`.

- [ ] **Step 3: Point remote at official repo**

```bash
cd /Users/eason/Desktop/floir.xyz
git remote remove origin
git remote add origin https://github.com/yichenme/floir.xyz.git
git remote -v
```

- [ ] **Step 4: Force-replace remote only after explicit user confirmation**

```bash
# DESTRUCTIVE — run only when user says to replace remote
git push -u origin HEAD:main --force
```

Do **not** force-push until the user explicitly confirms in chat.

---

## Spec coverage check

| Spec item | Task |
|-----------|------|
| Account button between Settings/Petals | 7 |
| Login/Register + validation + SHA-256 | 4, 5, 7 |
| Spawn gated / no guest | 5, 7 |
| Nickname vs `@account` nameplate | 9 |
| session_key restore | 4, 5, 7 |
| Inventory bottom-left in-game only | 8 |
| Delete→Store blue | 8 |
| Infinite stacks by numeric type+rarity | 1, 6, 8 |
| Count badge formatting | 1, 8 |
| Pickup overflow → inventory | 6 |
| No death loss + persist | 6 |
| Replace floir.xyz remote | 10 |
| Client/Server/Shared + code health splits | File structure + all tasks |

## Plan self-review notes

- No `token`/`deck`/`stash` naming remains in interfaces.
- `Server/Client.cc` stays a dispatcher; auth/inventory live in `Server/Account/*` and `EntityFunctions/InventoryOps.*`.
- Inventory UI is its own client module, not folded into loadout petal code beyond Store drop target wiring.
- Rarity v1 uses `PETAL_DATA[type].rarity` for drop instance rarity (numeric); stacking still keys `(type,rarity)` for future loot variance.
