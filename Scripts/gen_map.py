#!/usr/bin/env python3
"""Composite the Tiled map (main.tmj) + tiles/*.svg into one static SVG the
client blits as the world backdrop (Server/main-map.svg).

GID -> SVG mapping is maintained by hand here since we have no tileset.tsj.
Re-run after editing GID_TO_SVG or the tiles. Honours Tiled flip bits
(horizontal/vertical/diagonal) so rotated autotile pieces face correctly.
"""
import json, base64, gzip, re, os, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TILES = os.path.join(ROOT, 'tiles')
TMJ = os.path.join(ROOT, 'main.tmj')
OUTS = [os.path.join(ROOT, 'docs/main-map-render.svg'),
        os.path.join(ROOT, 'Server/main-map.svg'),
        os.path.expanduser('~/Desktop/floir-map-render.svg')]

GID_TO_SVG = {
    # bg / desert SW
    1: 'desert_c_0.svg', 2: 'desert_c_1.svg', 3: 'desert_c_2.svg',
    4: 'desert_c_3.svg', 5: 'desert_c_4.svg',
    # bg / grass north
    6: 'grass_c_0.svg', 7: 'grass_c_1.svg', 8: 'grass_c_2.svg', 9: 'grass_c_0.svg',
    # bg / grass2 SE jungle floor
    66: 'grass2_c_0.svg', 67: 'grass2_c_1.svg', 68: 'grass2_c_2.svg', 69: 'grass2_c_3.svg',
    # dirt
    22: 'dirt_l_0.svg', 23: 'dirt_tl_0.svg', 24: 'dirt_tri_0.svg', 25: 'dirt_c_0.svg',
    # water
    33: 'water_l_0.svg', 34: 'water_tl_0.svg', 35: 'water_tri_0.svg', 36: 'water_c_0.svg',
    # castle
    18: 'castle_l_0.svg', 19: 'castle_tl_0.svg', 20: 'castle_c_0.svg', 21: 'castle_tri_0.svg',
    # bush
    104: 'bush_c_0.svg', 105: 'bush_t_0.svg', 106: 'bush_tl_0.svg', 107: 'bush_tri_0.svg',
    # cliff
    112: 'scliff_c_0.svg', 113: 'scliff_l_0.svg', 114: 'scliff_tl_0.svg', 115: 'scliff_tli_0.svg',
    # transitions (biome-boundary edges)
    46: 'desert_r_0.svg', 70: 'desert_t_0.svg', 87: 'grass2_t_0.svg',
    48: 'grass2_b_0.svg',
    # 47 still unmapped -> layer fallback
}

# Draw order must match the .tmj (bottom -> top): transitions sit just above bg,
# below water/bush/cliff/dirt/castle. Drawing them last overlaid foliage/dirt.
LAYER_ORDER = ['bg', 'transitions', 'water', 'bush', 'cliff', 'dirt', 'castle']  # holes/warps skipped
LAYER_FALLBACK = {
    'bg': 'grass_c_0.svg', 'dirt': 'dirt_c_0.svg', 'water': 'water_c_0.svg',
    'bush': 'bush_c_0.svg', 'cliff': 'scliff_c_0.svg', 'castle': 'castle_c_0.svg',
    'transitions': 'grass2_t_0.svg',
}
# 64px/cell gives the composite a 3200x3264 native raster (vs 1600) so tiles
# stay crisp when the client upscales the backdrop across the 25k world.
CELL = 64
FLIP_H, FLIP_V, FLIP_D = 0x80000000, 0x40000000, 0x20000000


def load_symbol(name):
    p = os.path.join(TILES, name)
    if not os.path.exists(p):
        return None
    txt = re.sub(r'<\?xml[^>]*\?>\s*', '', open(p).read())
    m = re.search(r'<svg[^>]*viewBox="([^"]+)"[^>]*>(.*)</svg>', txt, re.DOTALL)
    return None if not m else (
        f'<symbol id="t_{name.replace(".","_")}" viewBox="{m.group(1)}" '
        f'preserveAspectRatio="none">{m.group(2)}</symbol>')


def flip_matrix(h, v, dg):
    a, b, c, d = 1, 0, 0, 1
    if dg:
        a, b, c, d = 0, 1, 1, 0   # transpose
    if h:
        a, c = -a, -c
    if v:
        b, d = -b, -d
    return a, b, c, d


