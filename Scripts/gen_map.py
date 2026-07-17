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

# --- Tileset (authoritative): gid -> image AND per-tile collision shapes -------
# main.tmj references tiles/tileset.tsj at firstgid 1, so gid = tile_id + 1. Each
# tile carries its image and, for solid tiles, an objectgroup with the exact
# collision shape(s) the map author drew (rects + polygons, in 0..256 tile space).
def _load_tileset():
    ts = json.load(open(os.path.join(TILES, 'tileset.tsj')))
    FIRST = 1
    img, coll = {}, {}
    for t in ts['tiles']:
        gid = t['id'] + FIRST
        if t.get('image'):
            img[gid] = t['image']
        og = t.get('objectgroup')
        if og:
            shapes = []
            for o in og['objects']:
                ox, oy = o.get('x', 0.0), o.get('y', 0.0)
                if 'polygon' in o:
                    shapes.append([(ox + p['x'], oy + p['y']) for p in o['polygon']])
                elif 'polyline' in o:
                    shapes.append([(ox + p['x'], oy + p['y']) for p in o['polyline']])
                else:
                    w, h = o.get('width', 0), o.get('height', 0)
                    if w > 0 and h > 0:
                        shapes.append([(ox, oy), (ox + w, oy), (ox + w, oy + h), (ox, oy + h)])
            if shapes:
                coll[gid] = shapes
    return img, coll

GID_TO_SVG, TILE_COLL = _load_tileset()


def _tile_shapes(svg_name):
    # Ordered opaque shapes of a tile as {d, fill, stroke, sw}, back->front, for
    # the client to build Path2D. All tiles are flat-fill (no gradients/filters).
    txt = open(os.path.join(TILES, svg_name)).read()

    def attr(el, name):
        m = re.search(name + r'="([^"]*)"', el)
        return m.group(1) if m else None

    out = []
    for el in re.findall(r'<(?:rect|path|polygon|polyline)\b[^>]*?/?>', txt):
        fill = attr(el, 'fill')
        stroke = attr(el, 'stroke')
        if (not fill or fill == 'none') and (not stroke or stroke == 'none'):
            continue
        sw = float(attr(el, 'stroke-width') or 0)
        if el.startswith('<rect'):
            x = float(attr(el, 'x') or 0); y = float(attr(el, 'y') or 0)
            w = float(attr(el, 'width') or 0); h = float(attr(el, 'height') or 0)
            d = f'M{x} {y}h{w}v{h}h{-w}Z'
        elif el.startswith('<path'):
            d = attr(el, 'd')
        else:  # polygon / polyline
            pts = attr(el, 'points') or ''
            nums = [float(v) for v in re.findall(r'-?\d*\.?\d+', pts)]
            if len(nums) < 6:
                continue
            d = 'M' + ' '.join(f'{nums[k]} {nums[k+1]}' for k in range(0, len(nums) - 1, 2))
            if el.startswith('<polygon'):
                d += 'Z'
        if not d:
            continue
        out.append({'d': d,
                    'fill': (fill if fill and fill != 'none' else None),
                    'stroke': (stroke if stroke and stroke != 'none' else None),
                    'sw': sw})
    return out


