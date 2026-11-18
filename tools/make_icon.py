"""Generates the app icons. Standard library only; run from anywhere:

    python3 tools/make_icon.py

Writes:
  assets/dhundarsh.ico, assets/dhundarsh_256.png        - main logo: three flowing
      staff lines with an eighth note, blue on a white rounded square
  assets/alt/spectrum.ico, assets/alt/spectrum_256.png  - alternate: a small
      circular spectrum on a dark rounded square
"""
import colorsys
import math
import os
import struct
import zlib

SIZES = [256, 128, 64, 48, 32, 24, 16]


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

def rounded_square(x, y, corner=0.21):
    """True if (x, y) in 0..1 lies inside a rounded square filling the canvas."""
    cx, cy = abs(x - 0.5), abs(y - 0.5)
    qx, qy = max(cx - (0.5 - corner), 0.0), max(cy - (0.5 - corner), 0.0)
    return math.hypot(qx, qy) <= corner


def downscale(rgba, src, dst):
    """Area-average resize (alpha-weighted) from src x src to dst x dst."""
    ratio = src / dst
    out = bytearray()
    for y in range(dst):
        y0, y1 = int(y * ratio), max(int((y + 1) * ratio), int(y * ratio) + 1)
        for x in range(dst):
            x0, x1 = int(x * ratio), max(int((x + 1) * ratio), int(x * ratio) + 1)
            acc = [0, 0, 0, 0]
            for yy in range(y0, y1):
                row = yy * src
                for xx in range(x0, x1):
                    i = (row + xx) * 4
                    a = rgba[i + 3]
                    acc[0] += rgba[i] * a
                    acc[1] += rgba[i + 1] * a
                    acc[2] += rgba[i + 2] * a
                    acc[3] += a
            n = (y1 - y0) * (x1 - x0)
            if acc[3]:
                out += bytes(min(255, acc[c] // acc[3]) for c in range(3))
            else:
                out += b"\0\0\0"
            out.append(acc[3] // n)
    return bytes(out)


def png(rgba, size):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    stride = size * 4
    raw = b"".join(b"\0" + rgba[y * stride:(y + 1) * stride] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def ico(images):
    """images: list of (size, png_bytes). PNG-compressed ICO entries (Windows Vista+)."""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, data = b"", b""
    for size, blob in images:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def save(master, master_size, folder, name):
    images = [(s, png(downscale(master, master_size, s), s)) for s in SIZES]
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, name + ".ico"), "wb") as f:
        f.write(ico(images))
    with open(os.path.join(folder, name + "_256.png"), "wb") as f:
        f.write(images[0][1])


# ---------------------------------------------------------------------------
# Main logo: staff lines and an eighth note
# ---------------------------------------------------------------------------

INK = (0x3F, 0x4B, 0xD0)


def wave_y(base, t):
    """Staff line shape: dips at the left end, runs flat, sweeps up at the right."""
    left = max(0.0, 1.0 - t / 0.25) ** 2
    right = max(0.0, (t - 0.62) / 0.38) ** 2
    return base + 0.045 * left - 0.075 * right


def note_icon(size=1024):
    """Rasterises strokes by stamping discs into a coverage mask at `size`."""
    ink = bytearray(size * size)

    def disc(cx, cy, r):
        x0, x1 = max(int(cx - r), 0), min(int(cx + r) + 1, size)
        y0, y1 = max(int(cy - r), 0), min(int(cy + r) + 1, size)
        r2 = r * r
        for y in range(y0, y1):
            dy = y + 0.5 - cy
            row = y * size
            for x in range(x0, x1):
                dx = x + 0.5 - cx
                if dx * dx + dy * dy <= r2:
                    ink[row + x] = 1

    def stroke(points, width):
        """points in 0..1 units; stamps discs densely along each segment."""
        r = width * size / 2
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            steps = max(int(math.hypot(bx - ax, by - ay) * size / (r * 0.25)), 1)
            for i in range(steps + 1):
                t = i / steps
                disc((ax + (bx - ax) * t) * size, (ay + (by - ay) * t) * size, r)

    width = 0.03
    x_start, x_end = 0.12, 0.88
    for base in (0.40, 0.55, 0.70):
        pts = []
        for i in range(81):
            t = i / 80
            pts.append((x_start + (x_end - x_start) * t, wave_y(base, t)))
        stroke(pts, width)

    # Note: stem, flag (ending in a small dot), and a filled, tilted head.
    stem_x = 0.51
    stroke([(stem_x, 0.16), (stem_x, 0.79)], width)
    flag = [(stem_x + 0.11 * t, 0.16 + 0.09 * t) for t in (i / 20 for i in range(21))]
    stroke(flag, width)
    disc(flag[-1][0] * size, flag[-1][1] * size, width * size * 0.75)

    hx, hy, rx, ry, tilt = stem_x - 0.04, 0.80, 0.05, 0.075, math.radians(20)
    c, s = math.cos(tilt), math.sin(tilt)
    for y in range(int((hy - 0.1) * size), int((hy + 0.1) * size)):
        for x in range(int((hx - 0.1) * size), int((hx + 0.1) * size)):
            dx, dy = (x + 0.5) / size - hx, (y + 0.5) / size - hy
            u, v = dx * c + dy * s, -dx * s + dy * c
            if (u / rx) ** 2 + (v / ry) ** 2 <= 1.0:
                ink[y * size + x] = 1

    # Compose: white-to-lavender rounded square with a soft border, blue ink on top.
    out = bytearray(size * size * 4)
    for y in range(size):
        fy = (y + 0.5) / size
        bg = (255 - int(14 * fy), 255 - int(10 * fy), 255)
        for x in range(size):
            fx = (x + 0.5) / size
            i = (y * size + x) * 4
            if not rounded_square(fx, fy):
                continue
            if ink[y * size + x]:
                out[i:i + 4] = bytes((*INK, 255))
            elif not rounded_square(0.5 + (fx - 0.5) * 1.03, 0.5 + (fy - 0.5) * 1.03):
                out[i:i + 4] = bytes((0xC6, 0xCB, 0xF0, 255))   # thin border
            else:
                out[i:i + 4] = bytes((*bg, 255))
    return bytes(out)


# ---------------------------------------------------------------------------
# Alternate: circular spectrum
# ---------------------------------------------------------------------------

BARS = 40


def bar_length(k):
    band = k if k < BARS // 2 else BARS - 1 - k
    t = band / (BARS // 2 - 1)
    return 0.07 + 0.11 * (0.55 + 0.45 * math.sin(t * 7.0 + 0.6)) * (1.0 - 0.45 * t)


def spectrum_shade(x, y):
    if not rounded_square(x, y):
        return (0.0, 0.0, 0.0, 0.0)
    top, bottom = (0.11, 0.08, 0.25), (0.03, 0.03, 0.08)
    col = [top[i] + (bottom[i] - top[i]) * y for i in range(3)]

    cx, cy = x - 0.5, y - 0.5
    r = math.hypot(cx, cy)
    angle = math.atan2(cx, -cy) % (2 * math.pi)

    inner = 0.235
    pos = angle / (2 * math.pi) * BARS
    k = int(pos) % BARS
    within = pos - int(pos)
    if 0.2 < within < 0.8 and inner < r < inner + bar_length(k):
        band = k if k < BARS // 2 else BARS - 1 - k
        hue = 0.50 + 0.42 * band / (BARS // 2 - 1)
        along = (r - inner) / bar_length(k)
        rgb = colorsys.hsv_to_rgb(hue % 1.0, 0.75 - 0.35 * along, 0.65 + 0.35 * along)
        return (*rgb, 1.0)

    ring = math.exp(-((r - 0.19) / 0.018) ** 2)
    core = 0.35 * math.exp(-(r / 0.13) ** 2)
    glow = min(ring + core, 1.0)
    col = [c + (g - c) * glow for c, g in zip(col, (0.85, 0.9, 1.0))]
    return (*col, 1.0)


def spectrum_icon(size=512, samples=2):
    out = bytearray()
    step = 1.0 / (size * samples)
    for py in range(size):
        for px in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(samples):
                for sx in range(samples):
                    r, g, b, a = spectrum_shade((px * samples + sx + 0.5) * step, (py * samples + sy + 0.5) * step)
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            alpha = acc[3] / (samples * samples)
            if alpha > 0:
                out += bytes(int(min(acc[i] / acc[3], 1.0) * 255 + 0.5) for i in range(3))
            else:
                out += b"\0\0\0"
            out.append(int(alpha * 255 + 0.5))
    return bytes(out)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assets = os.path.join(root, "assets")

    save(note_icon(1024), 1024, assets, "dhundarsh")
    print("wrote assets/dhundarsh.ico and assets/dhundarsh_256.png")

    save(spectrum_icon(512), 512, os.path.join(assets, "alt"), "spectrum")
    print("wrote assets/alt/spectrum.ico and assets/alt/spectrum_256.png")


if __name__ == "__main__":
    main()
