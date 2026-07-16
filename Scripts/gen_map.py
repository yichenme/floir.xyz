#!/usr/bin/env python3
"""Composite the Tiled map (main.tmj) + tiles/*.svg into one static SVG the
client blits as the world backdrop (Server/main-map.svg).

GID -> SVG mapping is maintained by hand here since we have no tileset.tsj.
Re-run after editing GID_TO_SVG or the tiles. Honours Tiled flip bits
(horizontal/vertical/diagonal) so rotated autotile pieces face correctly.
"""
import json, base64, gzip, re, os, shutil, math

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
    48: 'desert_b_0.svg', 47: 'desert_b_0.svg',
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
# Per-tile outward bleed (native SVG px) so neighbouring tiles overlap and no
# anti-aliased seam ("white gap line") shows between cells after the client
# upscales the backdrop.
BLEED = 0.75
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
                # Bleed the base ground AND full-square centre tiles (name *_c_*)
                # outward by BLEED px so their shared edges overlap and leave no
                # hairline seam ("white gap lines"). Centre tiles' shadows are
                # interior, so overlapping them doesn't double any edge shadow.
                # Shaped EDGE tiles (_l/_t/_tri/…) stay exact size: bleeding them
                # doubled their edge shadows and pushed the visible edge past the
                # collision polygon (players stood on the overhang).
                bleed = BLEED if (name == 'bg' or '_c_' in svg) else 0.0
                sz = CELL + 2 * bleed
                h_, v_, dg = bool(g & FLIP_H), bool(g & FLIP_V), bool(g & FLIP_D)
                if not (h_ or v_ or dg):
                    out.append(f'<use href="#{sid}" x="{x-bleed}" y="{y-bleed}" width="{sz}" height="{sz}"/>')
                else:
                    a, b, cc, dd = flip_matrix(h_, v_, dg)
                    cx, cy = x + CELL / 2, y + CELL / 2
                    out.append(f'<g transform="translate({cx} {cy}) matrix({a} {b} {cc} {dd} 0 0) '
                               f'translate({-sz/2} {-sz/2})"><use href="#{sid}" '
                               f'width="{sz}" height="{sz}"/></g>')
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
# Coarse per-cell terrain (minimap colours) plus a fine sub-cell collision mask
# rasterized from each blocking tile's actual fill shape, so the body collides
# with the visible asset outline rather than the whole grid cell.
LAYER_BLOCK_ID = {'water': 3, 'bush': 4, 'cliff': 5, 'dirt': 1, 'castle': 6}
BLOCK_LAYERS = ('castle', 'dirt', 'cliff', 'bush', 'water')
MASK_RES = 50   # per-cell collision-mask resolution -> 10 world units/pixel
FLIP_H_B, FLIP_V_B, FLIP_D_B = 0x80000000, 0x40000000, 0x20000000


