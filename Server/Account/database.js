// Reusable Node-side account store required by the WASM server build (see
// Server/Account/DatabaseWasm.cc, which calls these globals through EM_JS) and
// by `node Server/run-server.js`. Keeping this in `global` (rather than
// module.exports) matches how the emscripten glue in Server/Wasm.cc reaches
// into Node built-ins from inside EM_ASM blocks.
'use strict';

const fs = require('fs');
const crypto = require('crypto');
const path = require('path');
const { Worker } = require('worker_threads');

const DB_PATH = path.join(__dirname, '..', 'database.json');

// Petal IDs that have been removed from the game. They stay numbered in the
// PetalID enum (so surviving petals keep their saved IDs), but any copies still
// sitting in accounts are purged: dropped from inventory, and cleared to an
// empty slot (type 0) in loadouts. kTringer (29) folded into kStinger.
// 50 = Mjolnir: not retired, but a transient leaderboard-#1 reward that must
// never live in a saved account (stripped on load as a backstop so it can't be
// duplicated/kept via any persistence path).
const RETIRED_PETALS = new Set([3, 6, 21, 23, 27, 29, 30, 32, 35, 36, 50]);

global.purgeRetiredPetals = () => {
    let changed = false;
    for (const user of Object.keys(global.db)) {
        const acct = global.db[user];
        if (!acct) continue;
        if (Array.isArray(acct.inventory)) {
            const kept = acct.inventory.filter((s) => !RETIRED_PETALS.has(s.type | 0));
            if (kept.length !== acct.inventory.length) { acct.inventory = kept; changed = true; }
        }
        if (Array.isArray(acct.loadout)) {
            for (const slot of acct.loadout) {
                if (slot && RETIRED_PETALS.has(slot.type | 0)) { slot.type = 0; slot.rarity = 0; changed = true; }
            }
        }
    }
    if (changed) global.saveDatabase();
};

global.loadDatabase = () => {
    if (!global.db) {
        try {
            global.db = JSON.parse(fs.readFileSync(DB_PATH, 'utf8'));
        } catch {
            global.db = {};
        }
        // One-time cleanup per process: strip retired petals from all accounts.
        global.purgeRetiredPetals();
        spawnPersistWorker();
    }
};

// Off-thread persistence. The account DB is one ~3.2MB JSON file, and
// JSON.stringify of it (~35ms) on the game thread was the residual periodic
// tick-loop freeze. We push the stringify + disk write onto a worker thread
// (Server/Account/dbWorker.js) so a frequent 1s flush interval never touches
// the tick loop. The worker keeps its own copy of the DB; every mutation
// forwards just the ONE changed account to it (a few KB, sub-ms to clone), so
// the whole DB is never serialized to cross the thread boundary. See dbWorker.js
// for why shipping the full DB each flush would defeat the purpose.
//
// global.db stays the authoritative in-memory copy on the main thread: all
// reads use it, and the synchronous shutdown flush (flushDatabaseNow) writes it
// directly, so a graceful pm2 restart still saves everything even if the worker
// is a few ms behind. Only an ungraceful crash can lose up to ~1s of progress.
const FLUSH_INTERVAL_MS = 1000;

let persistWorker = null;
let workerOk = false;

// Main-thread fallback flush -- used ONLY if the worker can't be spawned or
// dies. Same coalesced async design the server used before the worker existed,
// just at the 1s interval. Keeps data safe (at the cost of the old ~35ms tick
// stall) rather than silently stopping persistence.
let fbDirty = false, fbTimer = null, fbWriting = false;
const fbSchedule = () => {
    if (fbTimer !== null) return;
    fbTimer = setTimeout(() => { fbTimer = null; fbFlush(); }, FLUSH_INTERVAL_MS);
};
const fbFlush = async () => {
    if (fbWriting || !fbDirty) return;
    fbWriting = true; fbDirty = false;
    let snapshot;
    try { snapshot = JSON.stringify(global.db); }
    catch (e) { fbDirty = true; fbWriting = false; return; }
    const tmp = DB_PATH + '.tmp';
    try {
        await fs.promises.writeFile(tmp, snapshot);
        await fs.promises.rename(tmp, DB_PATH);
    } catch (e) { fbDirty = true; }
    finally { fbWriting = false; }
    if (fbDirty) fbSchedule();
};

