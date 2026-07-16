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

global.loadDatabase = () => {
    if (!global.db) {
        try {
            global.db = JSON.parse(fs.readFileSync(DB_PATH, 'utf8'));
        } catch {
            global.db = {};
        }
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
