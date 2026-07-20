// Off-main-thread persistence worker for the account DB (spawned by
// Server/Account/database.js via worker_threads).
//
// WHY A WORKER, AND WHY INCREMENTAL: the whole account DB is one JSON file
// (~3.2MB / 750+ accounts and growing). Serializing it with JSON.stringify
// takes ~35ms, and doing that on the game thread every flush stalls the 20 TPS
// tick loop (a visible freeze). Moving the stringify to a worker only helps if
// the DATA doesn't also have to be serialized to cross the thread boundary --
// and it does: postMessage/structuredClone of the whole DB costs ~37ms
// (measured v8.serialize 36.7ms vs JSON.stringify 34.9ms), so shipping the
// full DB every flush just moves the same-size block to the clone step. No win.
//
// So this worker keeps its OWN copy of the DB. The main thread sends it one
// small changed-account per mutation ({type:'set'} -- a few KB, sub-ms to
// clone), the worker applies it, and the worker alone does the full
// JSON.stringify + atomic write on its own thread on a timer. The game thread
// never serializes the whole DB again. Main still keeps global.db as the
// authoritative in-memory copy for synchronous reads and for the synchronous
// shutdown flush (see database.js flushDatabaseNow), so this worker is a live
// checkpoint writer, not the source of truth.
'use strict';

const fs = require('fs');
const { parentPort, workerData } = require('worker_threads');

const DB_PATH = workerData.dbPath;
const FLUSH_INTERVAL_MS = workerData.intervalMs || 1000;
const TMP_PATH = DB_PATH + '.wtmp';

// The worker's private copy of the DB, kept in sync with the main thread's
// global.db by the per-mutation messages below.
let db = workerData.initialDb || {};
let dirty = false;
let writing = false;

parentPort.on('message', (msg) => {
    switch (msg.type) {
        case 'init':                 // full resync (startup / bulk mutation)
            db = msg.db || {};
            dirty = true;
            break;
        case 'set':                  // one account changed
            db[msg.user] = msg.acct;
            dirty = true;
            break;
        case 'del':
            delete db[msg.user];
            dirty = true;
            break;
        case 'flush':                // force a write now (best effort)
            flush();
            break;
    }
});

async function flush() {
    if (writing || !dirty) return;
    writing = true;
    // Clear dirty BEFORE serializing so a mutation arriving during the async
    // write re-marks it and gets picked up by the next tick (never dropped).
    dirty = false;
    let snapshot;
    try {
        snapshot = JSON.stringify(db);
    } catch (e) {
        dirty = true; writing = false; return;
    }
    try {
        await fs.promises.writeFile(TMP_PATH, snapshot);
        await fs.promises.rename(TMP_PATH, DB_PATH);   // atomic swap
    } catch (e) {
        dirty = true;   // write failed -- retry on the next interval
    } finally {
        writing = false;
    }
}

// The whole point: this timer -- and the JSON.stringify it drives -- runs on
// the worker thread, so a 1s flush interval never touches the game tick loop.
setInterval(flush, FLUSH_INTERVAL_MS);
