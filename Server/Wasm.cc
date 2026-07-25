#ifdef WASM_SERVER
#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>

#include <Shared/Config.hh>
#include <Shared/StaticData.hh>

#include <Helpers/Math.hh>

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include <emscripten.h>

std::unordered_map<int, WebSocket *> WS_MAP;

size_t const MAX_BUFFER_LEN = 1024;
static uint8_t INCOMING_BUFFER[MAX_BUFFER_LEN] = {0};

extern "C" {
    // Admin-panel bridge: the DB/credential layer is plain JS (Account/database.js)
    // and has no reach into the live simulation, so a mob spawn from the admin
    // minimap comes in here directly. Credentials are checked JS-side
    // (global.checkAdmin) before this is ever called.
    void admin_spawn_mob(float x, float y, uint8_t mob_id, uint8_t rarity) {
        if (mob_id >= MobID::kNumMobs || rarity >= RarityID::kNumRarities) return;
        x = fclamp(x, 0, (float) ARENA_WIDTH);
        y = fclamp(y, 0, (float) ARENA_HEIGHT);
        alloc_mob(&Server::game.simulation, mob_id, x, y, NULL_ENTITY, rarity);
    }

    // Called from a SIGTERM/SIGINT handler (see WebSocketServer() below) so a
    // deploy's `pm2 restart`/`stop` can't lose an alive player's progress from
    // the window since the last periodic persist_alive_progress() tick.
    void graceful_shutdown_flush() {
        Server::game.persist_alive_progress();
    }

    // Same bridge, for the admin minimap's live overlay: serializes every
    // alive player flower and mob's position (+ mob id/rarity) so the panel
    // can poll and draw live dots instead of the static zone bitmap alone.
    // Stashed into a JS global (Module.adminLiveJson) rather than returned
    // directly -- WASM exported functions can't hand back a std::string, and
    // the buffer needs to survive past this call's return for the HTTP
    // handler (running in the same JS turn) to read it.
    void admin_build_live_json() {
        std::ostringstream out;
        out << "{\"players\":[";
        bool first = true;
        Server::game.simulation.for_each<kCamera>([&](Simulation *sim, Entity &cam){
            if (!sim->ent_exists(cam.get_player())) return;
            Entity &player = sim->get_ent(cam.get_player());
            if (!player.has_component(kPhysics)) return;
            if (!first) out << ",";
            first = false;
            out << "{\"x\":" << player.get_x() << ",\"y\":" << player.get_y()
                << ",\"color\":" << (int) cam.get_color() << "}";
        });
        out << "],\"mobs\":[";
        first = true;
        Server::game.simulation.for_each<kMob>([&](Simulation *sim, Entity &ent){
            if (!first) out << ",";
            first = false;
            out << "{\"x\":" << ent.get_x() << ",\"y\":" << ent.get_y()
                << ",\"mob\":" << (int) ent.get_mob_id() << ",\"rarity\":" << (int) ent.get_mob_rarity() << "}";
        });
        out << "]}";
        std::string const s = out.str();
        EM_ASM({ Module.adminLiveJson = UTF8ToString($0, $1); }, s.c_str(), (int) s.size());
    }
}

// Serialized once at startup: MAP_DATA zone rectangles + colours, so the admin
// minimap can draw the same regions the game uses without duplicating this
// balance data by hand in admin.html (Shared stays the single source of truth).
static std::string build_admin_zones_json() {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < MAP_DATA.size(); ++i) {
        struct ZoneDefinition const &z = MAP_DATA[i];
        if (i) out << ",";
        out << "{\"left\":" << z.left << ",\"top\":" << z.top
            << ",\"right\":" << z.right << ",\"bottom\":" << z.bottom
            << ",\"color\":" << (z.color & 0xffffff)
            << ",\"name\":\"" << z.name << "\"}";
    }
    out << "]";
    return out.str();
}

extern "C" {
    void on_connect(int ws_id) {
        std::printf("client connect: [%d]\n", ws_id);
        WebSocket *ws = new WebSocket(ws_id);
        WS_MAP.insert({ws_id, ws});
    }

    void on_disconnect(int ws_id, int reason) {
        auto iter = WS_MAP.find(ws_id);
        if (iter == WS_MAP.end()) {
            std::printf("unknown ws disconnect: [%d]", ws_id);
            return;
        }
        std::printf("client disconnect: [%d]\n", ws_id);
        Client::on_disconnect(iter->second, reason, {});
        delete iter->second;
        WS_MAP.erase(ws_id);
    }

    void tick() {
        Server::tick();
    }

    void on_message(int ws_id, uint32_t len) {
        auto iter = WS_MAP.find(ws_id);
        //WebSocket *ws = WS_MAP[ws_id];
        if (iter == WS_MAP.end()) return;
        std::string_view message(reinterpret_cast<char const *>(INCOMING_BUFFER), len);
        Client::on_message(iter->second, message, 0);
    }
}