def _arc_points(x1, y1, rx, ry, phi_deg, large, sweep, x2, y2):
    # SVG elliptical arc -> sampled points (endpoint->center parameterization).
    if rx == 0 or ry == 0 or (x1 == x2 and y1 == y2):
        return [(x2, y2)]
    rx, ry = abs(rx), abs(ry)
    phi = math.radians(phi_deg)
    cosp, sinp = math.cos(phi), math.sin(phi)
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    x1p = cosp * dx + sinp * dy
    y1p = -sinp * dx + cosp * dy
    lam = x1p**2 / rx**2 + y1p**2 / ry**2
    if lam > 1:
        s = math.sqrt(lam); rx *= s; ry *= s
    num = rx**2 * ry**2 - rx**2 * y1p**2 - ry**2 * x1p**2
    den = rx**2 * y1p**2 + ry**2 * x1p**2
    co = math.sqrt(max(0.0, num / den)) if den else 0.0
    if large == sweep:
        co = -co
    cxp = co * rx * y1p / ry
    cyp = -co * ry * x1p / rx
    cx = cosp * cxp - sinp * cyp + (x1 + x2) / 2
    cy = sinp * cxp + cosp * cyp + (y1 + y2) / 2

    def ang(ux, uy, vx, vy):
        d = math.hypot(ux, uy) * math.hypot(vx, vy)
        c = max(-1.0, min(1.0, (ux * vx + uy * vy) / d)) if d else 1.0
        a = math.acos(c)
        return -a if (ux * vy - uy * vx) < 0 else a

    t1 = ang(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    out = []
    for s in range(1, 9):
        th = t1 + dt * s / 8.0
        px = cosp * rx * math.cos(th) - sinp * ry * math.sin(th) + cx
        py = sinp * rx * math.cos(th) + cosp * ry * math.sin(th) + cy
        out.append((px, py))
    return out


def _path_subpaths(d):
    # Char-scanning SVG path parser (handles packed arc flags). Curves & arcs
    # are sampled into polygon outlines. Returns a list of subpaths (each `M`
    # starts a new one) so composite fills aren't stitched into a single
    # self-intersecting ring.
    n = len(d)
    i = 0
    pts = []
    subpaths = []
    cx = cy = sx = sy = 0.0
    cmd = None
    prev_c2 = None

    def skip_sep():
        nonlocal i
        while i < n and d[i] in ' ,\t\r\n':
            i += 1

    def num():
        nonlocal i
        skip_sep()
        j = i
        if j < n and d[j] in '+-':
            j += 1
        seen_dot = False
        while j < n:
            ch = d[j]
            if ch.isdigit():
                j += 1
            elif ch == '.' and not seen_dot:  # SVG packs "1.2.3" as 1.2, .3
                seen_dot = True; j += 1
            else:
                break
        if j < n and d[j] in 'eE':
            j += 1
            if j < n and d[j] in '+-':
                j += 1
            while j < n and d[j].isdigit():
                j += 1
        v = float(d[i:j]); i = j; return v

    def flag():
        nonlocal i
        skip_sep()
        v = int(d[i]); i += 1; return v

    def more_coords():
        skip_sep()
        return i < n and (d[i].isdigit() or d[i] in '+-.')

    while i < n:
        skip_sep()
        if i >= n:
            break
        if d[i].isalpha():
            cmd = d[i]; i += 1
        rel = cmd.islower(); c = cmd.upper()
        if c == 'M':
            if len(pts) >= 2:
                subpaths.append(pts)
            pts = []
            x = num(); y = num()
            if rel: x += cx; y += cy
            cx, cy = x, y; sx, sy = x, y; pts.append((cx, cy)); cmd = 'l' if rel else 'L'
        elif c == 'L':
            x = num(); y = num()
            if rel: x += cx; y += cy
            cx, cy = x, y; pts.append((cx, cy))
        elif c == 'H':
            x = num()
            if rel: x += cx
            cx = x; pts.append((cx, cy))
        elif c == 'V':
            y = num()
            if rel: y += cy
            cy = y; pts.append((cx, cy))
        elif c in ('C', 'S'):
            if c == 'C':
                x1 = num(); y1 = num(); x2 = num(); y2 = num(); x = num(); y = num()
                if rel: x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy
            else:
                x2 = num(); y2 = num(); x = num(); y = num()
                if rel: x2 += cx; y2 += cy; x += cx; y += cy
                if prev_c2: x1 = 2 * cx - prev_c2[0]; y1 = 2 * cy - prev_c2[1]
                else: x1, y1 = cx, cy
            for s in range(1, 7):
                tt = s / 6.0; mt = 1 - tt
                pts.append((mt**3*cx + 3*mt**2*tt*x1 + 3*mt*tt**2*x2 + tt**3*x,
                            mt**3*cy + 3*mt**2*tt*y1 + 3*mt*tt**2*y2 + tt**3*y))
            prev_c2 = (x2, y2); cx, cy = x, y; continue
        elif c == 'Q':
            x1 = num(); y1 = num(); x = num(); y = num()
            if rel: x1 += cx; y1 += cy; x += cx; y += cy
            for s in range(1, 7):
                tt = s / 6.0; mt = 1 - tt
                pts.append((mt*mt*cx + 2*mt*tt*x1 + tt*tt*x,
                            mt*mt*cy + 2*mt*tt*y1 + tt*tt*y))
            cx, cy = x, y
        elif c == 'A':
            rx = num(); ry = num(); rot = num(); large = flag(); sweep = flag(); x = num(); y = num()
            if rel: x += cx; y += cy
            for p in _arc_points(cx, cy, rx, ry, rot, large, sweep, x, y):
                pts.append(p)
            cx, cy = x, y
        elif c == 'Z':
            pts.append((sx, sy)); cx, cy = sx, sy
        else:
            i += 1
        prev_c2 = None
    if len(pts) >= 2:
        subpaths.append(pts)
    return subpaths


def _path_points(d):
    # Backward-compatible flat vertex list (all subpaths concatenated).
    out = []
    for sp in _path_subpaths(d):
        out.extend(sp)
    return out


_MASK_CACHE = {}


def _tile_mask(svg_name):
    # Rasterize the union of a tile's opaque-fill shapes (rects, paths,
    # polylines/polygons with fill="#..."; opacity-only shadows ignored) into a
    # MASK_RES x MASK_RES coverage mask, supersampled 3x for a clean outline.
    if svg_name in _MASK_CACHE:
        return _MASK_CACHE[svg_name]
    from PIL import Image, ImageDraw
    txt = open(os.path.join(TILES, svg_name)).read()
    RES = MASK_RES * 3
    sc = RES / 256.0
    img = Image.new('1', (RES, RES), 0)
    dr = ImageDraw.Draw(img)
    drew = False

    def attr(el, name):
        m = re.search(name + r'="([^"]*)"', el)
        return m.group(1) if m else None

    # only elements that carry an opaque fill colour count as solid
    for el in re.findall(r'<(?:rect|path|polygon|polyline)\b[^>]*?/?>', txt):
        if not re.search(r'fill="#[0-9a-fA-F]{3,8}"', el):
            continue
        try:
            if el.startswith('<rect'):
                x = float(attr(el, 'x') or 0); y = float(attr(el, 'y') or 0)
                w = float(attr(el, 'width') or 0); h = float(attr(el, 'height') or 0)
                if w > 0 and h > 0:
                    dr.rectangle([x*sc, y*sc, (x+w)*sc, (y+h)*sc], fill=1); drew = True
            elif el.startswith('<path'):
                d = attr(el, 'd')
                if d:
                    poly = [(px*sc, py*sc) for px, py in _path_points(d)]
                    if len(poly) >= 3:
                        dr.polygon(poly, fill=1); drew = True
            else:  # polygon / polyline
                p = attr(el, 'points')
                if p:
                    nums = [float(v) for v in re.findall(r'-?\d*\.?\d+', p)]
                    poly = [(nums[k]*sc, nums[k+1]*sc) for k in range(0, len(nums) - 1, 2)]
                    if len(poly) >= 3:
                        dr.polygon(poly, fill=1); drew = True
        except Exception:
            pass
    if not drew:
        dr.rectangle([0, 0, RES, RES], fill=1)   # nothing parsed: block whole cell
    px = img.load()
    mask = []
    for sr in range(MASK_RES):
        row = []
        for sc in range(MASK_RES):
            cnt = 0
            for yy in range(sr * 3, sr * 3 + 3):
                for xx in range(sc * 3, sc * 3 + 3):
                    cnt += px[xx, yy]
            row.append(1 if cnt >= 5 else 0)   # majority of the 9 samples
        mask.append(row)
    _MASK_CACHE[svg_name] = mask
    return mask


def _poly_area(poly):
    a = 0.0
    for i in range(len(poly)):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % len(poly)]
        a += x1 * y2 - x2 * y1
    return abs(a) / 2