# Draw order must match the .tmj (bottom -> top). Object 'img' sprites (sewer/
# pyramid/factory landmarks) are drawn on top of the tiles, after this list.
LAYER_ORDER = ['bg', 'transitions', 'water', 'bush', 'cliff', 'dirt', 'castle']
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
BLEED = 3.0
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
    objgroups = {l['name']: l.get('objects', [])
                 for l in d['layers'] if l.get('type') == 'objectgroup'}
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
                # Bleed ONLY the walkable ground layers (bg + transitions) so the
                # ground has no hairline seams. Blocking tiles (water/bush/cliff/
                # dirt/castle) are NEVER bled: bleeding them pushed their visible
                # edge ~10 world units past the collision polygon, leaving a
                # walkable strip of visible water/wall. Un-bled, their visual edge
                # matches the collision outline exactly.
                bleed = BLEED if name in ('bg', 'transitions') else 0.0
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

    # Landmark image objects (sewer_entrance / pyramid / factory_entrance), drawn
    # on top of the tiles. Tiled tile-objects anchor at the BOTTOM-left corner,
    # so the top edge is y - height. tmj is 512 px/cell; the render is CELL px.
    sc = CELL / 512.0
    for o in objgroups.get('img', []):
        g = o.get('gid')
        if not g:
            continue
        gid = g & 0x1FFFFFFF
        svg = GID_TO_SVG.get(gid)
        if not svg or svg not in symbols:
            continue
        sid = f't_{svg.replace(".", "_")}'
        ox, oy = o['x'] * sc, (o['y'] - o.get('height', 0)) * sc
        ow, oh = o.get('width', 0) * sc, o.get('height', 0) * sc
        hf, vf, df = bool(g & FLIP_H), bool(g & FLIP_V), bool(g & FLIP_D)
        if not (hf or vf or df):
            out.append(f'<use href="#{sid}" x="{ox}" y="{oy}" width="{ow}" height="{oh}"/>')
        else:
            a, b, cc, dd = flip_matrix(hf, vf, df)
            mcx, mcy = ox + ow / 2, oy + oh / 2
            out.append(f'<g transform="translate({mcx} {mcy}) matrix({a} {b} {cc} {dd} 0 0) '
                       f'translate({-ow/2} {-oh/2})"><use href="#{sid}" '
                       f'width="{ow}" height="{oh}"/></g>')
        placements += 1

    out.append('</svg>')
    svg = '\n'.join(out)
    for p in OUTS:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'w').write(svg)

    # Client render asset: per-tile Path2D shapes + culled placements + landmark
    # objects. The client draws these as vector each frame (florr.io style).
    import json as _json
    used = set()
    map_placements = []
    for li, name in enumerate(LAYER_ORDER):
        lay = layers.get(name)
        if not lay:
            continue
        for r in range(H):
            for c in range(W):
                g = lay[r * W + c]
                gid = g & 0x1FFFFFFF
                if not gid:
                    continue
                used.add(gid)
                map_placements.append({'l': li, 'c': c, 'r': r, 'gid': gid,
                                       'flip': g & (FLIP_H | FLIP_V | FLIP_D)})
    map_objects = []
    W2 = 500.0 / 512.0                       # tmj px -> world
    for o in objgroups.get('img', []):
        g = o.get('gid')
        if not g:
            continue
        gid = g & 0x1FFFFFFF
        used.add(gid)
        map_objects.append({'gid': gid,
                            'x': o['x'] * W2, 'y': (o['y'] - o.get('height', 0)) * W2,
                            'w': o.get('width', 0) * W2, 'h': o.get('height', 0) * W2,
                            'flip': g & (FLIP_H | FLIP_V | FLIP_D)})
    map_tiles = {}
    for gid in sorted(used):
        svg_name = GID_TO_SVG.get(gid)
        if svg_name and os.path.exists(os.path.join(TILES, svg_name)):
            map_tiles[str(gid)] = _tile_shapes(svg_name)
    map_data = {'cell': 500, 'tileSize': 256, 'layers': LAYER_ORDER,
                'tiles': map_tiles, 'placements': map_placements, 'objects': map_objects}
    open(os.path.join(ROOT, 'Server/map-data.json'), 'w').write(
        _json.dumps(map_data, separators=(',', ':')))
    print(f'map-data.json: {len(map_tiles)} tiles, {len(map_placements)} placements, '
          f'{len(map_objects)} objects')

    print(f'placements={placements}, mapped GIDs={len(GID_TO_SVG)}')
    for n, c in unknown.items():
        if c:
            print(f'  still-unknown {n}: {dict(sorted(c.items()))}')

    write_tilemap_header(layers, objgroups, W, H)


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
                    # Draw each subpath separately so the opaque area is the UNION
                    # of all shapes (concatenating them into one polygon would
                    # bridge disjoint blobs and distort the outline).
                    for sp in _path_subpaths(d):
                        poly = [(px*sc, py*sc) for px, py in sp]
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


