#ifdef WASM_SERVER
#include <Server/Client.hh>
#include <Server/Server.hh>

#include <Shared/Config.hh>

#include <iostream>
#include <string>
#include <unordered_map>

#include <emscripten.h>

std::unordered_map<int, WebSocket *> WS_MAP;

size_t const MAX_BUFFER_LEN = 1024;
static uint8_t INCOMING_BUFFER[MAX_BUFFER_LEN] = {0};

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
    EM_ASM({
        const WSS = require("ws");
        const http = require("http");
        const fs = require("fs");
        const server = http.createServer(function(req, res) {
            // Admin panel API: POST JSON in, JSON out (credential re-checked in
            // global.adminApi, defined in Account/database.js).
            if (req.url === "/admin/api" && req.method === "POST") {
                let body = "";
                req.on("data", function(c){ body += c; });
                req.on("end", function(){
                    let out = "{\"ok\":false,\"error\":\"unavailable\"}";
                    // Bracket + string so the closure minifier can't rename the
                    // property (it must match global.adminApi in database.js).
                    try { if (globalThis["adminApi"]) out = globalThis["adminApi"](body); } catch (e) { out = "{\"ok\":false,\"error\":\"server error\"}"; }
                    res.writeHead(200, {"Content-Type": "application/json", "Cache-Control": "no-store"});
                    res.end(out);
                });
                return;
            }
            let encodeType = "text/html";
            let file = "index.html";
            switch (req.url) {
                case "/":
                    break;
                case "/admin":
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
        
        const wss = new WSS.Server({ "server": server });
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
        })
    }, SERVER_PORT, INCOMING_BUFFER, MAX_BUFFER_LEN);
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
