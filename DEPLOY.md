# Deploy Instructions (for agents)

> ⚠️ This file contains the production root password in plaintext, by the
> owner's request. Keep this repository **private** and rotate the password if
> it is ever exposed. Prefer SSH keys over password auth when possible.

## Server
- **IP:** `38.76.198.54`
- **User:** `root`
- **Password:** `8gqIktKMAzpe`
- **App dir:** `/var/www/floir.xyz`
- **Process:** PM2 `floir.xyz` → `node /var/www/floir.xyz/floir-server.js` (cwd `/var/www/floir.xyz`)
- **Web:** nginx 443/80 → `localhost:3000`. Client derives its WebSocket URL from `location.host`.

`ssh`/`scp` use `sshpass -e` with `SSHPASS` set to the password. The remote has
**no `rsync`** — use `scp` only (`-C` to compress).

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

## Upload + restart
Run from the local `Server/` directory:
```sh
export SSHPASS='8gqIktKMAzpe'
HOST=root@38.76.198.54
# scp is slow (~2MB can exceed a 3-min timeout); split big files / run in background if needed.
sshpass -e scp -C -o StrictHostKeyChecking=no \
  floir-server.js Account build floir-client.js floir-client.wasm index.html main-map.svg grass_bg.svg \
  $HOST:/var/www/floir.xyz/
sshpass -e ssh -o StrictHostKeyChecking=no $HOST \
  'cd /var/www/floir.xyz && pm2 restart floir.xyz --update-env'
```

## Verify (always by md5, NOT timestamps — scp/rsync can skip on size+mtime)
```sh
sshpass -e ssh -o StrictHostKeyChecking=no $HOST '
  cd /var/www/floir.xyz
  md5sum floir-client.wasm main-map.svg build/floir-server.js build/floir-server.wasm
  pm2 list | grep floir
  curl -sS -o /dev/null -w "localhost:3000 -> %{http_code}\n" http://localhost:3000/
  curl -skS -o /dev/null -w "https://floir.xyz -> %{http_code}\n" https://floir.xyz/
  ss -ltnp | grep 3000   # node should be listening
  pm2 logs floir.xyz --lines 20 --nostream | grep "Server running"
'
```
Compare the remote md5s against local (`md5 -q <file>` on macOS). Expect both
curls → `200` and a `Server running at http://localhost:3000` log line.