WebSocketServer::WebSocketServer() {
    std::string const zones_json = build_admin_zones_json();
    EM_ASM({
        const WSS = require("ws");
        const http = require("http");
        const fs = require("fs");
        // Static: MAP_DATA zone rectangles, serialized once at startup (Shared
        // stays the single source of truth -- admin.html never hand-copies
        // zone bounds/colours).
        const ADMIN_ZONES_JSON = UTF8ToString($3);
        const server = http.createServer(function(req, res) {
            // Admin panel API: POST JSON in, JSON out (credential re-checked in
            // global.adminApi, defined in Account/database.js).
            if (req.url === "/mgmt-2cead2784ab288121749639c/api" && req.method === "POST") {
                let body = "";
                req.on("data", function(c){ body += c; });
                req.on("end", function(){
                    let out = "{\"ok\":false,\"error\":\"unavailable\"}";
                    try {
                        const parsed = JSON.parse(body || "{}");
                        // Bracket + string on every parsed.* read below: this JS runs
                        // through Closure (--closure=1), which freely renames plain
                        // dot-notation properties on objects it doesn't recognize as
                        // externs. parsed comes from JSON.parse(networkBody), so its
                        // real keys ("mobId", "rarity", ...) never change -- but a
                        // dot-notation read like parsed.mobId got minified to
                        // parsed.Ga in one build, silently reading undefined (|0 -> 0)
                        // forever. Bracket-string access is never renamed.
                        // Spawning reaches into the live simulation, which the DB
                        // layer (global.adminApi) can't touch -- handled here,
                        // guarded by the same credential check.
                        if (parsed["action"] === "spawn" && globalThis["checkAdmin"]
                            && globalThis["checkAdmin"](parsed["user"], parsed["password"])) {
                            const x = +parsed["x"], y = +parsed["y"];
                            const mobId = parsed["mobId"] | 0, rarity = parsed["rarity"] | 0;
                            if (Number.isFinite(x) && Number.isFinite(y)) {
                                _admin_spawn_mob(x, y, mobId, rarity);
                                out = "{\"ok\":true}";
                            } else out = "{\"ok\":false,\"error\":\"bad coordinates\"}";
                        // Live player/mob positions for the minimap overlay --
                        // same simulation-reach requirement as spawn above.
                        } else if (parsed["action"] === "live" && globalThis["checkAdmin"]
                            && globalThis["checkAdmin"](parsed["user"], parsed["password"])) {
                            _admin_build_live_json();
                            out = "{\"ok\":true,\"live\":" + Module.adminLiveJson + "}";
                        // Bracket + string so the closure minifier can't rename the
                        // property (it must match global.adminApi in database.js).
                        } else if (globalThis["adminApi"]) out = globalThis["adminApi"](body);
                    } catch (e) { out = "{\"ok\":false,\"error\":\"server error\"}"; }
                    res.writeHead(200, {"Content-Type": "application/json", "Cache-Control": "no-store"});
                    res.end(out);
                });
                return;
            }
            if (req.url === "/mgmt-2cead2784ab288121749639c/zones.json" && req.method === "GET") {
                res.writeHead(200, {"Content-Type": "application/json", "Cache-Control": "no-store"});
                res.end(ADMIN_ZONES_JSON);
                return;
            }
            let encodeType = "text/html";
            let file = "index.html";
            switch (req.url) {
                case "/":
                    break;
                // Admin panel served only at a secret, unguessable path (not
                // "/admin") so it can't be found by probing common URLs. The
                // path lives only here in the server bundle and in admin.html
                // (itself only reachable via this path) -- never in the client
                // bundle or any player-facing content.
                case "/mgmt-2cead2784ab288121749639c":
                    file = "admin.html";
                    break;
                case "/floir-client.js":
                    encodeType = "application/javascript";
                    file = "floir-client.js";
                    break;
                case "/floir-client.wasm":
                    encodeType = "application/wasm";
                    file = "floir-client.wasm";
                    break;
                case "/grass_bg.svg":
                    encodeType = "image/svg+xml";
                    file = "grass_bg.svg";
                    break;
                case "/floir.webp":
                    encodeType = "image/webp";
                    file = "floir.webp";
                    break;
                case "/map-data.json":
                    encodeType = "application/json";
                    file = "map-data.json";
                    break;
                case "/map-overview.png":
                    encodeType = "image/png";
                    file = "map-overview.png";
                    break;
                default:
                    file = "";
                    break;
            }
            if (fs.existsSync(file)) {
                // Revalidate on every load (no-cache) with an ETag so browsers
                // get 304 when unchanged (no re-download) but always pick up a
                // fresh client after a deploy. Without this, heuristic caching
                // could serve a new floir-client.js against an old cached .wasm
                // -> mismatched memory layout -> corrupted in-game values.
                const stat = fs.statSync(file);
                const etag = '"' + stat.size.toString(16) + '-' + Math.round(stat.mtimeMs).toString(16) + '"';
                if (req.headers["if-none-match"] === etag) {
                    res.writeHead(304, {"ETag": etag, "Cache-Control": "no-cache"});
                    res.end();
                    return;
                }
                res.writeHead(200, {"Content-Type": encodeType, "Cache-Control": "no-cache", "ETag": etag});
                res.end(fs.readFileSync(file));
                return;
            }
            res.writeHead(404, {"Content-Type": encodeType});
            res.end();
        });

        server.listen($0, function() {
            console.log("Server running at http://localhost:"+$0);
        });
        
        // perMessage compression on the 20Hz binary state stream. Each client
        // gets ONE large kClientUpdate packet per tick (Game.cc), so this is one
        // deflate op per client per tick -- and Node `ws` runs zlib ASYNC on the
        // libuv threadpool (off the game-tick event loop / core), so the heavy
        // compression lands on otherwise-idle cores, not the single tick thread.
        // Cuts outbound bandwidth ~40-60% (the 60Mbps-cap saturation / lag fix).
        // Tuning for a single-core-bound, many-connection server:
        //   * level 3 -- fast, most of the ratio for far less CPU than level 6+.
        //   * server/clientNoContextTakeover -- no per-connection sliding window
        //     kept between messages: bounds memory across 100+ clients and lets
        //     each packet compress independently on any threadpool thread.
        //   * threshold 256 -- don't waste CPU compressing tiny packets.
        // ALL KEYS ARE QUOTED STRINGS: this runs through Closure (--closure=1),
        // which would rename plain-identifier option keys and silently disable
        // the whole config (same class of bug as the adminApi parsed[...] reads).
        const wss = new WSS.Server({
            "server": server,
            "perMessageDeflate": {
                "zlibDeflateOptions": { "level": 3, "memLevel": 7 },
                "serverNoContextTakeover": true,
                "clientNoContextTakeover": true,
                "threshold": 256,
                "concurrencyLimit": 20
            }
        });
        Module.ws_connections = {};
        let curr_id = 0;
        wss.on("connection", function(ws, req) {
            const ws_id = curr_id;
            Module.ws_connections[ws_id] = ws;
            _on_connect(ws_id);
            curr_id = (curr_id + 1) | 0;
            ws.on("message", function(message) {
                let data = new Uint8Array(message);
                const len = data.length > $2 ? $2 : data.length;
                data = data.subarray(0, len);
                HEAPU8.set(data, $1);
                _on_message(ws_id, len);
            });
            ws.on("close", function(reason) {
                _on_disconnect(ws_id, reason);
                delete Module.ws_connections[ws_id];
            });
        });

        // A deploy's `pm2 restart`/`stop` sends SIGTERM (SIGKILL after PM2's
        // kill-timeout if this hangs) -- without this handler the process died
        // immediately, and an alive player's progress was only as fresh as the
        // last periodic persist_alive_progress() tick (see Game.cc), losing up
        // to that window's worth of score/petals on every deploy. Flush
        // synchronously (writeFileSync under the hood) before exiting.
        if (!Module.shutdownHandlerInstalled) {
            Module.shutdownHandlerInstalled = true;
            const shutdown = function(signal){
                console.log("received " + signal + ", flushing progress before exit");
                // Bank alive players' progress into global.db (marks it dirty)...
                try { _graceful_shutdown_flush(); } catch (e) { console.error(e); }
                // ...then force the coalesced DB writer to flush to disk NOW
                // (saves are otherwise debounced ~3s; see database.js).
                try { if (globalThis["flushDatabaseNow"]) globalThis["flushDatabaseNow"](); } catch (e) { console.error(e); }
                process.exit(0);
            };
            process.on("SIGTERM", function(){ shutdown("SIGTERM"); });
            process.on("SIGINT", function(){ shutdown("SIGINT"); });
        }
    }, SERVER_PORT, INCOMING_BUFFER, MAX_BUFFER_LEN, zones_json.c_str());
}

