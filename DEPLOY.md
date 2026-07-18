# Deploy Instructions (for agents)

> ⚠️ This file contains production root passwords in plaintext, by the
> owner's request. Keep this repository **private** and rotate the password if
> it is ever exposed. Prefer SSH keys over password auth when possible.

## Server (NEW — target for the DNS cutover, not yet live at floir.xyz)
- **IP:** `38.76.196.43`
- **User:** `root`
- **Password:** `nwg8fBjc`
- **App dir:** `/var/www/floir.xyz`
- **Process:** PM2 `floir.xyz` → `node /var/www/floir.xyz/floir-server.js` (cwd `/var/www/floir.xyz`)
- **Web:** nginx 80 → `localhost:3000` (HTTP only for now — see TLS section below;
  443/TLS is NOT configured yet because it needs the domain pointed here first).
- **OS:** Debian 11 (bullseye) — **already past upstream EOL**: the
  `security.debian.org` and `bullseye-backports` repos 404. `/etc/apt/sources.list`
  was repointed to `archive.debian.org/debian bullseye main` only (no security
  channel available without a paid Debian ELTS subscription — worth a Debian 12
  reinstall at some point). `Acquire::Check-Valid-Until "false"` is set in
  `/etc/apt/apt.conf.d/99no-check-valid-until` so archived (frozen) Release
  files don't get rejected as stale.
- **Node 20** installed via NodeSource (`deb.nodesource.com/setup_20.x`) — the
  distro repo only has an ancient Node 12. **pm2 7.x** via `npm install -g pm2`,
  with `pm2 startup systemd` + `pm2 save` so the app survives a reboot.
- **`database.json` was migrated** from the old server (see History below) —
  this box owns the live accounts once DNS is cut over. It is **not** synced
  automatically from the old server after that point.
- **TLS not yet issued.** `floir.xyz` DNS still points at the OLD server, so a
  certbot HTTP-01 challenge run here would fail. Once DNS (the domain's A
  record) is switched to `38.76.196.43` and has propagated, run:
  ```sh
  sshpass -e ssh -o StrictHostKeyChecking=no root@38.76.196.43 \
    'certbot --nginx -d floir.xyz -d www.floir.xyz --non-interactive --agree-tos -m <owner-email>'
  ```
  This both issues the cert and rewrites the nginx site config to the
  redirect-to-https + 443 pattern (mirror the OLD server's config below).
  `python3-certbot-nginx` is already installed on this box.

## Server (OLD — current production, still live at floir.xyz until DNS is cut over)
- **IP:** `38.76.198.54`
- **User:** `root`
- **Password:** `8gqIktKMAzpe`
- **App dir:** `/var/www/floir.xyz`
- **Process:** PM2 `floir.xyz` → `node /var/www/floir.xyz/floir-server.js` (cwd `/var/www/floir.xyz`)
- **Web:** nginx 443/80 → `localhost:3000`. Client derives its WebSocket URL from `location.host`.
- Once the new server is confirmed serving `floir.xyz` correctly (DNS
  propagated + TLS issued + a final `database.json` resync — see below), this
  server can be decommissioned. Don't tear it down before that's confirmed.

`ssh`/`scp` use `sshpass -e` with `SSHPASS` set to the matching password. The
remotes have **no `rsync`** — use `scp` only (`-C` to compress).

## Migration history (2026-07-19): old server -> new server
`database.json` was copied old -> local -> new via `scp` (no direct
server-to-server transfer). Because the old server keeps taking live writes
until DNS actually cuts over, **re-run this resync immediately before flipping
DNS** to minimize the staleness window:
```sh
export SCRATCH=/tmp/floir-db-migrate   # or any local scratch dir
mkdir -p "$SCRATCH"
sshpass -e -p '8gqIktKMAzpe' scp -C -o StrictHostKeyChecking=no \
  root@38.76.198.54:/var/www/floir.xyz/database.json "$SCRATCH/database.json"
sshpass -e -p 'nwg8fBjc' scp -C -o StrictHostKeyChecking=no \
  "$SCRATCH/database.json" root@38.76.196.43:/var/www/floir.xyz/database.json
```
**Gotcha found during migration: `pm2 restart` did not reliably reload
`database.json` into the process's in-memory `global.db`** (observed once —
after uploading a newer file and running `pm2 restart`, the running process
kept serving/re-saving the OLDER in-memory snapshot, silently clobbering the
newer file on next write). Root cause wasn't fully isolated (possibly the WASM
tick loop delaying clean shutdown before pm2's kill timeout). **Whenever
`database.json` changes as part of a deploy, prefer a full stop+start over
`restart`, and verify with more than md5 of the file** (md5 only proves the
*disk* file is right — verify the *loaded* state too):
```sh
sshpass -e -p 'nwg8fBjc' ssh -o StrictHostKeyChecking=no root@38.76.196.43 '
  cd /var/www/floir.xyz
  pm2 delete floir.xyz
  ss -ltnp | grep 3000 || echo "port clear"   # confirm nothing still bound before restarting
  pm2 start floir-server.js --name floir.xyz
  pm2 save
  sleep 3
  md5sum database.json   # should be UNCHANGED from what you just uploaded
  # content-level check (a plain md5 of the file does not prove the RUNNING
  # process loaded it -- look up a username you know exists only in the
  # fresh snapshot, via the admin API search action, e.g.:
  curl -s -X POST http://localhost:3000/admin/api -H "Content-Type: application/json" \
    -d "{\"user\":\"admin\",\"password\":\"loveKK88\",\"action\":\"search\",\"query\":\"<known-recent-username>\"}"
'
```

