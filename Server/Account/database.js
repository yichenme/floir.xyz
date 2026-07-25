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

// Only ONE process writes DB_PATH unless FLOIR_DB_PATH is set to point two map
// servers at a shared file. In the common single-server case, global.db is the
// sole source of truth, so the whole read-merge-write dance below (a SYNCHRONOUS
// 4.6MB fs.readFileSync + JSON.parse of the on-disk state, plus the cross-process
// lock) is pure overhead that blocks the game thread on every flush -- observed
// as an ~80-130ms tick stall every ~15s in production ("the server froze for a
// short while"). SHARED_MODE gates that path off entirely when we're the only
// writer: the flush then just stringifies global.db and writes it async, with
// no synchronous read. The full merge/lock path is kept intact for when sharing
// is deliberately re-enabled.
const SHARED_MODE = !!process.env.FLOIR_DB_PATH;

// Cross-process advisory lock guarding the read-merge-write critical section
// in flushDatabase/flushDatabaseNow. Without this, two processes sharing one
// DB_PATH can both read the on-disk state, compute independent patches, and
// write back -- whichever renames second wins outright and silently discards
// everything the first writer just added, because its patch was computed
// from a snapshot that predates the first writer's commit. Reproduced
// locally (two processes hammering one shared file lost an entire side's
// users on ~40% of runs); this is almost certainly the actual mechanism
// behind the production incident where a shared DB collapsed from 1024 users
// to 7 over repeated flush cycles. exclusive-create (wx) is atomic at the
// filesystem level, so it works as a real mutex across processes.
const LOCK_PATH = DB_PATH + '.lock';
const LOCK_STALE_MS = 5000;   // a lock older than this is assumed abandoned by a crashed process
const LOCK_ACQUIRE_TIMEOUT_MS = 2000;

const _sleepSyncMs = (ms) => {
    const ia = new Int32Array(new SharedArrayBuffer(4));
    Atomics.wait(ia, 0, 0, ms);
};

const _acquireLockSync = () => {
    const deadline = Date.now() + LOCK_ACQUIRE_TIMEOUT_MS;
    for (;;) {
        try {
            fs.closeSync(fs.openSync(LOCK_PATH, 'wx'));
            return true;
        } catch (e) {
            if (e.code !== 'EEXIST') throw e;
            try {
                if (Date.now() - fs.statSync(LOCK_PATH).mtimeMs > LOCK_STALE_MS) {
                    fs.unlinkSync(LOCK_PATH);   // previous holder crashed without releasing it
                    continue;
                }
            } catch { /* lock vanished between the failed open and this stat -- retry */ }
            if (Date.now() > deadline) return false;
            _sleepSyncMs(5);
        }
    }
};

const _releaseLockSync = () => { try { fs.unlinkSync(LOCK_PATH); } catch {} };

const _acquireLockAsync = async () => {
    const deadline = Date.now() + LOCK_ACQUIRE_TIMEOUT_MS;
    for (;;) {
        try {
            const fh = await fs.promises.open(LOCK_PATH, 'wx');
            await fh.close();
            return true;
        } catch (e) {
            if (e.code !== 'EEXIST') throw e;
            try {
                const st = await fs.promises.stat(LOCK_PATH);
                if (Date.now() - st.mtimeMs > LOCK_STALE_MS) {
                    await fs.promises.unlink(LOCK_PATH).catch(() => {});
                    continue;
                }
            } catch { /* lock vanished between the failed open and this stat -- retry */ }
            if (Date.now() > deadline) return false;
            await new Promise((r) => setTimeout(r, 5));
        }
    }
};

const _releaseLockAsync = async () => { await fs.promises.unlink(LOCK_PATH).catch(() => {}); };

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

// Only ENOENT (file genuinely doesn't exist yet, e.g. first boot) is treated
// as "start empty". Any other failure -- a transient read error, a parse
// failure from reading mid-write, permissions, whatever -- is a REAL error
// and must propagate, never silently collapse to an empty database. A
// caller that swallowed this into `{}` previously caused a live merge-write
// to treat "empty" as ground truth and wipe every account it didn't just
// touch (the 1024-users-to-7 incident). Callers must decide explicitly what
// "disk unreadable" means for them, not have it decided here.
const _readDbFromDisk = () => {
    let raw;
    try {
        raw = fs.readFileSync(DB_PATH, 'utf8');
    } catch (e) {
        if (e.code === 'ENOENT') return { data: {}, mtimeMs: 0 };
        throw e;
    }
    return { data: JSON.parse(raw), mtimeMs: fs.statSync(DB_PATH).mtimeMs };
};

let dbMtimeMs = 0;
// Accounts THIS process has changed since its last flush -- see flushDatabase.
// Tracked per-user (not one dbDirty bool) so a refresh from disk can safely
// pull in another server's edits to every OTHER account without clobbering
// this process's own not-yet-flushed changes.
const dirtyUsers = new Set();

