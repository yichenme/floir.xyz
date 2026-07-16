
# Requirements
Please download the latest version of [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) and [CMake](https://cmake.org/download/). You will also need the latest version of gcc/g++ (>=c++20).

# Installation

## Native server (more performant):
**Accounts and inventory persistence require the WASM/Node server** (see [Accounts & Inventory](#accounts--inventory)). The native server is for local simulation only — it compiles and runs without `database.json`, but register/login/session-restore will fail and loadouts/inventory are not persisted across restarts.
```
> git clone --recurse-submodules https://github.com/trigonal-bacon/floir.xyz.git
```
You will need to compile uWebSockets first. For in-depth complation options please visit the [uWebSockets installation page](https://github.com/uNetworking/uWebSockets/tree/master).
```
> cd floir.xyz/Server/uWebSockets
> make
```
Then,
```
> cd floir.xyz/Server
> mkdir build
> cd build
> cmake ..
> make
> ./floir-server
```

## WebAssembly Server (doesn't require uWebSockets, but requires [Node.js](https://nodejs.org/en/download))
This is the server to use if you want working accounts/inventory (see [Accounts & Inventory](#accounts--inventory)).
```
> git clone https://github.com/trigonal-bacon/floir.xyz.git
> cd floir.xyz/Server
> mkdir build
> cd build
> cmake .. -DWASM_SERVER=1
> make
> cd ..
> npm install ws
> node run-server.js
```
Run `run-server.js` (not `build/floir-server.js` directly) — it loads `Account/database.js` first so the compiled server can read/write `Server/database.json`. The file is created automatically on first registration; delete it to reset all accounts.

## Client:
```
cd floir.xyz/Client
mkdir build
cd build
cmake ..
make
```
Then move the outputted ``wasm`` and ``js`` files, plus ``Client/public/index.html``, into whichever directory you run the server from (``Client/public`` for the native server, or alongside ``run-server.js`` in ``Server/`` if you're running the wasm server) — the server serves static files relative to its own working directory.

The server is served by default at ``localhost:9001``. You may change the port by modifying ``Shared/Config.cc``

# Accounts & Inventory
An account is required to spawn — the client shows a login/register panel instead of letting you play as a guest. Registering picks a username (letters/digits/underscore, 3+ chars) and password (4+ chars); credentials are hashed (SHA-256) and stored in `Server/database.json` by the WASM/Node server, which also issues a `session_key` the client saves locally to restore your session on refresh without re-entering a password.

Once logged in, your loadout and petal inventory persist across sessions: petals you pick up beyond a full loadout go into your inventory (stacked by petal type + rarity) instead of being lost, nothing is dropped on death, and you respawn with the same loadout/inventory you had before. In-game, the inventory panel (bottom-left) lets you swap stacked petals into your loadout; the equip slot that used to delete petals is now the blue "Store" slot, which stacks a petal into your inventory instead of discarding it.

# Hosting 
The client may be hosted with any http server (eg. ``nginx``, ``http-server``). The wasm server automatically hosts content at ``localhost:9001`` as well.

If hosting somewhere other than ``localhost``, use the  ``WS_URL`` constant in ``Shared/Config.cc`` to specify a websocket url.

# Compilation Flags

``DEBUG`` | ``Server & Client`` | ``Default: 0`` : compiles with assertions and failsafes. <br>
``WASM_SERVER`` | ``Server only`` | ``Default : 0`` : compiles to WASM/JS instead of a native binary. <br>
``TDM`` | ``Server only`` | ``Default: 0`` : enables TDM instead of FFA.<br>
``GENERAL_SPATIAL_HASH`` | ``Server only`` | ``Default: 0`` : uses the canonical hash grid implementation instead of a uniform grid; enable this to support large entities. <br>
``USE_CODEPOINT_LEN`` | ``Server & Client`` | ``Default: 0`` : uses the number of codepoints (characters) instead of byte length for string validation and truncation - useful for non-english characters. Should be the same on both server and client.

# License
[LICENSE](./LICENSE)