// Reusable Node-side account store required by the WASM server build (see
// Server/Account/DatabaseWasm.cc, which calls these globals through EM_JS) and
// by `node Server/run-server.js`. Keeping this in `global` (rather than
// module.exports) matches how the emscripten glue in Server/Wasm.cc reaches
// into Node built-ins from inside EM_ASM blocks.
'use strict';

const fs = require('fs');
const crypto = require('crypto');
const path = require('path');

// Override via FLOIR_DB_PATH so two map servers (Garden, Ant Hell -- separate
// processes, separate directories) can point at ONE shared file instead of
// each keeping its own account DB. Defaults to the old per-directory path so
// a single-server local run needs no env var.
const DB_PATH = process.env.FLOIR_DB_PATH || path.join(__dirname, '..', 'database.json');

// Petal IDs that have been removed from the game. They stay numbered in the
// PetalID enum (so surviving petals keep their saved IDs), but any copies still
// sitting in accounts are purged: dropped from inventory, and cleared to an
// empty slot (type 0) in loadouts. kTringer (29) folded into kStinger.
// 38 = kUniqueBasic ("Basic" at Unique rarity): retired, was a stat-less joke
// reward granted at a 0.1% chance -- no longer grantable (see Server/Spawn.cc,
// Server/Game.cc).
// 50 = Mjolnir: not retired, but a transient leaderboard-#1 reward that must
// never live in a saved account (stripped on load as a backstop so it can't be
// duplicated/kept via any persistence path).
const RETIRED_PETALS = new Set([3, 6, 21, 23, 27, 29, 30, 32, 35, 36, 38, 50]);

global.purgeRetiredPetals = () => {
    for (const user of Object.keys(global.db)) {
        const acct = global.db[user];
        if (!acct) continue;
        let changed = false;
        if (Array.isArray(acct.inventory)) {
            const kept = acct.inventory.filter((s) => !RETIRED_PETALS.has(s.type | 0));
            if (kept.length !== acct.inventory.length) { acct.inventory = kept; changed = true; }
        }
        if (Array.isArray(acct.loadout)) {
            for (const slot of acct.loadout) {
                if (slot && RETIRED_PETALS.has(slot.type | 0)) { slot.type = 0; slot.rarity = 0; changed = true; }
            }
        }
        if (changed) global.saveDatabase(user);
    }
};

const _readDbFromDisk = () => {
    try {
        return { data: JSON.parse(fs.readFileSync(DB_PATH, 'utf8')), mtimeMs: fs.statSync(DB_PATH).mtimeMs };
    } catch {
        return { data: {}, mtimeMs: 0 };
    }
};

let dbMtimeMs = 0;
// Accounts THIS process has changed since its last flush -- see flushDatabase.
// Tracked per-user (not one dbDirty bool) so a refresh from disk can safely
// pull in another server's edits to every OTHER account without clobbering
// this process's own not-yet-flushed changes.
const dirtyUsers = new Set();

global.loadDatabase = () => {
    if (!global.db) {
        const { data, mtimeMs } = _readDbFromDisk();
        global.db = data;
        dbMtimeMs = mtimeMs;
        // One-time cleanup per process: strip retired petals from all accounts.
        global.purgeRetiredPetals();
        return;
    }
    // Two map servers (Garden, Ant Hell) share one DB_PATH file. Every dbXxx
    // call already routes through here first, so this is where the other
    // process's writes actually become visible: if the file's mtime moved
    // since we last read it, pull in every account we haven't ourselves
    // dirtied since our last flush. (Our own dirty accounts stay as our
    // in-memory version until flushDatabase's merge-write reconciles them.)
    let stat;
    try { stat = fs.statSync(DB_PATH); } catch { return; }
    if (stat.mtimeMs <= dbMtimeMs) return;
    const { data } = _readDbFromDisk();
    for (const user of Object.keys(data)) {
        if (!dirtyUsers.has(user)) global.db[user] = data[user];
    }
    for (const user of Object.keys(global.db)) {
        if (!dirtyUsers.has(user) && !Object.prototype.hasOwnProperty.call(data, user)) delete global.db[user];
    }
    dbMtimeMs = stat.mtimeMs;
};

// Coalesced, ASYNC persistence. The whole account DB is one JSON file; writing
// it on every account mutation (and O(N) times per 15s persist pass) previously
// froze the tick loop for hundreds of ms. Now every mutation just marks the DB
// dirty and a single flush is scheduled at most once per FLUSH_INTERVAL_MS.
//
// The flush writes ASYNCHRONOUSLY (fs.writeFile) so the disk I/O -- the slow
// part, especially on a slow disk -- doesn't block the event loop / tick loop
// under load. Only the JSON.stringify itself is synchronous (CPU-bound, brief),
// and it's taken as a snapshot BEFORE the await so concurrent mutations during
// the write are safe and never lost. Design notes:
//   * Fixed-interval coalescing (schedule once, do NOT reset the timer on every
//     save) -- a debounce-reset would starve the flush on a busy server and
//     lose a lot of data on a crash. This guarantees a flush every ~3s.
//   * dirtyUsers is drained into a local snapshot BEFORE the async write, so a
//     mutation landing on some user during the write re-adds that user to
//     dirtyUsers and gets picked up by the next flush (rather than being
//     cleared away after the await).
//   * isWriting only prevents OVERLAPPING writes; it never causes a mutation to
//     skip marking the DB dirty.
//   * A temp-file + atomic rename avoids a truncated/corrupt DB if the process
//     dies mid-write.
//   * flushDatabaseNow stays SYNCHRONOUS: the shutdown handler (Wasm.cc) calls
//     it and then process.exit(0) without awaiting, so it must finish the write
//     before returning -- an async version would let the process exit mid-write.
let flushTimer = null;
let isWriting = false;
// The flush's JSON.stringify of the whole DB is synchronous and blocks the
// tick loop (~38ms at 3MB / 750 accounts, and growing). At the old 3s
// interval that was a visible stutter every 3 seconds; 15s makes it 5x
// rarer. Graceful shutdown (flushDatabaseNow, wired to SIGTERM/SIGINT) still
// saves everything on deploys/restarts, so only an ungraceful crash can lose
// up to ~15s of progress.
const FLUSH_INTERVAL_MS = 15000;

