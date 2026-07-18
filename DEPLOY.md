# Deploy Instructions (for agents)

> ⚠️ This file contains production root passwords in plaintext, by the
> owner's request. Keep this repository **private** and rotate the password if
> it is ever exposed. Prefer SSH keys over password auth when possible.

## Server (PRODUCTION — this is what `floir.xyz` resolves to)
- **IP:** `38.76.196.43`
- **User:** `root`
- **Password:** `nwg8fBjc`
- **App dir:** `/var/www/floir.xyz`
- **Process:** PM2 `floir.xyz` → `node /var/www/floir.xyz/floir-server.js` (cwd `/var/www/floir.xyz`)
- **Web:** nginx 443/80 → `localhost:3000`, TLS via certbot (`/etc/letsencrypt/live/floir.xyz/`,
  expires 2026-10-16, `certbot renew` handles renewal via its systemd timer).
  Client derives its WebSocket URL from `location.host`.
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
- **`database.json` is the live account store** — migrated here from the old
  server on 2026-07-19 (see History below). It is the sole source of truth for
  accounts now; nothing syncs from the old server anymore.

## Server (RETIRED — was production, DNS no longer points here)
- **IP:** `38.76.198.54`
- **User:** `root`
- **Password:** `8gqIktKMAzpe`
- DNS for `floir.xyz`/`www.floir.xyz` was cut over to `38.76.196.43` on
  2026-07-19. This box is no longer live-facing but has **not been
  decommissioned** — its `database.json` is a stale snapshot as of the
  cutover (do not treat it as authoritative; do not resync from it). Safe to
  tear down once you've confirmed nothing still depends on it, but no rush.

`ssh`/`scp` use `sshpass -e` with `SSHPASS` set to the matching password. The
remotes have **no `rsync`** — use `scp` only (`-C` to compress).

## Migration history (2026-07-19): old server → new server
`database.json` was copied old → local → new via `scp` (no direct
server-to-server transfer), then DNS was switched and TLS issued via certbot
(`--nginx -d floir.xyz -d www.floir.xyz --redirect`). If a similar migration
is ever needed again:
```sh
export SCRATCH=/tmp/floir-db-migrate
mkdir -p "$SCRATCH"
sshpass -e -p '<old-password>' scp -C -o StrictHostKeyChecking=no \
  root@<old-ip>:/var/www/floir.xyz/database.json "$SCRATCH/database.json"
sshpass -e -p '<new-password>' scp -C -o StrictHostKeyChecking=no \
  "$SCRATCH/database.json" root@<new-ip>:/var/www/floir.xyz/database.json
```
**Gotcha found during this migration: `pm2 restart` did not reliably reload
`database.json` into the process's in-memory `global.db`** (observed once —
after uploading a newer file and running `pm2 restart`, the running process
kept serving/re-saving the OLDER in-memory snapshot, silently clobbering the
newer file on next write). Root cause wasn't fully isolated (possibly the WASM
tick loop delaying clean shutdown before pm2's kill timeout). **Whenever
`database.json` changes as part of a deploy, prefer a full stop+start over
`restart`, and verify with more than md5 of the file** (md5 only proves the
*disk* file is right — verify the *loaded* state too):
```sh
sshpass -e ssh -o StrictHostKeyChecking=no $HOST '
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

## Perf incident (2026-07-19): unbounded `solid_circle` scan
Tick time regressed to 100-130ms (well over the 50ms/20TPS budget). Profiled
by temporarily timing each system in `Simulation::on_tick()` — `tick_entity_motion`
(Server/Process/Motion.cc) was ~80-100ms of that on its own. Root cause:
`Tilemap::solid_circle` (the large-mob terrain-tunnel fallback check added
earlier) had no adaptive stride, unlike `push_circle` — cost was
`O((2*rad/COLL_UNIT)^2)` uncapped, so a large high-rarity mob (radius can run
into the hundreds of units) could burn hundreds of thousands of sub-cell
checks per call, twice per terrain-colliding entity per tick. Fixed by giving
`solid_circle` the same `SAMPLES=24` adaptive stride `push_circle` already
had — brought tick time down to ~45-55ms. **If tick time regresses again,
profile first** (temporarily add timers around each `Simulation::on_tick()`
system call, deploy just that, read `pm2 logs`, then revert) rather than
guessing — this is the second time collision cost has been the culprit (see
git history / commit messages for "collision-perf").

## Build (local)
```sh
source /Users/eason/emsdk/emsdk_env.sh
python3 Scripts/gen_map.py                 # regen map + Tilemap.hh + map-overview.png (only if map/tiles changed)
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
- `admin.html`'s spawn minimap draws `/map-overview.png` (a static file,
  generated by `Scripts/gen_map.py` from the same collision-mask source the
  in-game minimap samples) — regenerate it (`python3 Scripts/gen_map.py`) and
  upload it whenever the map itself changes; it won't auto-update otherwise.

## Upload + restart
Run from the local `Server/` directory:
```sh
export SSHPASS='nwg8fBjc'
HOST=root@38.76.196.43
# scp is slow (~2MB can exceed a 3-min timeout); split big files / run in background if needed.
sshpass -e scp -C -o StrictHostKeyChecking=no \
  floir-server.js Account build floir-client.js floir-client.wasm index.html admin.html grass_bg.svg map-data.json map-overview.png \
  $HOST:/var/www/floir.xyz/
sshpass -e ssh -o StrictHostKeyChecking=no $HOST \
  'cd /var/www/floir.xyz && pm2 restart floir.xyz --update-env'
```

## Verify (always by md5, NOT timestamps — scp/rsync can skip on size+mtime)
```sh
sshpass -e ssh -o StrictHostKeyChecking=no $HOST '
  cd /var/www/floir.xyz
  md5sum floir-client.wasm map-data.json build/floir-server.js build/floir-server.wasm
  pm2 list | grep floir
  curl -sS -o /dev/null -w "localhost:3000 -> %{http_code}\n" http://localhost:3000/
  curl -skS -o /dev/null -w "https://floir.xyz -> %{http_code}\n" https://floir.xyz/
  ss -ltnp | grep 3000   # node should be listening
  pm2 logs floir.xyz --lines 20 --nostream | grep "Server running"
'
```
Compare the remote md5s against local (`md5 -q <file>` on macOS). Expect both
curls → `200` and a `Server running at http://localhost:3000` log line.