def _tile_polys(svg_name):
    # Largest opaque-fill shape of a tile as a polygon (verts in 0..256).
    txt = open(os.path.join(TILES, svg_name)).read()

    def attr(el, name):
        m = re.search(name + r'="([^"]*)"', el)
        return m.group(1) if m else None

    polys = []
    for el in re.findall(r'<(?:rect|path|polygon|polyline)\b[^>]*?/?>', txt):
        if not re.search(r'fill="#[0-9a-fA-F]{3,8}"', el):
            continue
        try:
            if el.startswith('<rect'):
                x = float(attr(el, 'x') or 0); y = float(attr(el, 'y') or 0)
                w = float(attr(el, 'width') or 0); h = float(attr(el, 'height') or 0)
                if w > 0 and h > 0:
                    polys.append([(x, y), (x + w, y), (x + w, y + h), (x, y + h)])
            elif el.startswith('<path'):
                d = attr(el, 'd')
                if d:
                    for p in _path_subpaths(d):
                        if len(p) >= 3:
                            polys.append(p)
            else:
                pts = attr(el, 'points')
                if pts:
                    n = [float(v) for v in re.findall(r'-?\d*\.?\d+', pts)]
                    p = [(n[k], n[k + 1]) for k in range(0, len(n) - 1, 2)]
                    if len(p) >= 3:
                        polys.append(p)
        except Exception:
            pass
    if not polys:
        return [(0, 0), (256, 0), (256, 256), (0, 256)]
    return max(polys, key=_poly_area)