const scheduleFlush = () => {
    if (flushTimer !== null) return;   // already scheduled (fixed interval, no reset)
    flushTimer = setTimeout(() => { flushTimer = null; flushDatabase(); }, FLUSH_INTERVAL_MS);
};

const flushDatabase = async () => {
    if (isWriting || dirtyUsers.size === 0) return;
    isWriting = true;
    // Snapshot which accounts we're flushing and their CURRENT in-memory state,
    // then clear dirty for exactly those users -- before any await, so a
    // mutation landing during the write re-marks its user dirty and isn't lost.
    const users = Array.from(dirtyUsers);
    dirtyUsers.clear();
    const patch = {};
    for (const u of users) patch[u] = global.db[u];   // undefined => deleted (ban)
    try {
        // Merge onto the CURRENT on-disk state, not our in-memory global.db --
        // the other map server may have flushed changes to OTHER accounts since
        // we last refreshed, and a blind dump of global.db would silently
        // discard those. Only the accounts we actually changed get applied.
        const { data: onDisk } = _readDbFromDisk();
        for (const u of users) {
            if (patch[u] === undefined) delete onDisk[u];
            else onDisk[u] = patch[u];
        }
        const snapshot = JSON.stringify(onDisk);
        const tmp = DB_PATH + '.tmp';
        await fs.promises.writeFile(tmp, snapshot);
        await fs.promises.rename(tmp, DB_PATH);   // atomic swap
        try { dbMtimeMs = fs.statSync(DB_PATH).mtimeMs; } catch {}
        // Pick up the other server's accounts we just read, so this process's
        // cache reflects them without waiting for the next loadDatabase() poll.
        for (const u of Object.keys(onDisk)) {
            if (!users.includes(u) && !dirtyUsers.has(u)) global.db[u] = onDisk[u];
        }
    } catch (e) {
        for (const u of users) dirtyUsers.add(u);   // write failed -- retry next flush
    } finally {
        isWriting = false;
    }
    if (dirtyUsers.size > 0) scheduleFlush();
};

global.saveDatabase = (user) => {
    if (user !== undefined) dirtyUsers.add(user);
    scheduleFlush();
};

// SYNCHRONOUS immediate flush -- called by the SIGTERM/SIGINT graceful-shutdown
// handler (Server/Wasm.cc) right before process.exit(0). It must block until the
// write completes (the process is exiting; there's no chance to await), so it
// always writes the CURRENT state synchronously, superseding any in-flight async
// write. A deploy's pm2 restart therefore never drops the last few seconds.
global.flushDatabaseNow = () => {
    if (flushTimer !== null) { clearTimeout(flushTimer); flushTimer = null; }
    try {
        // Same merge-onto-current-disk-state approach as the async flush (see
        // flushDatabase): only overwrite the accounts THIS process touched, so
        // a deploy restart on one map server can't stomp the other's writes.
        const { data: onDisk } = _readDbFromDisk();
        for (const u of dirtyUsers) {
            if (global.db[u] === undefined) delete onDisk[u];
            else onDisk[u] = global.db[u];
        }
        fs.writeFileSync(DB_PATH, JSON.stringify(onDisk));
        dirtyUsers.clear();
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

global.dbGetTalents = (user) => {
    global.loadDatabase();
    const acct = global.db[user];
    return acct ? JSON.stringify({ health: acct.talentHealth || 0, reload: acct.talentReload || 0 }) : null;
};

global.dbSetTalents = (user, healthRank, reloadRank) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct) return false;
    acct.talentHealth = healthRank;
    acct.talentReload = reloadRank;
    global.saveDatabase(user);
    return true;
};

// --- Admin panel (served at /admin via Server/Wasm.cc) -----------------------
// Single hardcoded admin credential; every request re-sends it (over HTTPS).
const ADMIN_USER = 'admin';
const ADMIN_PASS = 'loveYY66$$';

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
    // Permanently deletes the account record (password, session, loadout,
    // inventory, kills -- everything). There is no undo: the row is gone from
    // global.db and the next flush writes that removal to disk. This does NOT
    // block re-registration of the same username -- it deletes the account,
    // it is not a persistent username ban.
    if (req.action === 'ban') {
        const name = String(req.target || '');
        if (!Object.prototype.hasOwnProperty.call(global.db, name))
            return JSON.stringify({ ok: false, error: 'no such account' });
        delete global.db[name];
        global.saveDatabase(name);
        return JSON.stringify({ ok: true, message: 'permanently deleted ' + name });
    }
    return JSON.stringify({ ok: false, error: 'unknown action' });
};
