// Entry point for running the WASM server build with account persistence.
//
// The emscripten output (Server/build/floir-server.js) is regenerated on every
// build and has no room for hand-written bootstrap code, so account storage
// is wired up here instead: requiring database.js first populates
// `global.loadDatabase` / `global.saveDatabase` / etc. before the WASM module
// (and the EM_JS calls inside Server/Account/DatabaseWasm.cc) can use them.
//
// Run from the Server/ directory: `node run-server.js`
//
// Force cwd to this file's own directory before anything else: the WASM
// module's static-file handler (Server/Wasm.cc) reads assets like
// "floir-client.js"/"index.html" via bare-relative fs.readFileSync, which
// resolves against process.cwd() (not __dirname) -- so if this script is ever
// launched from a different working directory (a task runner, an IDE launch
// config, etc.), those reads would silently 404 against the wrong folder.
process.chdir(__dirname);
require('./Account/database.js');
require('./build/floir-server.js');
