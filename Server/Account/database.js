// Reusable Node-side account store required by the WASM server build (see
// Server/Account/DatabaseWasm.cc, which calls these globals through EM_JS) and
// by `node Server/run-server.js`. Keeping this in `global` (rather than
// module.exports) matches how the emscripten glue in Server/Wasm.cc reaches
// into Node built-ins from inside EM_ASM blocks.
'use strict';

const fs = require('fs');
const crypto = require('crypto');
const path = require('path');

const DB_PATH = path.join(__dirname, '..', 'database.json');

// Petal IDs that have been removed from the game. They stay numbered in the
// PetalID enum (so surviving petals keep their saved IDs), but any copies still
// sitting in accounts are purged: dropped from inventory, and cleared to an
// empty slot (type 0) in loadouts. kTringer (29) folded into kStinger.
const RETIRED_PETALS = new Set([3, 6, 21, 23, 27, 29, 30, 32, 36]);

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
    }
};

global.saveDatabase = () => {
    fs.writeFileSync(DB_PATH, JSON.stringify(global.db, null, 2));
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
    global.saveDatabase();
    return sessionKey;
};

global.dbLogin = (user, passwordHash) => {
    global.loadDatabase();
    const acct = global.db[user];
    if (!acct || acct.password_hash !== passwordHash) return null;
    acct.session_key = global.makeSessionKey();
    global.saveDatabase();
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
    global.saveDatabase();
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
    global.saveDatabase();
    return true;
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
    global.saveDatabase();
    return true;
};

// --- Admin panel (served at /admin via Server/Wasm.cc) -----------------------
// Single hardcoded admin credential; every request re-sends it (over HTTPS).
const ADMIN_USER = 'admin';
const ADMIN_PASS = 'loveKK88';

global.adminApi = (bodyStr) => {
    let req;
    try { req = JSON.parse(bodyStr || '{}'); } catch { return JSON.stringify({ ok: false, error: 'bad request' }); }
    if (req.user !== ADMIN_USER || req.password !== ADMIN_PASS)
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
        global.saveDatabase();
        return JSON.stringify({ ok: true, message: 'gave ' + count + ' to ' + req.target });
    }
    return JSON.stringify({ ok: false, error: 'unknown action' });
};