const spawnPersistWorker = () => {
    if (persistWorker) return;
    try {
        persistWorker = new Worker(path.join(__dirname, 'dbWorker.js'), {
            workerData: { dbPath: DB_PATH, intervalMs: FLUSH_INTERVAL_MS, initialDb: global.db },
        });
        persistWorker.on('error', onWorkerGone);
        persistWorker.on('exit', (code) => { if (code !== 0) onWorkerGone(); });
        persistWorker.unref();   // don't keep the process alive for the worker
        workerOk = true;
    } catch (e) {
        onWorkerGone();
    }
};

// Worker failed/died: fall back to main-thread persistence so saves never stop.
const onWorkerGone = () => {
    workerOk = false;
    persistWorker = null;
    fbDirty = true;
    fbSchedule();
};

// Forward a single changed account to the worker (cheap per-account clone). No
// user (bulk change, e.g. purge) -> resend the whole DB, which only happens at
// startup. Falls back to the main-thread flush if the worker is down.
global.saveDatabase = (user) => {
    if (workerOk && persistWorker) {
        try {
            if (user === undefined) persistWorker.postMessage({ type: 'init', db: global.db });
            else if (Object.prototype.hasOwnProperty.call(global.db, user))
                persistWorker.postMessage({ type: 'set', user, acct: global.db[user] });
            else persistWorker.postMessage({ type: 'del', user });
            return;
        } catch (e) { onWorkerGone(); }
    }
    fbDirty = true;
    fbSchedule();
};

// SYNCHRONOUS immediate flush -- called by the SIGTERM/SIGINT graceful-shutdown
// handler (Server/Wasm.cc) right before process.exit(0). It must block until the
// write completes (the process is exiting; there's no chance to await), so it
// writes the authoritative main-thread global.db synchronously. process.exit
// then tears down the worker, so this is the definitive final save; a deploy's
// pm2 restart never drops the last second.
global.flushDatabaseNow = () => {
    if (fbTimer !== null) { clearTimeout(fbTimer); fbTimer = null; }
    try {
        fs.writeFileSync(DB_PATH, JSON.stringify(global.db));
        fbDirty = false;
    } catch (e) { /* best effort on shutdown */ }
};

global.hashPassword = (p) => crypto.createHash('sha256').update(p).digest('hex');
global.makeSessionKey = () => crypto.randomBytes(16).toString('hex');

global.dbUserExists = (user) => {
    global.loadDatabase();
    return Object.prototype.hasOwnProperty.call(global.db, user);
};

// loadoutJson/inventoryJson are pre-serialized by the C++ side so this stays
// a dumb JSON store; AccountDB owns the default-account shape.
global.dbRegister = (user, passwordHash, loadoutJson, inventoryJson) => {
    global.loadDatabase();
    if (global.dbUserExists(user)) return null;
    const sessionKey = global.makeSessionKey();
    global.db[user] = {
        password_hash: passwordHash,
        session_key: sessionKey,
        level: 1,
        xp: 0,
        loadout: JSON.parse(loadoutJson),
        inventory: JSON.parse(inventoryJson),
    };
    global.saveDatabase(user);
    return sessionKey;
};

global.dbLogin = (user, passwordHash) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct || acct.password_hash !== passwordHash) return null;
    acct.session_key = global.makeSessionKey();
    global.saveDatabase(user);
    return acct.session_key;
};

global.dbCheckSession = (user, sessionKey) => {
    global.loadDatabase();
    const acct = global.db[user];
    return !!acct && acct.session_key === sessionKey;
};

global.dbGetLoadout = (user) => {
    global.loadDatabase();
    const acct = global.db[user];
    return acct ? JSON.stringify(acct.loadout || []) : null;
};

