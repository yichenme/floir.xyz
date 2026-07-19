#!/usr/bin/env python3
"""Convert a 110x110-viewBox SVG (florr-style mob/petal art) into a C++ draw()
header that uses the project's Renderer API (ctx.move_to/line_to + set_fill/
set_stroke). All curves and arcs are FLATTENED into polylines so the output only
needs move_to/line_to (matches Client/Assets/MjolnirArt.hh's approach).

Usage: python3 svg_to_draw.py in.svg NamespaceName > Out.hh
Paths inside <clipPath> are skipped (they define clips, not visible art).
Coordinates are emitted centred on the viewBox centre so draw() can scale by r.
"""
import sys, math, re
import xml.etree.ElementTree as ET

CURVE_SAMPLES = 16   # segments per bezier
ARC_SAMPLES   = 24   # segments per full-ish arc

def tokenize(d):
    # numbers (incl. scientific + signs) and command letters
    return re.findall(r'[MmLlHhVvCcSsQqTtAaZz]|-?\d*\.?\d+(?:[eE][-+]?\d+)?', d)

def bezier3(p0, p1, p2, p3, n):
    out = []
    for i in range(1, n + 1):
        t = i / n; u = 1 - t
        x = u*u*u*p0[0] + 3*u*u*t*p1[0] + 3*u*t*t*p2[0] + t*t*t*p3[0]
        y = u*u*u*p0[1] + 3*u*u*t*p1[1] + 3*u*t*t*p2[1] + t*t*t*p3[1]
        out.append((x, y))
    return out

def bezier2(p0, p1, p2, n):
    out = []
    for i in range(1, n + 1):
        t = i / n; u = 1 - t
        x = u*u*p0[0] + 2*u*t*p1[0] + t*t*p2[0]
        y = u*u*p0[1] + 2*u*t*p1[1] + t*t*p2[1]
        out.append((x, y))
    return out

def arc(p0, rx, ry, phi, large, sweep, p1, n):
    # SVG elliptical-arc endpoint -> centre parametrization, sampled.
    if rx == 0 or ry == 0 or p0 == p1:
        return [p1]
    phi = math.radians(phi)
    cosp, sinp = math.cos(phi), math.sin(phi)
    dx, dy = (p0[0]-p1[0])/2, (p0[1]-p1[1])/2
    x1p =  cosp*dx + sinp*dy
    y1p = -sinp*dx + cosp*dy
    rx, ry = abs(rx), abs(ry)
    l = x1p*x1p/(rx*rx) + y1p*y1p/(ry*ry)
    if l > 1:
        s = math.sqrt(l); rx *= s; ry *= s
    num = rx*rx*ry*ry - rx*rx*y1p*y1p - ry*ry*x1p*x1p
    den = rx*rx*y1p*y1p + ry*ry*x1p*x1p
    co = math.sqrt(max(0.0, num/den)) if den else 0.0
    if large == sweep: co = -co
    cxp =  co*rx*y1p/ry
    cyp = -co*ry*x1p/rx
    cx = cosp*cxp - sinp*cyp + (p0[0]+p1[0])/2
    cy = sinp*cxp + cosp*cyp + (p0[1]+p1[1])/2
    def ang(ux, uy, vx, vy):
        d = math.hypot(ux, uy) * math.hypot(vx, vy)
        c = max(-1.0, min(1.0, (ux*vx+uy*vy)/d)) if d else 1.0
        a = math.acos(c)
        return -a if (ux*vy - uy*vx) < 0 else a
    th0 = ang(1, 0, (x1p-cxp)/rx, (y1p-cyp)/ry)
    dth = ang((x1p-cxp)/rx, (y1p-cyp)/ry, (-x1p-cxp)/rx, (-y1p-cyp)/ry)
    if not sweep and dth > 0: dth -= 2*math.pi
    if sweep and dth < 0: dth += 2*math.pi
    steps = max(2, int(n * abs(dth) / (2*math.pi)) + 1)
    out = []
    for i in range(1, steps + 1):
        th = th0 + dth * i/steps
        x = cosp*rx*math.cos(th) - sinp*ry*math.sin(th) + cx
        y = sinp*rx*math.cos(th) + cosp*ry*math.sin(th) + cy
        out.append((x, y))
    return out

