# Floir.xyz — Accounts, Inventory, and Store Design

Date: 2026-07-16  
Base: fresh remake from [trigonal-bacon/gardn](https://github.com/trigonal-bacon/gardn), replacing [yichenme/floir.xyz](https://github.com/yichenme/floir.xyz)

## 1. Goals

- Remake gardn as floir.xyz with **account-bound petals**.
- Add **in-game inventory** (bottom-left) with stacking.
- Change loadout **Delete → Store** (blue, game-style).
- **No petal loss on death**.
- Title-screen **Account** panel (login/register) between Settings and Petals.
- After implementation, **replace** the remote `yichenme/floir.xyz` repo with this codebase.

### 1.1 Code layout constraints

- **Shared** — protocol, `(type, rarity)` stack types, validation, count formatting. Single source of truth; no duplicated rules on client/server.
- **Server** — auth, `database.json`, inventory mutations, pickup, death persistence. New concerns get new modules (`Server/Account/*`, `EntityFunctions/InventoryOps.*`); `Client.cc` stays a thin dispatcher.
- **Client** — Account panel, inventory UI, Store chrome, nameplates. New UI surfaces get new files (`Ui/TitleScreen/Account.cc`, `Ui/InGame/Inventory.cc`).
- Split before growing large files; delete trash/delete paths instead of deprecating.

## 2. Accounts & title UI

### 2.1 Account button

- Place an **Account** button between **Settings** and **Petals**.
- Same blue game-style button as existing title panels (`0xff5a9fdb`, line width 5, round radius 3).

### 2.2 Account panel

- Modes: **Login** and **Register**.
- Fields: username, password; register also has confirm password.
- Validation (client + server):
  - Allowed charset: `[A-Za-z0-9_]` only.
  - Username length: 3–16.
  - Password length: 4–32.
- Passwords stored as **SHA-256** hex hashes in `database.json` (never plaintext).
- On success: bind session to account, send `session_key`, push inventory/loadout sync.
- On failure: show error text in panel (taken username, bad password, invalid chars, etc.).

### 2.3 Spawn gating

- **Spawn requires a logged-in account.**
- If not logged in, prompt to register/login (no guest petal progress).
- Flower **nickname** (spawn name) stays separate from **account username**.

### 2.4 Nameplate

- Above flower: nickname.
- Below nickname: `@` + account username (e.g. `@alice`).

### 2.5 Session restore

- Client may store `session_key` + username in `localStorage`.
- On reconnect, server accepts valid `session_key` to restore the account without retyping password.

## 3. Inventory & Store (in-game)

### 3.1 Visibility & layout

- Inventory UI **only while alive / in-game**.
- **Bottom-left**: inventory open button (blue, game-style).
- Opening shows a scrollable inventory panel of petal stacks.
- All chrome matches existing gardn UI (fills, strokes, round radius, animations).

### 3.2 Store (replaces Delete)

- Loadout trash slot becomes **Store**.
- Label: `Store`; fill: blue (same family as title buttons, e.g. `0xff5a9fdb`).
- Dragging/selecting a loadout petal onto Store moves **one** unit into inventory:
  - If a matching stack exists → `count++`.
  - Else → create stack `{ type, rarity, count: 1 }`.
- No XP grant for storing; remove old `deleted_petals` trash-on-death pipeline.

### 3.3 Stacking model

- Inventory size is **infinite** (unbounded list of stacks).
- One stack per **(petal_type_id, rarity_id)** pair.
- Both `petal_type_id` and `rarity_id` are **numbers** (not text).
- Example stack record: `{ "type": 5, "rarity": 2, "count": 99 }`.

### 3.4 Stack count display

- Top-right of inventory petal icon:
  - `count == 1` → **hide** count.
  - `count > 1` → show `xN` with compact formatting:
    - `< 1000` → raw (`x99`, `x999`)
    - `>= 1000` → `k` (`x1k`, `x1.5k`)
    - `>= 1_000_000` → `m` (`x1m`, `x1.2m`)
- No maximum stack amount.

### 3.5 Loadout ↔ inventory

- Equip from inventory: consume 1 from stack; remove stack when count hits 0; place into a loadout slot.
- Unequip / Store: move from loadout into inventory stack as above.
- Swaps: loadout↔loadout, inventory↔loadout (and inventory reordering if needed).

### 3.6 Pickup rules

1. If an empty loadout slot exists → equip there (with type + rarity).
2. Else → **auto-add to inventory** (increment or create stack).
3. With infinite inventory, ground drops should not be rejected for “full inventory”.

### 3.7 Death

- **No death loss** of petals.
- On death / disconnect: persist current loadout + inventory to the account.
- On respawn: restore account loadout + inventory unchanged (aside from normal gameplay mid-life edits).

## 4. Data model

### 4.1 Account record (`database.json`)

Keys are account usernames.

```json
{
  "alice": {
    "password_hash": "<sha256 hex>",
    "session_key": "<opaque string>",
    "level": 1,
    "xp": 0,
    "loadout": [
      { "type": 1, "rarity": 0 },
      { "type": 0, "rarity": 0 }
    ],
    "inventory": [
      { "type": 5, "rarity": 2, "count": 99 },
      { "type": 3, "rarity": 0, "count": 1 }
    ]
  }
}
```

- `loadout`: fixed length `2 * MAX_SLOT_COUNT`; empty slot = `{ "type": 0, "rarity": 0 }` (`PetalID::kNone`).
- `inventory`: variable-length array of stacks; omit empty stacks.
- `session_key`: opaque reconnect credential after login/register.

### 4.2 Shared / ECS changes

Stock gardn ties rarity only to static `PETAL_DATA[type]`. Floir needs **instance rarity**:

- Camera / flower loadout entries carry `(PetalID type, uint8_t rarity)`.
- Drops carry `(drop_id, rarity)`.
- Inventory is **not** an ECS world component; it lives on the authenticated client session and syncs via dedicated packets.

Suggested constants:

- Keep `MAX_SLOT_COUNT` for loadout bars.
- No `MAX_INVENTORY_SLOTS` cap (infinite).

### 4.3 Protocol (additions to `Shared/Binary.hh`)

**Serverbound**

| Packet | Purpose |
|--------|---------|
| `kRegister` | username + password |
| `kLogin` | username + password |
| `kPetalSwap` | loadout↔loadout swap (rarity-aware) |
| `kPetalStore` | move one loadout slot → inventory (replaces old `kPetalDelete`) |
| `kEquipPetal` | take 1 from inventory stack → chosen/empty loadout slot |
| `kInventorySwap` | inventory drag onto loadout / reorder as needed |

**Clientbound**

| Packet | Purpose |
|--------|---------|
| `kAuthResponse` | success + `session_key`, or error string |
| `kInventoryUpdate` | full inventory stack list (and loadout mirror if not already in entity sync) |

**Delete opcode:** remove `kPetalDelete` and `deleted_petals` trash/XP behavior entirely; use `kPetalStore` only.

### 4.4 Rarity on drops

- Drops and loadout slots store numeric `rarity` per instance.
- On drop spawn, server assigns a numeric `rarity_id` (from drop/loot rules). Static `PETAL_DATA[type].rarity` may still describe the petal’s *base* tier for UI/gallery, but **instance `rarity_id` is authoritative** for stacking and account storage.

## 5. Runtime & persistence

- Prefer **WASM/Node server** so `database.json` load/save is straightforward (`fs` + in-memory cache).
- Hash passwords with Node `crypto` SHA-256 (or equivalent) before write.
- Save database after auth mutations and after petal mutations that change account `loadout` / `inventory`.
- Native uWebSockets path may be secondary; if unsupported initially, document WASM server as the supported host for accounts.

## 6. Client UI checklist

| Surface | Behavior |
|---------|----------|
| Title Account button | Between Settings and Petals |
| Account panel | Login / Register, validation, errors |
| Spawn | Blocked until logged in |
| Nameplate | nickname + `@account` |
| In-game inventory button | Bottom-left, alive only |
| Inventory panel | Scrollable stacks, count badges |
| Loadout Store slot | Blue, label `Store` |

## 7. Out of scope (this remake pass)

- Crafting, chat, squads, and other old-floir extras not listed above.
- Guest / IP progress profiles.
- Per-stack maximum caps.

## 8. Git / deploy outcome

1. Local tree starts as gardn clone in `floir.xyz`.
2. Remote `origin` → `https://github.com/yichenme/floir.xyz.git`.
3. After feature completion and verification, **force-replace** remote `main` with this remake (destructive to prior remote history/contents). Confirm before force-push.

## 9. Success criteria

- Unauthenticated players cannot spawn.
- Register/login works with charset/length rules; passwords hashed at rest.
- Petals persist on account across death and reconnect.
- Store moves petals into stacked inventory; counts render per rules.
- Full loadout pickups go to inventory automatically.
- Inventory UI is bottom-left and in-game only.
- Nameplate shows `@account` under nickname.
- Remote floir.xyz hosts this codebase.