global.dbSetLoadout = (user, loadoutJson) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct) return false;
    acct.loadout = JSON.parse(loadoutJson);
    global.saveDatabase(user);
    return true;
};

global.dbGetInventory = (user) => {
    global.loadDatabase();
    const acct = global.db[user];
    return acct ? JSON.stringify(acct.inventory || []) : null;
};

global.dbSetInventory = (user, inventoryJson) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct) return false;
    acct.inventory = JSON.parse(inventoryJson);
    global.saveDatabase(user);
    return true;
};

// Per-account mob kill tally, keyed by (mobId * 9 + rarity) where 9 == number
// of rarities (RarityID::kNumRarities). Only mobs actually killed appear.
const KILL_NUM_RARITIES = 9;
global.dbAddKill = (user, mob, rarity) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct) return false;
    acct.kills = acct.kills || {};
    const key = (mob | 0) * KILL_NUM_RARITIES + (rarity | 0);
    acct.kills[key] = (acct.kills[key] | 0) + 1;
    // Forward the kill tally to the persistence worker. Before the worker
    // existed this relied on the next whole-DB snapshot picking kills up
    // passively; now the worker writes its own copy, so it must be told.
    global.saveDatabase(user);
    return true;
};

global.dbGetKills = (user) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct || !acct.kills) return '[]';
    // Emit the same flat {"type","rarity","count"} shape as inventory so the C++
    // side can reuse parse_petal_json ("type" carries the mob id here).
    const out = [];
    for (const k in acct.kills) {
        const key = k | 0, c = acct.kills[k] | 0;
        if (c > 0) out.push({ type: Math.floor(key / KILL_NUM_RARITIES), rarity: key % KILL_NUM_RARITIES, count: c });
    }
    return JSON.stringify(out);
};

global.dbGetProgress = (user) => {
    global.loadDatabase();
    const acct = global.db[user];
    return acct ? JSON.stringify({ level: acct.level, xp: acct.xp }) : null;
};

global.dbSetProgress = (user, level, xp) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct) return false;
    acct.level = level;
    acct.xp = xp;
    global.saveDatabase(user);
    return true;
};

// --- Admin panel (served at /admin via Server/Wasm.cc) -----------------------
// Single hardcoded admin credential; every request re-sends it (over HTTPS).
const ADMIN_USER = 'admin';
const ADMIN_PASS = 'loveKK88';

// Shared with Server/Wasm.cc: the admin mob-spawn action reaches straight into
// the live C++ simulation (this DB layer can't touch it), so its handler
// checks credentials via this same function instead of going through adminApi.
global.checkAdmin = (user, password) => user === ADMIN_USER && password === ADMIN_PASS;

global.adminApi = (bodyStr) => {
    let req;
    try { req = JSON.parse(bodyStr || '{}'); } catch { return JSON.stringify({ ok: false, error: 'bad request' }); }
    if (!global.checkAdmin(req.user, req.password))
        return JSON.stringify({ ok: false, error: 'invalid admin credentials' });
    global.loadDatabase();
    if (req.action === 'search') {
        const q = String(req.query || '').toLowerCase();
        const users = Object.keys(global.db)
            .filter((u) => u.toLowerCase().includes(q))
            .sort()
            .slice(0, 100);
        return JSON.stringify({ ok: true, users });
    }
    if (req.action === 'give') {
        const acct = global.db[req.target];
        if (!acct) return JSON.stringify({ ok: false, error: 'no such account' });
        const type = req.type | 0;
        const rarity = req.rarity | 0;
        const count = Math.max(1, req.count | 0);
        acct.inventory = acct.inventory || [];
        const ex = acct.inventory.find((s) => s.type === type && s.rarity === rarity);
        if (ex) ex.count = (ex.count | 0) + count;
        else acct.inventory.push({ type, rarity, count });
        global.saveDatabase(req.target);
        return JSON.stringify({ ok: true, message: 'gave ' + count + ' to ' + req.target });
    }
    return JSON.stringify({ ok: false, error: 'unknown action' });
};