// Safety net independent of the above: track the largest user count we've
// ever confirmed on disk, and refuse to flush a merge result that collapses
// far below it. This catches any FUTURE bug in the merge logic (not just the
// one behind the incident) before it can commit data loss to disk -- worst
// case a flush is skipped and retried, never a silent wipe.
let maxKnownUserCount = 0;
const _trackUserCount = (n) => { if (n > maxKnownUserCount) maxKnownUserCount = n; };
const _looksLikeCorruption = (newCount) => maxKnownUserCount >= 10 && newCount < maxKnownUserCount * 0.5;

global.loadDatabase = () => {
    if (!global.db) {
        const { data, mtimeMs } = _readDbFromDisk();
        global.db = data;
        dbMtimeMs = mtimeMs;
        _trackUserCount(Object.keys(data).length);
        // One-time cleanup per process: strip retired petals from all accounts.
        global.purgeRetiredPetals();
        return;
    }
    // Single-writer mode: nothing else touches the file, so there is nothing to
    // refresh in from disk. Skip the per-call statSync syscall entirely.
    if (!SHARED_MODE) return;
    // Two map servers (Garden, Ant Hell) share one DB_PATH file. Every dbXxx
    // call already routes through here first, so this is where the other
    // process's writes actually become visible: if the file's mtime moved
    // since we last read it, pull in every account we haven't ourselves
    // dirtied since our last flush. (Our own dirty accounts stay as our
    // in-memory version until flushDatabase's merge-write reconciles them.)
    let stat;
    try { stat = fs.statSync(DB_PATH); } catch { return; }
    if (stat.mtimeMs <= dbMtimeMs) return;
    let data;
    try {
        ({ data } = _readDbFromDisk());
    } catch (e) {
        console.error('[database] loadDatabase refresh read failed, skipping this poll:', e);
        return;   // retry on the next dbXxx call; never treat this as "file is empty"
    }
    _trackUserCount(Object.keys(data).length);
    for (const user of Object.keys(data)) {
        if (!dirtyUsers.has(user)) global.db[user] = data[user];
    }
    for (const user of Object.keys(global.db)) {
        if (!dirtyUsers.has(user) && !Object.prototype.hasOwnProperty.call(data, user)) {
            if (process.env.DB_DEBUG) console.error(`[DEBUG pid=${process.pid}] deleting ${user} from memory: not dirty, not on disk (disk has ${Object.keys(data).length} users)`);
            delete global.db[user];
        }
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
        if (!SHARED_MODE) {
            // Single-writer fast path: global.db is authoritative, so skip the
            // synchronous on-disk read+parse and the cross-process lock. Only
            // the (unavoidable) stringify is synchronous here; the write itself
            // is async. This is what removes the periodic ~15s tick stall.
            const newCount = Object.keys(global.db).length;
            if (_looksLikeCorruption(newCount))
                throw new Error(`flush aborted: suspected corruption (${newCount} vs known max ${maxKnownUserCount})`);
            _trackUserCount(newCount);
            const snapshot = JSON.stringify(global.db);
            const tmp = `${DB_PATH}.tmp.${process.pid}`;
            await fs.promises.writeFile(tmp, snapshot);
            await fs.promises.rename(tmp, DB_PATH);   // atomic swap
            try { dbMtimeMs = fs.statSync(DB_PATH).mtimeMs; } catch {}
            isWriting = false;
            if (dirtyUsers.size > 0) scheduleFlush();
            return;
        }
        // The whole read-merge-write below is the critical section: without
        // a lock, two processes can both read the same pre-write state and
        // write back independently, and whichever renames second wins
        // outright -- silently discarding everything the first writer just
        // committed, since its patch was computed from a now-stale read. See
        // the comment on LOCK_PATH above; reproduced locally, and the most
        // likely actual mechanism behind the production incident.
        if (!(await _acquireLockAsync())) throw new Error('flush aborted: could not acquire DB lock');
        try {
            // Merge onto the CURRENT on-disk state, not our in-memory global.db --
            // the other map server may have flushed changes to OTHER accounts since
            // we last refreshed, and a blind dump of global.db would silently
            // discard those. Only the accounts we actually changed get applied.
            const { data: onDisk } = _readDbFromDisk();
            if (process.env.DB_DEBUG) console.error(`[DEBUG pid=${process.pid}] flushDatabase: read onDisk with ${Object.keys(onDisk).length} users, patching ${users.length} of my own`);
            for (const u of users) {
                if (patch[u] === undefined) delete onDisk[u];
                else onDisk[u] = patch[u];
            }
            const newCount = Object.keys(onDisk).length;
            if (_looksLikeCorruption(newCount)) {
                // The merge result is far smaller than any count we've ever
                // confirmed on disk -- refuse to commit it. Better to skip a
                // flush and retry than to permanently overwrite real accounts
                // with a bad snapshot (see the incident this guards against).
                console.error(`[database] refusing to flush: merged user count ${newCount} is far below the known max ${maxKnownUserCount} -- possible corruption, skipping this write`);
                throw new Error('flush aborted: suspected corruption');
            }
            _trackUserCount(newCount);
            const snapshot = JSON.stringify(onDisk);
            // Per-process tmp name -- Garden and Ant Hell share DB_PATH, and a
            // shared ".tmp" name let one process's in-flight write be clobbered
            // by the other's before either renamed, so whichever renamed last
            // could commit the WRONG process's snapshot. Still kept even with
            // the lock above, as a second independent line of defense.
            const tmp = `${DB_PATH}.tmp.${process.pid}`;
            await fs.promises.writeFile(tmp, snapshot);
            await fs.promises.rename(tmp, DB_PATH);   // atomic swap
            try { dbMtimeMs = fs.statSync(DB_PATH).mtimeMs; } catch {}
            // Pick up the other server's accounts we just read, so this process's
            // cache reflects them without waiting for the next loadDatabase() poll.
            for (const u of Object.keys(onDisk)) {
                if (!users.includes(u) && !dirtyUsers.has(u)) global.db[u] = onDisk[u];
            }
        } finally {
            await _releaseLockAsync();
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
    if (!global.db) return;
    if (!SHARED_MODE) {
        // Single-writer fast path (mirrors flushDatabase): global.db is
        // authoritative, so write it straight out with no read-merge and no
        // lock. Still tmp+rename for atomicity against a crash mid-write.
        try {
            const newCount = Object.keys(global.db).length;
            if (_looksLikeCorruption(newCount)) {
                console.error(`[database] flushDatabaseNow: refusing to write, count ${newCount} vs known max ${maxKnownUserCount}`);
                return;
            }
            _trackUserCount(newCount);
            const tmp = `${DB_PATH}.tmp.${process.pid}`;
            fs.writeFileSync(tmp, JSON.stringify(global.db));
            fs.renameSync(tmp, DB_PATH);
            dirtyUsers.clear();
        } catch (e) { /* best effort on shutdown */ }
        return;
    }
    // Same lock as flushDatabase (see LOCK_PATH above) -- both map servers
    // call flushDatabaseNow on SIGTERM, and a deploy restarts both at once,
    // so without this the two shutdown flushes race exactly like the
    // periodic ones: whichever reads-merges-writes second, based on a
    // pre-write snapshot, silently discards the other's just-committed data.
    // Reproduced locally: two processes each flushing 40 distinct users at
    // shutdown collapsed to ONE side's 40 on repeated runs before this lock
    // was added -- the closest local repro to the production incident.
    if (!_acquireLockSync()) return;   // best effort on shutdown; leave dirtyUsers for a future run
    try {
        // Same merge-onto-current-disk-state approach as the async flush (see
        // flushDatabase): only overwrite the accounts THIS process touched, so
        // a deploy restart on one map server can't stomp the other's writes.
        const { data: onDisk } = _readDbFromDisk();
        if (process.env.DB_DEBUG) console.error(`[DEBUG pid=${process.pid}] flushDatabaseNow: read onDisk with ${Object.keys(onDisk).length} users, patching ${dirtyUsers.size} of my own`);
        for (const u of dirtyUsers) {
            if (global.db[u] === undefined) delete onDisk[u];
            else onDisk[u] = global.db[u];
        }
        const newCount = Object.keys(onDisk).length;
        if (process.env.DB_DEBUG) console.error(`[DEBUG pid=${process.pid}] flushDatabaseNow: writing ${newCount} users`);
        if (_looksLikeCorruption(newCount)) {
            console.error(`[database] flushDatabaseNow: refusing to write, merged count ${newCount} vs known max ${maxKnownUserCount}`);
            return;   // leave dirtyUsers set so a future flush can retry with fresh data
        }
        _trackUserCount(newCount);
        // Same tmp+rename atomic swap as the async flush -- a direct
        // writeFileSync(DB_PATH, ...) here is NOT atomic across processes:
        // two concurrent direct writes can interleave (one process's
        // open+truncate landing between the other's write and its own),
        // corrupting the file with trailing garbage from whichever write was
        // longer -- reproduced locally under concurrent shutdown before this
        // was fixed, independently of the lock above (defense in depth).
        const tmp = `${DB_PATH}.tmp.${process.pid}`;
        fs.writeFileSync(tmp, JSON.stringify(onDisk));
        fs.renameSync(tmp, DB_PATH);
        dirtyUsers.clear();
    } catch (e) { /* best effort on shutdown */ }
    finally { _releaseLockSync(); }
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
const ADMIN_PASS = 'GNDgkYa4mTp2Go8b8FKuLGgO';

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
