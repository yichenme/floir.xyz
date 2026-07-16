// Entry point for running the WASM server build with account persistence.
//
// The emscripten output (Server/build/floir-server.js) is regenerated on every
// build and has no room for hand-written bootstrap code, so account storage
// is wired up here instead: requiring database.js first populates
// `global.loadDatabase` / `global.saveDatabase` / etc. before the WASM module
// (and the EM_JS calls inside Server/Account/DatabaseWasm.cc) can use them.
//
// Run from the Server/ directory: `node run-server.js`
require('./Account/database.js');
require('./build/floir-server.js');