def parse_path(d):
    """Return list of subpaths; each subpath is a list of (x,y) points."""
    t = tokenize(d); i = 0
    subpaths = []; cur = []
    x = y = 0.0; sx = sy = 0.0
    cmd = None; prev_ctrl = None; prev_cmd = None
    def num():
        nonlocal i
        v = float(t[i]); i += 1; return v
    while i < len(t):
        if re.match(r'[A-Za-z]', t[i]):
            cmd = t[i]; i += 1
        c = cmd
        if c in 'Mm':
            if c == 'm': x += num(); y += num()
            else: x = num(); y = num()
            if cur: subpaths.append(cur)
            cur = [(x, y)]; sx, sy = x, y
            cmd = 'l' if c == 'm' else 'L'
            prev_ctrl = None
        elif c in 'Ll':
            if c == 'l': x += num(); y += num()
            else: x = num(); y = num()
            cur.append((x, y)); prev_ctrl = None
        elif c in 'Hh':
            x = (x + num()) if c == 'h' else num(); cur.append((x, y)); prev_ctrl = None
        elif c in 'Vv':
            y = (y + num()) if c == 'v' else num(); cur.append((x, y)); prev_ctrl = None
        elif c in 'Cc':
            if c == 'c':
                p1 = (x+num(), y+num()); p2 = (x+num(), y+num()); p3 = (x+num(), y+num())
            else:
                p1 = (num(), num()); p2 = (num(), num()); p3 = (num(), num())
            cur += bezier3((x, y), p1, p2, p3, CURVE_SAMPLES); prev_ctrl = p2; x, y = p3
        elif c in 'Ss':
            if c == 's': p2 = (x+num(), y+num()); p3 = (x+num(), y+num())
            else: p2 = (num(), num()); p3 = (num(), num())
            p1 = (2*x - prev_ctrl[0], 2*y - prev_ctrl[1]) if prev_cmd in 'CcSs' and prev_ctrl else (x, y)
            cur += bezier3((x, y), p1, p2, p3, CURVE_SAMPLES); prev_ctrl = p2; x, y = p3
        elif c in 'Qq':
            if c == 'q': p1 = (x+num(), y+num()); p2 = (x+num(), y+num())
            else: p1 = (num(), num()); p2 = (num(), num())
            cur += bezier2((x, y), p1, p2, CURVE_SAMPLES); prev_ctrl = p1; x, y = p2
        elif c in 'Tt':
            p1 = (2*x - prev_ctrl[0], 2*y - prev_ctrl[1]) if prev_cmd in 'QqTt' and prev_ctrl else (x, y)
            if c == 't': p2 = (x+num(), y+num())
            else: p2 = (num(), num())
            cur += bezier2((x, y), p1, p2, CURVE_SAMPLES); prev_ctrl = p1; x, y = p2
        elif c in 'Aa':
            rx = num(); ry = num(); rot = num(); large = num(); sweep = num()
            if c == 'a': ex = x+num(); ey = y+num()
            else: ex = num(); ey = num()
            cur += arc((x, y), rx, ry, rot, large, sweep, (ex, ey), ARC_SAMPLES); x, y = ex, ey; prev_ctrl = None
        elif c in 'Zz':
            cur.append((sx, sy)); x, y = sx, sy
            subpaths.append(cur); cur = []
            prev_ctrl = None
        else:
            raise SystemExit('unknown cmd ' + c)
        prev_cmd = c
    if cur: subpaths.append(cur)
    return subpaths

NS = '{http://www.w3.org/2000/svg}'

def collect(elem, clip=False, acc=None):
    if acc is None: acc = []
    tag = elem.tag.replace(NS, '')
    inside_clip = clip or tag == 'clipPath'
    if tag == 'path' and not inside_clip:
        acc.append(elem)
    for ch in elem:
        collect(ch, inside_clip, acc)
    return acc

def hexcol(c, opacity=None):
    c = c.strip()
    if c.startswith('#'):
        h = c[1:]
        if len(h) == 3: h = ''.join(ch*2 for ch in h)
        a = 255 if opacity is None else max(0, min(255, int(float(opacity)*255)))
        return f'0x{a:02x}{h.lower()}'
    named = {'black':'0xff000000','white':'0xffffffff','none':None}
    return named.get(c, '0xff000000')

def main():
    svg_file, ns_name = sys.argv[1], sys.argv[2]
    tree = ET.parse(svg_file); root = tree.getroot()
    vb = root.get('viewBox').split()
    cx = float(vb[0]) + float(vb[2]) / 2
    cy = float(vb[1]) + float(vb[3]) / 2
    paths = collect(root)
    body = []
    for p in paths:
        fill = p.get('fill'); stroke = p.get('stroke')
        sw = p.get('stroke-width'); fo = p.get('fill-opacity')
        subs = parse_path(p.get('d'))
        def emit_points():
            for sub in subs:
                if len(sub) < 2: continue
                x0, y0 = sub[0]
                body.append(f'        ctx.move_to({x0-cx:.2f}f, {y0-cy:.2f}f);')
                for (px, py) in sub[1:]:
                    body.append(f'        ctx.line_to({px-cx:.2f}f, {py-cy:.2f}f);')
        # Fill (skip fill:none)
        if fill and fill != 'none':
            col = hexcol(fill, fo)
            if col:
                body.append(f'        ctx.set_fill({col});')
                body.append('        ctx.begin_path();')
                emit_points()
                body.append('        ctx.fill();')
        # Stroke (dandelion spokes, bee outline)
        if stroke and stroke != 'none':
            col = hexcol(stroke)
            body.append(f'        ctx.set_stroke({col});')
            body.append(f'        ctx.set_line_width({float(sw) if sw else 1.0:.2f}f);')
            body.append('        ctx.round_line_cap();')
            body.append('        ctx.round_line_join();')
            body.append('        ctx.begin_path();')
            emit_points()
            body.append('        ctx.stroke();')
    print('#pragma once')
    print(f'// Auto-generated from {svg_file} by scripts/svg_to_draw.py (110 viewBox,')
    print('// curves/arcs flattened to polylines). draw() is centred at the origin;')
    print('// scale r/45 maps the tile content to a mob of radius r.')
    print('#include <Client/Render/Renderer.hh>')
    print(f'namespace {ns_name} {{')
    print('    inline void draw(Renderer &ctx, float r) {')
    print('        RenderContext _c(&ctx);')
    print('        ctx.scale(r / 45.0f);')
    print('\n'.join(body))
    print('    }')
    print('}')

if __name__ == '__main__':
    main()