void Server::run() {
    EM_ASM({
        setInterval(_tick, $0);
    }, 1000 / TPS);
}

void Client::send_packet(uint8_t const *packet, size_t size) {
    if (ws == nullptr) return;
    ws->send(packet, size);
}

WebSocket::WebSocket(int id) : ws_id(id) {
    client.ws = this;
}

void WebSocket::send(uint8_t const *packet, size_t size) {
    EM_ASM({
        if (!Module.ws_connections || !Module.ws_connections[$0]) return;
        const ws = Module.ws_connections[$0];
        // ws.send() can defer the actual write (e.g. backpressure, per-message
        // compression), so callers that write multiple packets back-to-back
        // into the same native OUTGOING_PACKET buffer before the socket
        // flushes need each send to own its bytes. HEAPU8.subarray() is only
        // a view into the shared heap and would alias later writes; slice()
        // copies out a standalone Uint8Array.
        ws.send(HEAPU8.slice($1,$1+$2));
    }, ws_id, packet, size);
}

void WebSocket::end(int code, std::string const &message) {
    EM_ASM({
        if (!Module.ws_connections || !Module.ws_connections[$0]) return;
        const ws = Module.ws_connections[$0];
        ws.close($1, UTF8ToString($2));
    }, ws_id, code, message.c_str());
}

Client *WebSocket::getUserData() {
    return &client;
}

WebSocketServer Server::server;
#endif