def main():
    d = json.load(open(TMJ))
    W, H = d['width'], d['height']

    def decode(l):
        raw = base64.b64decode(l['data'])
        if l.get('compression') == 'gzip':
            raw = gzip.decompress(raw)
        return [int.from_bytes(raw[i:i+4], 'little') for i in range(0, len(raw), 4)]

    layers = {l['name']: decode(l) for l in d['layers'] if l.get('type') == 'tilelayer'}
    symbols = {f: load_symbol(f) for f in os.listdir(TILES) if f.endswith('.svg')}
    symbols = {k: v for k, v in symbols.items() if v}

    w_px, h_px = W * CELL, H * CELL
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w_px} {h_px}" '
           f'width="{w_px}" height="{h_px}">', '<defs>']
    out += list(symbols.values())
    out += ['</defs>', '<rect width="100%" height="100%" fill="#3d2a1e"/>']

    from collections import Counter
    unknown = {n: Counter() for n in LAYER_ORDER}
    placements = 0
    for name in LAYER_ORDER:
        if name not in layers:
            continue
        for r in range(H):
            for c in range(W):
                g = layers[name][r * W + c]
                if not g:
                    continue
                gid = g & 0x1FFFFFFF
                svg = GID_TO_SVG.get(gid)
                if svg is None:
                    unknown[name][gid] += 1
                    svg = LAYER_FALLBACK[name]
                if svg not in symbols:
                    continue
                sid = f't_{svg.replace(".","_")}'
                x, y = c * CELL, r * CELL
                h_, v_, dg = bool(g & FLIP_H), bool(g & FLIP_V), bool(g & FLIP_D)
                if not (h_ or v_ or dg):
                    out.append(f'<use href="#{sid}" x="{x}" y="{y}" width="{CELL}" height="{CELL}"/>')
                else:
                    a, b, cc, dd = flip_matrix(h_, v_, dg)
                    cx, cy = x + CELL / 2, y + CELL / 2
                    out.append(f'<g transform="translate({cx} {cy}) matrix({a} {b} {cc} {dd} 0 0) '
                               f'translate({-CELL/2} {-CELL/2})"><use href="#{sid}" '
                               f'width="{CELL}" height="{CELL}"/></g>')
                placements += 1
    out.append('</svg>')
    svg = '\n'.join(out)
    for p in OUTS:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'w').write(svg)
    print(f'placements={placements}, mapped GIDs={len(GID_TO_SVG)}')
    for n, c in unknown.items():
        if c:
            print(f'  still-unknown {n}: {dict(sorted(c.items()))}')

    write_tilemap_header(layers, W, H)


# --- Collision/minimap terrain grid ---------------------------------------
# Only the SOLID (center) tile of each blocking biome actually blocks, so the
# collision boundary follows the solid colour mass instead of the full grid
# cell + invisible autotile corners. Edge/fringe tiles stay walkable.
SOLID_BLOCK_GID = {'water': 36, 'bush': 104, 'cliff': 112, 'dirt': 25, 'castle': 20}
TERRAIN_ENUM = ['kGrass', 'kDirt', 'kBush', 'kWater', 'kJungle',
                'kCliff', 'kCastle', 'kStone']
# id used per blocking layer when its solid tile is present
LAYER_BLOCK_ID = {'water': 3, 'bush': 4, 'cliff': 5, 'dirt': 1, 'castle': 6}


def write_tilemap_header(layers, W, H):
    def masked(name, i):
        g = layers.get(name, [0] * (W * H))[i]
        return g & 0x1FFFFFFF

    terrain = []
    for i in range(W * H):
        t = 0  # grass / walkable default
        # blocking layers, high-priority first; only solid-center tiles block
        for name in ('castle', 'dirt', 'cliff', 'bush', 'water'):
            if masked(name, i) == SOLID_BLOCK_GID[name]:
                t = LAYER_BLOCK_ID[name]
                break
        else:
            if not masked('bg', i):
                t = 15  # void (outside arena)
        terrain.append(t)

    lines = [
        '#pragma once', '', '#include <cstdint>', '',
        '// Auto-generated by Scripts/gen_map.py. 50x51 terrain grid used by the',
        '// client minimap and the server collision. Only SOLID (center) tiles of a',
        '// blocking biome are marked blocked, so collision follows the visible',
        '// colour mass, not the full grid cell.', '',
        'namespace Tilemap {',
        f'    inline constexpr uint32_t GRID_W = {W};',
        f'    inline constexpr uint32_t GRID_H = {H};',
        '    inline constexpr float    CELL_SIZE = 500;', '',
        '    namespace TerrainID {',
        '        enum : uint8_t {',
        '            kGrass=0, kDirt=1, kBush=2, kWater=3, kJungle=4,',
        '            kCliff=5, kCastle=6, kStone=7, kVoid=15,',
        '        };', '    }', '',
        '    inline constexpr uint8_t TERRAIN[GRID_W * GRID_H] = {',
    ]
    for r in range(H):
        lines.append('        ' + ','.join(str(v) for v in terrain[r * W:(r + 1) * W]) + ',')
    lines += [
        '    };', '',
        '    inline uint8_t terrain_at(float x, float y) {',
        '        if (x < 0 || y < 0) return TerrainID::kVoid;',
        '        uint32_t c = (uint32_t)(x / CELL_SIZE);',
        '        uint32_t r = (uint32_t)(y / CELL_SIZE);',
        '        if (c >= GRID_W || r >= GRID_H) return TerrainID::kVoid;',
        '        return TERRAIN[r * GRID_W + c];',
        '    }', '',
        '    // Blocked: dirt, water, bush foliage, cliff, castle (solid tiles only).',
        '    inline bool blocks_movement(uint8_t t) {',
        '        switch (t) {',
        '            case TerrainID::kDirt:',
        '            case TerrainID::kWater:',
        '            case TerrainID::kJungle:',
        '            case TerrainID::kBush:',
        '            case TerrainID::kCliff:',
        '            case TerrainID::kCastle:',
        '                return true;',
        '            default:',
        '                return false;',
        '        }',
        '    }',
        '}', '',
    ]
    out = os.path.join(ROOT, 'Shared/Tilemap.hh')
    open(out, 'w').write('\n'.join(lines))
    from collections import Counter
    print('Tilemap.hh terrain dist:', dict(Counter(terrain)))


if __name__ == '__main__':
    main()