## Build (local)
```sh
source /Users/eason/emsdk/emsdk_env.sh
python3 Scripts/gen_map.py                 # regen map + Shared/Tilemap.hh (only if map/tiles changed)
cd Client/build && make                     # -> Client/build/floir-client.{js,wasm}
cd ../../Server/build && make               # -> Server/build/floir-server.{js,wasm}
# stage fresh client into the Server/ bundle:
cp Client/build/floir-client.js Client/build/floir-client.wasm Server/
```

## Prod layout — DON'T get this wrong
- `/var/www/floir.xyz/floir-server.js` must be the **wrapper** (identical to
  `Server/run-server.js`): it `require`s `./Account/database.js` (sets DB
  globals) then `./build/floir-server.js`. Running the raw emscripten output
  directly crashes — no DB globals, never opens the socket, nginx returns 502.
- `build/floir-server.js` + `build/floir-server.wasm` are a **matched pair**
  from the same `emcc` run. A stale js with a fresh wasm (or vice-versa) throws
  `TypeError: kb[a] is not a function`. Always upload BOTH and verify by md5.
- **Never upload `database.json`** — it holds live accounts. The upload set below
  excludes it, so it is preserved.
- `admin.html` calls back into the live simulation for mob-spawning (not just
  the JS/DB layer) via an exported WASM function (`admin_spawn_mob`, see
  `Server/Wasm.cc` + the `-sEXPORTED_FUNCTIONS` list in `Server/CMakeLists.txt`)
  and a `/admin/zones.json` route serialized from `Shared/StaticData.hh`
  `MAP_DATA` at server startup. Both ship inside `build/floir-server.js` — no
  separate file to remember, but if the admin minimap/spawn UI ever silently
  stops working after a deploy, check that this export wasn't dropped by a
  CMake edit.

## Upload + restart
Run from the local `Server/` directory. Target **both** servers while the old
one is still live (until DNS cutover + a final `database.json` resync — see
Migration history above — after which only the new server needs this):
```sh
export SSHPASS='8gqIktKMAzpe'   # old: 38.76.198.54 | new: nwg8fBjc @ 38.76.196.43
HOST=root@38.76.198.54
# scp is slow (~2MB can exceed a 3-min timeout); split big files / run in background if needed.
sshpass -e scp -C -o StrictHostKeyChecking=no \
  floir-server.js Account build floir-client.js floir-client.wasm index.html admin.html grass_bg.svg map-data.json \
  $HOST:/var/www/floir.xyz/
sshpass -e ssh -o StrictHostKeyChecking=no $HOST \
  'cd /var/www/floir.xyz && pm2 restart floir.xyz --update-env'
```
(`main-map.svg` from earlier deploys was renamed/replaced by `map-data.json` —
upload whichever your checkout actually has in `Server/`.)

## Verify (always by md5, NOT timestamps — scp/rsync can skip on size+mtime)
```sh
sshpass -e ssh -o StrictHostKeyChecking=no $HOST '
  cd /var/www/floir.xyz
  md5sum floir-client.wasm map-data.json build/floir-server.js build/floir-server.wasm
  pm2 list | grep floir
  curl -sS -o /dev/null -w "localhost:3000 -> %{http_code}\n" http://localhost:3000/
  curl -skS -o /dev/null -w "https://floir.xyz -> %{http_code}\n" https://floir.xyz/   # old server only until TLS is issued on the new one
  ss -ltnp | grep 3000   # node should be listening
  pm2 logs floir.xyz --lines 20 --nostream | grep "Server running"
'
```
Compare the remote md5s against local (`md5 -q <file>` on macOS). Expect both
curls → `200` and a `Server running at http://localhost:3000` log line. On the
new server (no TLS yet), check `http://38.76.196.43/` instead of the
`https://floir.xyz` line.

## New-server cutover checklist (once ready to make 38.76.196.43 production)
1. Do a final `database.json` resync (Migration history above) — do this
   **last**, immediately before the DNS change, to minimize the staleness gap.
2. Update the domain's DNS A record (`floir.xyz`, `www.floir.xyz`) to
   `38.76.196.43`. **This requires registrar/DNS-provider access the deploying
   agent does not have** — the account owner must do this step (or hand over
   DNS credentials).
3. Wait for DNS propagation (`dig floir.xyz` should return the new IP).
4. Run the certbot command from the NEW server section above to issue TLS and
   rewrite nginx to the redirect+443 pattern.
5. Verify `https://floir.xyz` serves the new server (200, WebSocket connects,
   a game session can log in and spawn) and `pm2 logs` show no errors.
6. Only after that's confirmed stable, decommission the OLD server
   (`38.76.198.54`) — don't tear it down before this point in case of rollback.
