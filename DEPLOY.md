# Deploy Instructions (for agents)

> ⚠️ This file contains production root passwords in plaintext, by the
> owner's request. Keep this repository **private** and rotate the password if
> it is ever exposed. Prefer SSH keys over password auth when possible.

## Server (PRODUCTION — this is what `floir.xyz` resolves to)
- **IP:** `38.76.196.218`
- **User:** `root`
- **Password:** `xsJi08TR`
- **Spec:** 32 vCPU / 32GB RAM / 60Mbps — headroom exists, but the WASM tick
  loop is single-threaded so vCPU count doesn't parallelize it; TPS=20 (50ms
  budget) is still the real ceiling (see perf incidents below).
- **App dir:** `/var/www/floir.xyz`
- **Process:** PM2 `floir.xyz` → `node /var/www/floir.xyz/floir-server.js` (cwd `/var/www/floir.xyz`)
- **Web:** nginx 443/80 → `localhost:3000`, TLS via certbot (`/etc/letsencrypt/live/floir.xyz/`,
  issued 2026-07-19, expires 2026-10-17, `certbot renew` handles renewal via
  its systemd timer). Client derives its WebSocket URL from `location.host`.
- **OS:** Debian, provisioned fresh 2026-07-19 (not the EOL bullseye image the
  old `.43` box runs — no repo-pinning workaround needed here).
- **Node 20** + **pm2 7.x**, `pm2 startup systemd` + `pm2 save` so the app
  survives a reboot. `package.json` only lists `ws` — `node_modules` was
  installed fresh here, not copied from `.43`.
- **`database.json` is the live account store.** DNS for `floir.xyz`/`www.floir.xyz`
  was cut over to this box on 2026-07-19, but see the **split-brain warning**
  below before treating this file as the sole source of truth.

## ⚠️ Open issue: split-brain database (since 2026-07-19 DNS cutover)
DNS was flipped to `.218` before SSL was issued here, so `https://floir.xyz`
was refused for a window (fixed same-day by running certbot — see History).
During that window, and likely still ongoing via cached DNS / open
connections, **both `.218` and the old `.43` box have been receiving real
player traffic and independently writing to their own `database.json`**
(confirmed both files had `mtime` == "now" when checked). They have
**diverged** — do not assume either is authoritative, and do not blindly
copy one over the other (the old stop+start migration recipe below would
silently destroy whichever side you overwrite). Reconciling them (e.g. by
account name, keeping whichever side has the higher level/most recent
activity per account) is unresolved and needs a careful pass, ideally during
a low-traffic window with both processes stopped. Until reconciled, treat
`.43` as still partially live — keep it updated on every deploy (see below).

## Server (OLD PRODUCTION — DNS no longer points here, but still gets stray live traffic — see split-brain warning above)
- **IP:** `38.76.196.43`
- **User:** `root`
- **Password:** `nwg8fBjc`
- **App dir:** `/var/www/floir.xyz`
- Same PM2/nginx/certbot layout as `.218` (still has its own valid cert for
  `floir.xyz`, expires 2026-10-16). Debian 11 (bullseye), **past upstream
  EOL** — `/etc/apt/sources.list` repointed to `archive.debian.org/debian
  bullseye main` only, `Acquire::Check-Valid-Until "false"` set in
  `/etc/apt/apt.conf.d/99no-check-valid-until`.
- Do **not** decommission or stop this box until the split-brain database is
  reconciled — it may still hold player progress not present on `.218`.

## Server (RETIRED — fully decommissioned from the app's perspective)
- **IP:** `38.76.198.54`
- **User:** `root`
- **Password:** `8gqIktKMAzpe`
- DNS for `floir.xyz`/`www.floir.xyz` was cut over to `38.76.196.43` on
  2026-07-19 (then to `38.76.196.218` later the same day). This box is no
  longer live-facing — its `database.json` is a stale snapshot as of the
  first cutover (do not treat it as authoritative; do not resync from it).
  Safe to tear down once confirmed nothing still depends on it, but no rush.

`ssh`/`scp` use `sshpass -e` with `SSHPASS` set to the matching password. The
remotes have **no `rsync`** — use `scp` only (`-C` to compress). **Deploy code
changes to BOTH `.218` and `.43`** until the split-brain is resolved and `.43`
is formally retired.

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
  # fresh snapshot, via the admin API search action). The admin panel lives at
  # a SECRET path and the credential is NOT stored here -- read ADMIN_PASS and
  # the path from Server/Account/database.js + Server/Wasm.cc, e.g.:
  curl -s -X POST http://localhost:3000/<SECRET_ADMIN_PATH>/api -H "Content-Type: application/json" \
    -d "{\"user\":\"admin\",\"password\":\"<ADMIN_PASS>\",\"action\":\"search\",\"query\":\"<known-recent-username>\"}"
'
```

## Incident (2026-07-19): DNS cut to `.218` before SSL was issued
DNS for `floir.xyz` was pointed at `38.76.196.218` while that box's nginx was
still HTTP-only (SSL setup from the earlier migration work was left pending
on DNS actually resolving there). Result: `https://floir.xyz` refused to
connect (`ERR_CONNECTION_REFUSED`) for anyone hitting `.218` over TLS, since
nothing was listening on 443. Fixed by running
`certbot --nginx -d floir.xyz -d www.floir.xyz --redirect` on `.218` (see
Server block above for the new cert's expiry). **If a future DNS cutover is
ever done again: issue the cert BEFORE or IMMEDIATELY AT the DNS switch, not
after** — there's no grace window once resolution changes. This is also where
the split-brain database issue (see warning above) originated — both boxes
kept taking real traffic across the gap.

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
Run from the local `Server/` directory. **Repeat for both `.218` (primary) and
`.43` (still partially live — see split-brain warning) until `.43` is
retired**:
```sh
export SSHPASS='xsJi08TR'          # .43 uses 'nwg8fBjc' instead
HOST=root@38.76.196.218            # or root@38.76.196.43
# scp is slow (~2MB can exceed a 3-min timeout); split big files / run in background if needed.
sshpass -e scp -C -o StrictHostKeyChecking=no \
  floir-server.js Account build floir-client.js floir-client.wasm index.html admin.html grass_bg.svg map-data.json map-overview.png \
  $HOST:/var/www/floir.xyz/
sshpass -e ssh -o StrictHostKeyChecking=no $HOST \
  'cd /var/www/floir.xyz && pm2 restart floir.xyz --update-env'
```
Note: `scp -C ... build ...` uploads the **directory** `build/` — pass `-r`
(`scp -C -r`) if your local `Server/build/` still has leftover CMake files
(`CMakeCache.txt`, `CMakeFiles/`, `Makefile`) alongside `floir-server.{js,wasm}`,
or the transfer fails/partial-copies. Clean those out of the *remote*
`build/` afterward if they get uploaded — the server only needs the `.js`/`.wasm` pair.

## Verify (always by md5, NOT timestamps — scp/rsync can skip on size+mtime)
Verify each server you deployed to (swap `$HOST`/`SSHPASS` per box, as above):
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