def _flip_mask(m, g):
    # Apply Tiled flip bits to an NxN coverage mask (same transform as _flip_poly).
    if not (g & (FLIP_D_B | FLIP_H_B | FLIP_V_B)):
        return m
    n = len(m)
    D, Hf, Vf = bool(g & FLIP_D_B), bool(g & FLIP_H_B), bool(g & FLIP_V_B)
    out = [[0] * n for _ in range(n)]
    for sy in range(n):
        row = m[sy]
        for sx in range(n):
            if not row[sx]:
                continue
            x, y = sx, sy
            if D:  x, y = y, x
            if Hf: x = n - 1 - x
            if Vf: y = n - 1 - y
            out[y][x] = 1
    return out


def _sync_collision_editor(gm, GW, GH):
    # Downsample the global collision mask (GW x GH, 10 world u/px) to the
    # editor's 25-world-u grid and patch collision-editor.html's B64_INIT.
    import base64
    import re
    EDITOR_UNIT = 25
    UW = GW * 10 // EDITOR_UNIT           # 1000
    UH = GH * 10 // EDITOR_UNIT           # 1020
    buf = bytearray((UW * UH + 7) // 8)
    half = EDITOR_UNIT / 2.0
    for uy in range(UH):
        py = int((uy * EDITOR_UNIT + half) / 10.0)
        if py >= GH:
            py = GH - 1
        base = py * GW
        rowbase = uy * UW
        for ux in range(UW):
            px = int((ux * EDITOR_UNIT + half) / 10.0)
            if px >= GW:
                px = GW - 1
            if gm[base + px]:
                idx = rowbase + ux
                buf[idx >> 3] |= 1 << (idx & 7)
    b64 = base64.b64encode(bytes(buf)).decode()

    targets = [os.path.join(ROOT, 'docs/collision-editor.html'),
               os.path.expanduser('~/Desktop/floir-collision-editor.html')]
    for path in targets:
        if not os.path.exists(path):
            continue
        html = open(path).read()
        html = re.sub(r'const B64_INIT="[^"]*";',
                      'const B64_INIT="' + b64 + '";', html)
        html = re.sub(r'UNIT=\d+;', 'UNIT=' + str(EDITOR_UNIT) + ';', html)
        html = re.sub(r'1 unit = \d+ world u',
                      '1 unit = ' + str(EDITOR_UNIT) + ' world u', html)
        open(path, 'w').write(html)
    print(f'synced collision editor: {UW}x{UH} units ({EDITOR_UNIT} world u/cell)')


def write_tilemap_header(layers, objgroups, W, H):
    def present(name, i):
        return (layers.get(name, [0] * (W * H))[i] & 0x1FFFFFFF) != 0

    # ---- coarse per-cell terrain (minimap colours) ----
    terrain = []
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
                    t = 15
            terrain.append(t)

    # ---- global collision mask: authoritative per-tile shapes from the tileset --
    # Every placed tile that carries a collision objectgroup contributes its exact
    # author-drawn shape(s) (rects + polygons, flip baked in). The `collision`
    # object rectangles and the landmark `img` sprites (sewer/pyramid/factory) add
    # their shapes too. All rasterised into one mask at SUB sub-cells/cell.
    SUB = MASK_RES                       # 50 sub-cells per 500 cell -> 10 u/px
    GW, GH = W * SUB, H * SUB
    from PIL import Image, ImageDraw
    gimg = Image.new('1', (GW, GH), 0)
    gdraw = ImageDraw.Draw(gimg)
    T2M = SUB / 256.0                    # tile-space (0..256) -> mask px within a cell
    PX2M = SUB / 512.0                   # tmj px (512/cell) -> mask px

    def _flip_pt(x, y, g):               # tile-space flip; matches _flip_poly order
        if g & FLIP_D_B: x, y = y, x
        if g & FLIP_H_B: x = 256 - x
        if g & FLIP_V_B: y = 256 - y
        return x, y

    # per-tile collision across every tile layer (bg/transitions carry none)
    for name in LAYER_ORDER:
        lay = layers.get(name)
        if not lay:
            continue
        for r in range(H):
            for c in range(W):
                g = lay[r * W + c]
                shapes = TILE_COLL.get(g & 0x1FFFFFFF)
                if not shapes:
                    continue
                ox, oy = c * SUB, r * SUB
                for sh in shapes:
                    pts = [(ox + fx * T2M, oy + fy * T2M)
                           for fx, fy in (_flip_pt(px, py, g) for px, py in sh)]
                    if len(pts) >= 3:
                        gdraw.polygon(pts, fill=1)

    # void (outside the map) blocks
    for r in range(H):
        for c in range(W):
            i = r * W + c
            if terrain[i] == 15 and not present('bg', i):
                gdraw.rectangle([c * SUB, r * SUB, c * SUB + SUB, r * SUB + SUB], fill=1)

    # `collision` object layer -> explicit rectangles / polygons (tmj px)
    for o in objgroups.get('collision', []):
        if 'polygon' in o:
            pts = [((o['x'] + p['x']) * PX2M, (o['y'] + p['y']) * PX2M) for p in o['polygon']]
            if len(pts) >= 3:
                gdraw.polygon(pts, fill=1)
        else:
            x, y = o['x'] * PX2M, o['y'] * PX2M
            w, h = o.get('width', 0) * PX2M, o.get('height', 0) * PX2M
            if w > 0 and h > 0:
                gdraw.rectangle([x, y, x + w, y + h], fill=1)

    # landmark `img` sprites -> collision shape scaled to the sprite (y = bottom)
    for o in objgroups.get('img', []):
        g = o.get('gid')
        if not g:
            continue
        shapes = TILE_COLL.get(g & 0x1FFFFFFF)
        if not shapes:
            continue
        ow, oh = o.get('width', 0), o.get('height', 0)
        otop = o['y'] - oh
        for sh in shapes:
            pts = []
            for px, py in sh:
                fx, fy = _flip_pt(px, py, g)
                pts.append(((o['x'] + fx / 256.0 * ow) * PX2M,
                            (otop + fy / 256.0 * oh) * PX2M))
            if len(pts) >= 3:
                gdraw.polygon(pts, fill=1)

    gm = bytearray(1 if v else 0 for v in gimg.getdata())

    # seal hairline seams (close = dilate then erode: fills gaps <= ~2px / 20u
    # without growing the outer boundary)
    try:
        from PIL import Image, ImageFilter
        im = Image.frombytes('L', (GW, GH), bytes(255 if v else 0 for v in gm))
        im = im.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3))
        gm = bytearray(1 if v else 0 for v in im.getdata())
    except Exception as e:
        print('seam close skipped:', e)

    # Fill isolated walkable pockets (the stray "white dots"): any small enclosed
    # walkable region unreachable from the main play area becomes solid.
    try:
        import numpy as np
        from scipy import ndimage
        arr = np.frombuffer(bytes(gm), dtype=np.uint8).reshape(GH, GW).copy()
        lbl, n = ndimage.label(arr == 0)          # 4-connected walkable regions
        if n > 1:
            sizes = ndimage.sum(np.ones_like(lbl), lbl, range(1, n + 1))
            filled = 0
            for i in range(1, n + 1):
                if sizes[i - 1] < 3000:            # < ~1 tile of area -> pocket
                    arr[lbl == i] = 1
                    filled += 1
            gm = bytearray(arr.tobytes())
            if filled:
                print(f'filled {filled} isolated walkable pocket(s)')
    except Exception as e:
        print('pocket fill skipped:', e)

    _sync_collision_editor(gm, GW, GH)

    # ---- slice into per-cell sub-masks; classify empty/full/detailed + dedup ----
    CELL_MASK = [0] * (W * H)
    detail = []
    detail_idx = {}
    for r in range(H):
        for c in range(W):
            bits = bytearray((SUB * SUB + 7) // 8)
            cnt = 0
            for sy in range(SUB):
                base = (r * SUB + sy) * GW + c * SUB
                for sx in range(SUB):
                    if gm[base + sx]:
                        k = sy * SUB + sx
                        bits[k >> 3] |= 1 << (k & 7)
                        cnt += 1
            if cnt == 0:
                CELL_MASK[r * W + c] = 0
            elif cnt == SUB * SUB:
                CELL_MASK[r * W + c] = 1
            else:
                key = bytes(bits)
                di = detail_idx.get(key)
                if di is None:
                    di = len(detail)
                    detail_idx[key] = di
                    detail.append(bits)
                CELL_MASK[r * W + c] = 2 + di

    submask = bytearray((len(detail) * SUB * SUB + 7) // 8)
    for di, bits in enumerate(detail):
        dbase = di * SUB * SUB
        for k in range(SUB * SUB):
            if (bits[k >> 3] >> (k & 7)) & 1:
                gbit = dbase + k
                submask[gbit >> 3] |= 1 << (gbit & 7)

    # ---- emit Tilemap.hh ----
    lines = [
        '#pragma once', '', '#include <cstdint>', '', '#include <cmath>', '',
        '// Auto-generated by Scripts/gen_map.py.',
        '//  - TERRAIN: coarse per-cell type (minimap colours).',
        '//  - CELL_MASK/SUBMASK: the whole block layer rasterised together into',
        '//    one union mask at SUB sub-cells/cell (10 world u/px). Each cell is',
        '//    empty(0), full(1) or an index into per-cell sub-masks. Collision',
        '//    follows the combined visible outline with no per-tile seams; only',
        '//    exposed unit faces block, so bodies slide smoothly along walls.', '',
        'namespace Tilemap {',
        f'    inline constexpr uint32_t GRID_W = {W};',
        f'    inline constexpr uint32_t GRID_H = {H};',
        '    inline constexpr float    CELL_SIZE = 500;',
        '    inline constexpr float    COLL_CELL = 4;   // minimap sampling step hint',
        f'    inline constexpr uint32_t SUB = {SUB};',
        '    inline constexpr float    COLL_UNIT = CELL_SIZE / SUB;   // 10 world u/px',
        '    inline constexpr uint32_t CU_W = GRID_W * SUB;',
        '    inline constexpr uint32_t CU_H = GRID_H * SUB;',
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
        f'    inline constexpr uint32_t NUM_DETAIL = {len(detail)};',
        '    inline constexpr uint16_t CELL_MASK[GRID_W * GRID_H] = {',
    ]
    for r in range(H):
        lines.append('        ' + ','.join(str(v) for v in CELL_MASK[r * W:(r + 1) * W]) + ',')
    lines += ['    };', '',
        f'    inline constexpr uint8_t SUBMASK[{len(submask)}] = {{',
    ]
    for k in range(0, len(submask), 32):
        lines.append('        ' + ','.join(str(v) for v in submask[k:k + 32]) + ',')
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
        '    // Is collision unit (ux,uy) solid? Out of bounds = solid (arena edge).',
        '    inline bool _usolid(int ux, int uy) {',
        '        if (ux<0||uy<0||ux>=(int)CU_W||uy>=(int)CU_H) return true;',
        '        uint16_t m=CELL_MASK[(ux/(int)SUB)+(uy/(int)SUB)*GRID_W];',
        '        if(!m) return false; if(m==1) return true;',
        '        uint32_t bit=(uint32_t)(m-2)*(SUB*SUB)+(uint32_t)(uy%(int)SUB)*SUB+(uint32_t)(ux%(int)SUB);',
        '        return (SUBMASK[bit>>3]>>(bit&7))&1u;',
        '    }', '',
        '    inline bool solid_at(float x, float y) {',
        '        return _usolid((int)std::floor(x/COLL_UNIT), (int)std::floor(y/COLL_UNIT));',
        '    }', '',
        '    inline bool solid_circle(float x, float y, float rad) {',
        '        int x0=(int)std::floor((x-rad)/COLL_UNIT), x1=(int)std::floor((x+rad)/COLL_UNIT);',
        '        int y0=(int)std::floor((y-rad)/COLL_UNIT), y1=(int)std::floor((y+rad)/COLL_UNIT);',
        '        float r2=rad*rad;',
        '        for (int uy=y0; uy<=y1; ++uy) for (int ux=x0; ux<=x1; ++ux) {',
        '            if (!_usolid(ux,uy)) continue;',
        '            float ax=ux*COLL_UNIT, ay=uy*COLL_UNIT;',
        '            float px=x<ax?ax:(x>ax+COLL_UNIT?ax+COLL_UNIT:x);',
        '            float py=y<ay?ay:(y>ay+COLL_UNIT?ay+COLL_UNIT:y);',
        '            float dx=x-px,dy=y-py; if (dx*dx+dy*dy<r2) return true;',
        '        }',
        '        return false;',
        '    }', '',
        '    // Push a circle out of the solid units. Only EXPOSED unit faces (those',
        '    // bordering an empty unit) collide, so a body slides smoothly along a',
        '    // wall instead of catching on the internal seam between two solid units.',
        '    // Deepest correction per pass so concave corners settle without jitter.',
        '    inline void push_circle(float &x, float &y, float rad) {',
        '        const float U=COLL_UNIT;',
        '        for (int pass=0; pass<6; ++pass) {',
        '            int cx0=(int)std::floor((x-rad)/U)-1, cx1=(int)std::floor((x+rad)/U)+1;',
        '            int cy0=(int)std::floor((y-rad)/U)-1, cy1=(int)std::floor((y+rad)/U)+1;',
        '            float best=0.f, bx=x, by=y; bool found=false;',
        '            for (int uy=cy0; uy<=cy1; ++uy) for (int ux=cx0; ux<=cx1; ++ux) {',
        '                if (!_usolid(ux,uy)) continue;',
        '                float ax0=ux*U, ay0=uy*U, ax1=ax0+U, ay1=ay0+U;',
        '                auto face=[&](float nx,float ny,float p0x,float p0y,float p1x,float p1y){',
        '                    float ex=p1x-p0x, ey=p1y-p0y, l2=ex*ex+ey*ey;',
        '                    float t=l2>0?((x-p0x)*ex+(y-p0y)*ey)/l2:0.f; if(t<0)t=0; else if(t>1)t=1;',
        '                    float qx=p0x+t*ex, qy=p0y+t*ey;',
        '                    float vx=x-qx, vy=y-qy, d=std::sqrt(vx*vx+vy*vy), dn=vx*nx+vy*ny;',
        '                    float dx,dy,pen;',
        '                    if (dn>=0.f && d>1e-4f){ dx=vx/d; dy=vy/d; pen=rad-d; }',
        '                    else { dx=nx; dy=ny; pen=rad-dn; }',
        '                    if (pen<=0.f) return;',
        '                    if (pen>best){ best=pen; bx=qx+dx*rad; by=qy+dy*rad; found=true; }',
        '                };',
        '                if (!_usolid(ux-1,uy)) face(-1,0, ax0,ay0, ax0,ay1);',
        '                if (!_usolid(ux+1,uy)) face( 1,0, ax1,ay0, ax1,ay1);',
        '                if (!_usolid(ux,uy-1)) face(0,-1, ax0,ay0, ax1,ay0);',
        '                if (!_usolid(ux,uy+1)) face(0, 1, ax0,ay1, ax1,ay1);',
        '            }',
        '            if (!found) break;',
        '            x=bx; y=by;',
        '        }',
        '    }',
        '}', '',
    ]
    out = os.path.join(ROOT, 'Shared/Tilemap.hh')
    open(out, 'w').write('\n'.join(lines))
    from collections import Counter
    print('Tilemap.hh terrain dist:', dict(Counter(terrain)))
    nfull = sum(1 for v in CELL_MASK if v == 1)
    ndet = sum(1 for v in CELL_MASK if v >= 2)
    print(f'collision mask: {len(detail)} unique sub-masks, {nfull} full + {ndet} detailed cells, '
          f'SUBMASK {len(submask)//1024}KB, {SUB}x{SUB}/cell (10 world u/px)')

    # Debug export: full-res red collision mask PNG (composited with the map by
    # the Desktop export step) + a downscaled preview under docs/.
    try:
        from PIL import Image
        red = Image.new('RGBA', (GW, GH), (0, 0, 0, 0))
        px = red.load()
        for y in range(GH):
            base = y * GW
            for x in range(GW):
                if gm[base + x]:
                    px[x, y] = (255, 0, 0, 140)
        red.save(os.path.join(ROOT, 'docs/collision-mask.png'))
        print('wrote docs/collision-mask.png (red hitbox mask)')
    except Exception as e:
        print('collision mask png skipped:', e)


if __name__ == '__main__':
    main()