def _flip_poly(poly, g):
    out = []
    for x, y in poly:
        if g & FLIP_D_B:
            x, y = y, x
        if g & FLIP_H_B:
            x = 256 - x
        if g & FLIP_V_B:
            y = 256 - y
        out.append((x, y))
    return out


def write_tilemap_header(layers, W, H):
    def present(name, i):
        return (layers.get(name, [0] * (W * H))[i] & 0x1FFFFFFF) != 0

    FULL = [(0, 0), (500, 0), (500, 500), (0, 500)]   # void / fallback cell
    terrain = []
    raw = [None] * (W * H)   # per-cell fill polygon (0..500) or None if walkable

    for r in range(H):
        for c in range(W):
            i = r * W + c
            t = 0
            for name in BLOCK_LAYERS:
                if present(name, i):
                    t = LAYER_BLOCK_ID[name]
                    break
            else:
                if not present('bg', i):
                    t = 15  # void: outside the map, must block
            terrain.append(t)

            if t == 15:
                raw[i] = FULL
                continue
            for name in BLOCK_LAYERS:
                g = layers.get(name, [0] * (W * H))[i]
                if not (g & 0x1FFFFFFF):
                    continue
                svg = GID_TO_SVG.get(g & 0x1FFFFFFF, LAYER_FALLBACK.get(name))
                if not svg or not os.path.exists(os.path.join(TILES, svg)):
                    p = FULL
                else:
                    p = [(x * 500 / 256, y * 500 / 256)
                         for x, y in _flip_poly(_tile_polys(svg), g)]
                raw[i] = p
                break

    # Seam fill: where two blocking cells touch, the shared edge must be solid.
    # If a blocking cell's fill doesn't reach an edge it shares with a blocking
    # neighbour, promote that cell to a full square so the "touching" strip is
    # unreachable -- and black on the minimap, which samples this same data.
    BLOCK = {1, 2, 3, 4, 5, 6, 15}
    SAMP = [40, 130, 250, 370, 460]

    def _in(px, py, poly):
        inside = False
        n = len(poly)
        j = n - 1
        for k in range(n):
            xi, yi = poly[k]
            xj, yj = poly[j]
            if ((yi > py) != (yj > py)) and (px < (xj - xi) * (py - yi) / (yj - yi) + xi):
                inside = not inside
            j = k
        return inside

    promote = [False] * (W * H)
    for r in range(H):
        for c in range(W):
            i = r * W + c
            if terrain[i] not in BLOCK or raw[i] is None or raw[i] is FULL:
                continue
            edges = [
                (c + 1 < W and terrain[i + 1] in BLOCK,  [(498, s) for s in SAMP]),  # E
                (c - 1 >= 0 and terrain[i - 1] in BLOCK, [(2,   s) for s in SAMP]),  # W
                (r - 1 >= 0 and terrain[i - W] in BLOCK, [(s,   2) for s in SAMP]),  # N
                (r + 1 < H and terrain[i + W] in BLOCK,  [(s, 498) for s in SAMP]),  # S
            ]
            # Promote only when an ENTIRE wall-facing edge is uncovered by the
            # fill -- a genuine full-width seam (grass strip wedged between two
            # blocking tiles). Partially-covered edges are real shaped shorelines
            # against open ground and are left alone.
            for nb_block, pts in edges:
                if nb_block and all(not _in(px, py, raw[i]) for px, py in pts):
                    promote[i] = True
                    break

    polys = []            # list of vert-lists in 0..500
    unique = {}           # rounded-tuple -> poly index (1-based)
    cell_poly = [0] * (W * H)

    def add_poly(p):
        key = tuple((round(x, 1), round(y, 1)) for x, y in p)
        if key not in unique:
            unique[key] = len(polys) + 1
            polys.append(p)
        return unique[key]

    for i in range(W * H):
        if raw[i] is None:
            continue
        cell_poly[i] = add_poly(FULL if promote[i] else raw[i])

    # flatten polygon vertices
    verts = []
    starts = []
    lens = []
    for p in polys:
        starts.append(len(verts) // 2)   # vertex index; C indexes POLY_VERTS[2*(s+i)]
        lens.append(len(p))
        for x, y in p:
            verts.append(x); verts.append(y)

    lines = [
        '#pragma once', '', '#include <cstdint>', '', '#include <cmath>', '',
        '// Auto-generated by Scripts/gen_map.py.',
        '//  - TERRAIN: coarse per-cell type (minimap colours).',
        '//  - CELL_POLY/POLY_*: each blocking cell references its tile\'s fill',
        '//    polygon (flip baked in, verts in 0..500 cell-local). Collision is',
        '//    exact point/circle-in-polygon, so precision is ~1 world unit and',
        '//    follows the visible asset outline. Void cells block the outside.', '',
        'namespace Tilemap {',
        f'    inline constexpr uint32_t GRID_W = {W};',
        f'    inline constexpr uint32_t GRID_H = {H};',
        '    inline constexpr float    CELL_SIZE = 500;',
        '    inline constexpr float    COLL_CELL = 4;   // minimap sampling step hint',
        '',
        '    namespace TerrainID {',
        '        enum : uint8_t {',
        '            kGrass=0, kDirt=1, kBush=2, kWater=3, kJungle=4,',
        '            kCliff=5, kCastle=6, kStone=7, kVoid=15,',
        '        };', '    }', '',
        '    inline constexpr uint8_t TERRAIN[GRID_W * GRID_H] = {',
    ]
    for r in range(H):
        lines.append('        ' + ','.join(str(v) for v in terrain[r * W:(r + 1) * W]) + ',')
    lines += ['    };', '',
        f'    inline constexpr uint32_t NUM_POLYS = {len(polys)};',
        '    inline constexpr float POLY_VERTS[] = {',
    ]
    # chunk verts for readability
    for k in range(0, len(verts), 16):
        lines.append('        ' + ','.join('%.1ff' % v for v in verts[k:k + 16]) + ',')
    lines += ['    };', '',
        '    inline constexpr uint32_t POLY_START[NUM_POLYS] = {' + ','.join(str(v) for v in starts) + '};',
        '    inline constexpr uint32_t POLY_LEN[NUM_POLYS] = {' + ','.join(str(v) for v in lens) + '};', '',
        '    inline constexpr uint16_t CELL_POLY[GRID_W * GRID_H] = {',
    ]
    for r in range(H):
        lines.append('        ' + ','.join(str(v) for v in cell_poly[r * W:(r + 1) * W]) + ',')
    lines += ['    };', '',
        '    inline uint8_t terrain_at(float x, float y) {',
        '        if (x < 0 || y < 0) return TerrainID::kVoid;',
        '        uint32_t c=(uint32_t)(x/CELL_SIZE), r=(uint32_t)(y/CELL_SIZE);',
        '        if (c>=GRID_W || r>=GRID_H) return TerrainID::kVoid;',
        '        return TERRAIN[r*GRID_W+c];',
        '    }', '',
        '    inline bool blocks_movement(uint8_t t) {',
        '        switch (t) { case TerrainID::kDirt: case TerrainID::kWater:',
        '            case TerrainID::kJungle: case TerrainID::kBush: case TerrainID::kCliff:',
        '            case TerrainID::kCastle: case TerrainID::kVoid: return true; default: return false; }',
        '    }', '',
        '    // point-in-polygon for local coords (verts POLY_START[p]..+POLY_LEN[p]).',
        '    inline bool _in_poly(float px, float py, uint32_t p) {',
        '        uint32_t s=POLY_START[p], n=POLY_LEN[p]; bool in=false;',
        '        for (uint32_t i=0,j=n-1;i<n;j=i++) {',
        '            float xi=POLY_VERTS[2*(s+i)], yi=POLY_VERTS[2*(s+i)+1];',
        '            float xj=POLY_VERTS[2*(s+j)], yj=POLY_VERTS[2*(s+j)+1];',
        '            if (((yi>py)!=(yj>py)) && (px < (xj-xi)*(py-yi)/(yj-yi)+xi)) in=!in;',
        '        }',
        '        return in;',
        '    }', '',
        '    inline bool solid_at(float x, float y) {',
        '        if (x<0||y<0) return false;',
        '        uint32_t c=(uint32_t)(x/CELL_SIZE), r=(uint32_t)(y/CELL_SIZE);',
        '        if (c>=GRID_W||r>=GRID_H) return false;',
        '        uint16_t pi=CELL_POLY[r*GRID_W+c]; if(!pi) return false;',
        '        return _in_poly(x-c*CELL_SIZE, y-r*CELL_SIZE, pi-1);',
        '    }', '',
        '    inline bool solid_circle(float x, float y, float rad) {',
        '        if (solid_at(x,y)) return true;',
        '        float d=rad*0.70710678f;',
        '        return solid_at(x+rad,y)||solid_at(x-rad,y)||solid_at(x,y+rad)||solid_at(x,y-rad)',
        '            ||solid_at(x+d,y+d)||solid_at(x+d,y-d)||solid_at(x-d,y+d)||solid_at(x-d,y-d);',
        '    }', '',
        '    inline void _closest_seg(float px,float py,float ax,float ay,float bx,float by,float&ox,float&oy){',
        '        float dx=bx-ax,dy=by-ay,l2=dx*dx+dy*dy;',
        '        float t=l2>0?((px-ax)*dx+(py-ay)*dy)/l2:0; if(t<0)t=0; else if(t>1)t=1;',
        '        ox=ax+t*dx; oy=ay+t*dy;',
        '    }', '',
        '    // Push a circle of radius rad out of the tile polygons it overlaps.',
        '    // Each pass resolves only the SINGLE deepest penetration among all',
        '    // overlapped cells, so the result is independent of cell iteration',
        '    // order -- this stops the frame-to-frame oscillation that made a body',
        '    // wedged into a concave wall corner jitter.',
        '    inline void push_circle(float &x, float &y, float rad) {',
        '        for (int pass=0; pass<8; ++pass) {',
        '            int c0=(int)std::floor((x-rad)/CELL_SIZE), c1=(int)std::floor((x+rad)/CELL_SIZE);',
        '            int r0=(int)std::floor((y-rad)/CELL_SIZE), r1=(int)std::floor((y+rad)/CELL_SIZE);',
        '            float best_pen=0.f, tgtx=x, tgty=y; bool found=false;',
        '            for (int rr=r0; rr<=r1; ++rr) for (int cc=c0; cc<=c1; ++cc) {',
        '                if (cc<0||rr<0||cc>=(int)GRID_W||rr>=(int)GRID_H) continue;',
        '                uint16_t pi=CELL_POLY[rr*GRID_W+cc]; if(!pi) continue; --pi;',
        '                float lx=x-cc*CELL_SIZE, ly=y-rr*CELL_SIZE;',
        '                uint32_t s=POLY_START[pi], n=POLY_LEN[pi];',
        '                float bestd2=1e18f, bx=0, by=0;',
        '                for (uint32_t i=0,j=n-1;i<n;j=i++) {',
        '                    float ox,oy; _closest_seg(lx,ly, POLY_VERTS[2*(s+j)],POLY_VERTS[2*(s+j)+1],',
        '                        POLY_VERTS[2*(s+i)],POLY_VERTS[2*(s+i)+1], ox,oy);',
        '                    float ddx=lx-ox, ddy=ly-oy, d2=ddx*ddx+ddy*ddy;',
        '                    if (d2<bestd2){bestd2=d2;bx=ox;by=oy;}',
        '                }',
        '                float bd=std::sqrt(bestd2);',
        '                bool inside=_in_poly(lx,ly,pi);',
        '                float nlx, nly, pen;',
        '                if (inside) {',
        '                    float ux=bx-lx, uy=by-ly, ul=std::sqrt(ux*ux+uy*uy);',
        '                    if (ul<=0.001f) continue;',
        '                    nlx=bx+ux/ul*rad; nly=by+uy/ul*rad; pen=ul+rad;',
        '                } else if (bd<rad && bd>0.001f) {',
        '                    nlx=bx+(lx-bx)/bd*rad; nly=by+(ly-by)/bd*rad; pen=rad-bd;',
        '                } else continue;',
        '                if (pen>best_pen){ best_pen=pen; tgtx=cc*CELL_SIZE+nlx; tgty=rr*CELL_SIZE+nly; found=true; }',
        '            }',
        '            if (!found) break;',
        '            x=tgtx; y=tgty;',
        '        }',
        '    }',
        '}', '',
    ]
    out = os.path.join(ROOT, 'Shared/Tilemap.hh')
    open(out, 'w').write('\n'.join(lines))
    from collections import Counter
    print('Tilemap.hh terrain dist:', dict(Counter(terrain)))
    print(f'collision: {len(polys)} unique polys, {sum(1 for v in cell_poly if v)} blocking cells, {len(verts)//2} verts')


if __name__ == '__main__':
    main()
