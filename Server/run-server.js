// Entry point for running the WASM server build with account persistence.
//
// The emscripten output (Server/build/floir-server.js) is regenerated on every
// build and has no room for hand-written bootstrap code, so account storage
// is wired up here instead: requiring database.js first populates
// `global.loadDatabase` / `global.saveDatabase` / etc. before the WASM module
// (and the EM_JS calls inside Server/Account/DatabaseWasm.cc) can use them.
//
// Run from the Server/ directory: `node run-server.js`

// --- libuv thread-pool sizing (MUST run before any async fs/zlib/dns work) ---
// The WebSocket state stream uses ws perMessageDeflate (see Wasm.cc). Node runs
// zlib compression ASYNC on the libuv thread pool -- the whole point is to keep
// the heavy per-packet deflate OFF the single game-tick core. But that pool
// defaults to only 4 threads (UV_THREADPOOL_SIZE unset), while the box has
// 32 vCPU with 31 cores otherwise idle (the tick loop is single-threaded).
//
// At peak, each of N clients gets one large kClientUpdate deflate per 20Hz tick
// (~2000 compressions/sec at 100 players). Funnelled through 4 threads, that
// pool saturates: compressed frames queue behind it (outbound latency + the ws
// send buffers grow = the "extreme lag"), the uplink never actually gets the
// bandwidth relief compression was added for, AND the DB flush's async
// fs.writeFile (database.js) has to wait behind the zlib backlog on the same 4
// threads. Sizing the pool to the idle cores lets compression + disk I/O
// actually run in parallel off the tick core, as the design intended.
//
// libuv reads this once, on first pool use, so it has to be set before the
// requires below touch anything async. Overridable from the environment.
process.env.UV_THREADPOOL_SIZE = process.env.UV_THREADPOOL_SIZE || '24';

require('./Account/database.js');
require('./build/floir-server.js');
